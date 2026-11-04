#!/usr/bin/env python3
"""Run a Development editor server, two persistent clients and a late-join probe.

Build the Editor target first, then wrap this entire runner with with_ue_slot.py.
The native harness must atomically publish each role.json and remain alive until
runner cleanup. This is loopback development evidence, not packaged acceptance.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import time
import uuid

import ue


ROLES = ("server", "driver", "observer")
LAUNCH_ROLES = ROLES + ("late",)
LATE_JOIN_MARKER = "VSMATCH_REQUEST_LATE_JOIN"
LATE_LEAVE_MARKER = "VSMATCH_REQUEST_LATE_LEAVE"
MAP = "/Game/Maps/Site01/VS_Site01"
MODE = "/Script/ValStrike.VSMatchGameMode"
CLIENT_CHECKS = (
    "network_role", "replicated_match", "replicated_preparation", "replicated_objective",
    "replicated_result", "fresh_pawn", "replicated_match_complete", "replicated_rematch",
)
REQUIRED_CHECKS = {
    "server": (
        "map_contract", "preparation_gates", "role_spawns", "plant_interrupted", "plant_completed",
        "postplant_attacker_death", "defuse_win", "detonation_win", "timeout_win", "elimination_win",
        "fresh_pawn_reset", "late_join_spectator", "disconnect_drop", "six_round_match", "role_swap",
        "rematch", "twelve_rounds", "deadline_gate", "resolved_gate", "plant_deadline_tie",
        "defuse_deadline_tie", "menu_cancels_interaction", "simultaneous_elimination", "invalid_objective_request",
        "preparation_input_lock", "resolved_input_lock", "reload_deadline_gate",
    ),
    "driver": CLIENT_CHECKS,
    "observer": CLIENT_CHECKS,
}
SPECTATE_ROLES = ("driver", "observer")
SPECTATE_MARKER = "VSSPECTATE "
SPECTATE_EVENTS = ("start", "cycle", "stop")
SPECTATE_STAGE_MARKER = "VSSPECTATE_STAGE "
SPECTATE_JOIN_MARKER = "VSMATCH_REQUEST_SPECTATE_JOIN"
MATE_ROLE = "mate"  # GPF-03 auxiliary teammate; like late, it writes no report.
IDENTITY = re.compile(r"\bteam=(\d+) id=(\d+)\b")


def match_port(value):
    try:
        port = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("match ports must be integers from 7831 through 7834") from error
    if not 7831 <= port <= 7834:
        raise argparse.ArgumentTypeError("match ports are reserved to 7831 through 7834")
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=match_port, default=7831)
    parser.add_argument("--timeout", type=timeout_seconds, default=240.0)
    parser.add_argument("--rendered", action="store_true", help="render client windows; server stays headless")
    return parser.parse_args(argv)


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
    """Absence is pending; existing malformed/partially written reports fail."""
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"), parse_constant=reject_nonfinite,
                          parse_float=finite_float, object_pairs_hook=unique_object)
    except (UnicodeError, ValueError, RecursionError) as error:
        raise RuntimeError(f"Malformed match report {path.name}: {error}") from error


def evaluate(reports, required_checks=None):
    required = REQUIRED_CHECKS if required_checks is None else required_checks
    if not isinstance(reports, dict) or set(reports) != set(ROLES) or set(required) != set(ROLES):
        raise RuntimeError("Evidence must contain exactly server, driver and observer roles")
    checks = []
    for role in ROLES:
        report = reports[role]
        if not isinstance(report, dict):
            raise RuntimeError(f"Missing or invalid {role} report")
        if (type(report.get("schema_version")) is not int or report["schema_version"] != 1
                or report.get("role") != role or report.get("completed") is not True):
            raise RuntimeError(f"Invalid identity, schema or completion in {role} report")
        if "error" not in report or (report["error"] is not None and not isinstance(report["error"], str)):
            raise RuntimeError(f"Invalid or missing error field in {role} report")
        if report["error"]:
            raise RuntimeError(f"{role} harness failed: {report['error']}")
        items = report.get("checks")
        if not isinstance(items, list) or not items:
            raise RuntimeError(f"No match checks in {role} report")
        names = set()
        for item in items:
            if not isinstance(item, dict):
                raise RuntimeError(f"Invalid match check in {role} report")
            name = item.get("name")
            if (not isinstance(name, str) or not name.strip() or name != name.strip() or name in names
                    or type(item.get("passed")) is not bool or "detail" not in item):
                raise RuntimeError(f"Invalid or duplicate match check in {role} report: {name!r}")
            try:
                json.dumps(item["detail"], allow_nan=False)
            except (TypeError, ValueError, RecursionError) as error:
                raise RuntimeError(f"Invalid measured detail in {role}: {name}") from error
            names.add(name)
            checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
        missing = set(required[role]) - names
        if missing:
            raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    return checks


def parse_spectate(text):
    """Return the VSSPECTATE events of one client log; a malformed line is a defect."""
    events = []
    for line in text.splitlines():
        index = line.find(SPECTATE_MARKER)
        if index < 0:
            continue
        payload = line[index + len(SPECTATE_MARKER):].strip()
        try:
            event = json.loads(payload, parse_constant=reject_nonfinite, object_pairs_hook=unique_object)
        except (ValueError, RecursionError) as error:
            raise RuntimeError(f"Malformed VSSPECTATE line: {payload!r}") from error
        if (not isinstance(event, dict) or set(event) != {"event", "viewer", "target", "round"}
                or event["event"] not in SPECTATE_EVENTS
                or any(type(event[key]) is not int or event[key] < 0 for key in ("viewer", "target", "round"))
                or event["viewer"] == 0 or (event["target"] == 0) != (event["event"] == "stop")):
            raise RuntimeError(f"Invalid VSSPECTATE event: {payload!r}")
        events.append(event)
    return events


def client_identity(report):
    """(team, connection id) from the client's replicated_match detail, or None."""
    for item in report.get("checks", []) if isinstance(report, dict) else []:
        if isinstance(item, dict) and item.get("name") == "replicated_match" and isinstance(item.get("detail"), str):
            found = IDENTITY.search(item["detail"])
            if found and int(found.group(1)) and int(found.group(2)):
                return int(found.group(1)), int(found.group(2))
    return None


def parse_spectate_stage(text):
    """The server's VSSPECTATE_STAGE line (GPF-03), or None when the stage was not reached."""
    found = []
    for line in (text or "").splitlines():
        index = line.find(SPECTATE_STAGE_MARKER)
        if index < 0:
            continue
        payload = line[index + len(SPECTATE_STAGE_MARKER):].strip()
        try:
            stage = json.loads(payload, parse_constant=reject_nonfinite, object_pairs_hook=unique_object)
        except (ValueError, RecursionError) as error:
            raise RuntimeError(f"Malformed VSSPECTATE_STAGE line: {payload!r}") from error
        if (not isinstance(stage, dict) or set(stage) != {"viewer", "teammate", "enemy", "round"}
                or any(type(stage[key]) is not int or stage[key] <= 0 for key in stage)
                or len({stage["viewer"], stage["teammate"], stage["enemy"]}) != 3):
            raise RuntimeError(f"Invalid VSSPECTATE_STAGE line: {payload!r}")
        found.append(stage)
    if len(found) > 1:
        raise RuntimeError("More than one VSSPECTATE_STAGE line")
    return found[0] if found else None


def evaluate_spectate(reports, logs, server_log=None):
    """GPT-07 death-spectate checks from client logs: pass, fail or not_reached (never a silent pass).

    `logs` maps a client role to its log text, or None when the log is absent. `server_log` may carry
    the GPF-03 stage line naming the dead viewer, its living teammate and the enemy; with it, the
    stage must produce a start on that teammate and a stop in a later round. The client line has no
    life state; in the stage the only dead players are the viewer and the enemy, both rejected targets.
    """
    identities = {role: client_identity(reports.get(role)) for role in SPECTATE_ROLES}
    stage = parse_spectate_stage(server_log)
    events = {}
    problems = []
    for role in SPECTATE_ROLES:
        if logs.get(role) is None:
            problems.append(f"{role} log absent")
            continue
        events[role] = parse_spectate(logs[role])
    teams = {role: identity[0] for role, identity in identities.items() if identity}
    seen = {role: len(items) for role, items in events.items()}

    def result(name, status, detail):
        return {"check": f"spectate: {name}", "status": status, "detail": detail}

    if stage is None and not any(event["event"] == "start" for items in events.values() for event in items):
        reason = ("the server did not reach the spectate stage and no client entered death spectate; "
                  "outside that stage driver and observer are on teams " + json.dumps(teams) + " and have no teammate")
        if problems:
            reason += "; " + ", ".join(problems)
        detail = {"reason": reason, "events": seen}
        return [result(name, "not_reached", detail)
                for name in ("teammate_start", "stop_on_round_reset", "no_enemy_or_dead_target")]

    invalid, sequence, unstopped, resets, same_round_stops, starts = [], [], [], 0, 0, []
    for role, items in events.items():
        identity = identities[role]
        if items and identity is None:
            invalid.append(f"{role}: team identity unavailable")
            continue
        enemies = {other[1] for other_role, other in identities.items()
                   if other_role != role and other and other[0] != identity[0]}
        if stage and identity and stage["viewer"] == identity[1]:
            enemies.add(stage["enemy"])
        current, started_round = 0, None
        for event in items:
            if event["viewer"] != identity[1]:
                invalid.append(f"{role}: viewer {event['viewer']} is not {identity[1]}")
            if event["event"] != "stop":
                target = event["target"]
                if target == identity[1] or target in enemies:
                    invalid.append(f"{role}: {event['event']} target {target} is self or an enemy")
            if event["event"] == "start":
                if current:
                    sequence.append(f"{role}: start while watching {current}")
                current, started_round = event["target"], event["round"]
                starts.append({"role": role, "viewer": event["viewer"], "target": event["target"], "round": event["round"]})
            elif event["event"] == "cycle":
                if not current or event["target"] == current:
                    sequence.append(f"{role}: cycle without a new target")
                current = event["target"]
            else:
                if not current:
                    sequence.append(f"{role}: stop without a start")
                elif event["round"] != started_round:
                    resets += 1
                else:
                    same_round_stops += 1
                current, started_round = 0, None
        if current:
            unstopped.append(role)

    detail = {"stage": stage, "starts": starts, "events": seen, "teams": teams}
    if problems:
        detail["problems"] = problems
    start_invalid = [item for item in invalid if ": start target" in item or "identity" in item or "viewer" in item]
    if stage is not None:
        viewer_roles = [role for role, identity in identities.items() if identity and identity[1] == stage["viewer"]]
        expected = [item for item in starts if item["viewer"] == stage["viewer"]
                    and item["target"] == stage["teammate"] and item["round"] == stage["round"]]
        if not viewer_roles:
            start_invalid.append(f"stage viewer {stage['viewer']} is neither driver nor observer")
        elif not expected:
            start_invalid.append(f"no start from {stage['viewer']} on teammate {stage['teammate']} in round {stage['round']}")
    checks = [result("teammate_start", "fail" if start_invalid else "pass", {**detail, "invalid": start_invalid})]
    if sequence or unstopped:
        checks.append(result("stop_on_round_reset", "fail", {**detail, "sequence": sequence, "unstopped": unstopped}))
    elif resets:
        checks.append(result("stop_on_round_reset", "pass", {**detail, "reset_stops": resets, "same_round_stops": same_round_stops}))
    else:
        # The stage always ends with a round reset, so a missing reset stop there is a defect.
        checks.append(result("stop_on_round_reset", "fail" if stage else "not_reached",
                             {**detail, "reason": "no spectate ended at a round reset", "same_round_stops": same_round_stops}))
    targeted = any(event["event"] != "stop" for items in events.values() for event in items)
    checks.append(result("no_enemy_or_dead_target", "fail" if invalid else "pass" if targeted else "not_reached",
                         {**detail, "invalid": invalid,
                          "dead_target": "the stage's dead players are the viewer and the enemy, both rejected; "
                                         "elsewhere the client's CanWatch re-check and spectate_rules_test cover it"}))
    return checks


def launch_command(executable, project, output, role, port, rendered=False):
    if role not in LAUNCH_ROLES + (MATE_ROLE,):
        raise ValueError(f"Unknown match role: {role}")
    port = match_port(port)
    destination = f"{MAP}?game={MODE}" if role == "server" else f"127.0.0.1:{port}?Name=VSMatch{role.title()}"
    command = [str(executable), str(project), destination,
               "-server" if role == "server" else "-game", "-nosound", "-unattended",
               f"-VSMatchSmoke={role}", f"-VSMatchOutput={output / (role + '.json')}",
               f"-abslog={output / (role + '.log')}", "-ExecCmds=t.MaxFPS 60"]
    if role == "server" or not rendered:
        command.append("-nullrhi")
    else:
        command += ["-windowed", "-ResX=960", "-ResY=540"]
    if role == "server":
        command += [f"-port={port}", "-multihome=127.0.0.1"]
    return command


def check_processes_running(processes, late_teardown_requested=False):
    """Native harnesses do not self-quit; even a zero exit loses runtime evidence."""
    for role, process in processes.items():
        if role == "late" and late_teardown_requested:
            continue
        code = process.poll()
        if code is not None:
            raise RuntimeError(f"{role} process exited unexpectedly with code {code} before runner cleanup")


def stop(processes):
    """Close only owned process groups, with a shared grace bound below the slot's 10 s."""
    processes = list(processes)
    for process in processes:
        try:
            if os.name == "posix":
                # Also clean surviving descendants when their group leader exited.
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
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root,
                              capture_output=True, text=True, check=False, timeout=15)
    status = subprocess.run(["git", "--no-optional-locks", "status", "--porcelain"], cwd=root,
                            capture_output=True, text=True, check=False, timeout=15)
    paths = [path for folder in ("Source", "Config", "Tests/Match")
             for path in (root / folder).rglob("*") if path.is_file() and "__pycache__" not in path.parts]
    paths += [root / "ValStrike.uproject", root / "Scripts/ue.py", root / "Scripts/test_match.py",
              root / "Scripts/with_ue_slot.py"]
    module = root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib"
    map_file = root / "Content/Maps/Site01/VS_Site01.umap"
    return {
        "revision": revision.stdout.strip() if revision.returncode == 0 else None,
        "revision_error": revision.stderr.strip() if revision.returncode else None,
        "dirty_checkout": bool(status.stdout.strip()) if status.returncode == 0 else None,
        "git_status_error": status.stderr.strip() if status.returncode else None,
        "source_sha256": {str(path.relative_to(root)): sha256(path) for path in sorted(paths) if path.is_file()},
        "editor_module_sha256": sha256(module) if module.is_file() else None,
        "map_sha256": sha256(map_file) if map_file.is_file() else None,
    }


def main(argv=None):
    args = arguments(argv)
    root = ue.ROOT
    if ue.host_platform() != "Mac":
        raise ue.SetupError("The match smoke runner currently verifies the pinned Mac Editor build only")
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, "Mac")
    project = ue.project_file(root)
    map_file = ue.require_file(root / "Content/Maps/Site01/VS_Site01.umap", "Restore the Site01 map with Git LFS.")
    with map_file.open("rb") as stream:
        if stream.read(64).startswith(b"version https://git-lfs.github.com/spec/"):
            raise ue.SetupError("Site01 is a Git LFS pointer; retrieve its real map before runtime testing")
    ue.require_file(root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib", "Build this checkout's Mac Editor target first.")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/Match" / run_id
    output.mkdir(parents=True)
    metadata = evidence_metadata(root)
    build = json.loads((engine / "Build/Build.version").read_text())
    processes, streams, commands = {}, [], {}
    reports = {role: None for role in ROLES}
    before_cleanup, cleanup_exit = {}, {}
    interrupted = None
    late_teardown_requested = False
    late_teardown_complete = False
    late_exit_after_leave = None

    def request_stop(signum, _frame):
        nonlocal interrupted
        interrupted = interrupted or f"Match smoke interrupted by signal {signum}"

    watched = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP) if os.name == "posix" else (signal.SIGINT, signal.SIGTERM)
    previous_handlers = {sig: signal.signal(sig, request_stop) for sig in watched}

    def healthy():
        if interrupted:
            raise RuntimeError(interrupted)
        check_processes_running(processes, late_teardown_requested)

    def launch(role):
        command = launch_command(executable, project, output, role, args.port, args.rendered)
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes[role] = subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                           start_new_session=os.name == "posix")

    def update_late_client():
        nonlocal late_teardown_requested, late_teardown_complete, late_exit_after_leave
        log = server_log.read_text(errors="replace") if server_log.exists() else ""
        if LATE_JOIN_MARKER in log and "late" not in processes:
            launch("late")
            print("Match smoke: launched late-join client", flush=True)
        if SPECTATE_JOIN_MARKER in log and MATE_ROLE not in processes:
            launch(MATE_ROLE)
            print("Match smoke: launched spectate teammate client", flush=True)
        if LATE_LEAVE_MARKER in log and not late_teardown_requested:
            if "late" not in processes:
                raise RuntimeError("Server requested late-client leave before requesting its join")
            # A client that exited on its own before this request is still a failure.
            healthy()
            late_teardown_requested = True
            stop([processes["late"]])
            late_teardown_complete = True
            late_exit_after_leave = processes["late"].poll()
            print("Match smoke: stopped requested late-join client", flush=True)

    problem = None
    began = time.monotonic()
    try:
        launch("server")
        startup_deadline = began + min(40.0, args.timeout)
        server_log = output / "server.log"
        while True:
            healthy()
            log = server_log.read_text(errors="replace") if server_log.exists() else ""
            if f"listening on port {args.port}" in log:
                break
            if time.monotonic() >= startup_deadline:
                raise RuntimeError("Server did not reach listening state before timeout; inspect server.log")
            time.sleep(0.2)
        launch("observer")
        launch("driver")
        print(f"Match smoke running; evidence: {output}", flush=True)
        while True:
            healthy()  # Check exit status even when a completed report exists.
            if time.monotonic() - began >= args.timeout:
                raise RuntimeError("Timed out before all three roles completed")
            update_late_client()
            healthy()
            reports = {role: load_report(output / f"{role}.json") for role in ROLES}
            if all(report is not None for report in reports.values()):
                evaluate(reports)
                if not late_teardown_complete:
                    raise RuntimeError("Completed reports lack a real requested late-client join and disconnect")
                healthy()
                break
            time.sleep(0.25)
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Match smoke interrupted"
    finally:
        before_cleanup = {role: process.poll() for role, process in processes.items()}
        if any(code is not None and not (role == "late" and late_teardown_requested)
               for role, code in before_cleanup.items()):
            problem = problem or f"A test process exited before cleanup: {before_cleanup}"
        try:
            # A successfully cleaned auxiliary group is no longer ours to signal;
            # its numeric PID could eventually be reused during the remaining rounds.
            stop(process for role, process in processes.items() if role != "late" or not late_teardown_complete)
        except (OSError, RuntimeError) as error:
            problem = problem or f"Process cleanup failed: {error}"
        finally:
            cleanup_exit = {role: process.poll() for role, process in processes.items()}
            for stream in streams:
                stream.close()
            for sig, handler in previous_handlers.items():
                signal.signal(sig, handler)

    checks = []
    spectate = []
    after = None
    try:
        reports = {role: load_report(output / f"{role}.json") for role in ROLES}
        checks = evaluate(reports)
        logs = {role: (output / f"{role}.log").read_text(errors="replace") if (output / f"{role}.log").is_file() else None
                for role in SPECTATE_ROLES}
        server_text = (output / "server.log").read_text(errors="replace") if (output / "server.log").is_file() else None
        spectate = evaluate_spectate(reports, logs, server_text)
        after = evidence_metadata(root)
        if any(metadata[key] != after[key] for key in ("source_sha256", "editor_module_sha256", "map_sha256")):
            problem = problem or "Source, Editor module or Site01 map changed during the runtime test"
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        problem = problem or f"Invalid runtime evidence: {error}"
    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    passed = passed and all(check["status"] != "fail" for check in spectate)
    result = {
        **metadata, "passed": passed, "error": problem, "engine": str(engine), "engine_build": build,
        "metadata_after": after, "rendered_clients": args.rendered,
        "conditions": "Local Development editor server, two persistent clients and one requested late-join client; "
                      "loopback, no audio, no artificial latency/loss; "
                      + ("rendered clients, headless server" if args.rendered else "null RHI for all roles"),
        "runner_command": [sys.executable, str(Path(__file__).resolve()), *(sys.argv[1:] if argv is None else argv)],
        "commands": commands, "process_pids": {role: process.pid for role, process in processes.items()},
        "exit_before_cleanup": before_cleanup, "exit_after_cleanup": cleanup_exit,
        "late_client": {"launched": "late" in processes, "teardown_requested": late_teardown_requested,
                        "teardown_completed": late_teardown_complete, "exit_after_leave": late_exit_after_leave},
        "elapsed_seconds": time.monotonic() - began, "checks": checks, "spectate_checks": spectate,
        "spectate_client": {"launched": MATE_ROLE in processes}, "reports": reports,
    }
    (output / "result.json").write_text(json.dumps(result, indent=2, allow_nan=False) + "\n")
    for check in checks:
        print(("PASS " if check["passed"] else "FAIL ") + check["check"] + ": " + json.dumps(check["detail"]))
    for check in spectate:
        print(check["status"].upper() + " " + check["check"] + ": " + json.dumps(check["detail"]))
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
