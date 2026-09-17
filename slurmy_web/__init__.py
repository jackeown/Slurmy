"""Local web UI and reusable experiment/monitoring services."""


def create_app():
    from .app import create_app as factory
    return factory()
