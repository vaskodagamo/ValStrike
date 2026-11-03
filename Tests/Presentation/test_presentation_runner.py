"""Strict evidence and isolated launch tests; no Unreal process is launched."""

from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("presentation_runner", SCRIPTS / "test_rifle_presentation.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class PresentationRunnerTests(unittest.TestCase):
    def setUp(self):
        self.required = {role: ("network_role", "visual_state") for role in runner.ROLES}
        self.reports = {
            role: {"schema_version": 1, "role": role, "completed": True, "error": None,
                   "checks": [{"name": name, "passed": True, "detail": {"count": 1}}
                              for name in self.required[role]]}
            for role in runner.ROLES
        }

    def test_valid_evidence_keeps_measurements_and_failure_outcomes(self):
        self.reports["owner"]["checks"][1]["passed"] = False
        checks = runner.evaluate(self.reports, self.required)
        self.assertEqual(len(checks), 6)
        self.assertEqual(checks[3], {"check": "owner: visual_state", "passed": False, "detail": {"count": 1}})
        self.assertFalse(all(check["passed"] for check in checks))

    def test_real_contract_requires_late_attach_and_owner_menu_evidence(self):
        reports = {
            role: {"schema_version": 1, "role": role, "completed": True, "error": None,
                   "checks": [{"name": name, "passed": True, "detail": {"count": 1}}
                              for name in runner.REQUIRED_CHECKS[role]]}
            for role in runner.ROLES
        }
        self.assertEqual(len(runner.evaluate(reports)), 39)
        reports["owner"]["checks"] = [item for item in reports["owner"]["checks"] if item["name"] != "menu_visible"]
        with self.assertRaisesRegex(RuntimeError, "menu_visible"):
            runner.evaluate(reports)
        reports["owner"]["checks"].append({"name": "menu_visible", "passed": True, "detail": {}})
        reports["observer"]["checks"] = [item for item in reports["observer"]["checks"]
                                            if item["name"] != "late_attach_no_replay"]
        with self.assertRaisesRegex(RuntimeError, "late_attach_no_replay"):
            runner.evaluate(reports)

    def test_missing_or_misidentified_role_fails(self):
        self.reports["observer"] = None
        with self.assertRaisesRegex(RuntimeError, "observer report"):
            runner.evaluate(self.reports, self.required)
        del self.reports["observer"]
        with self.assertRaisesRegex(RuntimeError, "exactly"):
            runner.evaluate(self.reports, self.required)

    def test_wrong_schema_unfinished_role_or_runtime_error_fails(self):
        original = deepcopy(self.reports["server"])
        for field, value in (("role", "owner"), ("completed", False), ("completed", 1),
                             ("schema_version", True), ("schema_version", 2), ("error", "setup failed")):
            with self.subTest(field=field, value=value):
                self.reports["server"] = {**original, field: value}
                with self.assertRaises(RuntimeError):
                    runner.evaluate(self.reports, self.required)

    def test_missing_duplicate_or_non_boolean_check_fails(self):
        original = deepcopy(self.reports["owner"]["checks"])
        invalid = original[:-1] + [{**original[-1], "passed": "true"}]
        for items in ([], original[:-1], original + original[:1], invalid):
            with self.subTest(items=items):
                self.reports["owner"]["checks"] = items
                with self.assertRaises(RuntimeError):
                    runner.evaluate(self.reports, self.required)

    def test_ports_are_limited_to_assigned_presentation_range(self):
        for value in (7851, "7852", 7853, 7854):
            self.assertEqual(runner.presentation_port(value), int(value))
        for value in (7850, 7855, 7777, 0, 65536, "not-a-port"):
            with self.subTest(value=value), self.assertRaises((ValueError, runner.argparse.ArgumentTypeError)):
                runner.presentation_port(value)

    def test_one_headless_server_two_rendered_clients_and_literal_paths(self):
        output = Path("/tmp/presentation $(literal)")
        commands = {role: runner.launch_command(Path("/tmp/UE editor"), Path("/tmp/Game.uproject"),
                                                 output, role, 7851) for role in runner.ROLES}
        self.assertIn(f"/Game/Maps/VS_Graybox?game={runner.GAME_MODE}", commands["server"])
        self.assertIn("-server", commands["server"])
        self.assertIn("-nullrhi", commands["server"])
        self.assertIn("-multihome=127.0.0.1", commands["server"])
        for role in ("owner", "observer"):
            self.assertIn("-game", commands[role])
            self.assertIn("-windowed", commands[role])
            self.assertIn("-ResX=1000", commands[role])
            self.assertIn("-ResY=700", commands[role])
            self.assertNotIn("-nullrhi", commands[role])
            self.assertNotIn("-server", commands[role])
        for role in runner.ROLES:
            self.assertIn(f"-VSRiflePresentationOutput={output / (role + '.json')}", commands[role])
            self.assertIn(f"-VSRiflePresentationTest={role}", commands[role])

    def test_required_screenshots_are_hashed_and_missing_or_invalid_images_fail(self):
        required = {"server": (), "owner": ("neutral",), "observer": ("remote",)}
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            png = b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 1000, 700) + b"fixture"
            owner = output / "owner_neutral.png"
            observer = output / "observer_remote.png"
            owner.write_bytes(png)
            with self.assertRaisesRegex(RuntimeError, "Missing rendered screenshot"):
                runner.collect_screenshots(output, required)
            observer.write_bytes(b"not png")
            with self.assertRaisesRegex(RuntimeError, "Invalid or empty PNG"):
                runner.collect_screenshots(output, required)
            observer.write_bytes(png)
            captures = runner.collect_screenshots(output, required)
            self.assertEqual(len(captures), 2)
            self.assertEqual(captures["owner: neutral"]["width"], 1000)
            self.assertEqual(len(captures["observer: remote"]["sha256"]), 64)

    def test_unwrapped_execution_is_rejected_before_engine_launch(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "slot"
            path.write_text(json.dumps({"owner_pid": -1}))
            with patch.object(runner.with_ue_slot, "lock_path", return_value=path):
                with self.assertRaisesRegex(RuntimeError, "with_ue_slot.py"):
                    runner.require_engine_slot()

    def test_existing_wrapper_descriptor_is_reused_without_nested_lock(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "slot"
            path.write_text(json.dumps({"owner_pid": runner.os.getppid()}))
            with path.open() as inherited, patch.object(runner.with_ue_slot, "lock_path", return_value=path):
                self.assertEqual(runner.require_engine_slot(), inherited.fileno())

    def test_cleanup_signals_only_owned_running_process_groups(self):
        exited = Mock(pid=101)
        exited.poll.return_value = 0
        running = Mock(pid=202)
        running.poll.side_effect = [None, 0, 0]
        with patch.object(runner.os, "name", "posix"), patch.object(runner.os, "killpg") as kill:
            runner.stop_processes([exited, running])
        kill.assert_called_once_with(202, runner.signal.SIGTERM)
        exited.wait.assert_called_once_with(timeout=1)
        running.wait.assert_called_once_with(timeout=1)


if __name__ == "__main__":
    unittest.main()
