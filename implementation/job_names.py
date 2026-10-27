"""Assign short readable job names, independently of stable filename-safe IDs."""


def unused_job_name(workflow_name, existing):
    base = str(workflow_name).strip()[:100]
    if not base:
        raise ValueError('A workflow name is required.')
    if base not in existing:
        return base
    number = 1
    while True:
        suffix = f' ({number})'
        candidate = base[:100-len(suffix)] + suffix
        if candidate not in existing:
            return candidate
        number += 1
