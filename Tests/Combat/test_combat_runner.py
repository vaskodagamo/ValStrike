"""Combat smoke evidence validation tests; these do not execute Unreal."""

from copy import deepcopy
import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("combat_runner", SCRIPTS / "test_combat.py")
combat = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(combat)


class CombatRunnerTests(unittest.TestCase):
    def setUp(self):
        self.required = {role: ("network_role", "replicated_state") for role in combat.ROLES}
        self.reports = {
            role: {"schema_version": 1, "role": role, "completed": True, "error": None,
                   "checks": [{"name": name, "passed": True, "detail": {"observed": 30}}
                              for name in self.required[role]]}
            for role in combat.ROLES
        }

    def evaluate(self):
        return combat.evaluate(self.reports, self.required)

    def test_complete_evidence_preserves_role_and_measured_detail(self):
        result = self.evaluate()
        self.assertEqual(len(result), 6)
        self.assertEqual(result[0], {"check": "server: network_role", "passed": True,
                                     "detail": {"observed": 30}})

    def test_missing_role_and_absent_report_fail_closed(self):
        self.reports["observer"] = None
        with self.assertRaisesRegex(RuntimeError, "observer report"):
            self.evaluate()
        del self.reports["observer"]
        with self.assertRaisesRegex(RuntimeError, "exactly"):
            self.evaluate()

    def test_wrong_identity_incomplete_and_boolean_schema_fail(self):
        original = deepcopy(self.reports["driver"])
        for field, value in (("role", "server"), ("completed", False), ("completed", 1),
                             ("schema_version", True), ("schema_version", 2)):
            with self.subTest(field=field, value=value):
                self.reports["driver"] = {**original, field: value}
                with self.assertRaisesRegex(RuntimeError, "identity, schema or completion"):
                    self.evaluate()

    def test_empty_duplicate_or_missing_mandatory_checks_fail(self):
        original = deepcopy(self.reports["server"]["checks"])
        for items in ([], original[:1], original + original[:1]):
            with self.subTest(items=items):
                self.reports["server"]["checks"] = items
                with self.assertRaises(RuntimeError):
                    self.evaluate()

    def test_false_is_preserved_and_truthy_non_booleans_rejected(self):
        self.reports["server"]["checks"][0]["passed"] = False
        self.assertFalse(self.evaluate()[0]["passed"])
        for value in ("true", 1, [], None):
            self.reports["server"]["checks"][0]["passed"] = value
            with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, "Invalid or duplicate"):
                self.evaluate()

    def test_runtime_error_rejects_otherwise_passing_reports(self):
        self.reports["observer"]["error"] = "two_players_not_ready"
        with self.assertRaisesRegex(RuntimeError, "two_players_not_ready"):
            self.evaluate()

    def test_additional_checks_are_preserved_and_can_fail_run(self):
        self.reports["driver"]["checks"].append({"name": "extra", "passed": False, "detail": "failure"})
        result = self.evaluate()
        self.assertEqual(len(result), 7)
        self.assertFalse(all(check["passed"] for check in result))

    def test_absent_malformed_and_nonfinite_files_are_distinct(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "server.json"
            self.assertIsNone(combat.load_report(path))
            for content in ('{"unfinished":', '{"number":NaN}', '{"number":Infinity}'):
                path.write_text(content)
                with self.subTest(content=content), self.assertRaisesRegex(RuntimeError, "Malformed"):
                    combat.load_report(path)
            path.write_text(json.dumps(self.reports["server"]))
            self.assertEqual(combat.load_report(path), self.reports["server"])

    def test_process_arguments_keep_paths_literal_and_reports_distinct(self):
        output = Path("/tmp/project $(literal)/combat results")
        commands = {role: combat.launch_command(Path("/tmp/UE editor"), Path("/tmp/Game.uproject"),
                                                output, role, 7793) for role in combat.ROLES}
        for role, command in commands.items():
            self.assertIn(f"-VSCombatOutput={output / (role + '.json')}", command)
            self.assertIn(f"-VSCombatSmoke={role}", command)
            self.assertIn("-nullrhi", command)
        self.assertIn("-multihome=127.0.0.1", commands["server"])
        self.assertIn("127.0.0.1:7793?Name=VSCombatDriver", commands["driver"])
        self.assertIn("127.0.0.1:7793?Name=VSCombatObserver", commands["observer"])
        self.assertNotIn("-server", commands["driver"])

    def test_cleanup_only_signals_running_handles_owned_by_runner(self):
        exited = Mock(pid=101)
        exited.poll.return_value = 0
        running = Mock(pid=202)
        running.poll.return_value = None
        with patch.object(combat.os, "name", "posix"), patch.object(combat.os, "killpg") as kill:
            combat.stop([exited, running])
        kill.assert_called_once_with(202, combat.signal.SIGTERM)
        exited.wait.assert_called_once_with(timeout=8)
        running.wait.assert_called_once_with(timeout=8)

    def test_unavailable_git_status_is_unknown_instead_of_clean(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            results = [combat.subprocess.CompletedProcess([], 0, "abc123\n", ""),
                       combat.subprocess.CompletedProcess([], 128, "", "LFS unavailable")]
            with patch.object(combat.subprocess, "run", side_effect=results):
                metadata = combat.evidence_metadata(root)
            self.assertEqual(metadata["revision"], "abc123")
            self.assertIsNone(metadata["dirty_checkout"])
            self.assertEqual(metadata["git_status_error"], "LFS unavailable")

    def test_required_runtime_checks_include_reload_interruption_for_each_role(self):
        self.assertEqual(sum(len(names) for names in combat.REQUIRED_CHECKS.values()), 55)
        for role in combat.ROLES:
            self.assertIn("reload_interrupted", combat.REQUIRED_CHECKS[role])

    def test_metadata_hashes_actual_mac_module_library(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            module = root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib"
            module.parent.mkdir(parents=True)
            module.write_bytes(b"fixture module")
            results = [combat.subprocess.CompletedProcess([], 0, "abc123\n", ""),
                       combat.subprocess.CompletedProcess([], 0, "", "")]
            with patch.object(combat.subprocess, "run", side_effect=results):
                metadata = combat.evidence_metadata(root)
            self.assertEqual(metadata["editor_module_sha256"], hashlib.sha256(b"fixture module").hexdigest())


if __name__ == "__main__":
    unittest.main()
