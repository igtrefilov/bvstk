#!/usr/bin/env python3
"""Manage only this checkout's hw_server processes, identified by PID/start time."""
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time


def identity(pid):
    try:
        fields = Path(f'/proc/{pid}/stat').read_text().rsplit(') ', 1)[1].split()
        if fields[0] == 'Z' or int(fields[2]) != pid or int(fields[3]) != pid:
            return None
        return fields[19]
    except (OSError, ValueError, IndexError):
        return None


def listening(port):
    for name in ('tcp', 'tcp6'):
        path = Path('/proc/net') / name
        if not path.exists():
            continue
        for line in path.read_text().splitlines()[1:]:
            fields = line.split()
            if fields[3] == '0A' and int(fields[1].rsplit(':', 1)[1], 16) == port:
                return True
    return False


def terminate(pid):
    # The caller has checked the saved PID and kernel start time. Signal the
    # whole session's process group, including Xilinx's shell/loader children.
    for sig, timeout in ((signal.SIGTERM, 5), (signal.SIGKILL, 1)):
        try:
            os.killpg(pid, sig)
        except ProcessLookupError:
            return
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                os.killpg(pid, 0)
            except ProcessLookupError:
                return
            time.sleep(0.1)


def main():
    action, port_text, state_path = sys.argv[1:]
    port = int(port_text)
    os.umask(0o077)
    state = Path(state_path)
    state.mkdir(parents=True, exist_ok=True)
    pidfile = state / f'{port}.pid'
    logfile = state / f'{port}.log'
    with (state / f'{port}.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        record = None
        if pidfile.exists():
            try:
                record = json.loads(pidfile.read_text())
                pid = record['pid']
                if not isinstance(pid, int) or pid <= 1 or identity(pid) != record['started']:
                    record = None
            except (ValueError, KeyError, TypeError):
                record = None
            if not record:
                pidfile.unlink()
        if action == 'stop':
            if not record:
                print(f'No managed hw_server on port {port}.')
                return
            terminate(record['pid'])
            pidfile.unlink()
            print(f'hw_server stopped on port {port}.')
            return
        if record:
            print(f'hw_server already running: PID {record["pid"]}, port {port}.')
            print(f'Log: {logfile}')
            return
        if listening(port):
            raise RuntimeError(f'Port {port} is occupied by another process; it was not changed.')
        binary = shutil.which('hw_server')
        if not binary:
            raise RuntimeError('hw_server not found in PATH')
        with logfile.open('ab', buffering=0) as log:
            process = subprocess.Popen([binary, '-s', f'tcp::{port}'],
                                       stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                       start_new_session=True, close_fds=True)
        started = identity(process.pid)
        try:
            if started is None:
                raise RuntimeError(f'hw_server exited during startup; see {logfile}')
            pidfile.write_text(json.dumps({'pid': process.pid, 'started': started}) + '\n')
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'hw_server exited with code {process.returncode}; see {logfile}')
                if listening(port):
                    print(f'hw_server started: PID {process.pid}, TCP port {port}.')
                    print(f'Log: {logfile}')
                    print(f'Remote XSCT: connect -url tcp:<server-ip>:{port}')
                    return
                time.sleep(0.1)
            raise RuntimeError(f'hw_server did not listen within 10 seconds; see {logfile}')
        except BaseException:
            if started is not None and identity(process.pid) == started:
                terminate(process.pid)
            process.wait()
            pidfile.unlink(missing_ok=True)
            raise


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)
