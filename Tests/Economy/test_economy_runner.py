"""Economy runner contract tests. These never launch Unreal or a compiler."""

from contextlib import redirect_stderr
from copy import deepcopy
import importlib.util
import io
from pathlib import Path
import unittest


SCRIPT = Path(__file__).resolve().parents[2] / "Scripts" / "test_economy.py"
SPEC = importlib.util.spec_from_file_location("economy_runner", SCRIPT)
economy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(economy)


def ledger(offset=0):
    values = {}
    for index in range(1, 8):
        values[f"open_{index}"] = 800 + offset + index
        values[f"owned_{index}"] = 2
    for index in range(1, 7):
        values[f"close_{index}"] = 700 + offset + index
        values[f"result_{index}"] = 900 + offset + index
    return values


def valid_reports(rendered=False):
    required = economy.required_checks(rendered)
    reports = {}
    for role in economy.ROLES:
        reports[role] = {"schema_version": 1, "role": role, "completed": True, "error": None,
                         "checks": [{"name": name, "passed": True, "detail": "observed"} for name in required[role]]}
    reports["server"]["ledger"] = {"1": ledger(), "2": ledger(1000)}
    reports["alpha"].update(player_id=1, checkpoints=ledger())
    reports["bravo"].update(player_id=2, checkpoints=ledger(1000))
    return reports


class EconomyReportTests(unittest.TestCase):
    def test_valid_reports_include_replication_cross_checks(self):
        checks = economy.evaluate(valid_reports(), economy.required_checks(False))
        names = [check["check"] for check in checks]
        self.assertIn("server: rewards_match_ledger", names)
        self.assertIn("alpha: replicated_checkpoints_match_server", names)
        self.assertIn("bravo: replicated_checkpoints_match_server", names)
        self.assertTrue(all(check["passed"] for check in checks))
        replicated = [check for check in checks if check["check"].endswith("replicated_checkpoints_match_server")]
        self.assertEqual(replicated[0]["detail"]["compared"], 26)

    def test_rendered_runs_require_screenshot_and_keyboard_evidence(self):
        reports = valid_reports(False)
        with self.assertRaisesRegex(RuntimeError, "screenshots_written"):
            economy.evaluate(reports, economy.required_checks(True))
        economy.evaluate(valid_reports(True), economy.required_checks(True))
        self.assertNotIn("keyboard_purchase", economy.required_checks(True)["bravo"])

    def test_replicated_value_mismatch_or_gap_fails_the_cross_check(self):
        reports = valid_reports()
        reports["bravo"]["checkpoints"]["result_3"] += 200
        del reports["alpha"]["checkpoints"]["close_2"]
        checks = {check["check"]: check for check in economy.evaluate(reports, economy.required_checks(False))}
        self.assertFalse(checks["bravo: replicated_checkpoints_match_server"]["passed"])
        self.assertIn("result_3: client 2103 != server 1903", checks["bravo: replicated_checkpoints_match_server"]["detail"]["mismatches"])
        self.assertFalse(checks["alpha: replicated_checkpoints_match_server"]["passed"])

    def test_identity_and_ledger_problems_are_errors(self):
        for mutate, message in (
            (lambda r: r["server"].pop("ledger"), "authoritative ledger"),
            (lambda r: r["alpha"].update(player_id=True), "player id"),
            (lambda r: r["alpha"].update(player_id=9), "lacks player 9"),
            (lambda r: r["bravo"].update(player_id=1), "same player id"),
            (lambda r: r["server"].update(completed=False), "identity, schema or completion"),
            (lambda r: r["server"].update(error="native_scenario_timeout"), "native_scenario_timeout"),
            (lambda r: r["alpha"]["checks"].append(r["alpha"]["checks"][0]), "duplicate"),
            (lambda r: r.pop("bravo"), "exactly"),
        ):
            reports = valid_reports()
            mutate(reports)
            with self.subTest(message=message), self.assertRaisesRegex(RuntimeError, message):
                economy.evaluate(reports, economy.required_checks(False))

    def test_missing_required_check_fails(self):
        reports = valid_reports()
        reports["server"]["checks"] = [c for c in reports["server"]["checks"] if c["name"] != "credit_cap"]
        with self.assertRaisesRegex(RuntimeError, "credit_cap"):
            economy.evaluate(reports, economy.required_checks(False))

    def test_failed_native_check_is_retained_not_raised(self):
        reports = valid_reports()
        reports["server"]["checks"][0]["passed"] = False
        checks = economy.evaluate(reports, economy.required_checks(False))
        self.assertFalse(checks[0]["passed"])


class EconomyLaunchTests(unittest.TestCase):
    def test_ports_are_bounded_to_the_assigned_range(self):
        self.assertEqual(economy.arguments([]).port, 7911)
        for port in (7911, 7914):
            self.assertEqual(economy.arguments(["--port", str(port)]).port, port)
        for argv in (["--port", "7910"], ["--port", "7915"], ["--port", "x"], ["--timeout", "nan"], ["--timeout", "0"]):
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                economy.arguments(argv)

    def test_launch_commands_use_match_mode_and_economy_switches_only(self):
        output = Path("/tmp/out dir")
        server = economy.launch_command(Path("/UE"), Path("/Game.uproject"), output, "server", 7912)
        self.assertEqual(server[2], "/Game/Maps/Site01/VS_Site01?game=/Script/ValStrike.VSMatchGameMode")
        self.assertIn("-server", server)
        self.assertIn("-port=7912", server)
        self.assertIn("-nullrhi", server)
        for role in economy.ROLES:
            command = economy.launch_command(Path("/UE"), Path("/Game.uproject"), output, role, 7912)
            self.assertIn(f"-VSEconomySmoke={role}", command)
            self.assertIn(f"-VSEconomyOutput={output / (role + '.json')}", command)
            self.assertFalse(any(arg.lower().startswith("-vsserver") for arg in command))
        alpha = economy.launch_command(Path("/UE"), Path("/Game.uproject"), output, "alpha", 7912)
        self.assertEqual(alpha[2], "127.0.0.1:7912?Name=VSEconomyAlpha")
        self.assertIn("-nullrhi", alpha)

    def test_rendered_option_renders_only_alpha(self):
        output = Path("/out")
        alpha = economy.launch_command(Path("/UE"), Path("/G"), output, "alpha", 7911, True)
        bravo = economy.launch_command(Path("/UE"), Path("/G"), output, "bravo", 7911, True)
        server = economy.launch_command(Path("/UE"), Path("/G"), output, "server", 7911, True)
        self.assertNotIn("-nullrhi", alpha)
        self.assertIn("-ResX=960", alpha)
        self.assertIn("-ResY=540", alpha)
        self.assertIn(f"-VSEconomyScreenshots={output / 'screenshots'}", alpha)
        self.assertIn("-nullrhi", bravo)
        self.assertIn("-nullrhi", server)
        with self.assertRaisesRegex(ValueError, "Unknown"):
            economy.launch_command(Path("/UE"), Path("/G"), output, "late", 7911)


if __name__ == "__main__":
    unittest.main()
