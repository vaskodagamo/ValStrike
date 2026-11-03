#!/usr/bin/env python3
"""Verify the SRV-03 dedicated-server contract with real Development editor processes.

Scenarios, each with its own server process on loopback ports 7871-7874:
  invalid  An unknown -VSServer switch fails startup with a code-2 exit line.
  empty    Practice server with exit-when-done and one player slot: a client joins, a second
           is refused as full, the first leaves, and the server exits 0 after the empty timeout.
  match    Site01 match server with exit-when-done and development fast timings: two idle
           clients play six timeout rounds; the server writes the result, closes both clients
           after the result grace and exits 0.
  joinkey  Private practice server with a join key: a client without the key and one with a
           wrong key are refused, one with the right key joins, and the key never appears in
           the server log. The server then exits after the empty timeout.

Build the Editor target first and wrap this whole runner with with_ue_slot.py. This is
loopback development evidence, not packaged, Linux or fleet acceptance.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
import uuid

import ue
from test_match import stop

SCENARIOS = ("invalid", "empty", "match", "joinkey")
JOIN_KEY = "lifecycle-Key_01"
WRONG_KEY = "lifecycle-Key_02"
MATCH_MAP = "/Game/Maps/Site01/VS_Site01"
MATCH_MODE = "/Script/ValStrike.VSMatchGameMode"
# Long enough for a cold editor client to join first: the timer also runs before anyone joins.
EMPTY_TIMEOUT_S = 30
RESULT_GRACE_S = 2
DRAIN_MS = 2000
EVENT = re.compile(r"(VSSERVER_[A-Z_]+)(?: (.*))?$")
FIELD = re.compile(r"(\w+)=(\S*)")


def server_port(value):
    try:
        port = int(value)
    except (TypeError, ValueError) as error:
        raise argparse.ArgumentTypeError("server lifecycle ports must be integers from 7871 through 7874") from error
    if not 7871 <= port <= 7874:
        raise argparse.ArgumentTypeError("server lifecycle ports are reserved to 7871 through 7874")
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
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=server_port, default=7871)
    parser.add_argument("--scenario", choices=SCENARIOS + ("all",), default="all")
    parser.add_argument("--timeout", type=timeout_seconds, default=180.0, help="per-scenario limit in seconds")
    parser.add_argument("--net-tick-rate", type=int, choices=(30, 60),
                        help="measurement only: override NetServerMaxTickRate for this run's servers")
    return parser.parse_args(argv)


def parse_events(log):
    """Return VSSERVER_* log events in order, each with its kind, key=value fields and payload."""
    events = []
    for line in log.splitlines():
        marker = line.find("VSSERVER_")
        if marker < 0:
            continue
        match = EVENT.match(line[marker:].rstrip())
        if not match:
            continue
        kind, payload = match.group(1), match.group(2) or ""
        fields = {} if kind == "VSSERVER_RESULT" else dict(FIELD.findall(payload))
        events.append({"kind": kind, "fields": fields, "payload": payload})
    return events


def transitions(events):
    return [(e["fields"].get("from"), e["fields"].get("to"), e["fields"].get("reason"),
             int(e["fields"].get("players", -1)), int(e["fields"].get("at_ms", -1)))
            for e in events if e["kind"] == "VSSERVER_STATE"]


def first(events, kind, **fields):
    for event in events:
        if event["kind"] == kind and all(event["fields"].get(k) == v for k, v in fields.items()):
            return event
    return None


def check(checks, name, passed, detail):
    checks.append({"name": name, "passed": bool(passed), "detail": detail})


def version_checks(checks, events, revision):
    version = first(events, "VSSERVER_VERSION")
    fields = version["fields"] if version else {}
    check(checks, "version_line", version is not None and fields.get("revision") == revision
          and fields.get("engine", "").startswith("5.8.4-") and fields.get("configuration") == "Development",
          fields or "missing VSSERVER_VERSION")


def path_check(checks, events, expected):
    path = [(a, b, c) for a, b, c, _players, _at in transitions(events)]
    check(checks, "state_path", path == expected, {"expected": expected, "actual": path})


def exit_checks(checks, events, code, process_exit, host):
    line = first(events, "VSSERVER_EXIT")
    check(checks, "exit_line", line is not None and line["fields"].get("code") == str(code),
          line["fields"] if line else "missing VSSERVER_EXIT")
    # The Mac editor binary returns 0 for every engine-requested exit; Linux/Windows return the code.
    expected = 0 if host == "Mac" else code
    check(checks, "process_exit", process_exit == expected,
          {"process_exit": process_exit, "expected": expected, "host": host})


def evaluate_invalid(log, process_exit, revision, host):
    checks, events = [], parse_events(log)
    version_checks(checks, events, revision)
    failure = first(events, "VSSERVER_FAILURE")
    check(checks, "failure_line", failure is not None and failure["fields"].get("code") == "2"
          and "VSServerTypo" in failure["payload"], failure["payload"] if failure else "missing VSSERVER_FAILURE")
    path_check(checks, events, [("starting", "ending", "failure"), ("ending", "exited", "drained")])
    check(checks, "never_ready", first(events, "VSSERVER_MAP") is None, "no VSSERVER_MAP line")
    exit_checks(checks, events, 2, process_exit, host)
    return checks


def evaluate_empty(log, process_exit, revision, host):
    checks, events = [], parse_events(log)
    version_checks(checks, events, revision)
    config = first(events, "VSSERVER_CONFIG")
    check(checks, "config_line", config is not None and config["fields"].get("exit_when_done") == "true"
          and config["fields"].get("empty_timeout_s") == str(EMPTY_TIMEOUT_S)
          and config["fields"].get("max_players") == "1", config["fields"] if config else "missing")
    map_line = first(events, "VSSERVER_MAP")
    check(checks, "map_line", map_line is not None and map_line["fields"].get("map") == "graybox"
          and map_line["fields"].get("mode") == "practice", map_line["fields"] if map_line else "missing")
    path_check(checks, events, [("starting", "ready", "listening"), ("ready", "allocated", "player_joined"),
                                ("allocated", "ending", "empty_timeout"), ("ending", "exited", "drained")])
    check(checks, "max_players_enforced", "PreLogin failure: Server full." in log,
          "engine GameSession refused the second client" if "PreLogin failure: Server full." in log
          else "no 'PreLogin failure: Server full.' line")
    counts = [int(e["fields"].get("count", -1)) for e in events if e["kind"] == "VSSERVER_PLAYERS"]
    check(checks, "player_counts", counts == [1, 0], counts)
    left = [e for e in events if e["kind"] == "VSSERVER_PLAYERS" and e["fields"].get("count") == "0"]
    ending = [t for t in transitions(events) if t[1] == "ending"]
    waited = ending[0][4] - int(left[-1]["fields"]["at_ms"]) if left and ending else None
    check(checks, "empty_timeout_measured", waited is not None and
          EMPTY_TIMEOUT_S * 1000 <= waited <= EMPTY_TIMEOUT_S * 1000 + 1000,
          {"empty_ms_before_ending": waited, "configured_ms": EMPTY_TIMEOUT_S * 1000})
    exit_checks(checks, events, 0, process_exit, host)
    return checks


def evaluate_joinkey(log, process_exit, revision, host):
    checks, events = [], parse_events(log)
    version_checks(checks, events, revision)
    config = first(events, "VSSERVER_CONFIG")
    check(checks, "config_line", config is not None and config["fields"].get("admission") == "join_key",
          config["fields"] if config else "missing")
    rejected = [e for e in events if e["kind"] == "VSSERVER_ADMISSION" and e["fields"].get("admission") == "join_key"]
    check(checks, "wrong_keys_rejected", len(rejected) == 2 and all("Join key" in e["payload"] for e in rejected)
          and log.count("PreLogin failure: Join key missing or wrong") == 2,
          [e["payload"] for e in rejected] or "no join_key rejection")
    check(checks, "key_never_logged", JOIN_KEY not in log and WRONG_KEY not in log, "neither key string in server.log")
    counts = [int(e["fields"].get("count", -1)) for e in events if e["kind"] == "VSSERVER_PLAYERS"]
    check(checks, "right_key_admitted", counts == [1, 0], counts)
    path_check(checks, events, [("starting", "ready", "listening"), ("ready", "allocated", "player_joined"),
                                ("allocated", "ending", "empty_timeout"), ("ending", "exited", "drained")])
    exit_checks(checks, events, 0, process_exit, host)
    return checks


def expected_rounds():
    # Idle players: every round ends by timeout and the defending team wins; sides swap after three.
    return [{"round": n, "attacking_team": 1 if n <= 3 else 2, "winning_team": 2 if n <= 3 else 1,
             "reason": "timeout"} for n in range(1, 7)]


def evaluate_match(log, process_exit, revision, host, session, result_file):
    checks, events = [], parse_events(log)
    version_checks(checks, events, revision)
    map_line = first(events, "VSSERVER_MAP")
    check(checks, "map_line", map_line is not None and map_line["fields"].get("map") == "site01"
          and map_line["fields"].get("mode") == "match", map_line["fields"] if map_line else "missing")
    tick_rate = int(map_line["fields"].get("net_tick_rate", 0)) if map_line else 0
    check(checks, "tick_rate_configured", tick_rate in (30, 60), {"net_tick_rate": tick_rate})
    path_check(checks, events, [("starting", "ready", "listening"), ("ready", "allocated", "player_joined"),
                                ("allocated", "in_match", "match_started"), ("in_match", "ending", "match_complete"),
                                ("ending", "exited", "drained")])
    states = {t[1]: t for t in transitions(events)}
    completed = first(events, "VSSERVER_MATCH", event="completed")
    grace = (states["ending"][4] - int(completed["fields"]["at_ms"])) if completed and "ending" in states else None
    check(checks, "result_grace", grace is not None and RESULT_GRACE_S * 1000 <= grace <= RESULT_GRACE_S * 1000 + 1000,
          {"completed_to_ending_ms": grace, "configured_ms": RESULT_GRACE_S * 1000})
    drained = (states["exited"][4] - states["ending"][4]) if "exited" in states and "ending" in states else None
    check(checks, "clients_closed", "ending" in states and states["ending"][3] == 2 and "exited" in states
          and states["exited"][3] == 0 and drained is not None and drained < DRAIN_MS,
          {"players_at_ending": states["ending"][3] if "ending" in states else None,
           "players_at_exit": states["exited"][3] if "exited" in states else None, "drain_ms": drained})
    line = first(events, "VSSERVER_RESULT")
    try:
        logged = json.loads(line["payload"]) if line else None
    except ValueError:
        logged = None
    if logged is not None:
        expected = {"schema_version": 1, "session_id": session, "match_index": 1, "status": "completed",
                    "map": "site01", "mode": "match", "source_revision": revision, "result": "draw",
                    "completed_rounds": 6, "scores": {"team_one": 3, "team_two": 3},
                    "participants": {"team_one": 1, "team_two": 1}, "rounds": expected_rounds()}
        mismatched = {key: logged.get(key) for key, value in expected.items() if logged.get(key) != value}
        check(checks, "result_json", not mismatched, mismatched or "all expected fields match")
    else:
        check(checks, "result_json", False, line["payload"] if line else "missing VSSERVER_RESULT")
    saved = None
    if result_file.is_file():
        try:
            saved = json.loads(result_file.read_text(encoding="utf-8"))
        except ValueError:
            saved = None
    check(checks, "result_file", saved is not None and saved == logged, str(result_file))
    check(checks, "result_consistent", first(events, "VSSERVER_RESULT_MISMATCH") is None,
          "rules score agrees with recorded rounds")
    health = [e for e in events if e["kind"] == "VSSERVER_HEALTH"]
    check(checks, "health_reported", bool(health) and all(int(e["fields"].get("frames", 0)) > 0
          and float(e["fields"].get("tick_hz", 0)) > 0 for e in health),
          [e["fields"] for e in health] or "no VSSERVER_HEALTH line")
    # The server runs without t.MaxFPS, so the measured rate must follow the configured cap.
    rates = [float(e["fields"].get("tick_hz", 0)) for e in health[1:]] or [float(e["fields"].get("tick_hz", 0)) for e in health]
    check(checks, "tick_rate_measured", bool(rates) and tick_rate > 0
          and all(0.9 * tick_rate <= rate <= 1.05 * tick_rate for rate in rates),
          {"configured": tick_rate, "measured_hz": rates})
    exit_checks(checks, events, 0, process_exit, host)
    return checks


def server_command(executable, project, output, scenario, port, session, net_tick_rate=None):
    if scenario not in SCENARIOS:
        raise ValueError(f"Unknown scenario: {scenario}")
    if net_tick_rate not in (None, 30, 60):
        raise ValueError("net tick rate must be 30 or 60")
    port = server_port(port)
    destination = f"{MATCH_MAP}?game={MATCH_MODE}" if scenario == "match" else ue.MAP
    # No t.MaxFPS: the server must run at its configured NetServerMaxTickRate.
    command = [str(executable), str(project), destination, "-server", "-nullrhi", "-nosound", "-unattended",
               f"-port={port}", "-multihome=127.0.0.1", f"-abslog={output / (scenario + '_server.log')}",
               f"-VSServerSession={session}"]
    if net_tick_rate is not None:
        command.append(f"-ini:Engine:[/Script/OnlineSubsystemUtils.IpNetDriver]:NetServerMaxTickRate={net_tick_rate}")
    if scenario == "invalid":
        return command + ["-VSServerTypo"]
    command += ["-VSServerExitWhenDone"]
    if scenario == "empty":
        return command + [f"-VSServerEmptyTimeout={EMPTY_TIMEOUT_S}", "-VSServerMaxPlayers=1"]
    if scenario == "joinkey":
        return command + [f"-VSServerEmptyTimeout={EMPTY_TIMEOUT_S}", f"-VSServerJoinKey={JOIN_KEY}"]
    return command + [f"-VSServerResultGrace={RESULT_GRACE_S}", "-VSServerEmptyTimeout=60", "-VSMatchFastTimings"]


def client_command(executable, project, output, scenario, port, name, join_key=None):
    if not re.fullmatch(r"[A-Za-z0-9]{1,24}", name):
        raise ValueError(f"Invalid client name: {name}")
    if scenario not in SCENARIOS:
        raise ValueError(f"Unknown scenario: {scenario}")
    if join_key is not None and not re.fullmatch(r"[A-Za-z0-9_-]{8,64}", join_key):
        raise ValueError("Invalid join key")
    destination = f"127.0.0.1:{server_port(port)}?Name={name}" + (f"?JoinKey={join_key}" if join_key else "")
    return [str(executable), str(project), destination, "-game", "-nullrhi",
            "-nosound", "-unattended", f"-abslog={output / f'{scenario}_{name}.log'}", "-ExecCmds=t.MaxFPS 60"]


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git(root, *args):
    result = subprocess.run(["git", "--no-optional-locks", *args], cwd=root, capture_output=True, text=True,
                            check=False, timeout=15)
    return result.stdout.strip() if result.returncode == 0 else None


class Scenario:
    """Owns one scenario's processes; every process is stopped before the scenario returns."""

    def __init__(self, name, executable, project, output, port, limit):
        self.name, self.executable, self.project, self.output, self.port = name, executable, project, output, port
        self.deadline = time.monotonic() + limit
        self.processes, self.streams, self.commands = {}, [], {}

    def launch(self, role, command):
        self.commands[role] = command
        stream = (self.output / f"{self.name}_{role}.stdout.log").open("w")
        self.streams.append(stream)
        self.processes[role] = subprocess.Popen(command, cwd=ue.ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                                start_new_session=os.name == "posix")

    def log(self, scenario):
        path = self.output / f"{scenario}_server.log"
        return path.read_text(errors="replace") if path.exists() else ""

    def wait_for(self, scenario, text, what):
        while text not in self.log(scenario):
            if self.processes["server"].poll() is not None:
                raise RuntimeError(f"Server exited before {what}")
            if time.monotonic() >= self.deadline:
                raise RuntimeError(f"Timed out waiting for {what}")
            time.sleep(0.2)

    def wait_server_exit(self):
        try:
            return self.processes["server"].wait(timeout=max(0.1, self.deadline - time.monotonic()))
        except subprocess.TimeoutExpired as error:
            raise RuntimeError("Server did not exit on its own before the scenario timeout") from error

    def stop_role(self, role):
        stop([self.processes[role]])
        return self.processes[role].poll()

    def close(self):
        exits_before = {role: process.poll() for role, process in self.processes.items()}
        stop(self.processes.values())
        for stream in self.streams:
            stream.close()
        return exits_before


def run_scenario(name, executable, project, output, port, limit, run_id, net_tick_rate=None):
    session = f"{run_id}-{name}".replace("_", "-")
    owner = Scenario(name, executable, project, output, port, limit)
    problem, process_exit, client_exits = None, None, {}
    began = time.monotonic()
    try:
        owner.launch("server", server_command(executable, project, output, name, port, session, net_tick_rate))
        if name == "invalid":
            process_exit = owner.wait_server_exit()
        elif name == "empty":
            owner.wait_for(name, "to=ready", "the ready state")
            owner.launch("client_a", client_command(executable, project, output, name, port, "VSServerProbeA"))
            owner.wait_for(name, "to=allocated", "the first client to join")
            owner.launch("client_b", client_command(executable, project, output, name, port, "VSServerProbeB"))
            owner.wait_for(name, "PreLogin failure: Server full.", "the second client to be refused")
            client_exits["client_b"] = owner.stop_role("client_b")
            client_exits["client_a"] = owner.stop_role("client_a")
            process_exit = owner.wait_server_exit()
        elif name == "joinkey":
            owner.wait_for(name, "to=ready", "the ready state")
            owner.launch("client_a", client_command(executable, project, output, name, port, "VSServerProbeA"))
            owner.wait_for(name, "PreLogin failure: Join key missing or wrong", "the keyless client to be refused")
            client_exits["client_a"] = owner.stop_role("client_a")
            owner.launch("client_b", client_command(executable, project, output, name, port, "VSServerProbeB", WRONG_KEY))
            while owner.log(name).count("PreLogin failure: Join key missing or wrong") < 2:
                if owner.processes["server"].poll() is not None:
                    raise RuntimeError("Server exited before the wrong-key client was refused")
                if time.monotonic() >= owner.deadline:
                    raise RuntimeError("Timed out waiting for the wrong-key client to be refused")
                time.sleep(0.2)
            client_exits["client_b"] = owner.stop_role("client_b")
            owner.launch("client_c", client_command(executable, project, output, name, port, "VSServerProbeC", JOIN_KEY))
            owner.wait_for(name, "to=allocated", "the keyed client to join")
            client_exits["client_c"] = owner.stop_role("client_c")
            process_exit = owner.wait_server_exit()
        else:
            owner.wait_for(name, "to=ready", "the ready state")
            owner.launch("client_a", client_command(executable, project, output, name, port, "VSServerProbeA"))
            owner.launch("client_b", client_command(executable, project, output, name, port, "VSServerProbeB"))
            process_exit = owner.wait_server_exit()
    except (RuntimeError, OSError) as error:
        problem = str(error)
    finally:
        exits_before = owner.close()
    if process_exit is None and exits_before.get("server") is not None:
        process_exit = exits_before["server"]  # Exited on its own before the runner waited for it.
    log = owner.log(name)
    revision = git(ue.ROOT, "rev-parse", "HEAD")
    host = ue.host_platform()
    if name == "invalid":
        checks = evaluate_invalid(log, process_exit, revision, host)
    elif name == "empty":
        checks = evaluate_empty(log, process_exit, revision, host)
    elif name == "joinkey":
        checks = evaluate_joinkey(log, process_exit, revision, host)
    else:
        result_file = ue.ROOT / "Saved/ServerResults" / f"{session}_match1.json"
        checks = evaluate_match(log, process_exit, revision, host, session, result_file)
    if problem:
        check(checks, "scenario_completed", False, problem)
    return {"scenario": name, "session": session, "passed": not problem and all(c["passed"] for c in checks),
            "error": problem, "elapsed_seconds": time.monotonic() - began, "server_exit": process_exit,
            "exit_before_cleanup": exits_before, "client_exit_after_stop": client_exits,
            "commands": owner.commands, "checks": checks}


def main(argv=None):
    args = arguments(argv)
    root = ue.ROOT
    host = ue.host_platform()
    if host != "Mac":
        raise ue.SetupError("The server lifecycle runner currently verifies the pinned Mac Editor build only")
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, host)
    project = ue.project_file(root)
    module = ue.require_file(root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib",
                             "Build this checkout's Mac Editor target first.")
    for key in ("graybox", "site01"):
        ue.playtest_check("validate_assets", root, key)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/ServerLifecycle" / run_id
    output.mkdir(parents=True)
    module_before = sha256(module)
    scenarios = SCENARIOS if args.scenario == "all" else (args.scenario,)
    results = []
    for name in scenarios:
        print(f"Server lifecycle: running {name}", flush=True)
        results.append(run_scenario(name, executable, project, output, args.port, args.timeout, run_id,
                                    args.net_tick_rate))
    module_after = sha256(module)
    problem = None if module_before == module_after else "Editor module changed during the runtime test"
    passed = not problem and all(result["passed"] for result in results)
    report = {
        "passed": passed, "error": problem, "revision": git(root, "rev-parse", "HEAD"),
        "dirty_checkout": bool(git(root, "status", "--porcelain")), "editor_module_sha256": module_before,
        "engine": str(engine), "engine_build": json.loads((engine / "Build/Build.version").read_text()),
        "conditions": "Local Development editor processes on loopback, null RHI, no audio, no artificial "
                      "latency/loss; the Mac editor binary is not a packaged dedicated server",
        "net_tick_rate_override": args.net_tick_rate,
        "runner_command": [sys.executable, str(Path(__file__).resolve()), *(sys.argv[1:] if argv is None else argv)],
        "scenarios": results,
    }
    (output / "result.json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n")
    for result in results:
        for item in result["checks"]:
            print(("PASS " if item["passed"] else "FAIL ") + f"{result['scenario']}: {item['name']}: "
                  + json.dumps(item["detail"]))
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
