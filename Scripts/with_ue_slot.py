#!/usr/bin/env python3
"""Serialize foreground Unreal work across Git worktrees (POSIX only).

Usage: python3 Scripts/with_ue_slot.py -- COMMAND [ARGS]
Wrap the whole build/test runner, not each child in a multiplayer test. The lock
file is persistent diagnostic metadata; only the kernel lock determines ownership.
"""

import fcntl
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
BUSY = 75
STOP_GRACE_SECONDS = 10
STOP_SIGNALS = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)


def lock_path():
    result = subprocess.run(["git", "rev-parse", "--git-common-dir"], cwd=ROOT,
                            capture_output=True, text=True, check=True)
    common = Path(result.stdout.strip())
    if not common.is_absolute():
        common = ROOT / common
    return common.resolve().parent / ".parallel-worktrees/.ue-slot.lock"


def run_child(argv, lock):
    child = None
    requested = None
    deadline = None

    def request_stop(signum, _frame):
        nonlocal requested
        if requested is None:
            requested = signum

    previous = {sig: signal.signal(sig, request_stop) for sig in STOP_SIGNALS}
    try:
        owner = {"owner_pid": os.getpid(), "cwd": str(Path.cwd()), "argv": argv}

        def record():
            lock.seek(0)
            lock.truncate()
            lock.write(json.dumps(owner) + "\n")
            lock.flush()

        record()
        # The inherited descriptor also protects the slot if this wrapper dies.
        # A separate session prevents terminal Ctrl-C from being delivered twice.
        child = subprocess.Popen(argv, start_new_session=True, pass_fds=(lock.fileno(),))
        owner["child_pid"] = child.pid
        record()
        while True:
            if requested is not None and deadline is None:
                print(f"UE slot: forwarding signal {requested} to child PID {child.pid}; waiting for cleanup.",
                      file=sys.stderr, flush=True)
                child.send_signal(requested)
                deadline = time.monotonic() + STOP_GRACE_SECONDS
            if deadline is not None and time.monotonic() >= deadline and child.poll() is None:
                print(f"UE slot: child PID {child.pid} did not stop; killing only that child.",
                      file=sys.stderr, flush=True)
                child.kill()
                return child.wait()
            try:
                return child.wait(timeout=0.1)
            except subprocess.TimeoutExpired:
                pass
    finally:
        # Do not unwind the lock context while our directly spawned child lives.
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=STOP_GRACE_SECONDS)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        for sig, handler in previous.items():
            signal.signal(sig, handler)


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if len(argv) < 2 or argv[0] != "--":
        print(__doc__.splitlines()[2], file=sys.stderr)
        return 2
    try:
        path = lock_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a+", encoding="utf-8") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                lock.seek(0)
                owner = lock.read().strip() or "owner metadata is being updated"
                print(f"UE slot busy ({path}): {owner}. Retry after that run finishes.", file=sys.stderr)
                return BUSY
            return run_child(argv[1:], lock)
    except FileNotFoundError as error:
        print(f"UE slot: executable or path not found: {error}", file=sys.stderr)
        return 127
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"UE slot: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    status = main()
    if status < 0:
        # Preserve signal termination for subprocess callers as well as shells.
        if -status not in (signal.SIGKILL, signal.SIGSTOP):
            signal.signal(-status, signal.SIG_DFL)
        os.kill(os.getpid(), -status)
    sys.exit(status)
