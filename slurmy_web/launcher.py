"""Start or reuse this checkout's loopback server and open the requested page."""
from __future__ import annotations

import argparse
import http.cookiejar
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

REPO = Path(__file__).resolve().parents[1]


def health(url):
    try:
        with urllib.request.urlopen(url + '/health', timeout=1) as response:
            data = json.load(response)
        if data.get('app') != 'slurmy-web' or data.get('repository') != str(REPO):
            raise RuntimeError('This port belongs to another app or checkout. Choose another --port.')
        return data
    except (urllib.error.URLError, TimeoutError):
        return None
    except (ValueError, AttributeError) as exc:
        raise RuntimeError('This port is occupied by another app. Choose another --port.') from exc


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default=os.environ.get('SLURMY_HOST', 'datalab'), help='SSH alias, not a web bind address')
    parser.add_argument('--port', type=int, default=int(os.environ.get('SLURMY_WEB_PORT', '8765')))
    parser.add_argument('--directory', type=Path, help='open this local workflow')
    parser.add_argument('--job', help='open a particular submitted Slurmy job')
    parser.add_argument('--new', action='store_true', help='open the workflow builder')
    parser.add_argument('--serve', action='store_true', help='run in the foreground (the default)')
    parser.add_argument('--background', action='store_true', help='start/reuse the server and return to the shell')
    parser.add_argument('--no-browser', action='store_true', help='do not open a browser')
    parser.add_argument('--stop', action='store_true', help='stop the local server, leaving Slurm jobs alone')
    args = parser.parse_args(argv)
    if not 1024 <= args.port <= 65535:
        parser.error('port must be between 1024 and 65535')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', args.host):
        parser.error('invalid SSH alias')
    for module in ('flask', 'waitress'):
        if importlib.util.find_spec(module) is None:
            parser.error(f'Install web dependencies: {sys.executable} -m pip install -r {REPO / "requirements.txt"}')
    url = f'http://127.0.0.1:{args.port}'
    def target_url():
        query = {'host': args.host}
        path = '/'
        if args.new:
            path = '/new'
        elif args.job:
            path = '/jobs/' + urllib.parse.quote(args.job, safe='')
        elif args.directory:
            path = '/workflow'
            query['directory'] = str(args.directory.resolve())
        return url + path + '?' + urllib.parse.urlencode(query)

    if (args.serve or not args.background) and not args.stop and not health(url):
        from waitress import create_server
        from . import create_app
        app = create_app()
        server = create_server(app, host='127.0.0.1', port=args.port, threads=8)
        def shutdown():
            server.close()
            server.task_dispatcher.shutdown()
            # Close persistent browser connections as well as the listening socket.
            # Only this owned server process is stopped; Slurm jobs are untouched.
            os.kill(os.getpid(), signal.SIGTERM)
        app.config['SHUTDOWN'] = shutdown
        target = target_url()
        print(f'Slurmy is listening at {url}', flush=True)
        print(target, flush=True)
        if not args.no_browser and not webbrowser.open(target):
            print('Open the URL above in your browser.', flush=True)
        try:
            server.run()
        except KeyboardInterrupt:
            shutdown()
        return 0
    existing = health(url)
    if args.stop:
        if not existing:
            print('Slurmy web app is not running.')
            return 0
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        with opener.open(url + '/') as response:
            html = response.read().decode()
        token = re.search(r'name="csrf-token" content="([a-f0-9]+)"', html)[1]
        request = urllib.request.Request(url + '/api/shutdown', data=b'{}', headers={
            'Content-Type': 'application/json', 'X-CSRF-Token': token, 'Origin': url})
        with opener.open(request) as response:
            response.read()
        deadline = time.monotonic() + 10
        while health(url):
            if time.monotonic() >= deadline:
                raise RuntimeError('Shutdown requested, but the server has not stopped yet.')
            time.sleep(0.1)
        print('Stopped the local web app. Submitted Slurm jobs are unaffected.')
        return 0
    if not existing:
        state = REPO / '.slurmy-web'
        state.mkdir(exist_ok=True)
        with (state / 'server.log').open('ab') as log:
            child = subprocess.Popen([sys.executable, str(REPO / 'slurmy-web.py'), '--serve', '--port', str(args.port)],
                                     cwd=REPO, stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            if health(url):
                break
            if child.poll() is not None:
                raise RuntimeError(f'Web app did not start. See {state / "server.log"}; the port may be occupied.')
            time.sleep(0.15)
        else:
            raise RuntimeError(f'Timed out starting the app. See {state / "server.log"}.')
    target = target_url()
    print(target)
    if not args.no_browser and not webbrowser.open(target):
        print('Open the URL above in your browser.')
    return 0


def entrypoint(argv=None):
    try:
        return main(argv)
    except urllib.error.HTTPError as exc:
        print(exc.read().decode(errors='replace'), file=sys.stderr)
        return 1
    except (OSError, RuntimeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
