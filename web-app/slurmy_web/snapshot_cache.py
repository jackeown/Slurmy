"""Serve saved monitoring data while refreshing once per key and once per host."""
import threading
import time
from threading import Thread


class SnapshotCache:
    def __init__(self, backoff=60, wait=5, limit=32):
        self.backoff, self.wait, self.limit = backoff, wait, limit
        self.entries, self.pending, self.host_locks = {}, {}, {}
        self.lock = threading.Lock()

    def peek(self, key):
        with self.lock:
            return self.entries.get(key, {}).get('value')

    def status(self, key, value=None):
        with self.lock:
            entry = self.entries.get(key, {})
            value = entry.get('value') if value is None else value
            updated = getattr(value, 'fetched_epoch', None)
            age = max(0, time.time() - updated) if updated is not None else None
            # A background result may arrive between get() and status(). Never
            # attach its newer timestamp/freshness to the older returned value.
            stale = (value is not entry.get('value') or key in self.pending or
                     bool(entry.get('error')) or (age is not None and age >= entry.get('ttl', 0)) or
                     time.monotonic() - entry.get('time', float('-inf')) >= entry.get('ttl', 0))
            return dict(refreshing=key in self.pending, error=entry.get('error', ''),
                        updated=updated, stale=stale,
                        age_seconds=int(age) if age is not None else None)

    def invalidate(self):
        # Keep good data available during post-operation refreshes.
        with self.lock:
            for entry in self.entries.values():
                entry['time'] = float('-inf')
                entry['retry'] = 0

    def get(self, key, ttl, loader, force=False):
        with self.lock:
            entry = self.entries.setdefault(key, dict(time=float('-inf'), retry=0))
            saved = entry.get('value')
            entry['ttl'] = ttl
            now = time.monotonic()
            if saved is not None and not force and now - entry['time'] < ttl:
                return saved
            if now < entry['retry']:
                if saved is not None:
                    return saved
                raise RuntimeError(entry['error'])
            pending = self.pending.get(key)
            start = pending is None
            if start:
                pending = self.pending[key] = threading.Event()
                host_lock = self.host_locks.setdefault(key[0], threading.Lock())
        if start:
            def load():
                try:
                    with host_lock:
                        value = loader()
                    with self.lock:
                        self.entries[key] = dict(value=value, time=time.monotonic(), retry=0, ttl=ttl)
                except Exception as exc:
                    with self.lock:
                        entry = self.entries.setdefault(key, {})
                        entry.update(error=str(exc), retry=time.monotonic() + self.backoff)
                finally:
                    with self.lock:
                        self.pending.pop(key, None)
                        while len(self.entries) > self.limit:
                            victim = next((k for k in self.entries if k != key and k not in self.pending), None)
                            if victim is None:
                                break
                            self.entries.pop(victim)
                        pending.set()
            Thread(target=load, name='slurmy-status-refresh', daemon=True).start()
        if saved is not None and not force:
            return saved
        if not pending.wait(min(2, self.wait) if saved is not None else self.wait):
            if saved is not None:
                return saved
            raise RuntimeError('Cluster refresh is running in the background; the next request will reuse it.')
        with self.lock:
            entry = self.entries.get(key, {})
            if 'value' in entry:
                return entry['value']
            raise RuntimeError(entry.get('error', 'Cluster refresh did not return data.'))
