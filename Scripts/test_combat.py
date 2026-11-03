#!/usr/bin/env python3
"""Run one editor server and two clients for the opt-in combat smoke test.

This is Development, loopback, null-RHI evidence, not packaged acceptance.
Build the Editor target first. Each process writes its completed JSON report to
the supplied -VSCombatOutput path; incomplete or missing evidence fails the run.
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

import ue


ROLES = ("server", "driver", "observer")
# These names are part of the runtime harness contract. A report cannot silently
# pass after a mandatory scenario disappears from the C++ harness.
COMMON_CHECKS = (
    "network_role", "initial_state", "body_damage", "reload_started", "reload_completed",
    "head_death", "dead_actions_blocked", "development_reset", "cover_blocks_damage",
    "automatic_fire_cadence", "empty_magazine", "partial_reload", "stop_combat_input",
    "validation_no_mutation", "reload_interrupted",
)
REQUIRED_CHECKS = {
    "server": COMMON_CHECKS + ("reject_duplicate", "reject_stale", "reject_cadence",
                                "reject_origin", "reject_direction", "reject_nonfinite",
                                "reject_reload", "reject_dead", "reject_empty"),
    "driver": COMMON_CHECKS + ("foreign_owner_request",),
    "observer": COMMON_CHECKS,
}


def reject_nonfinite(value):
    raise ValueError(f"Non-finite JSON number: {value}")


def load_report(path):
    """Return absent reports as None; never treat malformed reports as pending."""
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(), parse_constant=reject_nonfinite)
    except (UnicodeError, ValueError) as error:
        raise RuntimeError(f"Malformed combat report {path.name}: {error}") from error


def evaluate(reports, required_checks=None):
    """Validate all report identities and mandatory checks before accepting them."""
    required = REQUIRED_CHECKS if required_checks is None else required_checks
    if set(reports) != set(ROLES) or set(required) != set(ROLES):
        raise RuntimeError("Evidence must contain exactly server, driver and observer roles")
    checks = []
    for role in ROLES:
        report = reports[role]
        if not isinstance(report, dict):
            raise RuntimeError(f"Missing or invalid {role} report")
        if (type(report.get("schema_version")) is not int or report["schema_version"] != 1
                or report.get("role") != role or report.get("completed") is not True):
            raise RuntimeError(f"Invalid identity, schema or completion in {role} report")
        error = report.get("error")
        if error is not None and not isinstance(error, str):
            raise RuntimeError(f"Invalid error field in {role} report")
        if error:
            raise RuntimeError(f"{role} harness failed: {error}")
        items = report.get("checks")
        if not isinstance(items, list) or not items:
            raise RuntimeError(f"No combat checks in {role} report")
        names = set()
        for item in items:
            if not isinstance(item, dict):
                raise RuntimeError(f"Invalid combat check in {role} report")
            name = item.get("name")
            if (not isinstance(name, str) or not name.strip() or name in names
                    or type(item.get("passed")) is not bool or "detail" not in item):
                raise RuntimeError(f"Invalid or duplicate combat check in {role} report: {name!r}")
            names.add(name)
            checks.append({"check": f"{role}: {name}", "passed": item["passed"], "detail": item["detail"]})
        missing = set(required[role]) - names
        if missing:
            raise RuntimeError(f"Missing required {role} checks: {', '.join(sorted(missing))}")
    return checks


def launch_command(executable, project, output, role, port):
    if role not in ROLES:
        raise ValueError(f"Unknown combat role: {role}")
    destination = ue.MAP if role == "server" else f"127.0.0.1:{port}?Name=VSCombat{role.title()}"
    command = [str(executable), str(project), destination,
               "-server" if role == "server" else "-game", "-nullrhi", "-nosound", "-unattended",
               f"-VSCombatSmoke={role}", "-VSCombatSmokeStart=30",
               f"-VSCombatOutput={output / (role + '.json')}",
               f"-abslog={output / (role + '.log')}", "-ExecCmds=t.MaxFPS 60"]
    if role == "server":
        command += [f"-port={port}", "-multihome=127.0.0.1"]
    return command


def stop(processes):
    """Close only process groups created by this runner."""
    for process in processes:
        if process.poll() is None:
            try:
                if os.name == "posix":
                    os.killpg(process.pid, signal.SIGTERM)
                else:
                    process.terminate()
            except ProcessLookupError:
                pass
    for process in processes:
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            try:
                if os.name == "posix":
                    os.killpg(process.pid, signal.SIGKILL)
                else:
                    process.kill()
            except ProcessLookupError:
                pass
            process.wait(timeout=5)


def evidence_metadata(root):
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root,
                              capture_output=True, text=True, check=False)
    status = subprocess.run(["git", "--no-optional-locks", "status", "--porcelain"], cwd=root,
                            capture_output=True, text=True, check=False)
    paths = [path for folder in ("Source", "Config", "Tests/Combat")
             for path in (root / folder).rglob("*") if path.is_file() and "__pycache__" not in path.parts]
    paths += [root / "ValStrike.uproject", root / "Scripts/ue.py", root / "Scripts/test_combat.py"]
    hashes = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted(paths) if path.is_file()}
    module = root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib"
    return {
        "revision": revision.stdout.strip() if revision.returncode == 0 else None,
        "revision_error": revision.stderr.strip() if revision.returncode else None,
        "dirty_checkout": bool(status.stdout.strip()) if status.returncode == 0 else None,
        "git_status_error": status.stderr.strip() if status.returncode else None,
        "source_sha256": hashes,
        "editor_module_sha256": hashlib.sha256(module.read_bytes()).hexdigest() if module.is_file() else None,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=ue.port, default=7793)
    parser.add_argument("--timeout", type=float, default=150)
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 40:
        raise ValueError("--timeout must be finite and greater than 40 seconds")
    root = ue.ROOT
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(root)
    ue.map_file(root)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/Combat" / run_id
    output.mkdir(parents=True)
    metadata = evidence_metadata(root)
    processes, streams, commands = [], [], {}
    reports = {role: None for role in ROLES}

    def launch(role):
        command = launch_command(executable, project, output, role, args.port)
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes.append(subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                          start_new_session=os.name == "posix"))

    problem = None
    try:
        launch("server")
        began = time.monotonic()
        server_log = output / "server.log"
        while True:
            log = server_log.read_text(errors="replace") if server_log.exists() else ""
            if f"listening on port {args.port}" in log:
                break
            if processes[0].poll() is not None or time.monotonic() - began > 40:
                raise RuntimeError("Server did not reach listening state; inspect server.log")
            time.sleep(0.2)
        launch("observer")
        launch("driver")
        print(f"Combat smoke running; evidence: {output}", flush=True)
        while time.monotonic() - began < args.timeout:
            reports = {role: load_report(output / f"{role}.json") for role in ROLES}
            if all(report is not None for report in reports.values()):
                evaluate(reports)
                break
            if any(p.poll() is not None for p in processes):
                raise RuntimeError("A test process exited before all roles completed")
            time.sleep(0.5)
        else:
            raise RuntimeError("Timed out before all three roles completed")
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Combat smoke interrupted"
    finally:
        stop(processes)
        for stream in streams:
            stream.close()

    checks = []
    try:
        reports = {role: load_report(output / f"{role}.json") for role in ROLES}
        checks = evaluate(reports)
    except (RuntimeError, OSError) as error:
        problem = problem or f"Invalid runtime evidence: {error}"
    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    result = {**metadata, "passed": passed, "error": problem, "engine": str(engine),
              "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": "Three local editor processes, null RHI, no audio, loopback, no artificial latency/loss",
              "commands": commands, "checks": checks, "reports": reports}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for check in checks:
        print(("PASS " if check["passed"] else "FAIL ") + check["check"] + ": " + json.dumps(check["detail"]))
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
