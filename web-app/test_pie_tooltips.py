"""Shared pie hover hit testing and tooltip lifecycle use the real browser code."""
from pathlib import Path
import subprocess
import unittest


class PieTooltipTests(unittest.TestCase):
    def test_hover_targets_only_one_slice_and_tracks_chart_updates(self):
        script = r'''
const assert = require('node:assert/strict'), fs = require('node:fs'), vm = require('node:vm');
const tips = [], events = {}, documentEvents = {};
const context = {
  window: {innerWidth: 200, innerHeight: 200, addEventListener() {}},
  document: {
    body: {append: tooltip => tips.push(tooltip)},
    createElement: () => ({style: {}, offsetWidth: 80, offsetHeight: 30, setAttribute() {}}),
    addEventListener: (name, handler) => {documentEvents[name] = handler;}
  }
};
vm.createContext(context);
vm.runInContext(fs.readFileSync(process.argv[1], 'utf8'), context);
const {hitTest, setSegments} = context.window.SlurmyPie;
const slices = [
  {fraction: 1/3, label: 'alice: 32 cores (33.3%)'},
  {fraction: 1/6, label: 'bob: 16 cores (16.7%)'},
  {fraction: 1/2, label: 'Unallocated cores: 48 cores (50.0%)'}
];
assert.equal(hitTest(slices, 0, -0.8).label, slices[0].label);
assert.equal(hitTest(slices, 0.5, 0.5).label, slices[1].label);
assert.equal(hitTest(slices, -0.5, 0).label, slices[2].label);
assert.equal(hitTest(slices, -1, -1), null);
assert.equal(hitTest(slices, 0, 0), null);
assert.equal(hitTest([], 0.5, 0.5), null);
const chart = {title: 'old whole-chart label',
  removeAttribute(name) {delete this[name];},
  getBoundingClientRect: () => ({left: 0, top: 0, width: 100, height: 100}),
  addEventListener: (name, handler) => {events[name] = handler;}
};
setSegments(chart, slices);
assert.equal(chart.title, undefined);
assert.equal(tips.length, 1);
const hover = (x, y, pointerType='mouse') => events.pointermove({clientX:x, clientY:y, pointerType});
hover(75, 25);assert.equal(tips[0].textContent, slices[0].label);assert.equal(tips[0].hidden, false);
hover(75, 75);assert.equal(tips[0].textContent, slices[1].label);
setSegments(chart, [{fraction:2/3,label:'alice: 32 cores (66.7%)'}, {fraction:1/3,label:'bob: 16 cores (33.3%)'}]);
assert.equal(tips[0].textContent, 'alice: 32 cores (66.7%)');
assert.equal(tips.length, 1); // Refresh reuses the tooltip and handlers.
setSegments(chart, slices);
hover(25,50);assert.equal(tips[0].textContent, slices[2].label);
hover(0,0);assert.equal(tips[0].hidden, true);
hover(75,25);events.pointerleave();assert.equal(tips[0].hidden, true);
hover(75,25);documentEvents.keydown({key:'Escape'});assert.equal(tips[0].hidden, true);
hover(75,25,'touch');assert.equal(tips[0].hidden, true);
hover(75,25);setSegments(chart,[]);assert.equal(tips[0].hidden, true);
'''
        source = Path(__file__).parent/'slurmy_web/static/pie-tooltip.js'
        subprocess.run(['node', '-e', script, str(source)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
