'use strict';

// Keep chart accounting independent of the DOM so both views use the same data.
function clusterCoreUsage(data, mode) {
  const users = new Map();
  let unknown = 0;
  for (const job of data.jobs) {
    if (!/^(RUNNING|COMPLETING|SUSPENDED|CONFIGURING)/i.test(job.state)) continue;
    if (!Number.isFinite(job.reserved_cores)) { unknown++; continue; }
    if (job.reserved_cores > 0) users.set(job.user, (users.get(job.user) || 0) + job.reserved_cores);
  }
  const entries = [...users].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]))
    .map(([name, cores]) => ({name, cores}));
  const known = entries.reduce((sum, item) => sum + item.cores, 0);
  const allocated = Math.max(known, data.summary.allocated_cores || 0);
  const capacity = data.summary.total_cores;
  if (mode === 'total' && (!Number.isFinite(capacity) || capacity <= 0 || allocated > capacity)) {
    return {entries: [], denominator: 0, allocated, capacity, unknown, users: users.size,
      error: 'Total cluster capacity is unavailable or inconsistent with allocations. Try Refresh.'};
  }
  if (allocated > known) entries.push({name: 'Allocated · user unavailable', cores: allocated - known, color: '#929eaf'});
  if (mode === 'total' && capacity > allocated) entries.push({name: 'Unallocated cores', cores: capacity - allocated, color: '#d3dcd4'});
  return {entries, denominator: mode === 'total' ? capacity : allocated, allocated, capacity, unknown, users: users.size};
}
// Ownership is the SSH account, independent of how a task was submitted.
function clusterTasksForUser(data) {
  return data.jobs.filter(job => job.user === data.user && job.state.toUpperCase().startsWith('RUNNING'));
}

(() => {
  const find = selector => document.querySelector(selector);
  const host = document.body.dataset.host;
  const partitionSelect = find('#cluster-partition');
  const nodeScope = find('#cluster-node-scope');
  const jobChoice = find('#cluster-job-choice');
  if (!partitionSelect) return;
  let status = null;
  let coreUsageData = null;
  const coreUsageMode = find('#cluster-core-mode');
  let busy = false;
  let queued = false;
  const expandedNodes = new Set();
  const sorts = {nodes: {key: 'name', direction: 'asc'}, users: {key: 'jobs', direction: 'desc'},
                 jobs: {key: 'id', direction: 'desc'}};
  function ordered(kind, items) {
    const {key, direction} = sorts[kind], sign = direction === 'asc' ? 1 : -1;
    return [...items].sort((left, right) => {
      const a = left[key], b = right[key];
      if (a == null) return b == null ? 0 : 1;
      if (b == null) return -1;
      if (typeof a === 'number' && typeof b === 'number') return sign * (a - b);
      return sign * String(a).localeCompare(String(b), undefined, {numeric: true});
    });
  }
  document.querySelectorAll('table[data-cluster-sort]').forEach(table => {
    const kind = table.dataset.clusterSort;
    table.querySelectorAll('th[data-key]').forEach(th => {
      const label = th.textContent;
      const control = document.createElement('button');
      control.type = 'button';
      control.className = 'sort-button secondary';
      control.dataset.label = label;
      control.addEventListener('click', () => {
        const sort = sorts[kind];
        sort.direction = sort.key === th.dataset.key && sort.direction === 'asc' ? 'desc' : 'asc';
        sort.key = th.dataset.key;
        updateSortLabels();
        render();
      });
      th.replaceChildren(control);
    });
  });
  function updateSortLabels() {
    document.querySelectorAll('table[data-cluster-sort]').forEach(table => {
      const sort = sorts[table.dataset.clusterSort];
      table.querySelectorAll('th[data-key]').forEach(th => {
        const control = th.querySelector('button');
        const active = th.dataset.key === sort.key;
        control.textContent = `${control.dataset.label} ${active ? (sort.direction === 'asc' ? '↑' : '↓') : '↕'}`;
        if (active) th.setAttribute('aria-sort', sort.direction === 'asc' ? 'ascending' : 'descending');
        else th.removeAttribute('aria-sort');
      });
    });
  }
  updateSortLabels();
  const query = new URLSearchParams(location.search);
  coreUsageMode.value = query.get('cores') === 'total' ? 'total' : 'allocated';
  coreUsageMode.addEventListener('change', () => {
    const url = new URL(location.href);
    if (coreUsageMode.value === 'total') url.searchParams.set('cores', 'total');
    else url.searchParams.delete('cores');
    history.replaceState(null, '', url);
    if (coreUsageData) renderCoreUsage(coreUsageData);
  });
  partitionSelect.value = query.get('partition') || 'CPU-amd';
  if (!partitionSelect.value) partitionSelect.add(new Option(query.get('partition'), query.get('partition'), true, true));
  nodeScope.value = ['all', 'mine', 'job'].includes(query.get('nodes')) ? query.get('nodes') : 'mine';
  const requestedJob = query.get('job') || '';
  if (requestedJob) jobChoice.add(new Option(requestedJob, requestedJob));
  jobChoice.value = requestedJob;
  function syncNodeScopeUrl() {
    const url = new URL(location.href);
    if (nodeScope.value === 'mine') url.searchParams.delete('nodes');
    else url.searchParams.set('nodes', nodeScope.value);
    if (nodeScope.value === 'job' && jobChoice.value) url.searchParams.set('job', jobChoice.value);
    else url.searchParams.delete('job');
    history.replaceState(null, '', url);
  }
  function updateJobChoices() {
    const previous = jobChoice.value;
    const mine = clusterTasksForUser(status);
    const names = [...new Set(mine.map(job => job.slurmy_id || job.id))].sort((a, b) => a.localeCompare(b, undefined, {numeric: true}));
    jobChoice.replaceChildren(...names.map(name => new Option(name, name)));
    if (names.includes(previous)) jobChoice.value = previous;
    find('#cluster-job-choice-label').hidden = nodeScope.value !== 'job';
    jobChoice.disabled = names.length === 0;
  }

  function cell(value) {
    const td = document.createElement('td');
    td.textContent = value == null || value === '' ? '—' : String(value);
    return td;
  }
  function row(values) {
    const tr = document.createElement('tr');
    for (const value of values) tr.append(value instanceof Node ? value : cell(value));
    return tr;
  }
  function empty(body, columns, message) {
    const td = cell(message);
    td.colSpan = columns;
    body.append(row([td]));
  }
  function match(values, filter) {
    return values.some(value => String(value ?? '').toLowerCase().includes(filter));
  }
  function slurmyCell(job) {
    const result = cell(job.jobpairs == null ? (job.slurmy ? 'Metadata unavailable' : '—') : `${job.jobpairs.toLocaleString()} solver-problem pairs / ${job.batches.toLocaleString()} batches`);
    result.title = job.metadata_source || '';
    if (job.jobpairs != null && job.metadata_source === 'private job files') {
      result.replaceChildren();
      const a = document.createElement('a');
      a.href = `/jobs/${encodeURIComponent(job.slurmy_id)}?${new URLSearchParams({host})}`;
      a.textContent = `${job.jobpairs.toLocaleString()} solver-problem pairs / ${job.batches.toLocaleString()} batches →`;
      result.append(a);
    }
    return result;
  }
  function renderCoreUsage(data) {
    const chart=find('#cluster-core-pie'), legend=find('#cluster-core-legend');
    chart.replaceChildren();legend.replaceChildren();
    const totalView = coreUsageMode.value === 'total';
    const usage = clusterCoreUsage(data, coreUsageMode.value);
    const {entries, denominator, allocated, capacity, unknown} = usage;
    const descriptions=[], segments=[];
    find('#cluster-core-chart-explanation').textContent = totalView
      ? 'Percentages show each user’s share of all physical cores across the cluster. Unallocated cores may include offline or otherwise unavailable nodes.'
      : 'Percentages show each user’s share of allocated physical cores across the cluster.';
    chart.classList.toggle('is-empty',denominator===0);let offset=0;
    const palette=['#278866','#4479b5','#9367ac','#ca842c','#219a98','#bb555b','#628b51','#a66b8b'];
    entries.forEach(({name,cores,color:fixedColor},index)=>{
      const color=fixedColor || (index<palette.length?palette[index]:`hsl(${index*137.508%360} 48% 48%)`);
      const percent=100*cores/denominator,end=offset+percent;
      segments.push(`${color} ${offset}% ${end}%`);offset=end;
      const item=document.createElement('div');item.className='outcome-row';
      const label=document.createElement('span');label.className='outcome-name';
      const swatch=document.createElement('span');swatch.className='outcome-swatch';swatch.style.backgroundColor=color;swatch.setAttribute('aria-hidden','true');
      label.append(swatch,document.createTextNode(name));
      const count=document.createElement('span');count.textContent=`${cores.toLocaleString()} ${cores===1?'core':'cores'}`;count.className='outcome-count';
      const share=document.createElement('strong');share.textContent=`${percent.toFixed(1)}%`;share.className='outcome-percent';
      item.append(label,count,share);legend.append(item);
      descriptions.push(`${name}: ${cores.toLocaleString()} cores (${percent.toFixed(1)}%)`);
    });
    chart.style.background=denominator?`conic-gradient(${segments.join(',')})`:'';
    const description=usage.error || (denominator?descriptions.join('; '):unknown?'Core allocation counts unavailable.':'No cores currently allocated.');
    chart.setAttribute('aria-label',description);
    window.SlurmyPie.setSegments(chart,entries.map((entry,index)=>({fraction:entry.cores/denominator,label:descriptions[index]})));
    if(!denominator)chart.textContent=usage.error || (unknown?'Unavailable':'No allocated cores');
    find('#cluster-core-chart-status').textContent=`${allocated.toLocaleString()} allocated${Number.isFinite(capacity)?` / ${capacity.toLocaleString()} total`:''} cores · ${usage.users} users · all partitions · refreshed ${new Date(data.updated*1000).toLocaleTimeString()}`+
      (unknown?` · User allocation counts unavailable for ${unknown} active tasks.`:'');
  }
  async function refreshCoreUsage(selected, selectedRequest, fresh){
    try{
      let data;
      if(selected==='all')data=await selectedRequest;
      else{
        const response=await fetch(`/api/cluster?${new URLSearchParams({host,partition:'all',fresh:fresh?'1':'0'})}`);
        data=await response.json();if(!response.ok)throw new Error(data.error||'Could not load allocations.');
      }
      coreUsageData = data;
      renderCoreUsage(data);
    }catch(error){
      find('#cluster-core-chart-status').textContent=`Whole-cluster allocations unavailable: ${error.message}. Any previous chart remains visible.`;
    }
  }
  function render() {
    if (!status) return;
    const totals = status.summary;
    find('#cluster-node-user-scope').textContent = `“Running my tasks” includes every running SLURM task owned by ${status.user} in the selected partition, including tasks submitted outside Slurmy.`;
    find('#cluster-free').textContent = `${totals.free_nodes} / ${totals.nodes}`;
    find('#cluster-utilization').textContent = `${totals.utilization}%`;
    find('#cluster-utilization').title = `${totals.allocated_cores.toLocaleString()} of ${totals.total_cores.toLocaleString()} physical cores allocated`;
    find('#cluster-running').textContent = totals.running.toLocaleString();
    find('#cluster-pending').textContent = totals.pending.toLocaleString();
    find('#cluster-user-count').textContent = `· ${status.users.length}`;
    find('#cluster-job-count').textContent = `· ${status.jobs.length}`;

    const nodes = find('#cluster-nodes');
    nodes.replaceChildren();
    const nodeFilter = find('#cluster-node-filter').value.trim().toLowerCase();
    const running = status.jobs.filter(job => job.state.toUpperCase().startsWith('RUNNING'));
    const relevant = nodeScope.value === 'all' ? null : clusterTasksForUser(status).filter(job =>
      nodeScope.value === 'mine' || (job.slurmy_id || job.id) === jobChoice.value);
    const visible = status.nodes.filter(item => match([item.name, item.state], nodeFilter) &&
      (relevant === null || relevant.some(job => job.hostnames.includes(item.name))));
    find('#cluster-node-count').textContent = `· ${visible.length} / ${status.nodes.length} shown`;
    for (const node of ordered('nodes', visible)) {
      const used = cell(`${node.allocated} / ${node.total}`);
      const meter = document.createElement('progress');
      meter.max = node.total || 1;
      meter.value = node.allocated;
      used.append(meter);
      const name = document.createElement('td');
      const details = document.createElement('details');
      details.className = 'cluster-node-jobs-inline';
      details.open = expandedNodes.has(node.name);
      const summary = document.createElement('summary');
      summary.textContent = node.name;
      details.append(summary);
      const onNode = running.filter(job => job.hostnames.includes(node.name));
      const list = document.createElement('div');
      list.className = 'cluster-node-job-list';
      for (const job of onNode) {
        const item = document.createElement('div');
        const title = job.slurmy && job.user === status.user
          ? document.createElement('a') : document.createElement('span');
        title.textContent = `${job.name} · task ${job.id}`;
        if (title.tagName === 'A') title.href = `/jobs/${encodeURIComponent(job.slurmy_id)}?${new URLSearchParams({host})}`;
        const allocated = job.node_cores[node.name];
        item.append(title, document.createTextNode(` — ${job.user}, ${allocated == null ? 'core allocation unavailable' : `${allocated} physical cores allocated`}`));
        if (job.active_call) {
          const call = document.createElement('div');
          call.className = 'cluster-active-call';
          const detail = document.createElement('a');
          detail.href = `/jobs/${encodeURIComponent(job.slurmy_id)}?${new URLSearchParams({host, call: job.active_call.id})}`;
          detail.textContent = `${job.active_call.solver} — ${job.active_call.problem.split('/').pop()} · solver-problem pair ${job.active_call.id}`;
          detail.title = job.active_call.problem;
          call.append(detail);
          item.append(call);
        }
        list.append(item);
      }
      if (!onNode.length) list.textContent = 'No running tasks on this node.';
      details.append(list);
      details.addEventListener('toggle', () => {if (details.open) expandedNodes.add(node.name); else expandedNodes.delete(node.name);});
      name.append(details);
      const nodeRow = row([name, node.state, used, node.idle, node.other, node.total,
                           `${(node.memory_mib / 1024).toFixed(0)} GiB`]);
      nodes.append(nodeRow);
    }
    if (!nodes.children.length) empty(nodes, 7, nodeScope.value === 'all' ? 'No matching nodes.' : 'No nodes currently running matching tasks.');

    const users = find('#cluster-users');
    users.replaceChildren();
    const userFilter = find('#cluster-user-filter').value.trim().toLowerCase();
    for (const user of ordered('users', status.users.filter(item => match([item.name], userFilter)))) {
      users.append(row([user.name, user.jobs, user.running, user.pending, user.allocated_cores]));
    }
    if (!users.children.length) empty(users, 5, 'No matching users.');

    const jobs = find('#cluster-jobs');
    jobs.replaceChildren();
    const jobFilter = find('#cluster-job-filter').value.trim().toLowerCase();
    for (const job of ordered('jobs', status.jobs.filter(item => match([item.id, item.name, item.user, item.state, item.nodelist, item.slurmy_id], jobFilter)))) {
      const name = cell(job.name);
      if (job.slurmy) {
        const tag = document.createElement('small');
        tag.className = 'cluster-slurmy-label';
        tag.textContent = `Slurmy job · ${job.slurmy_id}`;
        name.append(tag);
      }
      const state = cell(job.state);
      state.title = job.reason || '';
      jobs.append(row([job.id, name, job.user, state, job.nodelist || job.reason || job.nodes,
                        job.reserved_cores, `${job.elapsed} / ${job.limit}`, slurmyCell(job)]));
    }
    if (!jobs.children.length) empty(jobs, 8, 'No matching active SLURM tasks.');
  }

  async function refresh(fresh = false) {
    if (busy) { queued = true; return; }
    busy = true;
    const button = find('#cluster-refresh');
    button.disabled = true;
    const message = find('#cluster-connection');
    message.textContent = status ? 'Refreshing cluster status… Previously loaded data remains visible.' : `Connecting to ${host}…`;
    try {
      const selected = partitionSelect.value;
      const url = `/api/cluster?${new URLSearchParams({host, partition: selected, fresh: fresh ? '1' : '0'})}`;
      const selectedRequest=fetch(url).then(async response=>{const data=await response.json();if(!response.ok)throw new Error(data.error||`Request failed (${response.status})`);return data;});
      const chartRequest=refreshCoreUsage(selected,selectedRequest,fresh);
      let data;try{data=await selectedRequest;}finally{await chartRequest;}
      if (partitionSelect.value !== selected) { queued = true; return; }
      status = data;
      const prior = selected;
      partitionSelect.replaceChildren(new Option('All partitions', 'all'), ...data.partitions.map(name => new Option(name, name)));
      if (![...partitionSelect.options].some(option => option.value === prior)) partitionSelect.add(new Option(prior, prior));
      partitionSelect.value = prior;
      updateJobChoices();
      render();
      message.textContent = `Connected to ${host} · ${selected} · refreshed ${new Date(data.updated * 1000).toLocaleTimeString()} · refreshes every 30 seconds`;
    } catch (error) {
      message.textContent = `Cluster data unavailable: ${error.message}. ${status ? 'Previously loaded data remains visible.' : 'Check SSH access, VPN, and partition name.'}`;
      if (!status) {
        for (const [selector, count] of [['#cluster-nodes', 7], ['#cluster-users', 5], ['#cluster-jobs', 8]]) {
          const body = find(selector);
          body.replaceChildren();
          empty(body, count, 'Could not load cluster data. Try Refresh.');
        }
      }
    } finally {
      button.disabled = false;
      busy = false;
      if (queued) { queued = false; queueMicrotask(() => refresh(true)); }
    }
  }
  partitionSelect.addEventListener('change', () => {
    const url = new URL(location.href);
    url.searchParams.set('partition', partitionSelect.value);
    history.replaceState(null, '', url);
    status = null;
    refresh(true);
  });
  find('#cluster-refresh').addEventListener('click', () => refresh(true));
  nodeScope.addEventListener('change', () => { updateJobChoices(); syncNodeScopeUrl(); render(); });
  jobChoice.addEventListener('change', () => { syncNodeScopeUrl(); render(); });
  for (const selector of ['#cluster-node-filter', '#cluster-user-filter', '#cluster-job-filter']) {
    find(selector).addEventListener('input', render);
  }
  refresh();
  setInterval(() => refresh(true), 30000);
})();
