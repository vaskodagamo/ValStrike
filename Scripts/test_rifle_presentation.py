#!/usr/bin/env python3
"""Run the rendered rifle presentation server/owner/observer smoke test.

Required launch (wrap this whole runner once, not its children):
python3 Scripts/with_ue_slot.py -- python3 Scripts/test_rifle_presentation.py --engine PATH
A wrapper exit of 75 means the shared engine slot is busy; retry after that run.
This is local Development Editor evidence, not packaged or LAN acceptance.
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
import struct
import subprocess
import sys
import time
import uuid

import test_combat as combat
import ue
import with_ue_slot


ROLES = ("server", "owner", "observer")
GAME_MODE = "/Script/ValStrike.VSRiflePresentationTestGameMode"
# Fixed names form the contract with the opt-in native presentation harness.
CLIENT_CHECKS = (
    "attached_assets", "flash_material", "no_collision", "visibility", "neutral_view", "dim_view",
    "accepted_shot_once", "flash_shader_ready", "duplicate_snapshot_quiet", "reload_pose", "reload_completed",
    "menu_release", "late_attach_no_replay", "death_cleanup", "new_pawn", "aim_unchanged",
)
REQUIRED_CHECKS = {
    "server": ("two_players", "authoritative_shot", "reload_completed", "menu_release", "death", "new_pawn"),
    "owner": CLIENT_CHECKS + ("menu_visible",),
    "observer": CLIENT_CHECKS,
}
CLIENT_SCREENSHOTS = ("neutral", "dim", "muzzle", "reload", "death")
REQUIRED_SCREENSHOTS = {"server": (), "owner": CLIENT_SCREENSHOTS + ("menu",),
                        "observer": CLIENT_SCREENSHOTS}


# 7851..7854: original rifle-presentation stream; 7891..7894: weapons stream,
# which now owns Presentation/. Other parallel sessions use different ports.
PORT_RANGES = ((7851, 7854), (7891, 7894))


def presentation_port(value):
    parsed = ue.port(value)
    if not any(low <= parsed <= high for low, high in PORT_RANGES):
        raise argparse.ArgumentTypeError("Rifle presentation ports are reserved to 7851..7854 and 7891..7894")
    return parsed


def require_engine_slot():
    """Verify the inherited wrapper descriptor without acquiring a second lock."""
    path = with_ue_slot.lock_path()
    try:
        identity = path.stat()
        owner = json.loads(path.read_text())
        if owner.get("owner_pid") != os.getppid():
            raise ValueError("Parent is not the slot owner")
        for entry in os.listdir("/dev/fd"):
            try:
                descriptor = int(entry)
                actual = os.fstat(descriptor)
            except (OSError, ValueError):
                continue
            if (actual.st_dev, actual.st_ino) == (identity.st_dev, identity.st_ino):
                return descriptor
    except (OSError, ValueError, AttributeError):
        pass
    raise RuntimeError("Run the whole test through python3 Scripts/with_ue_slot.py -- python3 "
                       "Scripts/test_rifle_presentation.py --engine PATH; do not acquire nested slots")


def validate_report(role, report, required_checks=None):
    required = REQUIRED_CHECKS if required_checks is None else required_checks
    if (role not in ROLES or not isinstance(report, dict)
            or type(report.get("schema_version")) is not int or report["schema_version"] != 1
            or report.get("role") != role or report.get("completed") is not True):
        raise RuntimeError(f"Missing or invalid identity/schema/completion in {role} report")
    error = report.get("error")
    if error is not None and not isinstance(error, str):
        raise RuntimeError(f"Invalid error field in {role} report")
    if error:
        raise RuntimeError(f"{role} presentation harness failed: {error}")
    items = report.get("checks")
    if not isinstance(items, list) or not items:
        raise RuntimeError(f"No presentation checks in {role} report")
    names, checks = set(), []
    for item in items:
        if not isinstance(item, dict):
            raise RuntimeError(f"Invalid presentation check in {role} report")
        name = item.get("name")
        if (not isinstance(name, str) or not name.strip() or name in names
                or type(item.get("passed")) is not bool or "detail" not in item):
            raise RuntimeError(f"Invalid or duplicate presentation check in {role} report: {name!r}")
        names.add(name)
        checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
    missing = set(required[role]) - names
    if missing:
        raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    return checks


def evaluate(reports, required_checks=None):
    if set(reports) != set(ROLES):
        raise RuntimeError("Evidence requires exactly server, owner and observer roles")
    return [check for role in ROLES for check in validate_report(role, reports[role], required_checks)]


def collect_screenshots(output, required_screenshots=None):
    required = REQUIRED_SCREENSHOTS if required_screenshots is None else required_screenshots
    captures = {}
    for role in ROLES:
        for name in required[role]:
            path = output / f"{role}_{name}.png"
            if not path.is_file():
                raise RuntimeError(f"Missing rendered screenshot: {path.name}")
            data = path.read_bytes()
            if len(data) <= 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
                raise RuntimeError(f"Invalid or empty PNG screenshot: {path.name}")
            width, height = struct.unpack(">II", data[16:24])
            if width == 0 or height == 0:
                raise RuntimeError(f"Empty screenshot dimensions: {path.name}")
            captures[f"{role}: {name}"] = {"path": str(path), "width": width, "height": height,
                                            "sha256": hashlib.sha256(data).hexdigest()}
    return captures


def launch_command(executable, project, output, role, port, map_key="graybox"):
    if role not in ROLES:
        raise ValueError(f"Unknown presentation role: {role}")
    port = presentation_port(port)
    destination = (f"{ue.selected_map(map_key)}?game={GAME_MODE}" if role == "server"
                   else f"127.0.0.1:{port}?Name=VSPresentation{role.title()}")
    command = [str(executable), str(project), destination,
               "-server" if role == "server" else "-game", "-nosound",
               f"-VSRiflePresentationTest={role}", "-VSRiflePresentationStart=30",
               f"-VSRiflePresentationMap={map_key}",
               f"-VSRiflePresentationOutput={output / (role + '.json')}",
               f"-abslog={output / (role + '.log')}", "-ExecCmds=t.MaxFPS 60"]
    if role == "server":
        command += ["-nullrhi", "-unattended", f"-port={port}", "-multihome=127.0.0.1"]
    else:
        command += ["-windowed", "-ResX=1000", "-ResY=700"]
    return command


def stop_processes(processes):
    """Bound cleanup below the wrapper grace period; signal only owned groups."""
    def send(process, signum):
        try:
            if os.name == "posix":
                os.killpg(process.pid, signum)
            elif signum == signal.SIGTERM:
                process.terminate()
            else:
                process.kill()
        except ProcessLookupError:
            pass

    for process in processes:
        if process.poll() is None:
            send(process, signal.SIGTERM)
    deadline = time.monotonic() + 5
    while any(process.poll() is None for process in processes) and time.monotonic() < deadline:
        time.sleep(0.1)
    for process in processes:
        if process.poll() is None:
            send(process, signal.SIGKILL)
    for process in processes:
        process.wait(timeout=1)


def evidence_metadata(root):
    metadata = combat.evidence_metadata(root)
    paths = [root / "Scripts/test_rifle_presentation.py", root / "Scripts/with_ue_slot.py"]
    paths += [path for path in (root / "Tests/Presentation").rglob("*")
              if path.is_file() and "__pycache__" not in path.parts]
    metadata["source_sha256"].update({str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                                       for path in paths if path.is_file()})
    metadata["asset_sha256"] = {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for folder in ("Content/Art/Weapons/Rifle02", "Content/Art/Weapons/Rifle01", "Content/Presentation/Rifle01")
        for path in sorted((root / folder).rglob("*")) if path.is_file()
    }
    return metadata


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=presentation_port, default=7891)
    parser.add_argument("--map", choices=tuple(ue.MAPS), default="graybox",
                        help="site01 keeps the map's authored lighting; graybox runs the neutral/dim lighting study")
    parser.add_argument("--timeout", type=float, default=150)
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 40:
        raise ValueError("--timeout must be finite and greater than 40 seconds")
    slot_descriptor = require_engine_slot()
    root = ue.ROOT
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(root)
    ue.map_file(root, args.map)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + args.map + "_" + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/RiflePresentation" / run_id
    output.mkdir(parents=True)
    metadata = evidence_metadata(root)
    processes, streams, commands = [], [], {}
    reports = {role: None for role in ROLES}
    captures, checks, problem = {}, [], None
    termination_signal = None

    def interrupted(signum, _frame):
        nonlocal termination_signal
        if termination_signal is None:
            termination_signal = signum
            raise InterruptedError(f"Presentation test interrupted by signal {signum}")

    handlers = {sig: signal.signal(sig, interrupted) for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}

    def launch(role):
        command = launch_command(executable, project, output, role, args.port, args.map)
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes.append(subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                          start_new_session=True, pass_fds=(slot_descriptor,)))

    try:
        launch("server")
        began = time.monotonic()
        server_log = output / "server.log"
        while True:
            log = server_log.read_text(errors="replace") if server_log.exists() else ""
            if f"listening on port {args.port}" in log:
                break
            if processes[0].poll() is not None or time.monotonic() - began > 40:
                raise RuntimeError("Presentation server did not listen; inspect server.log")
            time.sleep(0.2)
        launch("observer")
        launch("owner")
        print(f"Rendered rifle presentation test running; evidence: {output}", flush=True)
        while time.monotonic() - began < args.timeout:
            reports = {role: combat.load_report(output / f"{role}.json") for role in ROLES}
            for role, report in reports.items():
                if report is not None:
                    validate_report(role, report)
            if all(report is not None for report in reports.values()):
                checks = evaluate(reports)
                captures = collect_screenshots(output)
                break
            if any(process.poll() is not None for process in processes):
                raise RuntimeError("A presentation process exited before all three roles completed")
            time.sleep(0.2)
        else:
            raise RuntimeError("Timed out before all three presentation roles completed")
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Presentation test interrupted"
    finally:
        try:
            stop_processes(processes)
        finally:
            for stream in streams:
                stream.close()
            for sig, handler in handlers.items():
                signal.signal(sig, handler)

    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    result = {**metadata, "passed": passed, "error": problem, "engine": str(engine),
              "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "map": ue.selected_map(args.map),
              "lighting": ("authored map lighting (no fixture override)" if args.map == "site01"
                           else "fixture neutral 3.0/1.0 and dim 0.35/0.25 directional/sky intensities"),
              "conditions": "Local editor server and two rendered 1000x700 clients, no audio, loopback, no artificial latency/loss",
              "commands": commands, "checks": checks, "screenshots": captures, "reports": reports}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for check in checks:
        print(("PASS " if check["passed"] else "FAIL ") + check["check"] + ": " + json.dumps(check["detail"]))
    if problem:
        print(problem, file=sys.stderr)
    print(f"Result: {'PASS' if passed else 'FAIL'}; {output / 'result.json'}")
    return 128 + termination_signal if termination_signal else (0 if passed else 1)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ue.SetupError, RuntimeError, OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
