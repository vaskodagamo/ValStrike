#!/usr/bin/env python3
"""Verify the round economy.

Default: compile and run the standalone economy rules tests (no Unreal), keeping
commands, source hashes and output under Saved/Tests/Economy/. Add --sanitize for
address/undefined-behavior sanitizers.

--network: run a Development editor server and two clients on Site01 in match mode
(build the Editor target first and wrap the whole runner with with_ue_slot.py).
--rendered additionally renders the first client at 960x540 and saves screenshots.
That is loopback development evidence, not packaged or cross-machine acceptance.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import shlex
import signal
import socket
import subprocess
import sys
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
RULE_SOURCES = [
    ROOT / "Source/ValStrike/Economy/VSEconomyRules.cpp",
    ROOT / "Source/ValStrike/Economy/VSEconomyRules.h",
    ROOT / "Tests/Economy/VSEconomyRulesTests.cpp",
]
PORTS = range(7911, 7915)
MAP = "/Game/Maps/Site01/VS_Site01"
MODE = "/Script/ValStrike.VSMatchGameMode"
ROLES = ("server", "alpha", "bravo")
JOIN_MARKER = "VSECON join player=1 "
SERVER_CHECKS = (
    "economy_active", "starting_credits", "rewards_match_ledger", "purchase_deltas_match_ledger",
    "unaffordable_rejected", "starter_owned_rejected", "unknown_item_rejected", "replay_rejected",
    "out_of_phase_rejected", "dead_rejected", "spam_bounded", "refund_same_phase", "death_loses_purchase",
    "survivor_keeps_purchase", "plant_reward", "kill_reward", "loss_streak", "credit_cap",
    "side_swap_reset", "rematch_reset", "duplicate_round_events_ignored", "grant_notifications",
)
CLIENT_CHECKS = ("single_private_wallet", "menu_opens_in_buy_phase", "menu_closes_after_buy_phase",
                 "escape_menu_keeps_focus", "rematch_seen")
RENDERED_CHECKS = ("screenshots_written", "keyboard_purchase")


# ----------------------------------------------------------------------------- standalone rules

def run_rules(args):
    compiler = [args.compiler] if args.compiler else (["xcrun", "clang++"] if sys.platform == "darwin" else ["c++"])
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/Economy" / run_id
    output.mkdir(parents=True)
    binary = output / "economy_rules_tests"
    flags = ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Werror", "-fno-exceptions", "-fno-rtti", "-O1", "-g"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    command = compiler + flags + ["-I", str(ROOT / "Source/ValStrike"), str(RULE_SOURCES[0]), str(RULE_SOURCES[2]),
                                  "-o", str(binary)]
    result = {"passed": False, "sanitized": args.sanitize, "commands": [], "source_sha256": {}}
    env = os.environ.copy()
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    env["ASAN_OPTIONS"] = "halt_on_error=1"

    def run(label, argv, timeout):
        result["commands"].append(argv)
        print(shlex.join(argv), flush=True)
        completed = subprocess.run(argv, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=timeout, check=False)
        (output / f"{label}.log").write_text(completed.stdout)
        print(completed.stdout, end="", flush=True)
        result[f"{label}_exit"] = completed.returncode
        return completed.returncode

    try:
        result["source_sha256"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                   for path in RULE_SOURCES + [Path(__file__).resolve()]}
        if run("compiler", compiler + ["--version"], 15) != 0:
            return 1
        if run("compile", command, 120) != 0:
            return 1
        if run("tests", [str(binary)], 30) != 0:
            return 1
        result["passed"] = True
        return 0
    except (OSError, subprocess.TimeoutExpired) as error:
        result["error"] = str(error)
        print(f"Economy rules verification failed: {error}", file=sys.stderr)
        return 1
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"Evidence: {output / 'result.json'}", flush=True)


# ----------------------------------------------------------------------------- networked smoke

def economy_port(value):
    try:
        port = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("economy ports must be integers from 7911 through 7914") from error
    if port not in PORTS:
        raise argparse.ArgumentTypeError("economy ports are reserved to 7911 through 7914")
    return port


def timeout_seconds(value):
    try:
        timeout = float(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("timeout must be a finite positive number") from error
    if not math.isfinite(timeout) or timeout <= 0:
        raise argparse.ArgumentTypeError("timeout must be a finite positive number")
    return timeout


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sanitize", action="store_true", help="rules only: address/undefined-behavior sanitizers")
    parser.add_argument("--compiler", help="rules only: compiler; defaults to xcrun clang++ on macOS, c++ elsewhere")
    parser.add_argument("--network", action="store_true", help="run the server + two-client Unreal smoke test")
    parser.add_argument("--engine", help="network only: Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=economy_port, default=7911)
    parser.add_argument("--timeout", type=timeout_seconds, default=480.0,
                        help="whole-run seconds; the scenario has seven 30 s preparation phases")
    parser.add_argument("--startup-timeout", type=timeout_seconds, default=90.0,
                        help="seconds for the server to listen; cold first launches can exceed 40 s")
    parser.add_argument("--rendered", action="store_true", help="network only: render alpha at 960x540 and capture screenshots")
    return parser.parse_args(argv)


def required_checks(rendered):
    client = CLIENT_CHECKS + (RENDERED_CHECKS if rendered else ())
    return {"server": SERVER_CHECKS, "alpha": client, "bravo": CLIENT_CHECKS}


def reject_nonfinite(value):
    raise ValueError(f"Non-finite JSON number: {value}")


def finite_float(value):
    result = float(value)
    if not math.isfinite(result):
        reject_nonfinite(value)
    return result


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
        return json.loads(path.read_text(encoding="utf-8"), parse_constant=reject_nonfinite,
                          parse_float=finite_float, object_pairs_hook=unique_object)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise RuntimeError(f"Malformed economy report {path.name}: {error}") from error


def evaluate(reports, required):
    if not isinstance(reports, dict) or set(reports) != set(ROLES):
        raise RuntimeError("Evidence must contain exactly server, alpha and bravo roles")
    checks = []
    for role in ROLES:
        report = reports[role]
        if not isinstance(report, dict):
            raise RuntimeError(f"Missing or invalid {role} report")
        if report.get("schema_version") != 1 or type(report.get("schema_version")) is not int or \
                report.get("role") != role or report.get("completed") is not True:
            raise RuntimeError(f"Invalid identity, schema or completion in {role} report")
        if "error" not in report or (report["error"] is not None and not isinstance(report["error"], str)):
            raise RuntimeError(f"Invalid or missing error field in {role} report")
        if report["error"]:
            raise RuntimeError(f"{role} harness failed: {report['error']}")
        items = report.get("checks")
        if not isinstance(items, list) or not items:
            raise RuntimeError(f"No economy checks in {role} report")
        names = set()
        for item in items:
            name = item.get("name") if isinstance(item, dict) else None
            if (not isinstance(name, str) or not name or name != name.strip() or name in names
                    or type(item.get("passed")) is not bool or "detail" not in item):
                raise RuntimeError(f"Invalid or duplicate economy check in {role} report: {name!r}")
            names.add(name)
            checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
        missing = set(required[role]) - names
        if missing:
            raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    checks += cross_check_replication(reports)
    return checks


def cross_check_replication(reports):
    """Each client's replicated checkpoints must equal the server's authoritative ones."""
    server = reports["server"].get("ledger")
    if not isinstance(server, dict) or not server:
        raise RuntimeError("Server report lacks its authoritative ledger")
    checks = []
    seen_players = set()
    for role in ("alpha", "bravo"):
        client = reports[role]
        player = client.get("player_id")
        observed = client.get("checkpoints")
        if not isinstance(player, int) or isinstance(player, bool) or not isinstance(observed, dict):
            raise RuntimeError(f"{role} report lacks its player id or checkpoints")
        seen_players.add(player)
        expected = server.get(str(player))
        if not isinstance(expected, dict):
            raise RuntimeError(f"Server ledger lacks player {player} reported by {role}")
        mismatches = []
        compared = 0
        for key, value in expected.items():
            if key not in observed:
                mismatches.append(f"{key}: missing on client")
            elif observed[key] != value:
                mismatches.append(f"{key}: client {observed[key]} != server {value}")
            compared += 1
        passed = not mismatches and compared >= 13
        checks.append({"check": f"{role}: replicated_checkpoints_match_server", "passed": passed,
                       "detail": {"player": player, "compared": compared, "mismatches": mismatches[:8]}})
    if len(seen_players) != 2:
        raise RuntimeError("Both clients reported the same player id")
    return checks


def launch_command(executable, project, output, role, port, rendered=False):
    if role not in ROLES:
        raise ValueError(f"Unknown economy role: {role}")
    port = economy_port(port)
    destination = f"{MAP}?game={MODE}" if role == "server" else f"127.0.0.1:{port}?Name=VSEconomy{role.title()}"
    command = [str(executable), str(project), destination,
               "-server" if role == "server" else "-game", "-nosound", "-unattended",
               f"-VSEconomySmoke={role}", f"-VSEconomyOutput={output / (role + '.json')}",
               f"-abslog={output / (role + '.log')}", "-ExecCmds=t.MaxFPS 60"]
    if role == "alpha" and rendered:
        command += ["-windowed", "-ResX=960", "-ResY=540", f"-VSEconomyScreenshots={output / 'screenshots'}"]
    else:
        command.append("-nullrhi")
    if role == "server":
        command += [f"-port={port}", "-multihome=127.0.0.1"]
    return command


def check_processes_running(processes):
    for role, process in processes.items():
        code = process.poll()
        if code is not None:
            raise RuntimeError(f"{role} process exited unexpectedly with code {code} before runner cleanup")


def stop(processes):
    """Close only owned process groups, within the engine slot's grace period."""
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
    for process in processes:
        try:
            process.wait(timeout=2.0)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"Owned process PID {process.pid} did not stop after cleanup") from error


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def evidence_metadata(root):
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True, check=False, timeout=15)
    status = subprocess.run(["git", "--no-optional-locks", "status", "--porcelain"], cwd=root,
                            capture_output=True, text=True, check=False, timeout=15)
    paths = [path for folder in ("Source", "Config", "Tests/Economy")
             for path in (root / folder).rglob("*") if path.is_file() and "__pycache__" not in path.parts]
    paths += [root / "ValStrike.uproject", root / "Scripts/test_economy.py"]
    module = root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib"
    map_file = root / "Content/Maps/Site01/VS_Site01.umap"
    return {
        "revision": revision.stdout.strip() if revision.returncode == 0 else None,
        "dirty_checkout": bool(status.stdout.strip()) if status.returncode == 0 else None,
        "source_sha256": {str(path.relative_to(root)): sha256(path) for path in sorted(paths) if path.is_file()},
        "editor_module_sha256": sha256(module) if module.is_file() else None,
        "map_sha256": sha256(map_file) if map_file.is_file() else None,
    }


def run_network(args):
    sys.path.insert(0, str(ROOT / "Scripts"))
    import ue  # Read-only use of the shared helper for engine/project discovery.

    if ue.host_platform() != "Mac":
        raise ue.SetupError("The economy network runner currently verifies the pinned Mac Editor build only")
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, "Mac")
    project = ue.project_file(ROOT)
    map_file = ue.require_file(ROOT / "Content/Maps/Site01/VS_Site01.umap", "Restore the Site01 map with Git LFS.")
    with map_file.open("rb") as stream:
        if stream.read(64).startswith(b"version https://git-lfs.github.com/spec/"):
            raise ue.SetupError("Site01 is a Git LFS pointer; retrieve its real map before runtime testing")
    ue.require_file(ROOT / "Binaries/Mac/libUnrealEditor-ValStrike.dylib", "Build this checkout's Mac Editor target first.")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/Economy" / ("network_" + run_id)
    output.mkdir(parents=True)
    metadata = evidence_metadata(ROOT)
    build = json.loads((engine / "Build/Build.version").read_text())
    required = required_checks(args.rendered)
    processes, streams, commands = {}, [], {}
    interrupted = None

    def request_stop(signum, _frame):
        nonlocal interrupted
        interrupted = interrupted or f"Economy smoke interrupted by signal {signum}"

    watched = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP) if os.name == "posix" else (signal.SIGINT, signal.SIGTERM)
    previous_handlers = {sig: signal.signal(sig, request_stop) for sig in watched}

    def healthy():
        if interrupted:
            raise RuntimeError(interrupted)
        check_processes_running(processes)

    def launch(role):
        command = launch_command(executable, project, output, role, args.port, args.rendered)
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes[role] = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                           start_new_session=os.name == "posix")

    def wait_for(marker, limit, what):
        deadline = time.monotonic() + limit
        log = output / "server.log"
        while True:
            healthy()
            if marker in (log.read_text(errors="replace") if log.exists() else ""):
                return
            if time.monotonic() >= deadline:
                raise RuntimeError(f"{what} before timeout; inspect server.log")
            time.sleep(0.2)

    problem = None
    began = time.monotonic()
    reports = {role: None for role in ROLES}
    try:
        launch("server")
        wait_for(f"listening on port {args.port}", min(args.startup_timeout, args.timeout), "Server did not listen")
        # Join order fixes teams: alpha joins first and plays team One.
        launch("alpha")
        wait_for(JOIN_MARKER, 90.0, "Alpha did not join the economy")
        launch("bravo")
        print(f"Economy smoke running; evidence: {output}", flush=True)
        while True:
            healthy()
            if time.monotonic() - began >= args.timeout:
                raise RuntimeError("Timed out before all three roles completed")
            reports = {role: load_report(output / f"{role}.json") for role in ROLES}
            if all(report is not None for report in reports.values()):
                break
            time.sleep(0.25)
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Economy smoke interrupted"
    finally:
        before_cleanup = {role: process.poll() for role, process in processes.items()}
        if any(code is not None for code in before_cleanup.values()):
            problem = problem or f"A test process exited before cleanup: {before_cleanup}"
        try:
            stop(processes.values())
        except (OSError, RuntimeError) as error:
            problem = problem or f"Process cleanup failed: {error}"
        cleanup_exit = {role: process.poll() for role, process in processes.items()}
        for stream in streams:
            stream.close()
        for sig, handler in previous_handlers.items():
            signal.signal(sig, handler)

    checks = []
    after = None
    try:
        reports = {role: load_report(output / f"{role}.json") for role in ROLES}
        checks = evaluate(reports, required)
        after = evidence_metadata(ROOT)
        if any(metadata[key] != after[key] for key in ("source_sha256", "editor_module_sha256", "map_sha256")):
            problem = problem or "Source, Editor module or Site01 map changed during the runtime test"
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        problem = problem or f"Invalid runtime evidence: {error}"
    screenshots = sorted(str(path.relative_to(output)) for path in (output / "screenshots").glob("*.png"))
    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    result = {
        **metadata, "passed": passed, "error": problem, "engine": str(engine), "engine_build": build,
        "metadata_after": after, "rendered_alpha": args.rendered, "screenshots": screenshots,
        "conditions": "Local Development editor server and two clients on Site01 in match mode; loopback, no audio, "
                      "no artificial latency/loss; " + ("alpha rendered at 960x540" if args.rendered else "null RHI for all roles"),
        "runner_command": [sys.executable, str(Path(__file__).resolve()), *(sys.argv[1:])],
        "commands": commands, "exit_before_cleanup": before_cleanup, "exit_after_cleanup": cleanup_exit,
        "elapsed_seconds": time.monotonic() - began, "checks": checks, "reports": reports,
    }
    (output / "result.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    for check in checks:
        print(("PASS " if check["passed"] else "FAIL ") + check["check"] + ": " + json.dumps(check["detail"]))
    if problem:
        print(problem, file=sys.stderr)
    print(f"Result: {'PASS' if passed else 'FAIL'}; {output / 'result.json'}")
    return 0 if passed else 1


def main(argv=None):
    args = arguments(argv)
    return run_network(args) if args.network else run_rules(args)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
    except Exception as error:  # ue.SetupError is defined in the lazily imported helper.
        if type(error).__name__ != "SetupError":
            raise
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
