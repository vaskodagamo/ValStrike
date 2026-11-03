"""Tests for strict rendered look evidence; these do not execute Unreal."""

from copy import deepcopy
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("look_runner", SCRIPTS / "test_combat_look.py")
look = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(look)


class CombatLookRunnerTests(unittest.TestCase):
    def setUp(self):
        self.report = {"schema_version": 1, "completed": True, "error": None,
                       "checks": [{"name": name, "passed": True, "detail": {"delta": 2.0}}
                                  for name in look.REQUIRED_CHECKS]}

    def test_complete_evidence_preserves_measurements_and_failures(self):
        self.report["checks"][2]["passed"] = False
        checks = look.evaluate(self.report)
        self.assertEqual(len(checks), 8)
        self.assertEqual(checks[2], {"check": "up", "passed": False, "detail": {"delta": 2.0}})
        self.assertFalse(all(check["passed"] for check in checks))

    def test_missing_duplicate_and_mistyped_checks_fail_closed(self):
        original = deepcopy(self.report["checks"])
        invalid = original[:-1] + [{**original[-1], "passed": "true"}]
        for items in ([], original[:-1], original + original[:1], invalid):
            with self.subTest(items=items):
                self.report["checks"] = items
                with self.assertRaises(RuntimeError):
                    look.evaluate(self.report)

    def test_missing_incomplete_wrong_schema_and_error_reports_fail(self):
        for report in (None, {}, {**self.report, "completed": 1},
                       {**self.report, "schema_version": True},
                       {**self.report, "error": "viewport missing"}):
            with self.subTest(report=report), self.assertRaises(RuntimeError):
                look.evaluate(report)

    def test_all_eight_rendered_pngs_are_required_and_hashed(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "look.json"
            png = b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 1000, 700) + b"fixture"
            for name in look.REQUIRED_CHECKS:
                path.with_name(f"look_{name}.png").write_bytes(png)
            captures = look.screenshots(path)
            self.assertEqual(len(captures), 8)
            self.assertEqual(captures["up"]["width"], 1000)
            self.assertEqual(captures["up"]["height"], 700)
            self.assertEqual(len(captures["up"]["sha256"]), 64)
            path.with_name("look_focus_return.png").unlink()
            with self.assertRaisesRegex(RuntimeError, "Missing rendered screenshot"):
                look.screenshots(path)

    def test_empty_or_non_png_capture_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "look.json"
            screenshot = path.with_name("look_right.png")
            for data in (b"", b"not a PNG" * 10):
                screenshot.write_bytes(data)
                with self.subTest(data=data), self.assertRaisesRegex(RuntimeError, "Invalid or empty PNG"):
                    look.screenshots(path)

    def test_launch_is_rendered_standalone_and_paths_are_literal(self):
        output = Path("/tmp/look screenshots $(literal)")
        command = look.launch_command(Path("/tmp/UE editor"), Path("/tmp/Game.uproject"), output)
        self.assertIn("-game", command)
        self.assertIn("/Game/Maps/VS_Graybox", command)
        self.assertIn("-windowed", command)
        self.assertIn("-ResX=1000", command)
        self.assertIn("-ResY=700", command)
        self.assertIn(f"-VSCombatLookOutput={output / 'look.json'}", command)
        self.assertNotIn("-nullrhi", command)
        self.assertNotIn("-server", command)
        self.assertFalse(any("127.0.0.1" in arg for arg in command))


if __name__ == "__main__":
    unittest.main()
