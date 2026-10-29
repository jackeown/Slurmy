import * as THREE from './vendor/three/three.module.js';
import { OrbitControls } from './vendor/three/OrbitControls.js';

const canvas = document.querySelector('#outcome-cube');
const panel = document.querySelector('#outcome-cube-panel');
const viewport = document.querySelector('#outcome-cube-viewport');
const tooltip = document.querySelector('#outcome-cube-tooltip');
const legend = document.querySelector('#outcome-cube-legend');
const table = document.querySelector('#outcome-cube-table');
const toggle = document.querySelector('#toggle-outcome-cube');
const scene = new THREE.Scene();
const chart = new THREE.Group();
scene.add(chart);
const camera = new THREE.PerspectiveCamera(42, 1, 0.1, 1000);
let renderer;
try { renderer = new THREE.WebGLRenderer({canvas, antialias: true, alpha: true}); }
catch (error) {
  viewport.insertAdjacentText('beforeend', '3D graphics are unavailable in this browser. The exact counts remain available below.');
  canvas.hidden = true;
}
const controls = renderer && new OrbitControls(camera, canvas);
if (controls) {
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;
  controls.screenSpacePanning = true;
  controls.minPolarAngle = 0.08;
  controls.maxPolarAngle = Math.PI / 2 - 0.03;
  controls.minDistance = 2;
  controls.maxDistance = 120;
  controls.addEventListener('change', render);
}
scene.add(new THREE.AmbientLight(0xffffff, 2));
const lamp = new THREE.DirectionalLight(0xffffff, 2.5);
lamp.position.set(-3, 8, 5); scene.add(lamp);
const raycaster = new THREE.Raycaster(), pointer = new THREE.Vector2(), bars = [];
let statuses = [], solvers = [], counts = [];
const axisLabels = [];
let themeDirty = false, themeFrame = null;
let latestItems = [], preferredStatusOrder = [];
try { const saved = JSON.parse(localStorage.getItem('slurmy-cube-status-order') || '[]'); if (Array.isArray(saved)) preferredStatusOrder = saved.filter(key => typeof key === 'string'); } catch {}
const countSort = {key: 'solver', direction: 'asc'};

function make(tag, label) { const node = document.createElement(tag); if (label !== undefined) node.textContent = label; return node; }
function labelFor(item) { return (item.label === 'Not finished' ? 'Not Finished' : item.label) + (item.source ? ` (${item.source})` : ''); }
function renderTable() {
  table.replaceChildren(); legend.replaceChildren();
  const head = make('thead'), header = make('tr');
  for (const [index, title] of ['Solver', ...statuses.map(labelFor), 'Total'].entries()) {
    const key = index === 0 ? 'solver' : index === statuses.length + 1 ? 'total' : `status-${index - 1}`;
    const cell = make('th'), control = make('button', `${title} ${countSort.key === key ? countSort.direction === 'asc' ? '↑' : '↓' : '↕'}`);
    cell.scope = 'col';
    if (countSort.key === key) cell.setAttribute('aria-sort', countSort.direction === 'asc' ? 'ascending' : 'descending');
    control.type = 'button'; control.className = 'sort-button secondary';
    control.title = `Sort by ${title}`; control.setAttribute('aria-label', `Sort by ${title}`);
    control.addEventListener('click', () => {
      countSort.direction = countSort.key === key && countSort.direction === 'asc' ? 'desc' : 'asc';
      countSort.key = key; renderTable();
    });
    cell.append(control); header.append(cell);
  }
  head.append(header); table.append(head);
  const body = make('tbody');
  const rows = solvers.map((_, x) => x);
  rows.sort((a, b) => {
    if (countSort.key === 'solver') return (countSort.direction === 'asc' ? 1 : -1) * solvers[a].localeCompare(solvers[b]);
    const value = x => countSort.key === 'total' ? counts[x].reduce((sum, count) => sum + count, 0) : counts[x][Number(countSort.key.slice(7))];
    return (countSort.direction === 'asc' ? 1 : -1) * (value(a) - value(b)) || solvers[a].localeCompare(solvers[b]);
  });
  rows.forEach(x => {
    const solver = solvers[x];
    const row = make('tr'), name = make('th', solver); name.scope = 'row'; row.append(name);
    counts[x].forEach(count => row.append(make('td', count.toLocaleString())));
    row.append(make('td', counts[x].reduce((sum, count) => sum + count, 0).toLocaleString()));
    body.append(row);
  });
  table.append(body);
  statuses.forEach((status, z) => {
    const item = make('span'), swatch = make('i'), grip = make('span', '⠿');
    swatch.style.background = status.color;
    grip.className = 'legend-grip'; grip.setAttribute('aria-hidden', 'true');
    item.className = 'outcome-legend-item'; item.tabIndex = 0;
    item.dataset.statusKey = cubeStatusKey(status);
    item.setAttribute('role', 'listitem');
    item.setAttribute('aria-label', `${labelFor(status)}, position ${z + 1} of ${statuses.length}. Drag to reorder.`);
    item.title = 'Drag to reorder';
    item.append(grip, swatch, document.createTextNode(`${z + 1}. ${labelFor(status)}`));
    let drag = null;
    const clearDrag = () => {
      if (drag?.placeholder) drag.placeholder.remove();
      if (drag?.active) {
        item.hidePopover();
        item.removeAttribute('popover');
        item.style.cssText = drag.originalStyle;
      }
      drag = null; item.classList.remove('dragging');
    };
    item.addEventListener('pointerdown', event => {
      if (event.button !== 0 || !event.isPrimary) return;
      const bounds = item.getBoundingClientRect();
      drag = {pointer: event.pointerId, x: event.clientX, y: event.clientY,
        offsetX: event.clientX - bounds.left, offsetY: event.clientY - bounds.top,
        width: bounds.width, height: bounds.height, originalStyle: item.style.cssText,
        active: false, to: z};
      item.setPointerCapture(event.pointerId);
    });
    item.addEventListener('pointermove', event => {
      if (!drag || event.pointerId !== drag.pointer) return;
      if (!drag.active && Math.hypot(event.clientX - drag.x, event.clientY - drag.y) < 5) return;
      if (!drag.active) {
        drag.active = true;
        const placeholder = make('span');
        placeholder.className = 'legend-placeholder';
        placeholder.setAttribute('aria-hidden', 'true');
        placeholder.style.width = `${drag.width}px`; placeholder.style.height = `${drag.height}px`;
        item.before(placeholder); drag.placeholder = placeholder;
        item.classList.add('dragging');
        // Glass/filtered panels establish a containing block for fixed children.
        // The browser's top layer uses viewport coordinates regardless of those
        // ancestors, while keeping the real item and pointer capture intact.
        item.setAttribute('popover', 'manual');
        item.showPopover();
        item.style.width = `${drag.width}px`; item.style.height = `${drag.height}px`;
      }
      // Move the actual item with the pointer, preserving the initial grab offset.
      item.style.left = `${event.clientX - drag.offsetX}px`;
      item.style.top = `${event.clientY - drag.offsetY}px`;
      const bounds = legend.getBoundingClientRect();
      if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) {
        drag.to = -1; return;
      }
      const hit = document.elementFromPoint(event.clientX, event.clientY);
      if (hit === drag.placeholder) {
        drag.to = [...legend.children].filter(node => node !== item).indexOf(drag.placeholder);
        return;
      }
      const candidates = [...legend.querySelectorAll('.outcome-legend-item')].filter(node => node !== item);
      // Nearest rectangle also handles gaps between chips and wrapped rows.
      const distance = node => {
        const box = node.getBoundingClientRect();
        return Math.max(box.left - event.clientX, 0, event.clientX - box.right) ** 2 +
          Math.max(box.top - event.clientY, 0, event.clientY - box.bottom) ** 2;
      };
      const target = candidates.sort((a, b) => distance(a) - distance(b))[0];
      if (target) {
        const box = target.getBoundingClientRect();
        if (event.clientX >= box.left + box.width / 2) target.after(drag.placeholder);
        else target.before(drag.placeholder);
      }
      drag.to = [...legend.children].filter(node => node !== item).indexOf(drag.placeholder);
    });
    item.addEventListener('pointerup', event => {
      if (!drag || event.pointerId !== drag.pointer) return;
      const dropped = drag;
      clearDrag();
      if (item.hasPointerCapture(event.pointerId)) item.releasePointerCapture(event.pointerId);
      if (dropped.active && dropped.to >= 0) moveLegendStatus(item.dataset.statusKey, dropped.to);
    });
    item.addEventListener('pointercancel', clearDrag);
    item.addEventListener('lostpointercapture', clearDrag);
    legend.append(item);
  });
}
function moveLegendStatus(key, to) {
  const keys = statuses.map(cubeStatusKey), from = keys.indexOf(key);
  if (from < 0 || to < 0 || to >= keys.length || from === to) return;
  keys.splice(from, 1); keys.splice(to, 0, key);
  preferredStatusOrder = [...keys, ...preferredStatusOrder.filter(saved => !keys.includes(saved))];
  try { localStorage.setItem('slurmy-cube-status-order', JSON.stringify(preferredStatusOrder)); } catch {}
  update(latestItems, false);
  [...legend.children].find(item => item.dataset.statusKey === key)?.focus();
  const message = document.querySelector('#outcome-cube-reorder-status');
  if (message) message.textContent = `${labelFor(statuses[to])} moved to position ${to + 1}.`;
}
function clearChart() {
  while (chart.children.length) {
    const item = chart.children[0]; chart.remove(item);
    item.geometry?.dispose(); item.material?.map?.dispose(); item.material?.dispose();
  }
  bars.length = 0; axisLabels.length = 0;
}
function addLabel(text, x, y, z, width = 2.2) {
  const surface = document.createElement('canvas'); surface.width = 512; surface.height = 80;
  const ctx = surface.getContext('2d');
  ctx.font = '600 32px ' + (getComputedStyle(document.documentElement).getPropertyValue('--theme-font').trim() || 'system-ui');
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--ink').trim() || '#20332e';
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(text, 256, 40, 480);
  const texture = new THREE.CanvasTexture(surface);
  const sprite = new THREE.Sprite(new THREE.SpriteMaterial({map: texture, transparent: true, depthTest: false}));
  sprite.scale.set(width, width * 80 / 512, 1); sprite.position.set(x, y, z); chart.add(sprite); return sprite;
}
function addLine(x1, y1, z1, x2, y2, z2, color) {
  const geometry = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(x1, y1, z1), new THREE.Vector3(x2, y2, z2)]);
  const line = new THREE.Line(geometry, new THREE.LineBasicMaterial({color, transparent: true, opacity: 0.45}));
  chart.add(line); return line;
}
function resetCamera() {
  if (!controls) return;
  const extent = Math.max(solvers.length + 1, statuses.length + 1, 4);
  controls.target.set((solvers.length - 1) / 2, 1.1, (statuses.length - 1) / 2);
  camera.position.copy(controls.target).add(new THREE.Vector3(extent * 0.95, extent * 0.8, extent * 1.05));
  controls.update(); controls.saveState(); render();
}
function rebuild(resetView = false) {
  clearChart();
  if (!statuses.length || !solvers.length) { render(); return; }
  const maximum = Math.max(1, ...counts.flat());
  const gridColor = getComputedStyle(document.documentElement).getPropertyValue('--muted').trim() || '#66766f';
  for (let x = 0; x <= solvers.length; x++) addLine(x - 0.5, 0, -0.5, x - 0.5, 0, statuses.length - 0.5, gridColor);
  for (let z = 0; z <= statuses.length; z++) addLine(-0.5, 0, z - 0.5, solvers.length - 0.5, 0, z - 0.5, gridColor);
  counts.forEach((row, x) => row.forEach((count, z) => {
    if (!count) return;
    const height = 3.2 * count / maximum;
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(0.72, height, 0.72),
      new THREE.MeshStandardMaterial({color: statuses[z].color, roughness: 0.82}));
    mesh.position.set(x, height / 2, z);
    mesh.userData = {solver: solvers[x], status: labelFor(statuses[z]), count};
    chart.add(mesh); bars.push(mesh);
  }));
  solvers.forEach((solver, x) => axisLabels.push({kind: 'solver', item: addLabel(solver, x, 0, 0), offset: 0.6}));
  statuses.forEach((_, z) => axisLabels.push({kind: 'status', item: addLabel(String(z + 1), 0, 0, z, 0.65), offset: 0.65}));
  axisLabels.push({kind: 'solver', item: addLabel('Solvers', (solvers.length - 1) / 2, 0, 0), offset: 1.3});
  axisLabels.push({kind: 'status', item: addLabel('Statuses → legend', 0, 0, (statuses.length - 1) / 2, 2.7), offset: 1.7});
  axisLabels.push({kind: 'count', item: addLine(0, 0, 0, 0, 3.3, 0, gridColor), offset: 0});
  for (const fraction of [0, 0.5, 1]) {
    const y = 3.2 * fraction;
    axisLabels.push({kind: 'tick', item: addLine(0, y, 0, 0.12, y, 0, gridColor), offset: 0});
    axisLabels.push({kind: 'count', item: addLabel(Math.round(maximum * fraction).toLocaleString(), 0, y, 0, 0.9), offset: 0.5});
  }
  axisLabels.push({kind: 'count', item: addLabel('Count', 0, 3.55, 0, 1.3), offset: 0.6});
  if (resetView) resetCamera();
  else render();
}
function resize() {
  if (!renderer || panel.hidden) return;
  const width = viewport.clientWidth, height = viewport.clientHeight;
  if (!width || !height) return;
  renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  renderer.setSize(width, height, false);
  camera.aspect = width / height; camera.updateProjectionMatrix(); render();
}
function nearestAxisEdges(position, solverCount, statusCount) {
  const xSign = position.x >= (solverCount - 1) / 2 ? 1 : -1;
  const zSign = position.z >= (statusCount - 1) / 2 ? 1 : -1;
  return {xSign, zSign, x: xSign > 0 ? solverCount - 0.5 : -0.5,
    z: zSign > 0 ? statusCount - 0.5 : -0.5};
}
function updateAxisLabels() {
  const edge = nearestAxisEdges(chart.worldToLocal(camera.position.clone()), solvers.length, statuses.length);
  for (const {kind, item, offset} of axisLabels) {
    if (kind === 'solver') item.position.z = edge.z + edge.zSign * offset;
    else if (kind === 'status') item.position.x = edge.x + edge.xSign * offset;
    else {
      item.position.x = edge.x + edge.xSign * offset;
      item.position.z = edge.z;
      if (kind === 'tick') item.scale.x = edge.xSign;
    }
  }
}
function render() { if (renderer && !panel.hidden) { updateAxisLabels(); renderer.render(scene, camera); } }
function cubeStatusKey(item) { return JSON.stringify([item.label, item.source || '']); }
function orderCubeStatuses(items, preferred = []) {
  const ordered = [...items];
  // Stable ordering preserves the supplied order for all other default statuses.
  ordered.sort((a, b) => {
    if (!preferred.length) return Number(b.label === 'Not finished') - Number(a.label === 'Not finished');
    const rank = item => { const index = preferred.indexOf(cubeStatusKey(item)); return index < 0 ? preferred.length : index; };
    return rank(a) - rank(b);
  });
  return ordered;
}
function update(items, resetView = true) {
  latestItems = items || [];
  items = orderCubeStatuses(latestItems, preferredStatusOrder);
  const sortedStatus = countSort.key.startsWith('status-') ? statuses[Number(countSort.key.slice(7))] : null;
  if (sortedStatus) {
    const index = items.findIndex(item => cubeStatusKey(item) === cubeStatusKey(sortedStatus));
    countSort.key = index < 0 ? 'solver' : `status-${index}`;
  }
  const previousAxes = JSON.stringify([solvers, statuses.map(labelFor)]);
  const previousCounts = JSON.stringify(counts);
  statuses = (items || []).map(item => ({label: item.label, source: item.source, color: item.color}));
  solvers = [...new Set(statuses.flatMap((_, z) => (items[z].solvers || []).map(solver => solver.name)))].sort((a, b) => a.localeCompare(b));
  counts = solvers.map(solver => items.map(item => (item.solvers || []).find(entry => entry.name === solver)?.count || 0));
  const axesChanged = previousAxes !== JSON.stringify([solvers, statuses.map(labelFor)]);
  if (!axesChanged && previousCounts === JSON.stringify(counts)) return;
  renderTable(); rebuild(axesChanged && resetView); resize();
}
window.renderOutcomeCube = update;
update(window.outcomeCubeData || []);
toggle.addEventListener('click', () => {
  panel.hidden = !panel.hidden; toggle.setAttribute('aria-expanded', String(!panel.hidden));
  toggle.textContent = panel.hidden ? 'Explore all statuses in 3D' : 'Hide 3D comparison';
  if (!panel.hidden) { if(themeDirty){themeDirty=false;rebuild();} resize(); }
});
canvas.addEventListener('pointermove', event => {
  if (!renderer || event.buttons) { tooltip.hidden = true; return; }
  const bounds = canvas.getBoundingClientRect();
  pointer.set((event.clientX - bounds.left) / bounds.width * 2 - 1,
    -(event.clientY - bounds.top) / bounds.height * 2 + 1);
  raycaster.setFromCamera(pointer, camera);
  const hit = raycaster.intersectObjects(bars, false)[0];
  tooltip.hidden = !hit;
  if (hit) {
    const {solver, status, count} = hit.object.userData;
    tooltip.textContent = `${solver} · ${status}: ${count.toLocaleString()}`;
    tooltip.style.left = `${Math.min(bounds.width - tooltip.offsetWidth - 5, event.clientX - bounds.left + 14)}px`;
    tooltip.style.top = `${Math.min(bounds.height - tooltip.offsetHeight - 5, event.clientY - bounds.top + 14)}px`;
  }
});
canvas.addEventListener('pointerleave', () => { tooltip.hidden = true; });
new ResizeObserver(resize).observe(viewport);
new MutationObserver(() => {
  themeDirty = true;
  if(panel.hidden || themeFrame !== null)return;
  themeFrame = requestAnimationFrame(() => {
    themeFrame = null;
    if(!panel.hidden && themeDirty){themeDirty=false;rebuild();resize();}
  });
}).observe(document.documentElement,
  {attributes: true, attributeFilter: ['data-theme', 'data-color-theme', 'data-theme-revision']});
function animate() { requestAnimationFrame(animate); if (controls && !panel.hidden) controls.update(); }
animate();
