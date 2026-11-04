"""Weapon switching runner contract tests; no Unreal process is launched."""

import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
SOURCE = Path(__file__).resolve().parents[2] / "Source/ValStrike/Combat/VSWeaponSmoke.cpp"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("weapons_runner", SCRIPTS / "test_weapons.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def report(role, scenario, names, passed=True):
    return {"schema_version": 1, "role": role, "scenario": scenario, "completed": True, "error": None,
            "frames": {"count": 1, "mean_ms": 16.7, "max_ms": 16.7},
            "checks": [{"name": name, "passed": passed, "detail": "x"} for name in names]}


class WeaponRunnerTests(unittest.TestCase):
    def test_every_scenario_contract_passes_and_keeps_failures(self):
        for scenario in runner.SCENARIOS:
            with self.subTest(scenario=scenario):
                reports = {role: report(role, scenario, runner.REQUIRED_CHECKS[scenario][role]) for role in runner.ROLES}
                reports["driver"]["checks"][0]["passed"] = False
                checks = runner.evaluate(reports, scenario)
                self.assertEqual(len(checks), sum(len(names) for names in runner.REQUIRED_CHECKS[scenario].values()))
                self.assertFalse(checks[len(runner.REQUIRED_CHECKS[scenario]["server"])]["passed"])

    def test_required_validation_checks_cannot_silently_disappear(self):
        server = runner.REQUIRED_CHECKS["practice"]["server"]
        for name in ("reject_dead", "reject_phase", "reject_spam", "reject_duplicate", "reject_equip_time",
                     "switch_cancels_reload", "per_weapon_ammo", "fresh_pawn_loadout"):
            self.assertIn(name, server)
        self.assertIn("round_reset_restores_both", runner.REQUIRED_CHECKS["match"]["server"])
        self.assertIn("pistol_models", runner.REQUIRED_CHECKS["practice"]["observer"])
        reports = {role: report(role, "practice", runner.REQUIRED_CHECKS["practice"][role]) for role in runner.ROLES}
        reports["server"]["checks"] = [c for c in reports["server"]["checks"] if c["name"] != "reject_spam"]
        with self.assertRaisesRegex(RuntimeError, "reject_spam"):
            runner.evaluate(reports, "practice")

    def test_knife_checks_extend_practice_without_changing_firearm_checks(self):
        # Firearm contract sizes before KNIFE-01: 16 common, 25 per client, 24 on the server.
        self.assertEqual(len(runner.PRACTICE_COMMON), 16)
        self.assertEqual(len(runner.PRACTICE_CLIENT), 25)
        practice = runner.REQUIRED_CHECKS["practice"]
        self.assertEqual(len(practice["server"]), 24 + 15)
        self.assertEqual(len(practice["driver"]), 25 + 12)
        self.assertEqual(practice["driver"], practice["observer"])
        for role in runner.ROLES:
            for name in ("knife_equipped", "knife_slash_hit_once", "knife_wall", "knife_cancel_switch",
                         "knife_cancel_phase", "knife_cancel_reset", "knife_cancel_death"):
                self.assertIn(name, practice[role])
            self.assertEqual(len(set(practice[role])), len(practice[role]))
        for name in ("knife_reject_equip", "knife_reject_cooldown", "knife_reject_replay", "knife_reject_holstered"):
            self.assertIn(name, practice["server"])
        self.assertIn("knife_presentation_cues", practice["driver"])
        for scenario in ("visual", "match"):
            for names in runner.REQUIRED_CHECKS[scenario].values():
                self.assertFalse([name for name in names if name.startswith("knife_")])

    def test_every_required_name_exists_in_the_native_harness(self):
        source = SOURCE.read_text()
        for scenario in runner.SCENARIOS:
            for names in runner.REQUIRED_CHECKS[scenario].values():
                for name in names:
                    self.assertIn(f'TEXT("{name}")', source, name)
            for name in runner.CLIENT_SCREENSHOTS[scenario]:
                self.assertIn(f'Capture(TEXT("{name}"))', source, name)

    def test_identity_scenario_and_errors_are_strict(self):
        names = runner.REQUIRED_CHECKS["practice"]
        cases = [
            ("wrong scenario", lambda r: r.update(scenario="match")),
            ("wrong role", lambda r: r.update(role="observer")),
            ("incomplete", lambda r: r.update(completed=False)),
            ("harness error", lambda r: r.update(error="players_not_ready")),
            ("duplicate", lambda r: r["checks"].append(dict(r["checks"][0]))),
            ("non-boolean", lambda r: r["checks"][0].update(passed="true")),
        ]
        for label, mutate in cases:
            with self.subTest(label=label):
                reports = {role: report(role, "practice", names[role]) for role in runner.ROLES}
                mutate(reports["driver"])
                with self.assertRaises(RuntimeError):
                    runner.evaluate(reports, "practice")

    def test_ports_are_limited_to_the_weapons_range(self):
        for value in (7891, "7892", 7893, 7894):
            self.assertEqual(runner.weapons_port(value), int(value))
        for value in (7890, 7895, 7851, 7777, 0, "not-a-port"):
            with self.subTest(value=value), self.assertRaises((ValueError, runner.argparse.ArgumentTypeError)):
                runner.weapons_port(value)

    def test_launch_commands_select_map_mode_and_rendering(self):
        output = Path("/tmp/weapons $(literal)")
        practice = {role: runner.launch_command(Path("/tmp/UE"), Path("/tmp/Game.uproject"), output, role, 7893,
                                                "practice") for role in runner.ROLES}
        self.assertIn("/Game/Maps/VS_Graybox", practice["server"])
        for role in runner.ROLES:
            self.assertIn("-nullrhi", practice[role])
            self.assertIn(f"-VSWeaponSmoke={role}", practice[role])
            self.assertIn("-VSWeaponScenario=practice", practice[role])
            self.assertIn(f"-VSWeaponOutput={output / (role + '.json')}", practice[role])
            self.assertNotIn("-VSWeaponCapture", practice[role])
        match = {role: runner.launch_command(Path("/tmp/UE"), Path("/tmp/Game.uproject"), output, role, 7894,
                                             "match") for role in runner.ROLES}
        self.assertIn(f"/Game/Maps/Site01/VS_Site01?game={runner.MATCH_MODE}", match["server"])
        self.assertIn("-VSMatchMinimum=1", match["server"])
        self.assertIn("-nullrhi", match["server"])
        for role in ("driver", "observer"):
            self.assertIn("-VSWeaponCapture", match[role])
            self.assertIn("-ResX=1000", match[role])
            self.assertNotIn("-nullrhi", match[role])
            self.assertIn("127.0.0.1:7894?Name=VSWeapon" + role.title(), match[role])
        visual = {role: runner.launch_command(Path("/tmp/UE"), Path("/tmp/Game.uproject"), output, role, 7893,
                                              "visual") for role in runner.ROLES}
        self.assertIn("/Game/Maps/VS_Graybox", visual["server"])
        self.assertIn("-nullrhi", visual["server"])
        for role in ("driver", "observer"):
            self.assertIn("-VSWeaponCapture", visual[role])
            self.assertNotIn("-nullrhi", visual[role])
        with self.assertRaises(ValueError):
            runner.launch_command(Path("/tmp/UE"), Path("/tmp/Game.uproject"), output, "server", 7893, "other")

    def test_screenshots_are_required_only_when_rendered_and_must_be_png(self):
        self.assertEqual(runner.required_screenshots("practice")["driver"], ())
        self.assertFalse(runner.RENDERED["practice"])
        for scenario in ("visual", "match"):
            self.assertTrue(runner.RENDERED[scenario])
            self.assertTrue(runner.required_screenshots(scenario)["observer"])
        required = runner.required_screenshots("match")
        self.assertEqual(required["server"], ())
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            png = b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 1000, 700) + b"x"
            for role in ("driver", "observer"):
                for name in required[role]:
                    (output / f"{role}_{name}.png").write_bytes(png)
            captures = runner.collect_screenshots(output, required)
            self.assertEqual(len(captures), 2 * len(runner.CLIENT_SCREENSHOTS["match"]))
            (output / "observer_match_reset.png").write_bytes(b"not png")
            with self.assertRaisesRegex(RuntimeError, "Invalid or empty PNG"):
                runner.collect_screenshots(output, required)
            (output / "observer_match_reset.png").unlink()
            with self.assertRaisesRegex(RuntimeError, "Missing rendered screenshot"):
                runner.collect_screenshots(output, required)

    def test_unwrapped_execution_is_rejected_before_engine_launch(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "slot"
            path.write_text(json.dumps({"owner_pid": -1}))
            with patch.object(runner.with_ue_slot, "lock_path", return_value=path):
                with self.assertRaisesRegex(RuntimeError, "with_ue_slot.py"):
                    runner.require_engine_slot()

    def test_cleanup_signals_only_owned_running_process_groups(self):
        exited = Mock(pid=101)
        exited.poll.return_value = 0
        running = Mock(pid=202)
        running.poll.side_effect = [None, 0, 0]
        with patch.object(runner.os, "name", "posix"), patch.object(runner.os, "killpg") as kill:
            runner.stop_processes([exited, running])
        kill.assert_called_once_with(202, runner.signal.SIGTERM)


if __name__ == "__main__":
    unittest.main()
