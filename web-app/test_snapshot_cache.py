"""Slow cluster refreshes must not block saved data or multiply SSH work."""
import threading
import time
from types import SimpleNamespace
import unittest

from slurmy_web.snapshot_cache import SnapshotCache


class SnapshotCacheTests(unittest.TestCase):
    def test_stale_data_is_immediate_and_concurrent_requests_share_one_refresh(self):
        cache = SnapshotCache()
        key = ('host', 'job')
        saved = SimpleNamespace(fetched_epoch=1)
        updated = SimpleNamespace(fetched_epoch=2)
        self.assertIs(cache.get(key, 30, lambda: saved), saved)
        cache.invalidate()
        entered, release = threading.Event(), threading.Event()
        calls = []
        def slow():
            calls.append(1)
            entered.set()
            release.wait(2)
            return updated
        try:
            start = time.monotonic()
            self.assertIs(cache.get(key, 30, slow), saved)
            self.assertLess(time.monotonic() - start, .2)
            self.assertTrue(entered.wait(1))
            for _ in range(20):
                self.assertIs(cache.get(key, 30, slow), saved)
            self.assertEqual(len(calls), 1)
            self.assertTrue(cache.status(key)['refreshing'])
            pending = cache.pending[key]
        finally:
            release.set()
        self.assertTrue(pending.wait(1))
        self.assertIs(cache.get(key, 30, slow), updated)

    def test_one_active_collection_per_host_even_for_different_pages(self):
        cache = SnapshotCache()
        first, second = ('host', 'history'), ('host', 'detail')
        for key in (first, second):
            cache.get(key, 30, lambda: SimpleNamespace(fetched_epoch=1))
        cache.invalidate()
        entered, release, second_entered = threading.Event(), threading.Event(), threading.Event()
        def block():
            entered.set()
            release.wait(2)
            return SimpleNamespace(fetched_epoch=2)
        def later():
            second_entered.set()
            return SimpleNamespace(fetched_epoch=3)
        try:
            cache.get(first, 30, block)
            self.assertTrue(entered.wait(1))
            cache.get(second, 30, later)
            pending = cache.pending[second]
            self.assertFalse(second_entered.wait(.05))
        finally:
            release.set()
        self.assertTrue(pending.wait(1))
        self.assertTrue(second_entered.is_set())

    def test_failure_retains_good_data_and_backs_off_even_for_forced_refresh(self):
        cache = SnapshotCache()
        key = ('host', 'job')
        saved = SimpleNamespace(fetched_epoch=1)
        cache.get(key, 30, lambda: saved)
        calls = []
        def failed():
            calls.append(1)
            raise RuntimeError('network unavailable')
        self.assertIs(cache.get(key, 30, failed, force=True), saved)
        self.assertIn('network unavailable', cache.status(key)['error'])
        for _ in range(20):
            self.assertIs(cache.get(key, 30, failed, force=True), saved)
        self.assertEqual(calls, [1])

    def test_initial_loading_continues_without_starting_another_collection(self):
        cache = SnapshotCache(wait=.01)
        key = ('host', 'job')
        release = threading.Event()
        calls = []
        def slow():
            calls.append(1)
            release.wait(2)
            return SimpleNamespace(fetched_epoch=2)
        try:
            for _ in range(3):
                with self.assertRaisesRegex(RuntimeError, 'running in the background'):
                    cache.get(key, 30, slow)
            self.assertEqual(calls, [1])
            pending = cache.pending[key]
        finally:
            release.set()
        self.assertTrue(pending.wait(1))
        self.assertEqual(cache.get(key, 30, slow).fetched_epoch, 2)

    def test_incremental_api_preserves_definitions_results_and_metadata(self):
        from unittest.mock import patch
        from slurmy_web import app as web
        from slurmy_web.monitor import JobSnapshot, ClusterSnapshot, ResultRecord, TaskDefinition
        original = JobSnapshot('job', '/jobs/job', 1,
                               metadata={'task_count': 10001, 'planned_batch_tasks': [[0, 1]]},
                               tasks={0: TaskDefinition(0, problem='a.p'), 1: TaskDefinition(1, problem='b.p')},
                               task_batches={0: 0, 1: 0}, results={0: ResultRecord(0, True, 'ok')})
        incremental = JobSnapshot('job', '/jobs/job', 1, results={1: ResultRecord(1, True, 'ok')})
        first = ClusterSnapshot('datalab', '/jobs', 'job', 100, [original])
        second = ClusterSnapshot('datalab', '/jobs', 'job', 120, [incremental])
        with patch.object(web.workflows, 'migrate_user_runs'), \
             patch.object(web, 'RemoteCollector') as factory:
            factory.return_value.fetch.side_effect = [first, second]
            app = web.create_app()
            client = app.test_client()
            self.assertEqual(client.get('/api/jobs/job?fresh=1').status_code, 200)
            response = client.get('/api/jobs/job?fresh=1')
            self.assertEqual(response.status_code, 200)
            self.assertEqual(response.json['total'], 10001)
            self.assertEqual(response.json['completed'], 2)
            self.assertEqual([task['problem'] for task in response.json['tasks']], ['a.p', 'b.p'])
            self.assertEqual(factory.return_value.fetch.call_args.args, ('job', 40))
            self.assertEqual(response.json['refresh']['updated'], 120)

    def test_old_response_cannot_borrow_newer_refresh_timestamp_or_validity(self):
        cache = SnapshotCache()
        key = ('host', 'job')
        older = SimpleNamespace(fetched_epoch=time.time() - 100)
        newer = SimpleNamespace(fetched_epoch=time.time())
        cache.get(key, 30, lambda: older)
        cache.invalidate()
        entered, release = threading.Event(), threading.Event()
        def load():
            entered.set()
            release.wait(2)
            return newer
        try:
            returned = cache.get(key, 30, load)
            self.assertTrue(entered.wait(1))
            pending = cache.pending[key]
        finally:
            release.set()
        self.assertTrue(pending.wait(1))
        status = cache.status(key, returned)
        self.assertEqual(status['updated'], older.fetched_epoch)
        self.assertTrue(status['stale'])
        self.assertFalse(status['refreshing'])
        self.assertEqual(cache.status(key, newer)['updated'], newer.fetched_epoch)
        self.assertFalse(cache.status(key, newer)['stale'])
        cache.invalidate()
        self.assertTrue(cache.status(key, newer)['stale'])

    def test_cached_responses_are_not_fed_back_into_automatic_recovery(self):
        from unittest.mock import patch
        from slurmy_web import app as web
        from slurmy_web.monitor import ClusterSnapshot
        snapshot = ClusterSnapshot('datalab', '/jobs', '', time.time(), [])
        with patch.object(web.workflows, 'migrate_user_runs'), \
             patch.object(web, 'RemoteCollector') as collector:
            collector.return_value.fetch.return_value = snapshot
            app = web.create_app()
            with patch.object(app.config['RECOVERY_WATCH'], 'observe', return_value=[]) as observe:
                client = app.test_client()
                for _ in range(5):
                    self.assertEqual(client.get('/api/jobs').status_code, 200)
                self.assertEqual(collector.return_value.fetch.call_count, 1)
                self.assertEqual(observe.call_count, 1)
