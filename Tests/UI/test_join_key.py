#!/usr/bin/env python3
"""Rendered join-key check against a real keyed server. Development editor evidence only.

Starts `ue.py server --join-key` on loopback, then a rendered client whose menu is
driven by -VSJoinSmoke: an invalid key and a key in the address are refused before
travel, a keyless join is refused by the server with "Join key missing or wrong",
and a join with the key typed into the masked field connects.

The key reaches the client through VSJOINSMOKE_KEY, never its command line. After
the run every text artifact is scanned for the key. A hit in a line written by
ValStrike's own code, the runner output or result.json fails the run. Hits in
Unreal's own log lines (the engine logs travel URLs, login options and the server
command line) are counted per file and kind, then redacted so no artifact keeps
the key. Screenshots show the field masked; inspect them visually.
"""

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import ue
from test_movement import stop

KEY = "lan-Key_0001"  # The coordinator's documented test key; a test value only.
REDACTED = "<join-key-redacted>"
OWN_PREFIXES = ("VSJOIN_", "VSUI_", "VSSET_", "VSSERVER")
REQUIRED = (
    "initial_world_is_standalone", "key_supplied_out_of_band", "address_supplied", "escape_opens_menu",
    "shift_tab_reaches_address", "address_entry_updates_session", "tab_reaches_join_key",
    "invalid_key_refused_before_travel", "rejected_enter_keeps_key_focus", "key_in_address_refused",
    "key_field_cleared", "keyless_attempt_starts_connection", "keyless_join_refused_by_server",
    "refusal_keeps_menu_open", "address_survives_refusal", "shift_tab_reaches_join_key",
    "typed_key_is_kept_in_memory", "keyed_attempt_starts_connection", "keyed_join_connects",
    "connected_menu_opens", "connected_menu_keeps_key_masked_in_field",
)
SCREENSHOTS = ("menu", "invalid-key", "refused", "key-entered", "connected-menu")


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def png(path):
    if not path.is_file():
        return None
    data = path.read_bytes()
    if len(data) <= 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", data[16:24])
    return {"path": str(path), "width": width, "height": height} if width and height else None


def redact(value, key):
    """Recursively replace the key in strings, lists and dicts (for commands in result.json)."""
    if isinstance(value, str):
        return value.replace(key, REDACTED)
    if isinstance(value, list):
        return [redact(item, key) for item in value]
    if isinstance(value, dict):
        return {name: redact(item, key) for name, item in value.items()}
    return value


def line_kind(line, key):
    """Short, key-free description of where a log line came from."""
    text = re.sub(r"^\[[^\]]*\]\[[^\]]*\]", "", line).strip()
    match = re.match(r"([A-Za-z0-9_]+: (?:[A-Za-z]+: )?[A-Za-z0-9_ ]{0,24})", text)
    return (match.group(1) if match else text[:32]).replace(key, REDACTED).strip()


def scan_and_redact(directory, key):
    """Count key occurrences per text file and line kind; return them, then redact the files."""
    findings, own = [], []
    for path in sorted(directory.rglob("*")):
        if not path.is_file() or path.suffix.lower() in (".png", ".bin") or path.stat().st_size > 50_000_000:
            continue
        text = path.read_text(errors="replace")
        if key not in text:
            continue
        kinds = Counter()
        for line in text.splitlines():
            if key in line:
                kind = line_kind(line, key)
                kinds[kind] += line.count(key)
                if any(prefix in line for prefix in OWN_PREFIXES):
                    own.append({"file": str(path.relative_to(directory)), "kind": kind})
        findings.append({"file": str(path.relative_to(directory)), "occurrences": text.count(key), "kinds": dict(kinds)})
        path.write_text(text.replace(key, REDACTED))
    return findings, own


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--port", type=ue.port, default=7883)
    parser.add_argument("--timeout", type=float, default=150)
    parser.add_argument("--quit-timeout", type=float, default=30)
    args = parser.parse_args(argv)
    engine = ue.engine_path(args.engine)
    host = ue.host_platform()
    executable = ue.editor_binary(engine, host)
    project = ue.project_file(ROOT)
    ue.map_file(ROOT)
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/JoinKey" / run_id
    output.mkdir(parents=True)
    paths = {"server": output / "server.log", "client": output / "client.log"}

    # Exactly the launcher's server command shape, with the log moved into this run.
    server_command = ue.command(ue.parser().parse_args(["server", "--port", str(args.port), "--join-key", KEY]),
                                engine, ROOT, host)
    server_command = [f"-abslog={paths['server']}" if item.startswith("-abslog=") else item for item in server_command]
    client_command = [str(executable), str(project), ue.MAP, "-game", "-windowed", "-ResX=960", "-ResY=540",
                      "-nosound", "-unattended", f"-abslog={paths['client']}", "-ExecCmds=t.MaxFPS 60",
                      "-VSJoinSmoke", f"-VSJoinSmokeAddress=127.0.0.1:{args.port}",
                      f"-VSJoinSmokeScreenshotDir={output / 'screenshots'}"]
    if any(KEY in item for item in client_command):
        raise ValueError("The key must never be on the client command line")
    processes, streams, problem, client_exit = [], [], None, None

    def launch(role, command, env=None):
        stream = (output / f"{role}.stdout.log").open("w")
        streams.append(stream)
        process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, env=env,
                                   start_new_session=os.name == "posix")
        processes.append(process)
        return process

    try:
        began = time.monotonic()
        server = launch("server", server_command)
        while f"listening on port {args.port}" not in read(paths["server"]):
            if server.poll() is not None or time.monotonic() - began > 60:
                raise RuntimeError("Keyed server did not reach listening state")
            time.sleep(0.2)
        client = launch("client", client_command, env={**os.environ, "VSJOINSMOKE_KEY": KEY})
        print(f"Join-key smoke running; evidence: {output}", flush=True)
        while "VSJOIN_DONE " not in read(paths["client"]):
            if client.poll() is not None or server.poll() is not None:
                raise RuntimeError("A process exited before the join-key checks completed")
            if time.monotonic() - began > args.timeout:
                raise RuntimeError("Timed out before the join-key checks completed")
            time.sleep(0.2)
        try:
            client_exit = client.wait(timeout=args.quit_timeout)
        except subprocess.TimeoutExpired:
            raise RuntimeError(f"Quit did not end the client within {args.quit_timeout:g} s")
    except (RuntimeError, OSError) as error:
        problem = str(error)
    finally:
        stop(processes)
        for stream in streams:
            stream.close()

    log = read(paths["client"])
    server_log = read(paths["server"])
    checks = [{"check": name, "passed": status == "PASS", "detail": detail.strip()}
              for name, status, detail in re.findall(r"VSJOIN_CHECK ([a-z0-9_]+) (PASS|FAIL) ?([^\r\n]*)", log)]
    names = [item["check"] for item in checks]
    done = re.findall(r"VSJOIN_DONE checks=(\d+) failures=(\d+)", log)
    checks.append({"check": "required_checks_recorded_once", "passed": set(REQUIRED) <= set(names) and len(names) == len(set(names)),
                   "missing": sorted(set(REQUIRED) - set(names))})
    checks.append({"check": "runtime_report_complete",
                   "passed": len(done) == 1 and int(done[0][0]) == len(names) and int(done[0][1]) == 0})
    checks.append({"check": "quit_exits_client", "passed": client_exit == 0, "exit_code": client_exit})
    checks.append({"check": "server_refused_keyless_join", "passed": "Join key missing or wrong" in server_log
                   or "Join key missing or wrong" in log})
    for name in SCREENSHOTS:
        info = png(output / "screenshots" / f"{name}.png")
        checks.append({"check": f"screenshot_{name}", "passed": info is not None, "path": info and info["path"]})

    findings, own = scan_and_redact(output, KEY)
    checks.append({"check": "no_key_in_valstrike_log_lines", "passed": not own, "hits": own})
    engine_hits = sum(item["occurrences"] for item in findings)
    passed = not problem and all(item["passed"] for item in checks)
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain", "--", "Source", "Tests/UI"],
                                cwd=ROOT, capture_output=True, text=True).stdout.strip())
    result = {"passed": passed, "error": problem, "revision": revision, "dirty_source_checkout": dirty,
              "engine": str(engine), "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": "Local editor server (ue.py server --join-key) and rendered 960x540 editor client on loopback; "
                            "key passed to the client via environment only",
              "commands": redact({"server": server_command, "client": client_command}, KEY),
              "checks": checks,
              "key_scan": {"note": "Occurrences found before redaction. Unreal's own log lines include travel URLs, "
                                   "login options and the server command line; the files were redacted afterwards.",
                           "files": findings, "total_occurrences": engine_hits}}
    text = json.dumps(result, indent=2) + "\n"
    if KEY in text:
        raise AssertionError("result.json would contain the key")
    (output / "result.json").write_text(text)
    for item in checks:
        detail = item.get("detail") or ""
        print(("PASS " if item["passed"] else "FAIL ") + item["check"] + (f": {detail}" if detail else ""))
    print(f"Key occurrences written by Unreal itself (now redacted): {engine_hits} in {len(findings)} file(s)")
    for item in findings:
        print(f"  {item['file']}: {item['kinds']}")
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
