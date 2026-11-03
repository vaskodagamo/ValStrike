#!/usr/bin/env python3
"""Compile/run the standalone round rules, retaining commands and source hashes.

No Unreal launch, dependencies, exceptions, or RTTI. Outputs stay in this checkout.
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
SOURCES = [
    ROOT / "Source/ValStrike/Rules/VSMatchRules.cpp",
    ROOT / "Source/ValStrike/Rules/VSMatchRules.h",
    ROOT / "Tests/Rules/VSMatchRulesTests.cpp",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true", help="enable address/undefined-behavior sanitizers")
    parser.add_argument("--compiler", help="compiler executable; defaults to xcrun clang++ on macOS, c++ elsewhere")
    args = parser.parse_args()
    compiler = [args.compiler] if args.compiler else (["xcrun", "clang++"] if sys.platform == "darwin" else ["c++"])
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/RoundRules" / run_id
    output.mkdir(parents=True)
    binary = output / "round_rules_tests"
    flags = ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-fno-exceptions", "-fno-rtti", "-O1", "-g"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    command = compiler + flags + ["-I", str(ROOT / "Source/ValStrike"), str(SOURCES[0]), str(SOURCES[2]), "-o", str(binary)]
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
                                   for path in SOURCES + [Path(__file__).resolve()]}
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
        print(f"Round rules verification failed: {error}", file=sys.stderr)
        return 1
    finally:
        (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
        print(f"Evidence: {output / 'result.json'}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
