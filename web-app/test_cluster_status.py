"""Cluster status must show named configurations and physical reservations."""
import json
import unittest
from unittest.mock import Mock, patch

from slurmy_web.cluster import parse_status


def snapshot():
    call = json.dumps({'task_id': 0, 'solver': 'Vampire configured strategy',
                       'solver_name': 'legacy name', 'cores': 8, 'problem': '/Problems/SET/SET002+3.p'})
    return f'''PARTITIONS
CPU-amd
NODES
node1|mixed|128|80/48/0/128|250000|2
node2|mixed|32|8/24/0/32|100000|1
node1|mixed|128|80/48/0/128|250000|2
TOPOLOGY
node1|2
node2|1
ALLOCATIONS
JobId=1001 ArrayJobId=101 ArrayTaskId=0 JobName=study
   NumCPUs=8 NumNodes=1
   Nodes=node1 CPU_IDs=0-63 Mem=1000
JobId=102 JobName=distributed
   Nodes=node1 CPU_IDs=64-79 Mem=1000
   Nodes=node2 CPU_IDs=0-3,4-7 Mem=1000
JobId=103 JobName=waiting
   NumCPUs=64
JOBS
101_0|alice|study|RUNNING|1|8|0:10|1:00|node1|None|/home/alice/slurmy/jobs/study_123|slurmy:v1:calls=4:batches=1:batch=0
102|bob|distributed|RUNNING|2|24|0:20|2:00|node[1-2]|None|/home/bob/work|
103|alice|waiting|PENDING|1|64|0:00|1:00||Resources|/home/alice/work|
IDENTITY
alice
HOSTS
node1|node1
node2|node2
node[1-2]|node1,node2
CALLS
101_0|0|{call}
METADATA
/home/alice/slurmy/jobs/study_123|4|1
'''


class ClusterStatusTests(unittest.TestCase):
    def test_cluster_api_defaults_to_cpu_amd_and_honors_explicit_filter(self):
        from slurmy_web import workflows
        from slurmy_web.app import create_app
        with patch.object(workflows, 'migrate_user_runs'):
            app = create_app()
        collector = Mock(side_effect=lambda host, partition: parse_status(host, partition, snapshot()))
        app.config['CLUSTER_COLLECTOR'] = collector
        client = app.test_client()
        response = client.get('/api/cluster?host=test-host')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json['partition'], 'CPU-amd')
        collector.assert_called_with('test-host', 'CPU-amd')
        response = client.get('/api/cluster?host=test-host&partition=all')
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json['partition'], 'all')
        collector.assert_called_with('test-host', 'all')

    def test_current_configuration_name_comes_from_manifest_solver(self):
        status = parse_status('test-host', 'CPU-amd', snapshot())
        call = status['jobs'][0]['active_call']
        self.assertEqual(call['solver'], 'Vampire configured strategy')
        self.assertEqual(call['problem'], '/Problems/SET/SET002+3.p')

    def test_solver_core_request_is_distinct_from_exclusive_allocation(self):
        output = snapshot().replace('calls=4:batches=1:batch=0', 'calls=4:batches=1:cores=12:isolation=socket:batch=0')
        status = parse_status('test-host', 'CPU-amd', output)
        job = status['jobs'][0]
        self.assertEqual(job['solver_cores'],12)
        self.assertEqual(job['solver_isolation'],'socket')
        self.assertEqual(job['reserved_cores'],32)
        self.assertEqual(job['batch_index'],0)
        # Old jobs without queue-visible core requests can still use their manifest.
        old = parse_status('test-host', 'CPU-amd', snapshot())['jobs'][0]
        self.assertEqual(old['solver_cores'],8)
        self.assertEqual(old['reserved_cores'],32)

    def test_node_and_summary_counts_are_physical_cores(self):
        status = parse_status('test-host', 'all', snapshot())
        self.assertEqual(len(status['nodes']), 2)
        first, second = status['nodes']
        self.assertEqual((first['allocated'], first['idle'], first['total']), (40, 24, 64))
        self.assertEqual((second['allocated'], second['idle'], second['total']), (8, 24, 32))
        self.assertEqual(status['summary']['allocated_cores'], 48)
        self.assertEqual(status['summary']['total_cores'], 96)
        self.assertEqual(status['summary']['utilization'], 50)

    def test_actual_reserved_cores_not_solver_core_limit(self):
        status = parse_status('test-host', 'all', snapshot())
        job = status['jobs'][0]
        self.assertEqual(job['reserved_cores'], 32)
        self.assertEqual(job['node_cores'], {'node1': 32})
        self.assertEqual(status['users'][0]['allocated_cores'], 32)

    def test_multi_node_allocation_uses_each_nodes_topology(self):
        status = parse_status('test-host', 'all', snapshot())
        job = status['jobs'][1]
        self.assertEqual(job['node_cores'], {'node1': 8, 'node2': 8})
        self.assertEqual(job['reserved_cores'], 16)
        self.assertEqual(status['users'][1]['allocated_cores'], 16)

    def test_pending_or_unavailable_allocation_is_not_guessed(self):
        status = parse_status('test-host', 'all', snapshot())
        self.assertIsNone(status['jobs'][2]['reserved_cores'])
        missing = snapshot().replace('Nodes=node1 CPU_IDs=0-63 Mem=1000', '')
        status = parse_status('test-host', 'all', missing)
        self.assertIsNone(status['jobs'][0]['reserved_cores'])
        self.assertIsNone(status['users'][0]['allocated_cores'])

    def test_node_only_response_still_has_core_reservations(self):
        output = snapshot()
        start, end = output.index('NODES\n'), output.index('TOPOLOGY\n')
        output = output[:start] + 'NODES\n' + output[end:]
        status = parse_status('test-host', 'CPU-amd', output)
        self.assertEqual(status['nodes'], [])
        self.assertEqual(status['jobs'][0]['reserved_cores'], 32)


if __name__ == '__main__':
    unittest.main()
