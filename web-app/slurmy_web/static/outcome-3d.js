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
const countSort = {key: 'solver', direction: 'asc'};

function make(tag, label) { const node = document.createElement(tag); if (label !== undefined) node.textContent = label; return node; }
function labelFor(item) { return item.label + (item.source ? ` (${item.source})` : ''); }
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
    const item = make('span'), swatch = make('i'); swatch.style.background = status.color;
    item.append(swatch, document.createTextNode(`${z + 1}. ${labelFor(status)}`)); legend.append(item);
  });
}
function clearChart() {
  while (chart.children.length) {
    const item = chart.children[0]; chart.remove(item);
    item.geometry?.dispose(); item.material?.map?.dispose(); item.material?.dispose();
  }
  bars.length = 0;
}
function addLabel(text, x, y, z, width = 2.2) {
  const surface = document.createElement('canvas'); surface.width = 512; surface.height = 80;
  const ctx = surface.getContext('2d');
  ctx.font = '600 32px system-ui';
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue('--ink').trim() || '#20332e';
  ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.fillText(text, 256, 40, 480);
  const texture = new THREE.CanvasTexture(surface);
  const sprite = new THREE.Sprite(new THREE.SpriteMaterial({map: texture, transparent: true, depthTest: false}));
  sprite.scale.set(width, width * 80 / 512, 1); sprite.position.set(x, y, z); chart.add(sprite);
}
function addLine(x1, y1, z1, x2, y2, z2, color) {
  const geometry = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(x1, y1, z1), new THREE.Vector3(x2, y2, z2)]);
  chart.add(new THREE.Line(geometry, new THREE.LineBasicMaterial({color, transparent: true, opacity: 0.45})));
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
  solvers.forEach((solver, x) => addLabel(solver, x, 0, statuses.length + 0.1));
  statuses.forEach((_, z) => addLabel(String(z + 1), solvers.length + 0.15, 0, z, 0.65));
  addLabel('Solvers', (solvers.length - 1) / 2, 0, statuses.length + 0.8);
  addLabel('Statuses → legend', solvers.length + 1.2, 0, (statuses.length - 1) / 2, 2.7);
  addLine(-0.5, 0, -0.5, -0.5, 3.3, -0.5, gridColor);
  for (const fraction of [0, 0.5, 1]) {
    const y = 3.2 * fraction;
    addLine(-0.62, y, -0.5, -0.5, y, -0.5, gridColor);
    addLabel(Math.round(maximum * fraction).toLocaleString(), -1, y, -0.5, 0.9);
  }
  addLabel('Count', -1.1, 3.55, -0.5, 1.3);
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
function render() { if (renderer && !panel.hidden) renderer.render(scene, camera); }
function update(items) {
  const previousAxes = JSON.stringify([solvers, statuses.map(labelFor)]);
  const previousCounts = JSON.stringify(counts);
  statuses = (items || []).map(item => ({label: item.label, source: item.source, color: item.color}));
  solvers = [...new Set(statuses.flatMap((_, z) => (items[z].solvers || []).map(solver => solver.name)))].sort((a, b) => a.localeCompare(b));
  counts = solvers.map(solver => items.map(item => (item.solvers || []).find(entry => entry.name === solver)?.count || 0));
  const axesChanged = previousAxes !== JSON.stringify([solvers, statuses.map(labelFor)]);
  if (!axesChanged && previousCounts === JSON.stringify(counts)) return;
  renderTable(); rebuild(axesChanged); resize();
}
window.renderOutcomeCube = update;
update(window.outcomeCubeData || []);
toggle.addEventListener('click', () => {
  panel.hidden = !panel.hidden; toggle.setAttribute('aria-expanded', String(!panel.hidden));
  toggle.textContent = panel.hidden ? 'Explore all statuses in 3D' : 'Hide 3D comparison';
  if (!panel.hidden) resize();
});
document.querySelector('#reset-outcome-cube').addEventListener('click', () => { controls?.reset(); render(); });
document.querySelectorAll('[data-cube-pan]').forEach(button => button.addEventListener('click', () => {
  if (!controls) return;
  const direction = button.dataset.cubePan;
  controls.pan(direction === 'left' ? -40 : direction === 'right' ? 40 : 0,
    direction === 'up' ? -40 : direction === 'down' ? 40 : 0);
  controls.update();
}));
document.querySelectorAll('[data-cube-zoom]').forEach(button => button.addEventListener('click', () => {
  if (!controls) return;
  if (button.dataset.cubeZoom === 'in') controls.dollyIn(1.2);
  else controls.dollyOut(1.2);
  controls.update();
}));
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
new MutationObserver(() => { rebuild(); resize(); }).observe(document.documentElement,
  {attributes: true, attributeFilter: ['data-theme']});
function animate() { requestAnimationFrame(animate); if (controls && !panel.hidden) controls.update(); }
animate();
