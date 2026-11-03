"""Damage/scoreboard runtime runner contract tests. These tests never launch Unreal."""

from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("damage_runtime", HERE / "run_runtime.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def valid_reports():
    return {
        role: {"schema_version": 1, "role": role, "completed": True, "error": None,
               "checks": [{"name": name, "passed": True, "detail": "observed"} for name in runner.REQUIRED_CHECKS[role]]}
        for role in runner.ROLES
    }


class EvaluateTests(unittest.TestCase):
    def test_complete_reports_pass(self):
        checks = runner.evaluate(valid_reports())
        self.assertEqual(len(checks), sum(len(names) for names in runner.REQUIRED_CHECKS.values()))
        self.assertTrue(all(check["passed"] for check in checks))
        self.assertIn("observer: client_debug_draw", {check["check"] for check in checks})

    def test_failed_check_is_reported_not_hidden(self):
        reports = valid_reports()
        reports["server"]["checks"][0]["passed"] = False
        checks = runner.evaluate(reports)
        self.assertFalse(all(check["passed"] for check in checks))

    def test_missing_required_check_fails(self):
        for role in runner.ROLES:
            reports = valid_reports()
            reports[role]["checks"].pop()
            with self.assertRaisesRegex(RuntimeError, "Missing required"):
                runner.evaluate(reports)

    def test_harness_error_fails(self):
        reports = valid_reports()
        reports["driver"]["error"] = "native_scenario_timeout"
        with self.assertRaisesRegex(RuntimeError, "driver harness failed"):
            runner.evaluate(reports)

    def test_identity_schema_and_shape_are_validated(self):
        mutations = [
            lambda r: r.pop("observer"),
            lambda r: r["server"].update(role="driver"),
            lambda r: r["server"].update(schema_version=2),
            lambda r: r["server"].update(schema_version=True),
            lambda r: r["server"].update(completed=1),
            lambda r: r["server"].pop("error"),
            lambda r: r["server"].update(error=3),
            lambda r: r["server"].update(checks=[]),
            lambda r: r["server"]["checks"].append(dict(r["server"]["checks"][0])),
            lambda r: r["server"]["checks"][0].update(passed="yes"),
            lambda r: r["server"]["checks"][0].update(detail={"not": "a string"}),
            lambda r: r["server"]["checks"][0].update(name=""),
        ]
        for mutate in mutations:
            reports = deepcopy(valid_reports())
            mutate(reports)
            with self.assertRaises(RuntimeError):
                runner.evaluate(reports)


class ReportLoadingTests(unittest.TestCase):
    def test_absent_is_pending_and_malformed_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "server.json"
            self.assertIsNone(runner.load_report(path))
            path.write_text('{"a": 1, "a": 2}')
            with self.assertRaisesRegex(RuntimeError, "Duplicate"):
                runner.load_report(path)
            path.write_text('{"a": NaN}')
            with self.assertRaises(RuntimeError):
                runner.load_report(path)
            path.write_text('{"schema_version": 1')
            with self.assertRaises(RuntimeError):
                runner.load_report(path)
            path.write_text(json.dumps({"ok": True}))
            self.assertEqual(runner.load_report(path), {"ok": True})


class LaunchCommandTests(unittest.TestCase):
    def setUp(self):
        self.output = Path("/tmp/vs-damage")

    def test_server_is_headless_match_mode_on_reserved_port(self):
        command = runner.launch_command("UnrealEditor", "ValStrike.uproject", self.output, "server", 7922)
        self.assertEqual(command[2], "/Game/Maps/Site01/VS_Site01?game=/Script/ValStrike.VSMatchGameMode")
        self.assertIn("-server", command)
        self.assertIn("-nullrhi", command)
        self.assertIn("-port=7922", command)
        self.assertIn("-multihome=127.0.0.1", command)
        self.assertIn("-VSDamageSmoke=server", command)
        # The server allowlist rejects -VSServer* switches; none are used.
        self.assertFalse([arg for arg in command if arg.startswith("-VSServer")])

    def test_clients_render_at_960x540_with_screenshots(self):
        for role, name in runner.NAMES.items():
            command = runner.launch_command("UnrealEditor", "ValStrike.uproject", self.output, role, 7921)
            self.assertEqual(command[2], f"127.0.0.1:7921?Name={name}")
            self.assertIn("-game", command)
            self.assertNotIn("-nullrhi", command)
            self.assertIn("-ResX=960", command)
            self.assertIn("-ResY=540", command)
            self.assertIn(f"-VSDamageShots={self.output / 'Screenshots'}", command)
            self.assertIn(f"-VSDamageOutput={self.output / (role + '.json')}", command)

    def test_headless_clients_use_null_rhi(self):
        command = runner.launch_command("UnrealEditor", "ValStrike.uproject", self.output, "driver", 7921, headless=True)
        self.assertIn("-nullrhi", command)
        self.assertNotIn("-ResX=960", command)

    def test_ports_and_roles_are_restricted(self):
        for port in (7920, 7925, "x", None):
            with self.assertRaises(Exception):
                runner.launch_command("UnrealEditor", "p", self.output, "server", port)
        with self.assertRaises(ValueError):
            runner.launch_command("UnrealEditor", "p", self.output, "late", 7921)

    def test_screenshot_expectations(self):
        paths = runner.screenshot_paths(self.output)
        self.assertIn("observer_zones_standing", paths)
        self.assertIn("observer_zones_crouched", paths)
        self.assertIn("observer_scoreboard", paths)
        self.assertIn("driver_scoreboard_final", paths)
        self.assertTrue(all(path.parent == self.output / "Screenshots" for path in paths.values()))

    def test_argument_validation(self):
        args = runner.arguments([])
        self.assertEqual(args.port, 7921)
        self.assertGreaterEqual(args.startup_timeout, 40.0)
        for bad in (["--timeout", "0"], ["--timeout", "nan"], ["--startup-timeout", "-1"], ["--port", "7831"]):
            with self.assertRaises(SystemExit):
                runner.arguments(bad)


if __name__ == "__main__":
    unittest.main()
