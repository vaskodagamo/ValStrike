"""Strict evidence, cross-check and isolated launch tests; no Unreal process is launched."""

from copy import deepcopy
import argparse
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("shot_effects_runner", SCRIPTS / "test_shot_effects.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def server_shot(label, point, role="world", hit=True):
    return {"label": label, "accepted": 1, "hit": hit, "role": role, "point": list(point)}


def client_shot(label, point, role="world", kind="world"):
    return {"label": label, "effects": 1, "kind": kind, "role": role, "point": list(point),
            "eye": [-1000.0, -1000.0, 154.0], "start": [0, 0, 0], "end": list(point), "muzzle": False}


class ShotEffectsRunnerTests(unittest.TestCase):
    def setUp(self):
        points = {label: (-1950.0, -1000.0 + index, 154.0) for index, label in enumerate(runner.SHOT_LABELS)}
        self.reports = {}
        for role in runner.ROLES:
            if role == "server":
                shots = [server_shot(label, points[label], runner.EXPECTED_ROLE.get(label, "world"))
                         for label in runner.SHOT_LABELS]
            else:
                shots = [client_shot(label, points[label], runner.EXPECTED_ROLE.get(label, "world"))
                         for label in runner.SHOT_LABELS]
            self.reports[role] = {"schema_version": 1, "role": role, "completed": True, "error": None,
                                  "checks": [{"name": name, "passed": True, "detail": "ok"}
                                             for name in runner.REQUIRED_CHECKS[role]],
                                  "shots": shots}

    def test_complete_contract_passes_with_cross_checks(self):
        checks = runner.evaluate(self.reports)
        required = sum(len(names) for names in runner.REQUIRED_CHECKS.values())
        self.assertEqual(len(checks), required + 2 * len(runner.SHOT_LABELS))
        self.assertTrue(all(check["passed"] for check in checks))
        self.assertIn("shooter: impact_matches_server_player", [check["check"] for check in checks])

    def test_failed_check_is_kept_not_hidden(self):
        self.reports["observer"]["checks"][0]["passed"] = False
        checks = runner.evaluate(self.reports)
        self.assertFalse(all(check["passed"] for check in checks))

    def test_each_required_role_specific_check_is_enforced(self):
        for role, name in (("server", "no_effects_on_server"), ("shooter", "quiet_during_rejoin"),
                           ("observer", "rejoin_no_replay"), ("shooter", "pool_cap"), ("observer", "mark_faded")):
            with self.subTest(role=role, name=name):
                reports = deepcopy(self.reports)
                reports[role]["checks"] = [item for item in reports[role]["checks"] if item["name"] != name]
                with self.assertRaisesRegex(RuntimeError, name):
                    runner.evaluate(reports)

    def test_missing_or_misidentified_role_fails(self):
        reports = deepcopy(self.reports)
        reports["observer"] = None
        with self.assertRaisesRegex(RuntimeError, "observer report"):
            runner.evaluate(reports)
        del reports["observer"]
        with self.assertRaisesRegex(RuntimeError, "exactly"):
            runner.evaluate(reports)

    def test_wrong_schema_unfinished_role_or_runtime_error_fails(self):
        original = deepcopy(self.reports["server"])
        for field, value in (("role", "shooter"), ("completed", False), ("completed", 1), ("schema_version", True),
                             ("schema_version", 2), ("error", "players_not_ready"), ("shots", None)):
            with self.subTest(field=field, value=value):
                self.reports["server"] = {**original, field: value}
                with self.assertRaises(RuntimeError):
                    runner.evaluate(self.reports)

    def test_duplicate_or_non_boolean_check_fails(self):
        original = deepcopy(self.reports["shooter"]["checks"])
        for items in (original + original[:1], original[:-1] + [{**original[-1], "passed": "true"}]):
            with self.subTest(items=len(items)):
                self.reports["shooter"]["checks"] = items
                with self.assertRaises(RuntimeError):
                    runner.evaluate(self.reports)

    def test_cosmetic_impact_must_match_server_reference(self):
        self.reports["shooter"]["shots"][0]["point"][1] += runner.IMPACT_TOLERANCE_CM + 0.5
        checks = {check["check"]: check for check in runner.evaluate(self.reports)}
        self.assertFalse(checks["shooter: impact_matches_server_wall"]["passed"])
        self.assertTrue(checks["observer: impact_matches_server_wall"]["passed"])

    def test_observer_bound_follows_replicated_yaw_quantization(self):
        # 950 cm from the eye: half a 1.40625 degree step is ~11.7 cm, x1.25 plus 5 cm.
        bound = runner.impact_tolerance("observer", 950.0)
        self.assertAlmostEqual(bound, 5.0 + 1.25 * 950.0 * 0.012272, places=1)
        self.assertEqual(runner.impact_tolerance("shooter", 950.0), runner.IMPACT_TOLERANCE_CM)
        self.reports["observer"]["shots"][0]["point"][1] += bound - 1.0
        checks = {check["check"]: check for check in runner.evaluate(self.reports)}
        self.assertTrue(checks["observer: impact_matches_server_wall"]["passed"])
        self.reports["observer"]["shots"][0]["point"][1] += 2.0
        checks = {check["check"]: check for check in runner.evaluate(self.reports)}
        self.assertFalse(checks["observer: impact_matches_server_wall"]["passed"])

    def test_player_shot_must_hit_the_observer_on_server_and_client(self):
        index = runner.SHOT_LABELS.index("player")
        self.reports["shooter"]["shots"][index]["role"] = "world"
        checks = {check["check"]: check for check in runner.evaluate(self.reports)}
        self.assertFalse(checks["shooter: impact_matches_server_player"]["passed"])
        self.reports["shooter"]["shots"][index]["role"] = "observer"
        self.reports["server"]["shots"][index]["hit"] = False
        checks = {check["check"]: check for check in runner.evaluate(self.reports)}
        self.assertFalse(checks["observer: impact_matches_server_player"]["passed"])

    def test_invalid_missing_or_duplicate_shot_records_fail(self):
        original = deepcopy(self.reports["shooter"]["shots"])
        for shots in (original[:-1], original + original[:1],
                      original[:-1] + [{**original[-1], "point": [0, 0]}],
                      original[:-1] + [{**original[-1], "point": [0, float("nan"), 0]}],
                      original[:-1] + [{**original[-1], "point": [0, True, 0]}],
                      original[:-1] + [{**original[-1], "label": "unknown"}],
                      original[:-1] + [{key: value for key, value in original[-1].items() if key != "eye"}]):
            with self.subTest(shots=len(shots)):
                self.reports["shooter"]["shots"] = shots
                with self.assertRaises(RuntimeError):
                    runner.evaluate(self.reports)

    def test_screenshots_require_real_png_headers(self):
        required = {"server": (), "shooter": ("tracer_neutral",), "observer": ("tracer_neutral",)}
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder)
            with self.assertRaisesRegex(RuntimeError, "Missing"):
                runner.collect_screenshots(output, required)
            header = b"\x89PNG\r\n\x1a\n" + b"\x00\x00\x00\x0dIHDR" + struct.pack(">II", 1000, 700)
            (output / "shooter_tracer_neutral.png").write_bytes(header + b"\x00" * 8)
            (output / "observer_tracer_neutral.png").write_bytes(b"not a png" * 4)
            with self.assertRaisesRegex(RuntimeError, "observer_tracer_neutral"):
                runner.collect_screenshots(output, required)
            (output / "observer_tracer_neutral.png").write_bytes(header + b"\x00" * 8)
            captures = runner.collect_screenshots(output, required)
            self.assertEqual(captures["shooter: tracer_neutral"]["width"], 1000)

    def test_ports_and_launch_commands_are_isolated(self):
        with self.assertRaises(argparse.ArgumentTypeError):
            runner.effects_port("7871")
        self.assertEqual(runner.effects_port("7904"), 7904)
        output = Path("/tmp/vs-shot-effects")
        server = runner.launch_command("UnrealEditor", "ValStrike.uproject", output, "server", 7901, 45)
        self.assertIn("-nullrhi", server)
        self.assertIn("-port=7901", server)
        self.assertIn(runner.ue.MAP, server)
        self.assertFalse(any("?game=" in part for part in server))  # Practice mode, no test game mode.
        self.assertIn("-VSShotEffectsTest=server", server)
        shooter = runner.launch_command("UnrealEditor", "ValStrike.uproject", output, "shooter", 7901, 45)
        self.assertIn("127.0.0.1:7901?Name=VSShotFxShooter", shooter)
        self.assertNotIn("-nullrhi", shooter)
        self.assertIn("-VSShotEffectsPort=7901", shooter)
        self.assertIn("-VSShotEffectsStart=45", shooter)
        with self.assertRaises(ValueError):
            runner.launch_command("UnrealEditor", "ValStrike.uproject", output, "owner", 7901, 45)
        with self.assertRaises(ValueError):
            runner.launch_command("UnrealEditor", "ValStrike.uproject", output, "shooter", 7901, float("inf"))
        with self.assertRaises(argparse.ArgumentTypeError):
            runner.launch_command("UnrealEditor", "ValStrike.uproject", output, "shooter", 7861, 45)

    def test_runner_refuses_to_run_outside_the_engine_slot(self):
        with self.assertRaisesRegex(RuntimeError, "with_ue_slot"):
            runner.require_engine_slot()


if __name__ == "__main__":
    unittest.main()
