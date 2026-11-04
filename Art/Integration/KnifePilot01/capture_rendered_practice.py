#!/usr/bin/env python3
"""INT-ART-03 one-off rendered evidence capture of the existing weapons practice smoke.

Capture only, no checks of its own: the pass/fail record stays the unchanged null-RHI
`Scripts/test_weapons.py --scenario practice` run. This launches exactly that runner's
server/observer/driver commands (Scripts/test_weapons.launch_command), but with the two
clients windowed at 1000x700 instead of -nullrhi. It then grabs both client windows
repeatedly with macOS `screencapture -l` through the KNIFE-01 sequence. Each frame's
wall time is recorded so frames can be matched to the clients' LogVSKnifePresentation
cue lines.

  python3 Scripts/with_ue_slot.py -- \
    python3 Art/Integration/KnifePilot01/capture_rendered_practice.py --engine PATH
"""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "Scripts"))
import test_weapons as weapons  # noqa: E402
import ue  # noqa: E402

WINDOW_ID = r"""
import CoreGraphics
let pid = Int32(CommandLine.arguments[1])!
let list = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as! [[String: Any]]
var best = 0, area = 0.0
for w in list where (w[kCGWindowOwnerPID as String] as? Int32) == pid {
    let b = w[kCGWindowBounds as String] as! [String: Double]
    let a = (b["Width"] ?? 0) * (b["Height"] ?? 0)
    if a > area { area = a; best = w[kCGWindowNumber as String] as! Int }
}
print(best)
"""


def window_id(pid, deadline):
    helper = ROOT / "Saved/IntArt03/window_id.swift"
    helper.write_text(WINDOW_ID)
    while time.monotonic() < deadline:
        out = subprocess.run(["swift", str(helper), str(pid)], capture_output=True, text=True).stdout.strip()
        if out.isdigit() and int(out) > 0:
            return int(out)
        time.sleep(1.0)
    raise RuntimeError(f"No window for pid {pid}")


def grab(role, pid, folder, stop, frames):
    """The client replaces its startup window once loaded; re-resolve the id on failure."""
    folder.mkdir(parents=True, exist_ok=True)
    index, wid = 0, window_id(pid, time.monotonic() + 60)
    while not stop.is_set():
        path = folder / f"{role}_{index:04d}.jpg"
        stamp = time.time()
        result = subprocess.run(["screencapture", "-x", "-o", "-t", "jpg", f"-l{wid}", str(path)], capture_output=True)
        if result.returncode == 0 and path.exists() and path.stat().st_size > 0:
            frames.append({"role": role, "file": path.name, "wall_time": round(stamp, 3), "window": wid})
            index += 1
        else:
            wid = window_id(pid, time.monotonic() + 30)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine")
    parser.add_argument("--port", type=int, default=7894)
    parser.add_argument("--seconds", type=float, default=110)
    args = parser.parse_args()
    slot = weapons.require_engine_slot()
    engine = ue.engine_path(args.engine)
    executable, project = ue.editor_binary(engine, ue.host_platform()), ue.project_file(ue.ROOT)
    output = ROOT / "Saved/IntArt03/rendered_practice" / time.strftime("%Y%m%dT%H%M%S")
    output.mkdir(parents=True)
    processes, commands = {}, {}
    for role in ("server", "observer", "driver"):
        command = weapons.launch_command(executable, project, output, role, args.port, "practice")
        if role != "server":
            command.remove("-nullrhi")
            command += ["-windowed", "-ResX=1000", "-ResY=700"]
        commands[role] = command
        stream = (output / f"{role}.stdout.log").open("w")
        processes[role] = subprocess.Popen(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                           start_new_session=True, pass_fds=(slot,))
        if role == "server":
            began = time.monotonic()
            while f"listening on port {args.port}" not in ((output / "server.log").read_text(errors="replace")
                                                           if (output / "server.log").exists() else ""):
                if time.monotonic() - began > 60:
                    raise RuntimeError("server did not listen")
                time.sleep(0.2)
    stop, frames, threads = threading.Event(), [], []
    try:
        for role in ("observer", "driver"):
            thread = threading.Thread(target=grab, args=(role, processes[role].pid, output / "frames", stop, frames), daemon=True)
            thread.start()
            threads.append(thread)
        end = time.monotonic() + args.seconds
        while time.monotonic() < end and not all((output / f"{r}.json").exists() for r in ("server", "observer", "driver")):
            time.sleep(0.5)
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=5)
        weapons.stop_processes(list(processes.values()))
    reports = {r: json.loads((output / f"{r}.json").read_text()) if (output / f"{r}.json").exists() else None
               for r in ("server", "observer", "driver")}
    summary = {"purpose": "INT-ART-03 rendered evidence capture only; pass/fail is the null-RHI practice run",
               "commands": commands, "frames": frames,
               "smoke_reports_present": {r: v is not None for r, v in reports.items()}}
    (output / "capture.json").write_text(json.dumps(summary, indent=1) + "\n")
    print(f"VALSTRIKE_KNIFE_CAPTURE: {output} frames={len(frames)}")


if __name__ == "__main__":
    main()
