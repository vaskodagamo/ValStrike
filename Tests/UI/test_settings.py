#!/usr/bin/env python3
"""Run the rendered Settings screen checks across a real restart.

Two sequential Development editor game processes share an isolated -UserDir, so
the checkout's own GameUserSettings.ini is never read or written. The first run
starts from unset settings, changes sensitivity and window mode through the
menu and quits; the second run starts without window overrides and must restore
both. Local evidence only: no server, packaged client or cross-machine claims.
"""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
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

FIRST_CHECKS = (
    "fresh_settings_have_no_sensitivity", "unset_sensitivity_uses_default", "initial_window_is_windowed_960x540",
    "default_sensitivity_turns_expected_degrees", "escape_opens_menu", "menu_focuses_resume",
    "tab_reaches_settings_button", "enter_opens_settings", "settings_focuses_slider", "settings_show_default_sensitivity", "settings_show_current_window",
    "slider_right_arrow_applies_immediately", "slider_left_arrow_steps_back", "tab_reaches_sensitivity_value",
    "typed_value_clamps_to_maximum", "invalid_text_rejected_with_error", "typed_value_applies_immediately",
    "sensitivity_in_game_user_settings", "tab_reaches_window_mode", "left_selects_windowed_fullscreen",
    "window_mode_waits_for_apply", "tab_skips_fixed_desktop_resolution", "windowed_fullscreen_applied",
    "windowed_fullscreen_saved_to_game_user_settings", "shift_tab_returns_to_window_mode",
    "mouse_click_window_mode_arrow", "arrow_selects_windowed", "tab_reaches_resolution",
    "windowed_options_include_1280x720", "arrow_keys_select_1280x720", "tab_reaches_apply",
    "windowed_1280x720_applied", "windowed_saved_to_game_user_settings", "apply_reports_result",
    "mouse_click_back_button", "mouse_back_returns_to_session_page", "mouse_click_settings_button",
    "mouse_click_opens_settings", "reopened_settings_show_applied_window",
    "escape_returns_to_session_page", "back_focuses_settings_button", "escape_resumes_with_game_input",
    "new_sensitivity_scales_look", "look_ratio_matches_sensitivity_ratio",
)
EXCLUSIVE_CHECKS = ("left_selects_fullscreen", "click_apply_display", "exclusive_fullscreen_applied")
RESTART_CHECKS = (
    "restart_loads_saved_sensitivity", "restart_restores_windowed_1280x720", "restart_look_uses_saved_sensitivity",
    "restart_escape_opens_menu", "restart_tab_reaches_settings_button", "restart_enter_opens_settings",
    "restart_settings_show_saved_values", "restart_escape_twice_resumes",
)
FIRST_SCREENSHOTS = ("baseline", "menu", "settings", "settings-invalid", "settings-pending-windowed-fullscreen",
                     "windowed-fullscreen", "settings-pending-windowed", "windowed-1280x720", "after-resume")
RESTART_SCREENSHOTS = ("settings-after-restart",)


def read(path):
    return path.read_text(errors="replace") if path.exists() else ""


def png(path):
    """Return PNG dimensions and hash, or None for a missing/invalid file."""
    if not path.is_file():
        return None
    data = path.read_bytes()
    if len(data) <= 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        return None
    width, height = struct.unpack(">II", data[16:24])
    if not width or not height:
        return None
    return {"path": str(path), "width": width, "height": height, "sha256": hashlib.sha256(data).hexdigest()}


def ini_sections(text):
    """Minimal reader for Unreal's generated ini: last value wins per key."""
    sections, current = {}, None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            current = sections.setdefault(line[1:-1], {})
        elif current is not None and "=" in line and not line.startswith((";", "#")):
            key, value = line.split("=", 1)
            current[key.strip()] = value.strip()
    return sections


def parse_checks(log, phase):
    checks = [{"check": f"{phase}:{name}", "passed": status == "PASS", "detail": detail.strip()}
              for name, status, detail in re.findall(r"VSSET_CHECK ([a-z0-9_]+) (PASS|FAIL) ?([^\r\n]*)", log)]
    done = re.findall(r"VSSET_DONE checks=(\d+) failures=(\d+)", log)
    return checks, done


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", help="Unreal root; defaults to UE_ROOT")
    parser.add_argument("--timeout", type=float, default=180, help="Seconds per game process before failing")
    parser.add_argument("--quit-timeout", type=float, default=30, help="Seconds for the process to exit after Quit")
    parser.add_argument("--exclusive-fullscreen", action="store_true",
                        help="Also apply exclusive Fullscreen. On macOS this captures the display and may switch its mode briefly.")
    args = parser.parse_args(argv)
    for name in ("timeout", "quit_timeout"):
        value = getattr(args, name)
        if not math.isfinite(value) or value <= 0:
            raise ValueError(f"--{name.replace('_', '-')} must be finite and positive")
    engine = ue.engine_path(args.engine)
    executable = ue.editor_binary(engine, ue.host_platform())
    project = ue.project_file(ROOT)
    ue.map_file(ROOT)
    run_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex[:6]
    output = ROOT / "Saved/Tests/Settings" / run_id
    user_dir = output / "user"
    user_dir.mkdir(parents=True)
    commands, exits, problem = {}, {}, None

    def run(phase):
        command = [str(executable), str(project), ue.MAP, "-game", "-nosound", "-unattended",
                   f"-UserDir={user_dir}", f"-abslog={output / f'{phase}.log'}", "-ExecCmds=t.MaxFPS 60",
                   "-VSSettingsSmoke", f"-VSSettingsSmokePhase={phase}",
                   f"-VSSettingsSmokeScreenshotDir={output / 'screenshots' / phase}"]
        if phase == "first":
            # The restart deliberately omits these so persisted settings decide the window.
            command += ["-windowed", "-ResX=960", "-ResY=540"]
            if args.exclusive_fullscreen:
                command.append("-VSSettingsSmokeExclusiveFullscreen")
        commands[phase] = command
        log = output / f"{phase}.log"
        with (output / f"{phase}.stdout.log").open("w") as stream:
            process = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                       start_new_session=os.name == "posix")
            try:
                began = time.monotonic()
                while "VSSET_DONE " not in read(log):
                    if process.poll() is not None:
                        raise RuntimeError(f"{phase} process exited before its settings checks completed")
                    if time.monotonic() - began > args.timeout:
                        raise RuntimeError(f"{phase} process timed out before its settings checks completed")
                    time.sleep(0.2)
                try:
                    exits[phase] = process.wait(timeout=args.quit_timeout)
                except subprocess.TimeoutExpired:
                    raise RuntimeError(f"{phase} process did not exit within {args.quit_timeout:g} s after Quit")
            finally:
                stop([process])

    stored = {}
    try:
        print(f"Settings smoke running; evidence: {output}", flush=True)
        run("first")
        inis = sorted(user_dir.glob("Saved/Config/*/GameUserSettings.ini"))
        if len(inis) != 1:
            raise RuntimeError(f"Expected one GameUserSettings.ini under the isolated user dir, found {len(inis)}")
        text = inis[0].read_text(errors="replace")
        (output / "GameUserSettings.after-first.ini").write_text(text)
        stored = {"path": str(inis[0]), "sections": ini_sections(text)}
        run("restart")
    except (RuntimeError, OSError) as error:
        problem = str(error)

    checks, reports = [], {}
    for phase, required in (("first", FIRST_CHECKS + (EXCLUSIVE_CHECKS if args.exclusive_fullscreen else ())),
                            ("restart", RESTART_CHECKS)):
        log = read(output / f"{phase}.log")
        phase_checks, done = parse_checks(log, phase)
        names = [item["check"].split(":", 1)[1] for item in phase_checks]
        checks += phase_checks
        checks.append({"check": f"{phase}:required_checks_recorded_once",
                       "passed": set(required) <= set(names) and len(names) == len(set(names)),
                       "missing": sorted(set(required) - set(names))})
        checks.append({"check": f"{phase}:runtime_report_complete",
                       "passed": len(done) == 1 and int(done[0][0]) == len(names) and int(done[0][1]) == 0})
        checks.append({"check": f"{phase}:quit_exits_process", "passed": exits.get(phase) == 0, "exit_code": exits.get(phase)})
        reports[phase] = {
            "windows": [json.loads(raw) for raw in re.findall(r"VSSET_WINDOW ([^\r\n]+)", log)],
            "look": [json.loads(raw) for raw in re.findall(r"VSSET_LOOK ([^\r\n]+)", log)],
            "transitions": [json.loads(raw) for raw in re.findall(r"VSSET_TRANSITION ([^\r\n]+)", log)],
            "clicks": re.findall(r"VSSET_CLICK ([^\r\n]+)", log),
        }

    sections = stored.get("sections", {})
    ours = sections.get("ValStrike.UserSettings", {})
    engine_settings = sections.get("/Script/Engine.GameUserSettings", {})
    checks.append({"check": "disk:sensitivity_saved", "passed": ours.get("MouseSensitivity") == "0.360",
                   "value": ours.get("MouseSensitivity")})
    checks.append({"check": "disk:windowed_1280x720_saved", "passed": engine_settings.get("FullscreenMode") == "2"
                   and engine_settings.get("ResolutionSizeX") == "1280" and engine_settings.get("ResolutionSizeY") == "720",
                   "values": {key: engine_settings.get(key) for key in ("FullscreenMode", "ResolutionSizeX", "ResolutionSizeY")}})
    checks.append({"check": "disk:engine_settings_version_saved", "passed": bool(engine_settings.get("Version")),
                   "value": engine_settings.get("Version")})

    screenshots = {}
    for phase, names in (("first", FIRST_SCREENSHOTS + (("fullscreen",) if args.exclusive_fullscreen else ())),
                         ("restart", RESTART_SCREENSHOTS)):
        for name in names:
            info = png(output / "screenshots" / phase / f"{name}.png")
            screenshots[f"{phase}/{name}"] = info
            checks.append({"check": f"screenshot:{phase}/{name}", "passed": info is not None})

    passed = not problem and all(item["passed"] for item in checks)
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    dirty = bool(subprocess.run(["git", "status", "--porcelain", "--", "Source", "Tests/UI"],
                                cwd=ROOT, capture_output=True, text=True).stdout.strip())
    hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in sorted((ROOT / "Source/ValStrike").rglob("*")) if path.is_file()}
    result = {"passed": passed, "error": problem, "revision": revision, "dirty_source_checkout": dirty,
              "engine": str(engine), "engine_build": json.loads((engine / "Build/Build.version").read_text()),
              "conditions": "Two sequential local Development editor game processes, rendered, no audio, isolated -UserDir; "
                            + ("exclusive Fullscreen exercised" if args.exclusive_fullscreen else "exclusive Fullscreen not exercised"),
              "commands": commands, "checks": checks, "stored_settings": stored, "reports": reports,
              "screenshots": screenshots, "source_sha256": hashes}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    for item in checks:
        detail = item.get("detail") or ""
        print(("PASS " if item["passed"] else "FAIL ") + item["check"] + (f": {detail}" if detail else ""))
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
