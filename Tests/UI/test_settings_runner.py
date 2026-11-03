#!/usr/bin/env python3
"""Engine-free checks for the rendered Settings runner's evidence parsing."""

import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_settings as runner


class IniTests(unittest.TestCase):
    def test_reads_engine_and_custom_sections(self):
        text = """;METADATA=(Diff=true, UseCommands=true)
[/Script/Engine.GameUserSettings]
FullscreenMode=1
ResolutionSizeX=1280
FullscreenMode=2

[ValStrike.UserSettings]
MouseSensitivity = 0.360
"""
        sections = runner.ini_sections(text)
        self.assertEqual(sections["/Script/Engine.GameUserSettings"]["FullscreenMode"], "2")
        self.assertEqual(sections["/Script/Engine.GameUserSettings"]["ResolutionSizeX"], "1280")
        self.assertEqual(sections["ValStrike.UserSettings"]["MouseSensitivity"], "0.360")

    def test_ignores_keys_outside_sections(self):
        self.assertEqual(runner.ini_sections("MouseSensitivity=1\n[A]\nB=2\n"), {"A": {"B": "2"}})


class LogTests(unittest.TestCase):
    def test_parses_checks_with_and_without_detail(self):
        log = ("LogValStrike: Display: VSSET_CHECK escape_opens_menu PASS \n"
               "LogValStrike: Display: VSSET_CHECK new_sensitivity_scales_look FAIL expected=25.200 yaw=8.400\n"
               "LogValStrike: Display: VSSET_DONE checks=2 failures=1\n")
        checks, done = runner.parse_checks(log, "first")
        self.assertEqual([item["check"] for item in checks],
                         ["first:escape_opens_menu", "first:new_sensitivity_scales_look"])
        self.assertEqual([item["passed"] for item in checks], [True, False])
        self.assertEqual(checks[1]["detail"], "expected=25.200 yaw=8.400")
        self.assertEqual(done, [("2", "1")])

    def test_required_check_names_are_unique(self):
        for names in (runner.FIRST_CHECKS, runner.EXCLUSIVE_CHECKS, runner.RESTART_CHECKS):
            self.assertEqual(len(names), len(set(names)))
        self.assertFalse(set(runner.FIRST_CHECKS) & set(runner.RESTART_CHECKS))


class PngTests(unittest.TestCase):
    def write(self, data):
        handle = tempfile.NamedTemporaryFile(suffix=".png", delete=False)
        handle.write(data)
        handle.close()
        self.addCleanup(Path(handle.name).unlink)
        return Path(handle.name)

    def test_accepts_png_header_with_dimensions(self):
        path = self.write(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 960, 540) + b"\0" * 8)
        info = runner.png(path)
        self.assertEqual((info["width"], info["height"]), (960, 540))

    def test_rejects_missing_empty_and_zero_sized(self):
        self.assertIsNone(runner.png(Path("/nonexistent/settings.png")))
        self.assertIsNone(runner.png(self.write(b"")))
        self.assertIsNone(runner.png(self.write(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 0, 540))))


if __name__ == "__main__":
    unittest.main()
