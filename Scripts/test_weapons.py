#!/usr/bin/env python3
"""Run the server/driver/observer weapon switching smoke test.

Required launch (wrap this whole runner once, not its children):
  python3 Scripts/with_ue_slot.py -- python3 Scripts/test_weapons.py --engine PATH --scenario practice
  python3 Scripts/with_ue_slot.py -- python3 Scripts/test_weapons.py --engine PATH --scenario visual
  python3 Scripts/with_ue_slot.py -- python3 Scripts/test_weapons.py --engine PATH --scenario match

practice: graybox, practice game mode, null-RHI clients. Dense validation timeline:
equip validation (dead, gated, duplicate, spam, invalid, mid-reload), per-weapon
ammo/damage, semi-automatic pistol, equip time blocking fire, HUD and models;
then the knife: equip 3 and Q across three slots, slash/stab hit, out of range,
wall, raw rejections, and cancellation on switch, phase, reset and death.
visual: graybox, rendered clients. Spaced first-person/observer captures of the
rifle, raising pistol, pistol, pistol flash, pistol reload and rifle again.
match: Site01, match game mode with one player per team, rendered clients.
Preparation gate, pistol/rifle combat and a real Match/ round reset.

A wrapper exit of 75 means the shared engine slot is busy; retry later. This is
local Development Editor evidence, not packaged, dedicated-server or LAN acceptance.
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


ROLES = ("server", "driver", "observer")
SCENARIOS = ("practice", "visual", "match")
# Rendered clients are required for captures and forbidden for the dense timeline,
# whose fixed schedule a rendering stall (readback, first-draw compiles) would break.
RENDERED = {"practice": False, "visual": True, "match": True}
MATCH_MODE = "/Script/ValStrike.VSMatchGameMode"
PORT_RANGE = (7891, 7894)  # Weapons stream loopback ports.

# Fixed names form the contract with the native harness in Combat/VSWeaponSmoke.cpp.
PRACTICE_COMMON = (
    "initial_loadout", "hud_rifle", "equip_started", "hud_pistol_equipping", "equip_blocks_fire",
    "pistol_semi_auto", "pistol_damage", "pistol_reload_started", "switch_cancels_reload", "per_weapon_ammo",
    "hud_rifle_after_switch", "equip_last", "raw_equip_validation", "gate_blocks_equip", "dead_blocks_equip",
    "fresh_pawn_loadout",
)
PRACTICE_CLIENT = PRACTICE_COMMON + (
    "rifle_models", "pistol_assets", "no_collision", "pistol_raising", "pistol_models", "pistol_shot_pulse",
    "rifle_models_after_switch", "no_replay_on_reequip", "dead_models_hidden",
)
VISUAL_CLIENT = (
    "visual_rifle_models", "visual_pistol_raising", "visual_pistol_models", "visual_pistol_flash",
    "visual_pistol_reload", "visual_rifle_after_switch",
)
MATCH_COMMON = (
    "preparation_blocks_equip", "active_rifle", "match_equip_pistol", "match_pistol_damage",
    "match_pistol_reload", "match_rifle_elimination", "round_reset_restores_both",
)
MATCH_CLIENT = MATCH_COMMON + ("match_rifle_models", "match_pistol_models", "round_reset_models")
# KNIFE-01, appended to practice after the firearm timeline; firearm names are unchanged.
KNIFE_COMMON = (
    "knife_equipped", "knife_slash_hit_once", "knife_out_of_range", "knife_wall", "knife_cancel_switch",
    "knife_equip_last", "knife_cancel_phase", "knife_cancel_reset", "knife_stab_hit", "knife_cancel_death",
)
KNIFE_SERVER = KNIFE_COMMON + (
    "knife_reject_equip", "knife_reject_cooldown", "knife_reject_invalid", "knife_reject_replay",
    "knife_reject_holstered",
)
KNIFE_CLIENT = KNIFE_COMMON + ("knife_hides_firearms", "knife_presentation_cues")
REQUIRED_CHECKS = {
    "practice": {
        "server": PRACTICE_COMMON + ("server_no_visuals", "reject_equip_time", "reject_holstered", "reject_duplicate",
                                     "reject_spam", "reject_invalid", "reject_phase", "reject_dead") + KNIFE_SERVER,
        "driver": PRACTICE_CLIENT + KNIFE_CLIENT,
        "observer": PRACTICE_CLIENT + KNIFE_CLIENT,
    },
    "visual": {"server": ("visual_server_sequence",), "driver": VISUAL_CLIENT, "observer": VISUAL_CLIENT},
    "match": {
        "server": MATCH_COMMON + ("preparation_gates_closed", "reject_phase"),
        "driver": MATCH_CLIENT,
        "observer": MATCH_CLIENT,
    },
}
CLIENT_SCREENSHOTS = {
    "practice": (),
    "visual": ("rifle", "pistol_equipping", "pistol", "pistol_muzzle", "pistol_reload", "rifle_after_switch"),
    "match": ("match_rifle", "match_pistol", "match_elimination", "match_reset"),
}


def weapons_port(value):
    parsed = ue.port(value)
    if not PORT_RANGE[0] <= parsed <= PORT_RANGE[1]:
        raise argparse.ArgumentTypeError("Weapon test ports are reserved to 7891..7894")
    return parsed


def required_screenshots(scenario):
    shots = CLIENT_SCREENSHOTS[scenario]
    return {"server": (), "driver": shots, "observer": shots}


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
                       "Scripts/test_weapons.py --engine PATH; do not acquire nested slots")


def validate_report(role, report, scenario, required_checks=None):
    required = REQUIRED_CHECKS[scenario] if required_checks is None else required_checks
    if (role not in ROLES or not isinstance(report, dict)
            or type(report.get("schema_version")) is not int or report["schema_version"] != 1
            or report.get("role") != role or report.get("scenario") != scenario
            or report.get("completed") is not True):
        raise RuntimeError(f"Missing or invalid identity/scenario/schema/completion in {role} report")
    error = report.get("error")
    if error is not None and not isinstance(error, str):
        raise RuntimeError(f"Invalid error field in {role} report")
    if error:
        raise RuntimeError(f"{role} weapon harness failed: {error}")
    items = report.get("checks")
    if not isinstance(items, list) or not items:
        raise RuntimeError(f"No weapon checks in {role} report")
    names, checks = set(), []
    for item in items:
        if not isinstance(item, dict):
            raise RuntimeError(f"Invalid weapon check in {role} report")
        name = item.get("name")
        if (not isinstance(name, str) or not name.strip() or name in names
                or type(item.get("passed")) is not bool or "detail" not in item):
            raise RuntimeError(f"Invalid or duplicate weapon check in {role} report: {name!r}")
        names.add(name)
        checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
    missing = set(required[role]) - names
    if missing:
        raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    return checks


def evaluate(reports, scenario, required_checks=None):
    if set(reports) != set(ROLES):
        raise RuntimeError("Evidence requires exactly server, driver and observer roles")
    return [check for role in ROLES for check in validate_report(role, reports[role], scenario, required_checks)]


def collect_screenshots(output, required):
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


def launch_command(executable, project, output, role, port, scenario):
    if role not in ROLES:
        raise ValueError(f"Unknown weapon role: {role}")
    if scenario not in SCENARIOS:
        raise ValueError(f"Unknown weapon scenario: {scenario}")
    rendered = RENDERED[scenario]
    port = weapons_port(port)
    if role == "server":
        destination = (f"{ue.selected_map('site01')}?game={MATCH_MODE}" if scenario == "match"
                       else ue.selected_map("graybox"))
    else:
        destination = f"127.0.0.1:{port}?Name=VSWeapon{role.title()}"
    command = [str(executable), str(project), destination,
               "-server" if role == "server" else "-game", "-nosound", "-unattended",
               f"-VSWeaponSmoke={role}", "-VSWeaponSmokeStart=30", f"-VSWeaponScenario={scenario}",
               f"-VSWeaponOutput={output / (role + '.json')}",
               f"-abslog={output / (role + '.log')}", "-ExecCmds=t.MaxFPS 60"]
    if role == "server":
        command += ["-nullrhi", f"-port={port}", "-multihome=127.0.0.1"]
        if scenario == "match":
            command.append("-VSMatchMinimum=1")
    elif rendered:
        command += ["-windowed", "-ResX=1000", "-ResY=700", "-VSWeaponCapture"]
    else:
        command.append("-nullrhi")
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
    paths = [root / "Scripts/test_weapons.py", root / "Scripts/with_ue_slot.py"]
    paths += [path for path in (root / "Tests/Weapons").rglob("*")
              if path.is_file() and "__pycache__" not in path.parts]
    metadata["source_sha256"].update({str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                                       for path in paths if path.is_file()})
    metadata["asset_sha256"] = {
        str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for folder in ("Content/Art/Weapons/Rifle01", "Content/Art/Weapons/Pistol01", "Content/Presentation/Rifle01")
        for path in sorted((root / folder).rglob("*")) if path.is_file()
    }
    return metadata


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--scenario", choices=SCENARIOS, default="practice")
    parser.add_argument("--port", type=weapons_port, default=7893)
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--server-start-timeout", type=float, default=60,
                        help="seconds for the server to listen; a cold first launch can be slow")
    args = parser.parse_args(argv)
    for name in ("timeout", "server_start_timeout"):
        value = getattr(args, name)
        if not math.isfinite(value) or value <= 10:
            raise ValueError(f"--{name.replace('_', '-')} must be finite and greater than 10 seconds")
    slot_descriptor = require_engine_slot()
    root = ue.ROOT
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(root)
    ue.map_file(root, "site01" if args.scenario == "match" else "graybox")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    rendered = RENDERED[args.scenario]
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + args.scenario + "_" + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/Weapons" / run_id
    output.mkdir(parents=True)
    metadata = evidence_metadata(root)
    screenshots = required_screenshots(args.scenario)
    processes, streams, commands = [], [], {}
    reports = {role: None for role in ROLES}
    captures, checks, problem = {}, [], None
    termination_signal = None

    def interrupted(signum, _frame):
        nonlocal termination_signal
        if termination_signal is None:
            termination_signal = signum
            raise InterruptedError(f"Weapon test interrupted by signal {signum}")

    handlers = {sig: signal.signal(sig, interrupted) for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}

    def launch(role):
        command = launch_command(executable, project, output, role, args.port, args.scenario)
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
            if processes[0].poll() is not None or time.monotonic() - began > args.server_start_timeout:
                raise RuntimeError("Weapon test server did not listen; inspect server.log")
            time.sleep(0.2)
        launch("observer")
        launch("driver")
        print(f"Weapon {args.scenario} test running; evidence: {output}", flush=True)
        while time.monotonic() - began < args.timeout:
            reports = {role: combat.load_report(output / f"{role}.json") for role in ROLES}
            for role, report in reports.items():
                if report is not None:
                    validate_report(role, report, args.scenario)
            if all(report is not None for report in reports.values()):
                checks = evaluate(reports, args.scenario)
                time.sleep(1.0)  # Let the last requested screenshots finish writing.
                captures = collect_screenshots(output, screenshots)
                break
            if any(process.poll() is not None for process in processes):
                raise RuntimeError("A weapon test process exited before all three roles completed")
            time.sleep(0.2)
        else:
            raise RuntimeError("Timed out before all three weapon roles completed")
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Weapon test interrupted"
    finally:
        try:
            stop_processes(processes)
        finally:
            for stream in streams:
                stream.close()
            for sig, handler in handlers.items():
                signal.signal(sig, handler)

    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    clients = "two rendered 1000x700 clients" if rendered else "two null-RHI clients"
    result = {**metadata, "passed": passed, "error": problem, "scenario": args.scenario, "rendered": rendered,
              "engine": str(engine), "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": f"Local editor server (null RHI) and {clients}, no audio, loopback, no artificial latency/loss",
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
