#!/usr/bin/env python3
"""Run the rendered standalone mouse-look regression harness.

Uses one local Editor game process and requires all eight check screenshots.
This is Development rendered-input evidence, not multiplayer or packaged proof.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import uuid

import test_combat as combat
import ue


REQUIRED_CHECKS = (
    "right", "left", "up", "down", "upper_pitch_limit", "lower_pitch_limit",
    "focus_ignores_look", "focus_return",
)


def evaluate(report):
    if (not isinstance(report, dict) or type(report.get("schema_version")) is not int
            or report["schema_version"] != 1 or report.get("completed") is not True):
        raise RuntimeError("Missing, invalid or incomplete look report")
    error = report.get("error")
    if error is not None and not isinstance(error, str):
        raise RuntimeError("Invalid error field in look report")
    if error:
        raise RuntimeError(f"Look harness failed: {error}")
    items = report.get("checks")
    if not isinstance(items, list) or not items:
        raise RuntimeError("No checks in look report")
    names, checks = set(), []
    for item in items:
        if not isinstance(item, dict):
            raise RuntimeError("Invalid look check")
        name = item.get("name")
        if (not isinstance(name, str) or not name.strip() or name in names
                or type(item.get("passed")) is not bool or "detail" not in item):
            raise RuntimeError(f"Invalid or duplicate look check: {name!r}")
        names.add(name)
        checks.append({"check": name, "passed": item["passed"], "detail": item["detail"]})
    missing = set(REQUIRED_CHECKS) - names
    if missing:
        raise RuntimeError(f"Missing required look checks: {', '.join(sorted(missing))}")
    return checks


def screenshots(report_path):
    """Require a nonempty PNG with dimensions for every rendered checkpoint."""
    evidence = {}
    for name in REQUIRED_CHECKS:
        path = report_path.with_name(f"{report_path.stem}_{name}.png")
        if not path.is_file():
            raise RuntimeError(f"Missing rendered screenshot: {path.name}")
        data = path.read_bytes()
        if len(data) <= 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
            raise RuntimeError(f"Invalid or empty PNG screenshot: {path.name}")
        width, height = struct.unpack(">II", data[16:24])
        if width == 0 or height == 0:
            raise RuntimeError(f"Empty PNG dimensions: {path.name}")
        evidence[name] = {"path": str(path), "width": width, "height": height,
                          "sha256": hashlib.sha256(data).hexdigest()}
    return evidence


def launch_command(executable, project, output):
    return [str(executable), str(project), ue.MAP, "-game", "-windowed",
            "-ResX=1000", "-ResY=700", "-nosound", "-VSCombatLookSmoke",
            f"-VSCombatLookOutput={output / 'look.json'}",
            f"-abslog={output / 'look.log'}", "-ExecCmds=t.MaxFPS 60"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--timeout", type=float, default=120)
    parser.add_argument("--hold-open", type=float, default=0, metavar="SECONDS",
                        help="Keep this process open after collecting evidence for visual inspection")
    args = parser.parse_args(argv)
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        raise ValueError("--timeout must be finite and positive")
    if not math.isfinite(args.hold_open) or args.hold_open < 0:
        raise ValueError("--hold-open must be finite and nonnegative")
    root = ue.ROOT
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(root)
    ue.map_file(root)
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = root / "Saved/Tests/CombatLook" / run_id
    output.mkdir(parents=True)
    metadata = combat.evidence_metadata(root)
    runner_path = Path(__file__).resolve()
    metadata["source_sha256"][str(runner_path.relative_to(root))] = hashlib.sha256(runner_path.read_bytes()).hexdigest()
    command = launch_command(executable, project, output)
    report_path = output / "look.json"
    report, checks, captures, problem = None, [], {}, None
    processes = []
    stream = (output / "look.stdout.log").open("w")
    try:
        processes.append(subprocess.Popen(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT,
                                          start_new_session=os.name == "posix"))
        began = time.monotonic()
        print(f"Rendered look smoke running; evidence: {output}", flush=True)
        while time.monotonic() - began < args.timeout:
            report = combat.load_report(report_path)
            if report is not None:
                checks = evaluate(report)
                captures = screenshots(report_path)
                break
            if processes[0].poll() is not None:
                raise RuntimeError("Rendered process exited before its look report completed")
            time.sleep(0.2)
        else:
            raise RuntimeError("Timed out before rendered look harness completed")
        if args.hold_open:
            print(f"Look evidence collected; holding window for up to {args.hold_open:g} seconds", flush=True)
            hold_until = time.monotonic() + args.hold_open
            while time.monotonic() < hold_until and processes[0].poll() is None:
                time.sleep(0.2)
    except (RuntimeError, OSError, KeyboardInterrupt) as error:
        problem = str(error) or "Rendered look smoke interrupted"
    finally:
        combat.stop(processes)
        stream.close()

    passed = not problem and bool(checks) and all(check["passed"] for check in checks)
    result = {**metadata, "passed": passed, "error": problem, "engine": str(engine),
              "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": "One local standalone editor game, rendered 1000x700 window, no audio",
              "command": command, "checks": checks, "screenshots": captures, "report": report}
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
