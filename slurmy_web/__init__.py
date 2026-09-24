"""Local web UI and reusable workflow/job monitoring services."""


def create_app():
    from .app import create_app as factory
    return factory()
