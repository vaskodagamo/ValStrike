#!/usr/bin/env python3
"""Capture explicitly labeled, development-only round HUD fixtures in Unreal.

Run the entire foreground runner through Scripts/with_ue_slot.py. This exercises
presentation and pawn/menu lifetime only; it is not match replication evidence.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import struct
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "Scripts"))
import ue
from test_movement import stop

FIXTURES = (
    "unavailable", "waiting", "preparation", "active_carrier", "planting",
    "planted_defusing", "dropped", "placement_pending", "resolved", "role_swap",
    "round_draw", "complete_win", "complete_draw", "long_status", "spectator_no_pawn",
    "unavailable_no_pawn", "menu_open", "resolved_defused",
)
MAPS = {"graybox": "/Game/Maps/VS_Graybox", "site01": "/Game/Maps/Site01/VS_Site01"}


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes():
    paths = sorted(path for path in (ROOT / "Source").rglob("*")
                   if path.is_file() and path.suffix in {".cpp", ".h", ".cs"})
    paths.append(Path(__file__).resolve())
    return {str(path.relative_to(ROOT)): sha256(path) for path in paths}


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def png_size(path):
    try:
        with path.open("rb") as stream:
            header = stream.read(24)
        if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
            return None
        return list(struct.unpack(">II", header[16:24]))
    except OSError:
        return None


def capture(args, engine, executable, module, resolution, output):
    width, height = (int(part) for part in resolution.split("x"))
    output.mkdir(parents=True)
    log = output / "preview.log"
    screenshots = output / "screenshots"
    screenshots.mkdir()
    command = [str(executable), str(ue.project_file(ROOT)), MAPS[args.map], "-game",
               "-windowed", f"-ResX={width}", f"-ResY={height}", "-nosound", "-unattended",
               "-VSRoundHUDPreview", f"-VSRoundHUDPreviewDir={screenshots}",
               f"-abslog={log}", "-ExecCmds=t.MaxFPS 30"]
    before_source = source_hashes()
    before_module = sha256(module)
    process, problem, exit_code, interrupted = None, None, None, False
    try:
        with (output / "stdout.log").open("w") as stream:
            process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                       start_new_session=os.name == "posix")
            print(f"Round HUD presentation preview {resolution}: {output}", flush=True)
            began = time.monotonic()
            while "VSROUND_PREVIEW_DONE " not in read(log):
                if process.poll() is not None:
                    raise RuntimeError("Client exited before the fixture sequence completed")
                if time.monotonic() - began > args.timeout:
                    raise RuntimeError("Round HUD preview timed out")
                time.sleep(0.25)
            try:
                exit_code = process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                raise RuntimeError("Client did not terminate within 30 seconds after Quit")
    except KeyboardInterrupt:
        interrupted = True
        problem = "Preview interrupted; owned client cleanup requested"
    except (RuntimeError, OSError) as error:
        problem = str(error)
    finally:
        stop([process] if process else [])

    text = read(log)
    checks = [{"check": name, "passed": status == "PASS"}
              for name, status in re.findall(r"VSROUND_PREVIEW_CHECK ([a-z_]+) (PASS|FAIL)", text)]
    native_count = len(checks)
    done = re.findall(r"VSROUND_PREVIEW_DONE checks=(\d+) failures=(\d+) fixtures=(\d+)", text)
    native_names = {item["check"] for item in checks}
    required_checks = {"practice_provider_is_unavailable", "preview_starts_with_real_pawn",
                       "menu_open_without_pawn", "menu_does_not_pause_preview_world"}
    required_checks.update(f"{name}_pawn_lifetime" for name in FIXTURES)
    checks.append({"check": "native_preview_checks_complete_once", "passed": native_names == required_checks
                   and native_count == len(required_checks)})
    checks.append({"check": "fixture_sequence_completed", "passed": len(done) == 1
                   and tuple(map(int, done[0])) == (native_count, 0, len(FIXTURES))})
    checks.append({"check": "client_exited_cleanly", "passed": exit_code == 0, "exit_code": exit_code})
    states, state_errors = [], []
    for raw in re.findall(r"VSROUND_PREVIEW_STATE ([^\r\n]+)", text):
        try:
            state = json.loads(raw)
            if not isinstance(state, dict) or not {"fixture", "available", "has_pawn", "phase", "menu"} <= state.keys():
                raise ValueError("Missing fixture-state fields")
            states.append(state)
        except (ValueError, TypeError) as error:
            state_errors.append(str(error))
    checks.append({"check": "fixture_inputs_recorded_in_order", "passed": not state_errors
                   and [state["fixture"] for state in states] == list(FIXTURES)})
    for name in FIXTURES:
        path = screenshots / f"{name}.png"
        size = png_size(path)
        checks.append({"check": f"native_png_{name}", "passed": size == [width, height],
                       "path": str(path), "pixel_size": size})
    checks.append({"check": "source_unchanged_during_capture", "passed": before_source == source_hashes()})
    checks.append({"check": "module_unchanged_during_capture", "passed": before_module == sha256(module)})
    passed = not problem and all(item["passed"] for item in checks)
    result = {
        "passed": passed, "error": problem, "interrupted": interrupted,
        "conditions": "Explicitly labeled local presentation fixtures, rendered standalone development editor; no replicated match, no networking, no audio. Visual acceptance requires screenshot inspection.",
        "command": command, "engine_build": json.loads((engine / "Build/Build.version").read_text()),
        "source_sha256": before_source, "module_path": str(module), "module_sha256": before_module,
        "fixture_inputs": states, "state_errors": state_errors, "checks": checks,
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for item in checks:
        print(("PASS " if item["passed"] else "FAIL ") + item["check"], flush=True)
    if problem:
        print(problem, file=sys.stderr, flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="Unreal 5.8.4 root; defaults to UE_ROOT")
    parser.add_argument("--resolutions", nargs="+", choices=("960x540", "1280x720"),
                        default=["960x540", "1280x720"])
    parser.add_argument("--map", choices=tuple(MAPS), default="graybox")
    parser.add_argument("--timeout", type=float, default=100,
                        help="Maximum seconds per fixture sequence before cleanup")
    args = parser.parse_args()
    engine = ue.engine_path(args.engine)
    host = ue.host_platform()
    if host != "Mac":
        raise ue.SetupError("This preview runner currently records the verified Mac editor module layout only.")
    executable = ue.editor_binary(engine, host)
    module = ue.require_file(ROOT / "Binaries/Mac/libUnrealEditor-ValStrike.dylib",
                             "Build this checkout's Editor target through the engine-slot wrapper first.")
    package = ROOT / "Content" / (MAPS[args.map].removeprefix("/Game/") + ".umap")
    ue.require_file(package, "Retrieve the selected committed map using Git LFS.")
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/UI/Round" / run_id
    output.mkdir(parents=True)

    def request_stop(_signum, _frame):
        raise KeyboardInterrupt

    previous = {sig: signal.signal(sig, request_stop) for sig in (signal.SIGINT, signal.SIGTERM)}
    results = []
    try:
        for resolution in dict.fromkeys(args.resolutions):
            result = capture(args, engine, executable, module, resolution, output / resolution)
            results.append({"resolution": resolution, "passed": result["passed"], "error": result["error"],
                            "report": str(output / resolution / "result.json")})
            if result["interrupted"]:
                break
    finally:
        for sig, handler in previous.items():
            signal.signal(sig, handler)
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.strip()
    dirty = subprocess.run(["git", "status", "--porcelain", "--", "Source", "Tests/UI/Round"],
                           cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
    passed = len(results) == len(set(args.resolutions)) and all(result["passed"] for result in results)
    summary = {"passed": passed, "revision": revision, "dirty_source_checkout": bool(dirty),
               "results": results, "preview_only": True}
    (output / "result.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Round HUD preview: {'PASS' if passed else 'FAIL'}; {output / 'result.json'}", flush=True)
    return 0 if passed else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ue.SetupError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
