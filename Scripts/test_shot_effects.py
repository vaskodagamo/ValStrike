#!/usr/bin/env python3
"""Run the rendered shot-effects server/shooter/observer test.

Required launch (wrap this whole runner once, not its children):
python3 Scripts/with_ue_slot.py -- python3 Scripts/test_shot_effects.py --engine PATH
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


ROLES = ("server", "shooter", "observer")
GAME_MODE = "/Script/ValStrike.VSShotEffectsTestGameMode"
# Fixed names form the contract with the opt-in native fixture (Effects/VSShotEffectsTest.cpp).
SHOT_LABELS = ("wall", "floor", "cover", "player", "wall_dim", "muzzle", "rejoin")
CLIENT_CHECKS = (
    "subsystem_present", "effect_shaders_ready", "effect_materials", "tracer_short_lived",
    "mark_hold", "mark_fading", "mark_faded", "effects_inert", "respawn_no_replay", "long_mark_timing",
    "burst_accounting", "pool_cap", "no_unexpected_effects", "first_effect_no_stall",
) + tuple(f"shot_{label}" for label in SHOT_LABELS)
REQUIRED_CHECKS = {
    "server": ("two_players", "no_effects_on_server", "player_damage", "respawn", "burst_shots",
               "observer_rejoined", "no_effects_on_server_end", "no_unexpected_server_shots")
              + tuple(f"server_{label}" for label in SHOT_LABELS),
    "shooter": CLIENT_CHECKS + ("quiet_during_rejoin",),
    "observer": CLIENT_CHECKS + ("rejoin_no_replay",),
}
CLIENT_SCREENSHOTS = ("start", "tracer_neutral", "fade", "player", "tracer_dim", "muzzle", "marks_neutral", "marks_dim", "rejoin")
REQUIRED_SCREENSHOTS = {"server": (), "shooter": CLIENT_SCREENSHOTS, "observer": CLIENT_SCREENSHOTS}
# Cosmetic impact points must match the server's reference trace for stationary targets.
# The shooter's own client knows its exact aim. Observers see a remote shooter's yaw only
# through replicated actor rotation, which Unreal quantizes to 8 bits per axis by default
# (1.40625 degree steps), so their bound grows with distance by half a step (+25% for
# oblique surfaces). View pitch replicates at 16 bits and is negligible.
IMPACT_TOLERANCE_CM = 5.0
REMOTE_YAW_HALF_STEP_DEG = 360.0 / 256.0 / 2.0
EXPECTED_ROLE = {"player": "observer"}


def impact_tolerance(role, distance_cm):
    if role == "shooter":
        return IMPACT_TOLERANCE_CM
    return IMPACT_TOLERANCE_CM + 1.25 * distance_cm * math.tan(math.radians(REMOTE_YAW_HALF_STEP_DEG))


def effects_port(value):
    parsed = ue.port(value)
    if not 7871 <= parsed <= 7874:
        raise argparse.ArgumentTypeError("Shot effects ports are reserved to 7871..7874")
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
                       "Scripts/test_shot_effects.py --engine PATH; do not acquire nested slots")


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
        raise RuntimeError(f"{role} shot-effects harness failed: {error}")
    items = report.get("checks")
    if not isinstance(items, list) or not items:
        raise RuntimeError(f"No shot-effects checks in {role} report")
    if not isinstance(report.get("shots"), list):
        raise RuntimeError(f"Missing shot records in {role} report")
    names, checks = set(), []
    for item in items:
        if not isinstance(item, dict):
            raise RuntimeError(f"Invalid shot-effects check in {role} report")
        name = item.get("name")
        if (not isinstance(name, str) or not name.strip() or name in names
                or type(item.get("passed")) is not bool or "detail" not in item):
            raise RuntimeError(f"Invalid or duplicate shot-effects check in {role} report: {name!r}")
        names.add(name)
        checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
    missing = set(required[role]) - names
    if missing:
        raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    return checks


def finite_vector(value):
    return (isinstance(value, list) and len(value) == 3
            and all(isinstance(item, (int, float)) and not isinstance(item, bool) and math.isfinite(item) for item in value))


def shot_records(role, report):
    records = {}
    for item in report["shots"]:
        label = item.get("label") if isinstance(item, dict) else None
        if (label not in SHOT_LABELS or label in records or not finite_vector(item.get("point"))
                or (role != "server" and not finite_vector(item.get("eye")))):
            raise RuntimeError(f"Invalid or duplicate shot record in {role} report: {item!r}")
        records[label] = item
    missing = set(SHOT_LABELS) - set(records)
    if missing:
        raise RuntimeError(f"Missing {role} shot records: {', '.join(sorted(missing))}")
    return records


def cross_check(reports, labels=SHOT_LABELS):
    """Compare each client's cosmetic impact with the server's reference trace."""
    server = shot_records("server", reports["server"])
    checks = []
    for role in ("shooter", "observer"):
        client = shot_records(role, reports[role])
        for label in labels:
            reference, observed = server[label], client[label]
            distance = math.dist(reference["point"], observed["point"])
            tolerance = impact_tolerance(role, math.dist(observed["eye"], observed["point"]))
            expected = EXPECTED_ROLE.get(label, "world")
            passed = (reference.get("hit") is True and reference.get("role") == expected
                      and observed.get("role") == expected and distance <= tolerance)
            checks.append({"check": f"{role}: impact_matches_server_{label}", "passed": passed,
                           "detail": f"distance_cm={distance:.2f} tolerance_cm={tolerance:.2f} "
                                     f"server_role={reference.get('role')} client_role={observed.get('role')} "
                                     f"kind={observed.get('kind')}"})
    return checks


def evaluate(reports, required_checks=None):
    if set(reports) != set(ROLES):
        raise RuntimeError("Evidence requires exactly server, shooter and observer roles")
    checks = [check for role in ROLES for check in validate_report(role, reports[role], required_checks)]
    return checks + cross_check(reports)


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


def launch_command(executable, project, output, role, port, start):
    if role not in ROLES:
        raise ValueError(f"Unknown shot-effects role: {role}")
    port = effects_port(port)
    if not math.isfinite(start) or not 10 <= start <= 300:
        raise ValueError("Shared start offset must be between 10 and 300 seconds")
    destination = (f"{ue.MAP}?game={GAME_MODE}" if role == "server"
                   else f"127.0.0.1:{port}?Name=VSShotFx{role.title()}")
    command = [str(executable), str(project), destination,
               "-server" if role == "server" else "-game", "-nosound",
               f"-VSShotEffectsTest={role}", f"-VSShotEffectsStart={start:g}", f"-VSShotEffectsPort={port}",
               f"-VSShotEffectsOutput={output / (role + '.json')}",
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
    paths = [root / "Scripts/test_shot_effects.py", root / "Scripts/with_ue_slot.py"]
    paths += [path for path in (root / "Tests/Effects").rglob("*") if path.is_file() and "__pycache__" not in path.parts]
    metadata["source_sha256"].update({str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                                       for path in paths if path.is_file()})
    metadata["asset_sha256"] = {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for folder in ("Content/Effects", "Content/Art/Weapons/Rifle01", "Content/Presentation/Rifle01")
        for path in sorted((root / folder).rglob("*")) if path.is_file()
    }
    return metadata


def wait_for_listen(log_path, process, port, limit):
    began = time.monotonic()
    while True:
        log = log_path.read_text(errors="replace") if log_path.exists() else ""
        if f"listening on port {port}" in log:
            return time.monotonic() - began
        if process.poll() is not None or time.monotonic() - began > limit:
            raise RuntimeError("Shot-effects server did not listen; inspect server.log "
                               "(a cold first launch can be slow: rerun once or raise --server-start-timeout)")
        time.sleep(0.2)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=effects_port, default=7871)
    parser.add_argument("--start", type=float, default=45.0,
                        help="Shared start offset on the server clock; clients must be connected by then")
    parser.add_argument("--server-start-timeout", type=float, default=90.0)
    parser.add_argument("--timeout", type=float, default=300.0)
    args = parser.parse_args(argv)
    for name in ("timeout", "server_start_timeout"):
        value = getattr(args, name)
        if not math.isfinite(value) or value <= 20:
            raise ValueError(f"--{name.replace('_', '-')} must be finite and greater than 20 seconds")
    slot_descriptor = require_engine_slot()
    root = ue.ROOT
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(root)
    ue.map_file(root)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/ShotEffects" / run_id
    output.mkdir(parents=True)
    metadata = evidence_metadata(root)
    processes, streams, commands = [], [], {}
    reports = {role: None for role in ROLES}
    captures, checks, problem = {}, [], None
    server_listen_seconds = None
    termination_signal = None

    def interrupted(signum, _frame):
        nonlocal termination_signal
        if termination_signal is None:
            termination_signal = signum
            raise InterruptedError(f"Shot-effects test interrupted by signal {signum}")

    handlers = {sig: signal.signal(sig, interrupted) for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}

    def launch(role):
        command = launch_command(executable, project, output, role, args.port, args.start)
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes.append(subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                          start_new_session=True, pass_fds=(slot_descriptor,)))

    try:
        began = time.monotonic()
        launch("server")
        server_listen_seconds = wait_for_listen(output / "server.log", processes[0], args.port, args.server_start_timeout)
        launch("observer")
        launch("shooter")
        print(f"Rendered shot-effects test running; evidence: {output}", flush=True)
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
                raise RuntimeError("A shot-effects process exited before all three roles completed")
            time.sleep(0.2)
        else:
            raise RuntimeError("Timed out before all three shot-effects roles completed")
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Shot-effects test interrupted"
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
              "conditions": "Local editor dedicated server (-server -nullrhi) and two rendered 1000x700 "
                            "-game clients, no audio, loopback, no artificial latency/loss",
              "server_listen_seconds": server_listen_seconds, "start_offset_seconds": args.start,
              "commands": commands, "checks": checks, "screenshots": captures, "reports": reports}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for check in checks:
        print(("PASS " if check["passed"] else "FAIL ") + check["check"] + ": " + json.dumps(check["detail"]))
    if problem:
        print(problem, file=sys.stderr)
    print(f"Result: {'PASS' if passed else 'FAIL'} ({sum(c['passed'] for c in checks)}/{len(checks)}); "
          f"{output / 'result.json'}")
    return 128 + termination_signal if termination_signal else (0 if passed else 1)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ue.SetupError, RuntimeError, OSError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
