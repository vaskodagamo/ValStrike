#!/usr/bin/env python3
"""Run three real editor processes and check authoritative/replicated movement.

This is an opt-in Development smoke test, not packaged or visual acceptance.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import signal
import socket
import statistics
import subprocess
import sys
import time
import uuid

import ue


def records(path, complete_only=True):
    result = []
    if path.exists():
        for line in path.read_text(errors="replace").splitlines(keepends=True):
            if complete_only and not line.endswith("\n"):
                continue  # The running engine may still be writing this record.
            if "VSMOVE_JSON " in line:
                try:
                    result.append(json.loads(line.split("VSMOVE_JSON ", 1)[1]))
                except json.JSONDecodeError:
                    raise RuntimeError(f"Malformed movement evidence in {path.name}")
    return result


def evaluate(evidence):
    checks = []

    def check(name, passed, detail):
        checks.append({"check": name, "passed": bool(passed), "detail": detail})

    for role in ("server", "driver", "observer"):
        data = evidence[role]
        check(f"{role}: evidence identity", bool(data) and all(i.get("role") == role for i in data),
              {"roles": sorted({str(i.get("role")) for i in data})})
        errors = [item for item in data if item.get("event") == "error"]
        check(f"{role}: completed", any(i.get("event") == "done" for i in data) and not errors,
              {"errors": errors, "records": len(data)})
        samples = [i for i in data if i.get("event") == "sample" and i.get("player") == "VSDriver"]
        expected_role = {"server": 3, "driver": 2, "observer": 1}[role]
        check(f"{role}: network role", bool(samples) and all(
            i["net_role"] == expected_role and i["authority"] == (role == "server") for i in samples),
            {"expected_role": expected_role, "observed": sorted({i["net_role"] for i in samples})})

        def window(start, end):
            return [i for i in samples if start <= i["t"] <= end]

        def median(items, field):
            return statistics.median(i[field] for i in items) if items else None

        for label, start, end, low, high in (
            ("run", 1.6, 2.4, 400, 480),
            ("walk", 3.6, 4.8, 170, 230),
            ("crouch move", 6.0, 6.8, 150, 205),
        ):
            group = window(start, end)
            speed = median(group, "speed")
            check(f"{role}: {label} speed", len(group) >= 4 and low <= (speed or 0) <= high,
                  {"samples": len(group), "median_cm_s": speed})

        crouched = window(6.0, 6.8)
        standing = window(7.5, 7.9)
        check(f"{role}: crouch state and capsule", len(crouched) >= 4 and
              all(i["crouched"] and abs(i["half_height"] - 56) < 1 for i in crouched),
              {"samples": len(crouched), "half_height": median(crouched, "half_height")})
        check(f"{role}: return to standing", len(standing) >= 3 and
              all(not i["crouched"] and abs(i["half_height"] - 88) < 1 for i in standing),
              {"samples": len(standing), "half_height": median(standing, "half_height")})
        pose = crouched + standing
        foot_error = max((abs(i["body_min_z"] - (i["z"] - i["half_height"])) for i in pose), default=999)
        check(f"{role}: body feet stay aligned", len(crouched) >= 4 and len(standing) >= 3 and foot_error < 8,
              {"max_foot_error_cm": foot_error})
        span_error = max((abs(i["body_max_z"] - i["body_min_z"] - (112 if i["crouched"] else 176)) for i in pose), default=999)
        check(f"{role}: body resizes with crouch", len(pose) >= 7 and span_error < 2,
              {"max_height_error_cm": span_error})
        if role == "driver":
            eye_error = max((abs(i["eye_z"] - i["z"] - (32 if i["crouched"] else 64)) for i in pose), default=999)
            check("driver: camera follows crouch", len(pose) >= 7 and eye_error < 1,
                  {"max_eye_height_error_cm": eye_error})
        floor = median(standing, "z")
        jump = window(8.0, 9.5)
        height = max((i["z"] for i in jump), default=0) - (floor or 0)
        landed = window(10.0, 10.8)
        check(f"{role}: jump and land", floor is not None and 50 < height < 120 and
              any(not i["grounded"] and i["vz"] > 50 for i in jump) and
              len(landed) >= 4 and all(i["grounded"] and abs(i["z"] - floor) < 5 for i in landed),
              {"jump_height_cm": height, "landing_samples": len(landed)})
        wall = window(16.5, 17.5)
        wall_x = median(wall, "x")
        check(f"{role}: boundary blocks movement", len(wall) >= 5 and
              wall_x is not None and 1905 <= wall_x <= 1920 and
              all(i["x"] <= 1920 for i in window(11.0, 18.0)) and
              (median(wall, "speed") or 0) < 5,
              {"x": wall_x, "speed": median(wall, "speed")})
        stairs = window(21.0, 24.0)
        plateau = [i for i in stairs if i["grounded"] and 162 <= i["z"] <= 175]
        check(f"{role}: stairs reach platform", len(plateau) >= 2,
              {"grounded_platform_samples": len(plateau), "max_z": max((i["z"] for i in stairs), default=None)})
        cover = window(30.0, 31.0)
        cover_x = median(cover, "x")
        check(f"{role}: cover blocks movement", len(cover) >= 5 and
              cover_x is not None and -640 <= cover_x <= -630 and
              (median(cover, "speed") or 0) < 5,
              {"x": cover_x, "speed": median(cover, "speed")})

    # The server must see both players before test-only positioning begins.
    ready = [r for r in evidence["server"] if r.get("event") == "ready"]
    check("distinct initial spawns", any(r.get("initial_spawn_distance", 0) > 100 for r in ready), ready)
    player_ids = {role: sorted({r["player_id"] for r in data if r.get("event") == "sample"})
                  for role, data in evidence.items()}
    check("same driver observed in all processes", len(player_ids["server"]) == 1 and
          player_ids["server"] == player_ids["driver"] == player_ids["observer"], player_ids)
    return checks


def stop(processes):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=ue.port, default=7789)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    root = ue.ROOT
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(root)
    ue.map_file(root)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", args.port))
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/Movement" / run_id
    output.mkdir(parents=True)
    processes, streams, commands = [], [], {}
    paths = {role: output / f"{role}.log" for role in ("server", "driver", "observer")}

    def launch(role):
        destination = ue.MAP if role == "server" else f"127.0.0.1:{args.port}?Name=VS{role.title()}"
        command = [str(executable), str(project), destination,
                   "-server" if role == "server" else "-game", "-nullrhi", "-nosound", "-unattended",
                   f"-VSMovementSmoke={role}", "-VSMovementSmokeStart=30", f"-abslog={paths[role]}",
                   "-ExecCmds=t.MaxFPS 60"]
        if role == "server":
            command += [f"-port={args.port}", "-multihome=127.0.0.1"]
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        processes.append(subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                          start_new_session=os.name == "posix"))

    problem = None
    try:
        launch("server")
        began = time.monotonic()
        while True:
            log = paths["server"].read_text(errors="replace") if paths["server"].exists() else ""
            if f"listening on port {args.port}" in log:
                break
            if processes[0].poll() is not None or time.monotonic() - began > 40:
                raise RuntimeError("Server did not reach listening state; inspect server.log")
            time.sleep(0.2)
        launch("observer")
        launch("driver")
        print(f"Movement smoke running; evidence: {output}", flush=True)
        while time.monotonic() - began < args.timeout:
            evidence = {role: records(path) for role, path in paths.items()}
            if any(item.get("event") == "error" for data in evidence.values() for item in data):
                raise RuntimeError("Runtime harness reported an error; inspect result.json and role logs")
            if all(any(i.get("event") == "done" for i in data) for data in evidence.values()):
                break
            if any(p.poll() is not None for p in processes):
                raise RuntimeError("A test process exited before all roles completed")
            time.sleep(0.5)
        else:
            raise RuntimeError("Timed out before all three roles completed")
    except (RuntimeError, OSError) as error:
        problem = str(error)
    finally:
        stop(processes)
        for stream in streams:
            stream.close()

    try:
        evidence = {role: records(path, complete_only=False) for role, path in paths.items()}
        checks = evaluate(evidence)
    except (RuntimeError, KeyError, TypeError, ValueError) as error:
        problem = f"Invalid runtime evidence: {error}"
        checks = []
    passed = not problem and all(check["passed"] for check in checks)
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root, capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain"], cwd=root,
                                capture_output=True, text=True).stdout.strip())
    hashes = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted((root / "Source/ValStrike").rglob("*")) if path.is_file()}
    result = {"passed": passed, "error": problem, "revision": revision, "engine": str(engine),
              "dirty_checkout": dirty, "source_sha256": hashes,
              "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": "Three local editor processes, null RHI, loopback, no artificial latency/loss",
              "commands": commands, "checks": checks}
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
