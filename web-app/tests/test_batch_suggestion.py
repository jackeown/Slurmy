"""Batch defaults use live partition capacity and count actual selected pairs."""
import _bootstrap  # noqa: F401 -- support direct unittest discovery
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from slurmy_web import batch_suggestion as batch


def config(cores=8, **extra):
    return dict(cores=cores, threads_per_core=1, **extra)


class BatchSuggestionTests(unittest.TestCase):
    def setUp(self):
        self.nodes = [dict(sockets=2, cores_per_socket=32, threads_per_core=2) for _ in range(10)]

    def test_all_cores_uses_dedicated_socket_or_node_capacity(self):
        for node, capacity in [('false',20),('true',10)]:
            result=batch.estimate([config(cores='all',exclusive_cpu='true',exclusive_node=node)],1000,100,self.nodes)
            self.assertEqual(result['hardware_parallelism'],capacity)
        with self.assertRaisesRegex(ValueError,'requires dedicated'):
            batch.estimate([config(cores='all',exclusive_cpu='false')],1000,100,self.nodes)

    def test_shared_socket_and_node_and_outstanding_limit(self):
        for extra, capacity in (({}, 80), ({'exclusive_cpu': 'true'}, 20),
                                ({'exclusive_node': 'true'}, 10)):
            with self.subTest(extra=extra):
                result = batch.estimate([config(**extra)], 1001, 100, self.nodes)
                self.assertEqual(result['hardware_parallelism'], capacity)
                self.assertEqual(result['batch_size'], (1001+min(max(1,capacity//2),50)-1)//min(max(1,capacity//2),50))
        capped = batch.estimate([config()], 1001, 32, self.nodes)
        self.assertEqual(capped['maximum_parallelism'], 32)
        self.assertEqual(capped['batch_size'], 32)

    def test_mixed_configs_use_minimum_and_whole_node_takes_precedence(self):
        result = batch.estimate([config(), config(exclusive_cpu='true'),
                                 config(exclusive_cpu='true', exclusive_node='true')], 1200, 100, self.nodes)
        self.assertEqual(result['configuration_parallelism'], [80, 20, 10])
        self.assertEqual(result['batch_size'], 240)

    def test_heterogeneous_nodes_multi_socket_request_and_threads(self):
        nodes = self.nodes[:1] + [dict(sockets=2, cores_per_socket=16, threads_per_core=1)]
        result = batch.estimate([config(40, exclusive_cpu='true')], 100, 100, nodes)
        self.assertEqual(result['hardware_parallelism'], 1)
        result = batch.estimate([dict(cores=8, threads_per_core=2)], 100, 100, nodes)
        self.assertEqual(result['hardware_parallelism'], 8)

    def test_half_hardware_capacity_is_applied_before_submission_cap(self):
        node=dict(sockets=1,cores_per_socket=10,threads_per_core=1)
        result=batch.estimate([config(1)],1000,50,[node])
        self.assertEqual(result['assumed_hardware_parallelism'],5)
        self.assertEqual(result['assumed_parallelism'],5)
        self.assertEqual(result['batch_size'],200)
        result=batch.estimate([config(1)],1000,50,self.nodes)
        self.assertEqual(result['assumed_hardware_parallelism'],320)
        self.assertEqual(result['assumed_parallelism'],50)
        self.assertEqual(result['batch_size'],20)
        single=batch.estimate([config(10)],1000,50,[node])
        self.assertEqual(single['assumed_parallelism'],1)

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

    def test_submission_fit_uses_real_grouping_instead_of_total_pairs_division(self):
        configurations=[config(1,solver_name=name,command='./solver {{problem}}',wc_limit=1,cpu_limit=1,mem_limit='1GiB') for name in ['A','B']]
        # Both solvers share one allocation group: ceil(2000/39)=52; size 40 fits 50.
        plan=batch.submission_fit(configurations,[1000,1000],39,'solver-major',100,self.nodes)
        self.assertEqual(plan['estimated_batches'],52)
        self.assertEqual(plan['submission_budget'],50)
        self.assertEqual(plan['recommended_batch_size'],40)
        self.assertEqual(plan['recommended_batches'],50)
        for order in ('solver-major','problem-major','random'):
            packed=batch.submission_fit(configurations,[1000,1000],41,order,100,self.nodes)
            self.assertEqual(packed['estimated_batches'],49)
            self.assertEqual(packed['group_pair_counts'],[2000])

    def test_zero_and_finite_pair_limits_share_the_same_allocation_group(self):
        base=config(1,solver_name='Same solver',command='./solver {{problem}}',wc_limit=1,cpu_limit=1,mem_limit='1MiB')
        for field in ('wc_limit','cpu_limit','mem_limit'):
            configurations=[base,{**base,field:0}]
            plan=batch.submission_fit(configurations,[1,1],100,'solver-major',32,self.nodes)
            self.assertEqual(plan['estimated_batches'],1,field)

    def test_submission_fit_respects_window_and_impossible_number_of_configurations(self):
        configurations=[config(index+1,solver_name=str(index),command='./solver {{problem}}') for index in range(51)]
        plan=batch.submission_fit(configurations,[1]*51,100,'solver-major',100,self.nodes)
        self.assertEqual(plan['minimum_batches'],51)
        self.assertIsNone(plan['recommended_batch_size'])
        plan=batch.submission_fit([config(1)],[1000],1,'problem-major',32,self.nodes)
        self.assertEqual(plan['recommended_batch_size'],32)
        self.assertEqual(plan['recommended_batches'],32)

    def test_dedicated_batches_still_pack_sequential_pairs(self):
        plan=batch.submission_fit([config(1,exclusive_node='true')],[100],25,'solver-major',32,self.nodes)
        self.assertEqual(plan['estimated_batches'],4)

    def test_many_solvers_with_matching_allocations_can_fit_the_submission_window(self):
        configurations=[config(1,solver_name=str(index)) for index in range(51)]
        plan=batch.submission_fit(configurations,[1]*51,1,'solver-major',50,self.nodes)
        self.assertEqual(plan['minimum_batches'],1)
        self.assertEqual(plan['recommended_batch_size'],2)
        self.assertEqual(plan['recommended_batches'],26)

    def test_custom_account_without_reported_cap_can_use_a_larger_workflow_window(self):
        result=batch.estimate([config(1)],1000,200,self.nodes,account_submit_limit=None)
        self.assertEqual(result['assumed_parallelism'],200)
        self.assertEqual(result['batch_size'],5)
        plan=batch.submission_fit([config(1)],[1000],5,'solver-major',200,self.nodes,
                                  active_tasks=60,account_submit_limit=None)
        self.assertEqual(plan['submission_budget'],200)
        self.assertEqual(plan['estimated_batches'],200)
        self.assertIsNone(plan['account_submit_limit'])
        self.assertIsNone(plan['account_free_slots'])

    def test_six_hour_budget_caps_suggestion_even_when_submission_window_cannot_fit(self):
        configurations=[config(1,solver_name='A',wc_limit=3600)]
        with patch.object(batch,'partition_nodes',return_value=self.nodes),patch.object(batch.workflows,'problem_glob',return_value=('pattern',[str(i) for i in range(1000)])):
            result=batch.suggest(dict(host='datalab',partition='CPU-amd',mode='interactive',configurations=configurations,globs=['pattern'],max_outstanding=50,batch_size=100,get_computer_info=True))
        self.assertEqual(result['wall_time_capacity'],5)
        self.assertEqual(result['batch_size'],5)
        self.assertEqual(result['suggested_batches'],200)
        self.assertFalse(result['suggested_fits'])
        self.assertTrue(result['selected_size_exceeds_wall'])
        self.assertLessEqual(60+5*(3600+80),21600)

    def test_postprocessing_budget_combines_each_config_and_none_is_zero(self):
        configurations=[config(1,wc_limit=100,postprocessor_mode='custom',postprocessor_directory='/unused',
                               postprocessor_command='python3 parser.py',postprocessor_timeout=600),
                        config(1,wc_limit=500,postprocessor_mode='none')]
        with patch.object(batch,'partition_nodes',return_value=self.nodes),patch.object(batch.workflows,'problem_glob',return_value=('pattern',['a'])):
            result=batch.suggest(dict(host='datalab',partition='CPU-amd',mode='interactive',configurations=configurations,globs=['pattern'],get_computer_info=False))
        self.assertEqual(result['configuration_postprocessor_seconds'],[605,0])
        self.assertEqual(result['max_pair_budget_seconds'],735)
        self.assertEqual(result['wall_time_capacity'],(21600-60)//735)

    def test_default_postprocessing_budget_and_timeout_validation(self):
        with patch.object(batch,'partition_nodes',return_value=self.nodes),patch.object(batch.workflows,'problem_glob',return_value=('pattern',['a'])):
            result=batch.suggest(dict(host='datalab',partition='CPU-amd',mode='interactive',configurations=[config(1,wc_limit=100)],globs=['pattern']))
        self.assertEqual(result['max_postprocessor_seconds'],35)
        self.assertEqual(result['max_pair_budget_seconds'],215)
        for timeout in (0,-1,601,'NaN',float('inf')):
            with self.subTest(timeout=timeout),self.assertRaises(ValueError):
                batch.workflows.postprocessor_settings({'postprocessor_timeout':timeout},check_paths=False)

    def test_unbounded_wall_limits_are_not_given_a_false_six_hour_guarantee(self):
        with patch.object(batch,'partition_nodes',return_value=self.nodes),patch.object(batch.workflows,'problem_glob',return_value=('pattern',['a','b'])):
            result=batch.suggest(dict(host='datalab',partition='CPU-amd',mode='interactive',configurations=[config(1,wc_limit=0)],globs=['pattern']))
        self.assertTrue(result['wall_time_unknown'])
        self.assertIsNone(result['wall_time_capacity'])

    def test_existing_account_tasks_reduce_available_slots_and_full_queue_has_no_immediate_fit(self):
        plan=batch.submission_fit([config(1)],[1000],10,'problem-major',32,self.nodes,active_tasks=40)
        self.assertEqual(plan['submission_budget'],10)
        self.assertEqual(plan['recommended_batch_size'],100)
        full=batch.submission_fit([config(1)],[1000],1000,'problem-major',32,self.nodes,active_tasks=50)
        self.assertEqual(full['submission_budget'],0)
        self.assertIsNone(full['recommended_batch_size'])
