#!/usr/bin/env python3
"""Run real Unreal UI/menu/travel checks. Development editor evidence only."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import ue
from test_movement import stop


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=ue.port, default=7813)
    parser.add_argument("--failure-port", type=ue.port, default=7814)
    parser.add_argument("--timeout", type=float, default=110)
    parser.add_argument("--quit-timeout", type=float, default=30, help="Maximum seconds for native client termination after Quit game")
    parser.add_argument("--render", "--rendered", action="store_true", help="Render the driver window and capture native screenshots")
    args = parser.parse_args()
    if args.port == args.failure_port:
        raise ValueError("Server and failure ports must be different")
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(ROOT)
    ue.map_file(ROOT)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    # Hold a silent UDP endpoint so a different process cannot accidentally make
    # the expected timeout test succeed during this run.
    failure_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    failure_socket.bind(("127.0.0.1", args.failure_port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/UI" / run_id
    output.mkdir(parents=True)
    paths = {role: output / f"{role}.log" for role in ("server", "driver")}
    commands, processes, streams = {}, [], []

    def launch(role):
        command = [str(executable), str(project), ue.MAP,
                   "-server" if role == "server" else "-game", "-nosound", "-unattended",
                   f"-abslog={paths[role]}", "-ExecCmds=t.MaxFPS 60"]
        if role == "server":
            command += ["-nullrhi", f"-port={args.port}", "-multihome=127.0.0.1"]
        else:
            command += ["-VSUISmoke", f"-VSUISmokeAddress=127.0.0.1:{args.port}",
                        f"-VSUISmokeFailureAddress=127.0.0.1:{args.failure_port}",
                        f"-VSUISmokeScreenshotDir={output / 'screenshots'}"]
            if not args.render:
                command.append("-nullrhi")
            else:
                command += ["-windowed", "-ResX=960", "-ResY=540"]
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=os.name == "posix")
        processes.append(process)
        return process

    problem, driver_exit, quit_wait_seconds = None, None, None
    try:
        server = launch("server")
        began = time.monotonic()
        while f"listening on port {args.port}" not in read(paths["server"]):
            if server.poll() is not None or time.monotonic() - began > 40:
                raise RuntimeError("Server did not reach listening state")
            time.sleep(0.2)
        driver = launch("driver")
        print(f"UI smoke running; evidence: {output}", flush=True)
        while "VSUI_DONE " not in read(paths["driver"]):
            if driver.poll() is not None or server.poll() is not None:
                raise RuntimeError("A process exited before UI checks completed")
            if time.monotonic() - began > args.timeout:
                raise RuntimeError("Timed out before UI checks completed")
            time.sleep(0.2)
        try:
            quit_began = time.monotonic()
            driver_exit = driver.wait(timeout=args.quit_timeout)
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"Quit game did not terminate the native client process within {args.quit_timeout:g} seconds")
        finally:
            quit_wait_seconds = time.monotonic() - quit_began
    except (RuntimeError, OSError) as error:
        problem = str(error)
    finally:
        stop(processes)
        failure_socket.close()
        for stream in streams:
            stream.close()

    log = read(paths["driver"])
    checks = [{"check": name, "passed": status == "PASS"}
              for name, status in re.findall(r"VSUI_CHECK ([a-z_]+) (PASS|FAIL)", log)]
    done = re.findall(r"VSUI_DONE checks=(\d+) failures=(\d+)", log)
    required = {"baseline_combat_data_is_unavailable", "escape_opens_menu", "opening_flushes_held_forward",
                "tab_changes_menu_focus", "shift_tab_restores_menu_focus", "slate_escape_resumes",
                "invalid_address_rejected_before_travel", "address_text_entry_updates_session",
                "enter_connect_focuses_enabled_cancel", "real_server_travel_completed",
                "connected_menu_blocks_forward", "connected_backward_moves_after_resume",
                "disconnect_reopens_menu", "cancel_returns_to_fresh_local_world", "cancel_feedback_survives_travel",
                "connection_error_survives_travel", "connection_error_is_actionable"}
    names = {item["check"] for item in checks}
    checks.append({"check": "required_checks_recorded_once", "passed": required <= names and len(names) == len(checks)})
    checks.append({"check": "runtime_report_complete", "passed": len(done) == 1 and int(done[0][0]) == len(names) and int(done[0][1]) == 0})
    checks.append({"check": "quit_shuts_down_game_engine", "passed": "LogExit: Game engine shut down" in log and "Log file closed" in log})
    checks.append({"check": "quit_exits_client", "passed": driver_exit == 0, "exit_code": driver_exit,
                   "wait_seconds": quit_wait_seconds, "timeout_seconds": args.quit_timeout})
    if args.render:
        for name in ("baseline", "menu", "invalid-address", "connected-menu", "disconnected", "cancelled", "connection-error"):
            path = output / "screenshots" / f"{name}.png"
            checks.append({"check": f"screenshot_{name}", "passed": path.exists() and path.stat().st_size > 0,
                           "path": str(path)})
    passed = not problem and all(item["passed"] for item in checks)
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    # Scope status to source/test paths; Git LFS assets are neither touched nor inspected by this runner.
    dirty = bool(subprocess.run(["git", "status", "--porcelain", "--", "Source", "Tests/UI"],
                                cwd=ROOT, capture_output=True, text=True).stdout.strip())
    hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted((ROOT / "Source/ValStrike").rglob("*")) if path.is_file()}
    result = {"passed": passed, "error": problem, "revision": revision, "engine": str(engine),
              "dirty_source_checkout": dirty, "source_sha256": hashes,
              "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": f"Two local editor processes; driver {'rendered' if args.render else 'null RHI'}; loopback; no audio or artificial latency/loss; engine-configured connection timeout (no override)",
              "commands": commands, "checks": checks}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for item in checks:
        print(("PASS " if item["passed"] else "FAIL ") + item["check"])
    if problem:
        print(problem, file=sys.stderr)
    print(f"Result: {'PASS' if passed else 'FAIL'}; {output / 'result.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ue.SetupError, OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
