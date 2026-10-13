'use strict';
(() => {
  const find = selector => document.querySelector(selector);
  const host = document.body.dataset.host;
  const partitionSelect = find('#cluster-partition');
  const nodeScope = find('#cluster-node-scope');
  const jobChoice = find('#cluster-job-choice');
  if (!partitionSelect) return;
  let status = null;
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
  partitionSelect.value = query.get('partition') || 'CPU-amd';
  if (!partitionSelect.value) partitionSelect.add(new Option(query.get('partition'), query.get('partition'), true, true));
  if (['mine', 'job'].includes(query.get('nodes'))) nodeScope.value = query.get('nodes');
  const requestedJob = query.get('job') || '';
  if (requestedJob) jobChoice.add(new Option(requestedJob, requestedJob));
  jobChoice.value = requestedJob;
  function syncNodeScopeUrl() {
    const url = new URL(location.href);
    if (nodeScope.value === 'all') url.searchParams.delete('nodes');
    else url.searchParams.set('nodes', nodeScope.value);
    if (nodeScope.value === 'job' && jobChoice.value) url.searchParams.set('job', jobChoice.value);
    else url.searchParams.delete('job');
    history.replaceState(null, '', url);
  }
  function updateJobChoices() {
    const previous = jobChoice.value;
    const mine = status.jobs.filter(job => job.user === status.user && job.state.toUpperCase().startsWith('RUNNING'));
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
    const result = cell(job.jobpairs == null ? (job.slurmy ? 'Metadata unavailable' : '—') : `${job.jobpairs.toLocaleString()} calls / ${job.batches.toLocaleString()} batches`);
    result.title = job.metadata_source || '';
    if (job.jobpairs != null && job.metadata_source === 'private job files') {
      result.replaceChildren();
      const a = document.createElement('a');
      a.href = `/jobs/${encodeURIComponent(job.slurmy_id)}?${new URLSearchParams({host})}`;
      a.textContent = `${job.jobpairs.toLocaleString()} calls / ${job.batches.toLocaleString()} batches →`;
      result.append(a);
    }
    return result;
  }
  function render() {
    if (!status) return;
    const totals = status.summary;
    find('#cluster-free').textContent = `${totals.free_nodes} / ${totals.nodes}`;
    find('#cluster-utilization').textContent = `${totals.utilization}%`;
    find('#cluster-utilization').title = `${totals.allocated_cpus.toLocaleString()} of ${totals.total_cpus.toLocaleString()} Slurm CPUs allocated`;
    find('#cluster-running').textContent = totals.running.toLocaleString();
    find('#cluster-pending').textContent = totals.pending.toLocaleString();
    find('#cluster-user-count').textContent = `· ${status.users.length}`;
    find('#cluster-job-count').textContent = `· ${status.jobs.length}`;

    const nodes = find('#cluster-nodes');
    nodes.replaceChildren();
    const nodeFilter = find('#cluster-node-filter').value.trim().toLowerCase();
    const running = status.jobs.filter(job => job.state.toUpperCase().startsWith('RUNNING'));
    const relevant = nodeScope.value === 'all' ? null : running.filter(job => job.user === status.user &&
      (nodeScope.value === 'mine' || (job.slurmy_id || job.id) === jobChoice.value));
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
        title.textContent = `${job.name} · ${job.id}`;
        if (title.tagName === 'A') title.href = `/jobs/${encodeURIComponent(job.slurmy_id)}?${new URLSearchParams({host})}`;
        item.append(title, document.createTextNode(` — ${job.user}, ${job.cpus} Slurm CPUs`));
        list.append(item);
      }
      if (!onNode.length) list.textContent = 'No running jobs on this node.';
      details.append(list);
      details.addEventListener('toggle', () => {if (details.open) expandedNodes.add(node.name); else expandedNodes.delete(node.name);});
      name.append(details);
      const nodeRow = row([name, node.state, used, node.idle, node.other, node.total,
                           `${(node.memory_mib / 1024).toFixed(0)} GiB`]);
      nodes.append(nodeRow);
    }
    if (!nodes.children.length) empty(nodes, 7, nodeScope.value === 'all' ? 'No matching nodes.' : 'No nodes currently running matching jobs.');

    const users = find('#cluster-users');
    users.replaceChildren();
    const userFilter = find('#cluster-user-filter').value.trim().toLowerCase();
    for (const user of ordered('users', status.users.filter(item => match([item.name], userFilter)))) {
      users.append(row([user.name, user.jobs, user.running, user.pending, user.allocated_cpus]));
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
        tag.textContent = `Slurmy · ${job.slurmy_id}`;
        name.append(tag);
      }
      const state = cell(job.state);
      state.title = job.reason || '';
      jobs.append(row([job.id, name, job.user, state, job.nodelist || job.reason || job.nodes,
                       job.cpus, `${job.elapsed} / ${job.limit}`, slurmyCell(job)]));
    }
    if (!jobs.children.length) empty(jobs, 8, 'No matching active Slurm jobs.');
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
      const response = await fetch(url);
      const data = await response.json();
      if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`);
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
