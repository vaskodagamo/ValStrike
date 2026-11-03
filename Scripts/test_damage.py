#!/usr/bin/env python3
"""Compile/run the standalone damage model, hit-zone layout and stats ledger tests.

No Unreal launch, dependencies, exceptions, or RTTI. Retains commands, compiler
output and source hashes under Saved/Tests/Damage/<run id>/ in this checkout.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import uuid


ROOT = Path(__file__).resolve().parents[1]
SUITES = {
    "damage": {
        "sources": [
            ROOT / "Source/ValStrike/Damage/VSDamageModel.cpp",
            ROOT / "Source/ValStrike/Damage/VSHitZoneLayout.cpp",
            ROOT / "Tests/Damage/VSDamageTests.cpp",
        ],
        "headers": [
            ROOT / "Source/ValStrike/Damage/VSDamageModel.h",
            ROOT / "Source/ValStrike/Damage/VSHitZoneLayout.h",
        ],
    },
    "stats": {
        "sources": [
            ROOT / "Source/ValStrike/Scoreboard/VSStatsLedger.cpp",
            ROOT / "Tests/Scoreboard/VSStatsLedgerTests.cpp",
        ],
        "headers": [ROOT / "Source/ValStrike/Scoreboard/VSStatsLedger.h"],
    },
}


def compiler_command(args):
    if args.compiler:
        return [args.compiler]
    return ["xcrun", "clang++"] if sys.platform == "darwin" else ["c++"]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true", help="enable address/undefined-behavior sanitizers")
    parser.add_argument("--compiler", help="compiler executable; defaults to xcrun clang++ on macOS, c++ elsewhere")
    args = parser.parse_args(argv)
    compiler = compiler_command(args)
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/Damage" / run_id
    output.mkdir(parents=True)
    flags = ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Werror", "-fno-exceptions", "-fno-rtti",
             "-O1", "-g"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    every_file = [path for suite in SUITES.values() for path in suite["sources"] + suite["headers"]]
    result = {"passed": False, "sanitized": args.sanitize, "commands": [], "source_sha256": {}, "suites": {}}
    env = os.environ.copy()
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    env["ASAN_OPTIONS"] = "halt_on_error=1"

    def run(label, command, timeout):
        result["commands"].append(command)
        print(shlex.join(command), flush=True)
        completed = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=timeout, check=False)
        (output / f"{label}.log").write_text(completed.stdout)
        print(completed.stdout, end="", flush=True)
        result[f"{label}_exit"] = completed.returncode
        return completed

    try:
        result["source_sha256"] = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                   for path in every_file + [Path(__file__).resolve()]}
        if run("compiler", compiler + ["--version"], 15).returncode != 0:
            return 1
        for name, suite in SUITES.items():
            binary = output / f"{name}_tests"
            # Test files include "Damage/..." and "Scoreboard/..." relative to Source/ValStrike.
            command = compiler + flags + ["-I", str(ROOT / "Source/ValStrike")]
            command += [str(path) for path in suite["sources"]] + ["-o", str(binary)]
            if run(f"{name}_compile", command, 180).returncode != 0:
                return 1
            completed = run(f"{name}_tests", [str(binary)], 60)
            summary = [line for line in completed.stdout.splitlines() if "cases passed" in line]
            result["suites"][name] = {"exit": completed.returncode, "summary": summary[-1] if summary else None}
            if completed.returncode != 0:
                return 1
        result["passed"] = True
        return 0
    except (OSError, subprocess.TimeoutExpired) as error:
        result["error"] = str(error)
        print(f"Damage verification failed: {error}", file=sys.stderr)
        return 1
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"Evidence: {output / 'result.json'}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
