#!/usr/bin/env python3
"""Run the damage/scoreboard runtime smoke: Site01 match mode, one Development
editor server and two clients (driver, observer) on loopback.

Build the Editor target first, then wrap this entire runner with with_ue_slot.py:
  python3 Scripts/with_ue_slot.py -- python3 Tests/Damage/run_runtime.py --engine PATH

Clients render at 960x540 by default (needed for the zone and scoreboard
screenshots); --headless uses the null RHI for every role and skips screenshots.
This is loopback development evidence, not packaged or LAN acceptance.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import ue  # noqa: E402

ROLES = ("server", "driver", "observer")
MAP = "/Game/Maps/Site01/VS_Site01"
MODE = "/Script/ValStrike.VSMatchGameMode"
PORTS = range(7921, 7925)
NAMES = {"driver": "VSDamageDriver", "observer": "VSDamageObserver"}
CLIENT_CHECKS = (
    "client_zones_present", "client_stats_actor", "client_crouched_zones", "stats_replicated_kill",
    "scoreboard_hidden_by_default", "scoreboard_tab_shows", "scoreboard_tab_hides", "scoreboard_respects_menu",
    "client_fresh_pawn_zones", "stats_replicated_final", "scoreboard_round_score",
)
REQUIRED_CHECKS = {
    "server": (
        "zones_on_every_pawn", "legacy_regions_removed", "zones_collision_profile", "zones_standing_layout",
        "stats_actor_spawned", "hook_respects_preparation_gate", "zones_crouched_layout", "crouched_trace_zones",
        "zones_restand_layout", "weapon_trace_zones", "real_fire_thigh", "real_fire_upper_arm", "real_fire_foot",
        "real_fire_lethal_headshot", "kill_event_once", "economy_kill_reward", "hook_rejects_invalid",
        "dead_zones_disabled", "stats_kill_attributed", "stats_life_state", "fresh_pawn_zones",
        "stats_persist_across_rounds", "real_fire_pistol_headshot", "stats_final",
    ),
    "driver": CLIENT_CHECKS,
    "observer": CLIENT_CHECKS + ("client_debug_draw",),
}
SCREENSHOTS = {
    "observer": ("zones_standing", "zones_crouched", "scoreboard", "scoreboard_final"),
    "driver": ("scoreboard", "scoreboard_final"),
}


def damage_port(value):
    try:
        port = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("ports must be integers from 7921 through 7924") from error
    if port not in PORTS:
        raise argparse.ArgumentTypeError("damage smoke ports are reserved to 7921 through 7924")
    return port


def positive_seconds(value):
    try:
        seconds = float(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("timeouts must be finite positive numbers") from error
    if not math.isfinite(seconds) or seconds <= 0:
        raise argparse.ArgumentTypeError("timeouts must be finite positive numbers")
    return seconds


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=damage_port, default=7921)
    parser.add_argument("--timeout", type=positive_seconds, default=300.0)
    parser.add_argument("--startup-timeout", type=positive_seconds, default=90.0,
                        help="server listen deadline; a cold first launch can exceed 40 s")
    parser.add_argument("--headless", action="store_true", help="null RHI clients; screenshots are skipped")
    return parser.parse_args(argv)


def reject_constant(value):
    raise ValueError(f"Non-finite JSON number: {value}")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load_report(path):
    """Absence is pending; an existing malformed report fails."""
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"), parse_constant=reject_constant,
                          object_pairs_hook=unique_object)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise RuntimeError(f"Malformed damage report {path.name}: {error}") from error


def evaluate(reports, required=None):
    required = REQUIRED_CHECKS if required is None else required
    if not isinstance(reports, dict) or set(reports) != set(ROLES):
        raise RuntimeError("Evidence must contain exactly server, driver and observer reports")
    checks = []
    for role in ROLES:
        report = reports[role]
        if not isinstance(report, dict):
            raise RuntimeError(f"Missing or invalid {role} report")
        if type(report.get("schema_version")) is not int or report["schema_version"] != 1 \
                or report.get("role") != role or report.get("completed") is not True:
            raise RuntimeError(f"Invalid identity, schema or completion in {role} report")
        if "error" not in report or (report["error"] is not None and not isinstance(report["error"], str)):
            raise RuntimeError(f"Invalid or missing error field in {role} report")
        if report["error"]:
            raise RuntimeError(f"{role} harness failed: {report['error']}")
        items = report.get("checks")
        if not isinstance(items, list) or not items:
            raise RuntimeError(f"No checks in {role} report")
        names = set()
        for item in items:
            if not isinstance(item, dict):
                raise RuntimeError(f"Invalid check in {role} report")
            name = item.get("name")
            if not isinstance(name, str) or not name or name in names or type(item.get("passed")) is not bool \
                    or not isinstance(item.get("detail"), str):
                raise RuntimeError(f"Invalid or duplicate check in {role} report: {name!r}")
            names.add(name)
            checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
        missing = set(required[role]) - names
        if missing:
            raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    return checks


def screenshot_paths(output):
    return {f"{role}_{name}": output / "Screenshots" / f"{role}_{name}.png"
            for role, names in SCREENSHOTS.items() for name in names}


def launch_command(executable, project, output, role, port, headless=False):
    if role not in ROLES:
        raise ValueError(f"Unknown damage smoke role: {role}")
    port = damage_port(port)
    if role == "server":
        destination = f"{MAP}?game={MODE}"
    else:
        destination = f"127.0.0.1:{port}?Name={NAMES[role]}"
    command = [str(executable), str(project), destination, "-server" if role == "server" else "-game",
               "-nosound", "-unattended", f"-VSDamageSmoke={role}", f"-VSDamageOutput={output / (role + '.json')}",
               f"-abslog={output / (role + '.log')}", "-ExecCmds=t.MaxFPS 60"]
    if role == "server" or headless:
        command.append("-nullrhi")
    else:
        command += ["-windowed", "-ResX=960", "-ResY=540", f"-VSDamageShots={output / 'Screenshots'}"]
    if role == "server":
        command += [f"-port={port}", "-multihome=127.0.0.1"]
    return command


def check_processes_running(processes):
    for role, process in processes.items():
        code = process.poll()
        if code is not None:
            raise RuntimeError(f"{role} process exited unexpectedly with code {code} before runner cleanup")


def stop(processes):
    """Close only the owned process groups, within the slot's 10 s grace."""
    processes = list(processes)
    for process in processes:
        try:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGTERM)
            elif process.poll() is None:
                process.terminate()
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 5.0
    for process in processes:
        try:
            process.wait(timeout=max(0.01, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            pass
    for process in processes:
        try:
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGKILL)
            elif process.poll() is None:
                process.kill()
        except ProcessLookupError:
            pass
    deadline = time.monotonic() + 2.0
    for process in processes:
        try:
            process.wait(timeout=max(0.01, deadline - time.monotonic()))
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"Owned process PID {process.pid} did not stop after cleanup") from error


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def evidence_metadata(root):
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True,
                              check=False, timeout=15)
    status = subprocess.run(["git", "--no-optional-locks", "status", "--porcelain"], cwd=root,
                            capture_output=True, text=True, check=False, timeout=15)
    paths = [path for folder in ("Source", "Config", "Tests/Damage", "Tests/Scoreboard")
             for path in (root / folder).rglob("*") if path.is_file() and "__pycache__" not in path.parts]
    paths += [root / "ValStrike.uproject", root / "Scripts/ue.py", root / "Scripts/with_ue_slot.py"]
    module = root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib"
    map_file = root / "Content/Maps/Site01/VS_Site01.umap"
    return {
        "revision": revision.stdout.strip() if revision.returncode == 0 else None,
        "dirty_checkout": bool(status.stdout.strip()) if status.returncode == 0 else None,
        "source_sha256": {str(path.relative_to(root)): sha256(path) for path in sorted(paths) if path.is_file()},
        "editor_module_sha256": sha256(module) if module.is_file() else None,
        "map_sha256": sha256(map_file) if map_file.is_file() else None,
    }


def main(argv=None):
    args = arguments(argv)
    if ue.host_platform() != "Mac":
        raise ue.SetupError("The damage smoke runner currently verifies the pinned Mac Editor build only")
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, "Mac")
    project = ue.project_file(ROOT)
    map_file = ue.require_file(ROOT / "Content/Maps/Site01/VS_Site01.umap", "Restore the Site01 map with Git LFS.")
    with map_file.open("rb") as stream:
        if stream.read(64).startswith(b"version https://git-lfs.github.com/spec/"):
            raise ue.SetupError("Site01 is a Git LFS pointer; retrieve the real map before runtime testing")
    ue.require_file(ROOT / "Binaries/Mac/libUnrealEditor-ValStrike.dylib", "Build this checkout's Mac Editor target first.")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/DamageRuntime" / run_id
    (output / "Screenshots").mkdir(parents=True)
    metadata = evidence_metadata(ROOT)
    build = json.loads((engine / "Build/Build.version").read_text())
    processes, streams, commands = {}, [], {}
    before_cleanup, cleanup_exit = {}, {}
    interrupted = None

    def request_stop(signum, _frame):
        nonlocal interrupted
        interrupted = interrupted or f"Damage smoke interrupted by signal {signum}"

    watched = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)
    previous = {sig: signal.signal(sig, request_stop) for sig in watched}

    def healthy():
        if interrupted:
            raise RuntimeError(interrupted)
        check_processes_running(processes)

    def launch(role):
        command = launch_command(executable, project, output, role, args.port, args.headless)
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes[role] = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                           start_new_session=True)

    problem = None
    began = time.monotonic()
    try:
        launch("server")
        server_log = output / "server.log"
        deadline = began + min(args.startup_timeout, args.timeout)
        while True:
            healthy()
            log = server_log.read_text(errors="replace") if server_log.exists() else ""
            if f"listening on port {args.port}" in log:
                break
            if time.monotonic() >= deadline:
                raise RuntimeError("Server did not reach listening state before the startup timeout; inspect server.log")
            time.sleep(0.2)
        launch("observer")
        launch("driver")
        print(f"Damage smoke running; evidence: {output}", flush=True)
        while True:
            healthy()
            if time.monotonic() - began >= args.timeout:
                raise RuntimeError("Timed out before all three roles completed")
            reports = {role: load_report(output / f"{role}.json") for role in ROLES}
            if all(report is not None for report in reports.values()):
                evaluate(reports)
                time.sleep(1.0)  # let the last requested screenshot reach disk
                healthy()
                break
            time.sleep(0.25)
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Damage smoke interrupted"
    finally:
        before_cleanup = {role: process.poll() for role, process in processes.items()}
        if any(code is not None for code in before_cleanup.values()):
            problem = problem or f"A test process exited before cleanup: {before_cleanup}"
        try:
            stop(processes.values())
        except (OSError, RuntimeError) as error:
            problem = problem or f"Process cleanup failed: {error}"
        finally:
            cleanup_exit = {role: process.poll() for role, process in processes.items()}
            for stream in streams:
                stream.close()
            for sig, handler in previous.items():
                signal.signal(sig, handler)

    checks, after, shots = [], None, {}
    try:
        reports = {role: load_report(output / f"{role}.json") for role in ROLES}
        checks = evaluate(reports)
        after = evidence_metadata(ROOT)
        if any(metadata[key] != after[key] for key in ("source_sha256", "editor_module_sha256", "map_sha256")):
            problem = problem or "Source, Editor module or Site01 map changed during the runtime test"
        if not args.headless:
            shots = {name: str(path) for name, path in screenshot_paths(output).items()}
            missing = [name for name, path in screenshot_paths(output).items() if not path.is_file() or path.stat().st_size == 0]
            if missing:
                problem = problem or f"Missing screenshots: {', '.join(missing)}"
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        problem = problem or f"Invalid runtime evidence: {error}"
        reports = {}
    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    result = {
        **metadata, "passed": passed, "error": problem, "engine": str(engine), "engine_build": build,
        "metadata_after": after, "headless": args.headless,
        "conditions": "Local Development editor dedicated server (match mode, Site01) and two clients; loopback, "
                      "no audio, no artificial latency/loss; "
                      + ("null RHI for all roles" if args.headless else "clients rendered at 960x540, server headless"),
        "runner_command": [sys.executable, str(Path(__file__).resolve()), *(sys.argv[1:] if argv is None else argv)],
        "commands": commands, "process_pids": {role: process.pid for role, process in processes.items()},
        "exit_before_cleanup": before_cleanup, "exit_after_cleanup": cleanup_exit,
        "elapsed_seconds": time.monotonic() - began, "screenshots": shots, "checks": checks, "reports": reports,
    }
    (output / "result.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    for check in checks:
        print(("PASS " if check["passed"] else "FAIL ") + check["check"] + ": " + check["detail"])
    if problem:
        print(problem, file=sys.stderr)
    print(f"Result: {'PASS' if passed else 'FAIL'}; {output / 'result.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ue.SetupError, OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
