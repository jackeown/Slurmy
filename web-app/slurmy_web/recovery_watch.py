"""Retry jobs observed to lose all workers; never infer idleness from counts alone."""
import threading
import time


class RecoveryWatch:
    def __init__(self, idle_seconds=15, retry_seconds=60, max_attempts=3):
        self.idle_seconds = idle_seconds
        self.retry_seconds = retry_seconds
        self.max_attempts = max_attempts
        self.entries = {}
        self.blocked = set()
        self.lock = threading.Lock()

    def hosts(self):
        with self.lock:
            return {host for (host, _), entry in self.entries.items()
                    if entry['attempts'] < self.max_attempts}

    def stop(self, host, job_id):
        with self.lock:
            self.blocked.add((host, job_id))
            self.entries.pop((host, job_id), None)

    def resume(self, host, job_id):
        with self.lock:
            self.blocked.discard((host, job_id))
            self.entries[(host, job_id)] = dict(completed=0, attempts=0, idle=None, retry=0)

    def observe(self, host, jobs, busy=(), now=None):
        now = time.monotonic() if now is None else now
        retry = []
        with self.lock:
            for job in jobs:
                key = (host, job.job_id)
                if key in self.blocked:
                    continue
                state = getattr(job, 'state', '')
                if state == 'STOPPED' or job.completed >= job.task_count:
                    self.entries.pop(key, None)
                    continue
                controller = getattr(job, 'controller_state', 'unknown')
                submitting = getattr(job, 'submission_state', '') == 'submitting'
                active = bool(job.active_records) or controller == 'active'
                # Do not revive old historical failures merely by opening history.
                if active or submitting:
                    self.entries.setdefault(key, dict(completed=job.completed, attempts=0, idle=None, retry=0))
                entry = self.entries.get(key)
                if entry is None:
                    continue
                if job.completed > entry['completed']:
                    entry.update(completed=job.completed, attempts=0)
                if not getattr(job, 'queue_available', False) or active or controller != 'inactive' or state != 'INCOMPLETE' or host + ':' + job.job_id in busy:
                    entry['idle'] = None
                    continue
                if entry['idle'] is None:
                    entry['idle'] = now
                if (now - entry['idle'] >= self.idle_seconds and now >= entry['retry']
                        and entry['attempts'] < self.max_attempts):
                    entry.update(attempts=entry['attempts'] + 1, retry=now + self.retry_seconds, idle=now)
                    retry.append(job)
        return retry
