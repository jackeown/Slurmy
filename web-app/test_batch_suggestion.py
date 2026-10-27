"""Batch defaults use live partition capacity and count actual selected pairs."""
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from slurmy_web import batch_suggestion as batch


def config(cores=8, **extra):
    return dict(cores=cores, threads_per_core=1, **extra)


class BatchSuggestionTests(unittest.TestCase):
    def setUp(self):
        self.nodes = [dict(sockets=2, cores_per_socket=32, threads_per_core=2) for _ in range(10)]

    def test_shared_socket_and_node_and_outstanding_limit(self):
        for extra, capacity in (({}, 80), ({'exclusive_cpu': 'true'}, 20),
                                ({'exclusive_node': 'true'}, 10)):
            with self.subTest(extra=extra):
                result = batch.estimate([config(**extra)], 1001, 100, self.nodes)
                self.assertEqual(result['hardware_parallelism'], capacity)
                self.assertEqual(result['batch_size'], (1001+2*capacity-1)//(2*capacity))
        capped = batch.estimate([config()], 1001, 32, self.nodes)
        self.assertEqual(capped['maximum_parallelism'], 32)
        self.assertEqual(capped['batch_size'], 16)

    def test_mixed_configs_use_minimum_and_whole_node_takes_precedence(self):
        result = batch.estimate([config(), config(exclusive_cpu='true'),
                                 config(exclusive_cpu='true', exclusive_node='true')], 1200, 100, self.nodes)
        self.assertEqual(result['configuration_parallelism'], [80, 20, 10])
        self.assertEqual(result['batch_size'], 60)

    def test_heterogeneous_nodes_multi_socket_request_and_threads(self):
        nodes = self.nodes[:1] + [dict(sockets=2, cores_per_socket=16, threads_per_core=1)]
        result = batch.estimate([config(40, exclusive_cpu='true')], 100, 100, nodes)
        self.assertEqual(result['hardware_parallelism'], 1)
        result = batch.estimate([dict(cores=8, threads_per_core=2)], 100, 100, nodes)
        self.assertEqual(result['hardware_parallelism'], 8)

    def test_no_pairs_impossible_configuration_and_rounding(self):
        for configurations, count, outstanding in (([config()], 0, 32), ([config(65)], 100, 32),
                                                    ([config()], 100, 0), ([], 100, 32)):
            with self.assertRaises(ValueError):
                batch.estimate(configurations, count, outstanding, self.nodes)
        self.assertEqual(batch.estimate([config()], 1, 32, self.nodes)['batch_size'], 1)

    def test_live_topology_deduplicates_nodes_and_rejects_errors(self):
        with patch.object(batch.subprocess, 'run', return_value=SimpleNamespace(
                returncode=0, stdout='n1|2|32|2\nn1|2|32|2\nn2|1|16|1\n')) as run:
            self.assertEqual(len(batch.partition_nodes('datalab', 'CPU-amd')), 2)
            self.assertIn('CPU-amd', run.call_args.args[0][-1])
        for result in (SimpleNamespace(returncode=0, stdout=''),
                       SimpleNamespace(returncode=0, stdout='n1|?|32|2'),
                       SimpleNamespace(returncode=1, stderr='connection failed')):
            with patch.object(batch.subprocess, 'run', return_value=result), self.assertRaises(ValueError):
                batch.partition_nodes('datalab', 'CPU-amd')

    def test_problem_sources_are_deduplicated_and_all_input_modes_supported(self):
        base = dict(host='datalab', partition='CPU-amd', max_outstanding=32)
        with patch.object(batch, 'partition_nodes', return_value=self.nodes), \
             patch.object(batch.workflows, 'problem_glob', return_value=('pattern', ['a.p', 'b.p'])) as glob, \
             patch.object(batch.workflows, 'existing', return_value='file'), \
             patch.object(batch.workflows, 'imported_rows', return_value=[config(), config()]):
            for mode in ('interactive', 'configurations', 'jobpairs'):
                result = batch.suggest(dict(base, mode=mode, configurations=[config(), config()], globs=['first', 'overlapping']))
                self.assertEqual(result['total_pairs'], 2 if mode == 'jobpairs' else 4)
            self.assertFalse(glob.call_args.kwargs['materialize'])

    def test_api_recalculates_without_persistent_cache(self):
        from slurmy_web.app import create_app
        with patch.object(batch.workflows, 'migrate_user_runs'):
            client = create_app().test_client()
        with client.session_transaction() as session:
            session['csrf'] = 'test'
        data = dict(host='datalab', partition='CPU-amd', mode='interactive', max_outstanding=32,
                    configurations=[config()], globs=['problem'])
        with patch.object(batch.workflows, 'problem_glob', return_value=('problem', ['a.p'])), \
             patch.object(batch, 'partition_nodes', side_effect=[self.nodes, self.nodes[:1]]) as nodes:
            first = client.post('/api/suggest-batch-size', json=data, headers={'X-CSRF-Token': 'test'})
            second = client.post('/api/suggest-batch-size', json=data, headers={'X-CSRF-Token': 'test'})
        self.assertEqual(first.status_code, 200)
        self.assertEqual(first.json['hardware_parallelism'], 80)
        self.assertEqual(second.json['hardware_parallelism'], 8)
        self.assertEqual(nodes.call_count, 2)
