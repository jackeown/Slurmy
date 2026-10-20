"""Exercise the chart's real JavaScript accounting without a cluster connection."""
from pathlib import Path
import subprocess
import unittest


class CoreUsageChartTests(unittest.TestCase):
    def test_denominators_unallocated_and_unattributed_cores(self):
        script = r'''
const fs = require('node:fs'), vm = require('node:vm'), assert = require('node:assert/strict');
const context = {document: {body: {dataset: {}}, querySelector: () => null}};
vm.createContext(context);
vm.runInContext(fs.readFileSync(process.argv[1], 'utf8'), context);
const chart = context.clusterCoreUsage;
// User scope must include ordinary sbatch/srun tasks as well as Slurmy arrays.
const owned = context.clusterTasksForUser({user: 'alice', jobs: [
  {id: '101_0', user: 'alice', state: 'RUNNING', slurmy: true},
  {id: '102', user: 'alice', state: 'RUNNING', slurmy: false},
  {id: '103', user: 'bob', state: 'RUNNING', slurmy: false},
  {id: '104', user: 'alice', state: 'PENDING', slurmy: false}
]});
assert.equal(owned.map(job => job.id).join(','), '101_0,102');

const data = {summary: {allocated_cores: 48, total_cores: 96}, jobs: [
  {user: 'alice', state: 'RUNNING', reserved_cores: 32},
  {user: 'bob', state: 'RUNNING', reserved_cores: 16},
  {user: 'pending', state: 'PENDING', reserved_cores: 64}
]};
let allocated = chart(data, 'allocated'), total = chart(data, 'total');
assert.equal(allocated.denominator, 48);
assert.equal(100 * allocated.entries[0].cores / allocated.denominator, 100 * 32/48);
assert.equal(allocated.entries.length, 2);
assert.equal(total.denominator, 96);
assert.equal(100 * total.entries[0].cores / total.denominator, 100 * 32/96);
assert.equal(total.entries[2].name, 'Unallocated cores');
assert.equal(total.entries[2].cores, 48);
assert.equal(total.entries.reduce((sum, row) => sum + row.cores, 0), 96);
assert.equal(total.users, 2);
// Missing per-user allocation data must not turn allocated cores into free cores.
const partial = {...data, jobs: [data.jobs[0], {user: 'bob', state: 'RUNNING', reserved_cores: null}]};
total = chart(partial, 'total');
assert.equal(total.entries.find(row => row.name === 'Allocated · user unavailable').cores, 16);
assert.equal(total.entries.find(row => row.name === 'Unallocated cores').cores, 48);
assert.equal(total.unknown, 1);
assert.equal(chart(partial, 'allocated').denominator, 48);
// An idle cluster is a full unallocated pie; the allocated-only view is empty.
const idle = {summary: {allocated_cores: 0, total_cores: 96}, jobs: []};
assert.equal(chart(idle, 'allocated').denominator, 0);
assert.equal(chart(idle, 'total').entries[0].cores, 96);
assert.equal(chart(idle, 'total').denominator, 96);
const full = {...data, summary: {allocated_cores: 48, total_cores: 48}};
assert.equal(chart(full, 'total').entries.length, 2);
for (const capacity of [0, 16, undefined]) {
  const invalid = chart({...data, summary: {...data.summary, total_cores: capacity}}, 'total');
  assert.ok(invalid.error);
  assert.equal(invalid.denominator, 0);
}
'''
        source = Path(__file__).parent/'slurmy_web/static/cluster.js'
        subprocess.run(['node', '-e', script, str(source)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
