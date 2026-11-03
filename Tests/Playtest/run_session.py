#!/usr/bin/env python3
"""Keep one development server/client session inside one foreground Unreal slot.

Always wrap this whole runner, not its individual children:
  python3 Scripts/with_ue_slot.py -- python3 Tests/Playtest/run_session.py --engine PATH host --map site01 --clients 2
  python3 Scripts/with_ue_slot.py -- python3 Tests/Playtest/run_session.py --engine PATH join --map site01 --address HOST_LAN_IP:7777

An automated pass means observed editor connection/map-travel logs only. It does
not prove gameplay, replication correctness, a packaged build, or a human LAN test.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import ue

MAP_ASSETS = {"graybox": "/Game/Maps/VS_Graybox", "site01": "/Game/Maps/Site01/VS_Site01"}
MODE_CLASSES = {"practice": "VSGameMode", "match": "VSMatchGameMode"}
STARTUP_SECONDS = 60.0
CONNECT_SECONDS = 90.0
CLEANUP_SECONDS = 5.0
POLL_SECONDS = 0.25
STOP_SIGNALS = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)
FAILURE_PATTERN = re.compile(
    r"(?:Network\s*Failure|Travel\s*Failure|ConnectionTimeout|PendingConnectionFailure|"
    r"Fatal error|=== Critical error|Assertion failed|failed to Listen\(|Failed to bind socket)", re.I)


class SessionError(RuntimeError):
    pass


class StopRequested(Exception):
    def __init__(self, signum):
        self.signum = signum


def duration(value):
    number = float(value)
    if not 5 <= number <= 300:
        raise argparse.ArgumentTypeError("duration must be 5–300 seconds")
    return number


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--engine", help="pinned Unreal root; defaults to UE_ROOT")
    actions = result.add_subparsers(dest="action", required=True)
    host = actions.add_parser("host", help="one editor server and optional local clients")
    host.add_argument("--mode", choices=tuple(MODE_CLASSES), default="practice")
    host.add_argument("--port", type=ue.port, default=7777)
    host.add_argument("--clients", type=int, choices=(0, 1, 2), default=1)
    join = actions.add_parser("join", help="client processes joining a validated host:port")
    join.add_argument("--address", required=True, type=ue.address)
    join.add_argument("--clients", type=int, choices=(1, 2), default=1)
    for action in (host, join):
        action.add_argument("--map", choices=tuple(MAP_ASSETS), default="graybox",
                            help="local preflight and expected joined map; does not send map travel options")
        action.add_argument("--duration-seconds", type=duration,
                            help="automated observation period AFTER connection; otherwise Ctrl-C stops")
        action.add_argument("--headless-clients", action="store_true",
                            help="null-rendered clients for connection smoke testing; no visual evidence")
    return result


def read_log(path):
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError:
        return ""


def failure_lines(text):
    # Avoid matching an incomplete last write while a process is still logging.
    return [line.rstrip() for line in text.splitlines(keepends=True)
            if line.endswith("\n") and FAILURE_PATTERN.search(line)][:10]


def map_load_complete(text, map_asset):
    return re.search(r"Took [\d.,]+ seconds to LoadMap\(" + re.escape(map_asset) + r"\)", text)


def server_evidence(text, map_asset, port, mode):
    return {
        "listening": re.search(r"listening on port " + str(port) + r"\b", text) is not None,
        "map_load_complete": bool(map_load_complete(text, map_asset)),
        "expected_game_mode": re.search(r"Game class is '" + MODE_CLASSES[mode] + r"'", text) is not None,
        "join_success_count": len(re.findall(r"\bJoin succeeded:", text)),
        "failures": failure_lines(text),
    }


def client_evidence(text, map_asset):
    welcome = re.search(r"Welcomed by server \(Level: " + re.escape(map_asset) + r"(?:[?,])", text)
    complete = map_load_complete(text[welcome.end():], map_asset) if welcome else None
    return {"welcomed_for_expected_map": welcome is not None,
            "map_load_completed_after_welcome": complete is not None,
            "failures": failure_lines(text)}


def evaluate_logs(logs, args):
    map_asset = MAP_ASSETS[args.map]
    evidence = {"clients": {}, "connection_checks_passed": False}
    if args.action == "host":
        server = server_evidence(logs.get("server", ""), map_asset, args.port, args.mode)
        evidence["server"] = server
        host_ok = (server["listening"] and server["map_load_complete"] and server["expected_game_mode"]
                   and server["join_success_count"] >= args.clients and not server["failures"])
    else:
        host_ok = True  # No server log is available on the joining Mac.
    for index in range(1, args.clients + 1):
        role = f"client{index}"
        evidence["clients"][role] = client_evidence(logs.get(role, ""), map_asset)
    evidence["connection_checks_passed"] = host_ok and all(
        c["welcomed_for_expected_map"] and c["map_load_completed_after_welcome"] and not c["failures"]
        for c in evidence["clients"].values())
    return evidence


def child_command(args, engine, root, host, role, log):
    if role == "server":
        parsed = ue.parser().parse_args(["server", "--map", args.map, "--mode", args.mode,
                                         "--port", str(args.port)])
    else:
        address = f"127.0.0.1:{args.port}" if args.action == "host" else args.address
        parsed = ue.parser().parse_args(["client", "--address", address])
    argv = ue.command(parsed, engine, root, host)
    argv = [item for item in argv if not item.lower().startswith("-abslog=")]
    argv += [f"-abslog={log}", "-DisablePlugins=AndroidFileServer"]
    if role != "server" and args.headless_clients:
        argv += ["-nullrhi", "-nosound", "-unattended"]
    return argv


def stop_children(children):
    """Stop only process groups created by this runner; finish inside slot grace."""
    for child in children:
        try:
            os.killpg(child.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + CLEANUP_SECONDS
    for child in children:
        try:
            child.wait(timeout=max(0.01, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            pass
    # A launcher can exit while a descendant remains. The group ID still belongs
    # to that launched session until the group disappears; never signal by name.
    for child in children:
        try:
            os.killpg(child.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    for child in children:
        child.wait(timeout=1)


def run_session(args, engine, root, output, check_stop):
    processes, streams, commands, paths = {}, [], {}, {}
    evidence = {}
    problem, stopped, status = None, None, "starting"
    live_exit_codes = {}

    def launch(role):
        path = output / f"{role}.log"
        paths[role] = path
        argv = child_command(args, engine, root, "Mac", role, path)
        commands[role] = argv
        stream = (output / f"{role}.stdout.log").open("w", encoding="utf-8")
        streams.append(stream)
        processes[role] = subprocess.Popen(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                           start_new_session=True)
        print(f"Started {role}, PID {processes[role].pid}; {path}", flush=True)

    def current_logs():
        return {role: read_log(path) for role, path in paths.items()}

    def inspect_live():
        check_stop()
        for role, process in processes.items():
            if process.poll() is not None:
                live_exit_codes[role] = process.returncode
                raise SessionError(f"{role} exited early with code {process.returncode}; inspect {paths[role]}")
        logs = current_logs()
        for role, text in logs.items():
            failures = failure_lines(text)
            if failures:
                raise SessionError(f"{role} reported a connection/runtime error: {failures[0]}")
        return logs

    try:
        if args.action == "host":
            # Diagnostic only; the Unreal listener itself is the readiness proof.
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as port_probe:
                port_probe.bind(("0.0.0.0", args.port))
            launch("server")
            deadline = time.monotonic() + STARTUP_SECONDS
            while True:
                logs = inspect_live()
                state = server_evidence(logs["server"], MAP_ASSETS[args.map], args.port, args.mode)
                if state["listening"] and state["map_load_complete"] and state["expected_game_mode"]:
                    break
                if time.monotonic() >= deadline:
                    raise SessionError("Server did not load the expected map/mode and listen within 60 seconds")
                time.sleep(POLL_SECONDS)
        for index in range(1, args.clients + 1):
            launch(f"client{index}")
        status = "connecting"
        deadline = time.monotonic() + CONNECT_SECONDS
        while True:
            logs = inspect_live()
            evidence = evaluate_logs(logs, args)
            if evidence["connection_checks_passed"]:
                break
            if time.monotonic() >= deadline:
                raise SessionError("Connection/map travel did not complete within 90 seconds; inspect per-process logs")
            time.sleep(POLL_SECONDS)
        status = "connected"
        print("Expected local startup/connection checks observed. Ctrl-C stops only this session.", flush=True)
        deadline = time.monotonic() + args.duration_seconds if args.duration_seconds else None
        while True:
            logs = inspect_live()
            evidence = evaluate_logs(logs, args)
            if deadline is not None and time.monotonic() >= deadline:
                if not evidence["connection_checks_passed"]:
                    raise SessionError("Connection evidence no longer satisfies the expected session")
                status = "completed"
                break
            time.sleep(POLL_SECONDS)
    except StopRequested as error:
        stopped, status = error.signum, "stopped"
    except (OSError, SessionError, ue.SetupError, ValueError) as error:
        problem, status = str(error), "failed"
    finally:
        # Evaluate before intentional disconnects create shutdown warnings.
        evidence = evaluate_logs(current_logs(), args)
        stop_children(list(processes.values()))
        for stream in streams:
            stream.close()
    return {"status": status, "passed": status == "completed" and not problem
            and evidence.get("connection_checks_passed", False), "error": problem,
            "stop_signal": stopped, "commands": commands,
            "pids": {name: child.pid for name, child in processes.items()},
            "exit_codes_before_cleanup": live_exit_codes,
            "exit_codes_after_owned_cleanup": {name: child.returncode for name, child in processes.items()},
            "evidence": evidence}


def source_hashes(root):
    paths = sorted((root / "Source/ValStrike").rglob("*"))
    paths += [root / "Scripts/ue.py", root / "Scripts/check_playtest.py",
              root / "Tests/Playtest/run_session.py"]
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in paths if path.is_file()}


def main(argv=None, root=ROOT):
    args = parser().parse_args(argv)
    if args.action == "host" and args.mode == "match" and args.map != "site01":
        print("ERROR: match mode requires --map site01 and the integrated match class.", file=sys.stderr)
        return 2
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:8]
    output = root / "Saved/Playtest" / run_id
    output.mkdir(parents=True)
    result = {"run_id": run_id, "passed": False, "status": "preflight", "commands": {},
              "scope": "development Editor connection/map-travel evidence only",
              "headless_clients": args.headless_clients, "human_playtest": False,
              "packaged_verified": False, "replication_correctness_verified": False,
              "action": args.action, "map": args.map, "expected_local_client_count": args.clients,
              "endpoint": f"127.0.0.1:{args.port}" if args.action == "host" else args.address,
              "duration_seconds_after_connection": args.duration_seconds}
    requested = None

    def on_signal(signum, _frame):
        nonlocal requested
        requested = signum

    def check_stop():
        if requested is not None:
            raise StopRequested(requested)

    previous = {sig: signal.signal(sig, on_signal) for sig in STOP_SIGNALS}
    began = time.monotonic()
    try:
        checker = importlib.import_module("check_playtest")
        if ue.host_platform() != "Mac" or os.name != "posix":
            raise SessionError("This rehearsal runner is for the pinned macOS toolchain")
        engine = ue.engine_path(args.engine)
        result["toolchain"] = ue.check_xcode()
        result["engine_build"] = json.loads((engine / "Build/Build.version").read_text())
        result["checkout"] = checker.checkout_revision(root)
        result["assets"] = checker.validate_assets(root, args.map)
        result["source_sha256"] = source_hashes(root)
        result["native_identity"] = checker.native_identity(root, args.map)
        if result["native_identity"]["engine_root"] != str(engine):
            raise SessionError("Selected engine differs from the verified Editor build receipt; rebuild with the selected engine.")
        check_stop()
        if args.action == "host" and args.mode == "match":
            result["native_match_probe"] = checker.probe_native(engine, root, args.map, "match")
            if result["native_match_probe"].get("passed") is not True:
                raise SessionError("Native match probe did not pass; server was not launched")
        check_stop()
        result.update(run_session(args, engine, root, output, check_stop))
    except StopRequested as error:
        result.update(status="stopped", stop_signal=error.signum)
    except Exception as error:
        result.update(status="failed", error=str(error))
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)
        result["wall_seconds"] = round(time.monotonic() - began, 3)
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Session {result['status']}; report: {output / 'result.json'}", flush=True)
    if result.get("error"):
        print(result["error"], file=sys.stderr)
    if result.get("stop_signal"):
        return 128 + result["stop_signal"]
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
