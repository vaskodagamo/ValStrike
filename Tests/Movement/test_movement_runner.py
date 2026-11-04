"""Movement runner network-emulation tests. These tests never launch Unreal."""

from contextlib import redirect_stderr
import importlib.util
import io
from pathlib import Path
import sys
import unittest


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("movement_runner", SCRIPTS / "test_movement.py")
movement = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(movement)
ue = movement.ue


def parse(*argv):
    return movement.build_parser().parse_args(list(argv))


def packet_log(lag, variance, loss):
    # Shape of the engine lines (NetEmulationHelper.cpp ParseSettings), including unrelated settings around them.
    return ("[2026.11.04-10.00.00:000][  0]LogNet: PktLoss set to %d\n"
            "[2026.11.04-10.00.00:000][  0]LogNet: PktLossMinSize set to 0\n"
            "[2026.11.04-10.00.00:000][  0]LogNet: PktLag set to %d\n"
            "[2026.11.04-10.00.00:000][  0]LogNet: PktLagVariance set to %d\n"
            "[2026.11.04-10.00.00:000][  0]LogNet: PktLagMin set to 999\n") % (loss, lag, variance)


class ArgumentTests(unittest.TestCase):
    def test_default_run_has_no_emulation_and_unchanged_argv(self):
        args = parse()
        self.assertEqual((args.port, args.timeout), (7789, 120))
        switches, requested = movement.net_emulation(args)
        self.assertEqual((switches, requested), ([], None))
        command = movement.launch_command("server", "/E", "/P.uproject", 7789, "/L/server.log", switches)
        self.assertEqual(command, ["/E", "/P.uproject", ue.MAP, "-server", "-nullrhi", "-nosound", "-unattended",
                                   "-VSMovementSmoke=server", "-VSMovementSmokeStart=30", "-abslog=/L/server.log",
                                   "-ExecCmds=t.MaxFPS 60", "-port=7789", "-multihome=127.0.0.1"])
        self.assertFalse([item for item in command if item.startswith("-Pkt")])

    def test_profiles_reuse_ue_composition(self):
        for profile, values in ue.NET_PROFILES.items():
            with self.subTest(profile=profile):
                args = parse("--net-profile", profile)
                switches, requested = movement.net_emulation(args)
                self.assertEqual(switches, ue.net_emulation_switches(args))
                self.assertEqual(requested, dict(zip(movement.PACKET_SETTINGS, values)))
        switches, requested = movement.net_emulation(parse("--net-profile", "typical", "--pkt-loss", "0"))
        self.assertEqual(switches, ["-PktLag=40", "-PktLagVariance=10", "-PktLoss=0"])
        self.assertEqual(requested, {"PktLag": 40, "PktLagVariance": 10, "PktLoss": 0})
        _switches, requested = movement.net_emulation(parse("--pkt-lag", "100"))
        self.assertEqual(requested, {"PktLag": 100, "PktLagVariance": 0, "PktLoss": 0})

    def test_invalid_emulation_is_rejected_before_launch(self):
        for option, value in (("--pkt-lag", "-1"), ("--pkt-lag", "1001"), ("--pkt-lag", "1.5"),
                              ("--pkt-lag-variance", "501"), ("--pkt-loss", "101"), ("--net-profile", "awful")):
            with self.subTest(option=option, value=value), redirect_stderr(io.StringIO()), \
                    self.assertRaises(SystemExit):
                parse(option, value)
        with self.assertRaisesRegex(ue.SetupError, "cannot exceed --pkt-lag"):
            movement.net_emulation(parse("--pkt-lag-variance", "5"))

    def test_every_role_gets_the_same_emulation_and_correction_logging(self):
        switches, _requested = movement.net_emulation(parse("--net-profile", "poor"))
        plain = {role: movement.launch_command(role, "/E", "/P", 7789, f"/L/{role}.log", [])
                 for role in movement.ROLES}
        for role in movement.ROLES:
            with self.subTest(role=role):
                command = movement.launch_command(role, "/E", "/P", 7789, f"/L/{role}.log", switches)
                self.assertEqual(command[-3:], ["-PktLag=90", "-PktLagVariance=30", "-PktLoss=3"])
                self.assertIn("-ExecCmds=t.MaxFPS 60,p.NetShowCorrections 1", command)
                expected = [item if not item.startswith("-ExecCmds=") else
                            "-ExecCmds=t.MaxFPS 60,p.NetShowCorrections 1" for item in plain[role]]
                self.assertEqual(command[:-3], expected)
        self.assertEqual(movement.launch_command("driver", "/E", "/P", 7789, "/L/d.log", [])[2],
                         "127.0.0.1:7789?Name=VSDriver")


class EmulationValidityTests(unittest.TestCase):
    requested = {"PktLag": 40, "PktLagVariance": 10, "PktLoss": 1}

    def logs(self, **override):
        result = {role: packet_log(40, 10, 1) for role in movement.ROLES}
        result.update(override)
        return result

    def test_no_request_needs_no_log_lines(self):
        self.assertIsNone(movement.emulation_problem({role: "" for role in movement.ROLES}, None))

    def test_requested_values_in_every_log_are_valid(self):
        self.assertIsNone(movement.emulation_problem(self.logs(), self.requested))
        repeated = packet_log(40, 10, 1) * 2  # More than one net driver may parse the switches.
        self.assertIsNone(movement.emulation_problem(self.logs(driver=repeated), self.requested))
        self.assertEqual(movement.packet_log_values(packet_log(40, 10, 1)),
                         {"PktLag": [40], "PktLagVariance": [10], "PktLoss": [1]})

    def test_lan_zeros_must_still_be_logged(self):
        zeros = {"PktLag": 0, "PktLagVariance": 0, "PktLoss": 0}
        self.assertIsNone(movement.emulation_problem({r: packet_log(0, 0, 0) for r in movement.ROLES}, zeros))
        problem = movement.emulation_problem({r: "" for r in movement.ROLES}, zeros)
        self.assertRegex(problem, "^Invalid network emulation run")

    def test_missing_process_log_or_line_is_invalid(self):
        for role in movement.ROLES:
            with self.subTest(role=role, case="empty"):
                problem = movement.emulation_problem(self.logs(**{role: ""}), self.requested)
                self.assertIn(f"{role}: no 'LogNet: PktLag set to' line", problem)
            with self.subTest(role=role, case="absent"):
                logs = self.logs()
                del logs[role]
                self.assertIn(f"{role}: no", movement.emulation_problem(logs, self.requested))
        only_lag = "LogNet: PktLag set to 40\nLogNet: PktLoss set to 1\n"
        problem = movement.emulation_problem(self.logs(observer=only_lag), self.requested)
        self.assertIn("observer: no 'LogNet: PktLagVariance set to' line", problem)
        self.assertNotIn("observer: no 'LogNet: PktLag set to'", problem)

    def test_wrong_or_conflicting_values_are_invalid(self):
        problem = movement.emulation_problem(self.logs(server=packet_log(90, 10, 1)), self.requested)
        self.assertIn("server: PktLag set to [90], requested 40", problem)
        conflicting = packet_log(40, 10, 1) + packet_log(40, 10, 0)
        problem = movement.emulation_problem(self.logs(driver=conflicting), self.requested)
        self.assertIn("driver: PktLoss set to [0, 1], requested 1", problem)

    def test_other_categories_and_lookalike_settings_do_not_count(self):
        lookalike = ("LogTemp: PktLag set to 40\nLogNet: PktLagMin set to 40\nLogNet: PktLagMax set to 10\n"
                     "LogNet: PktIncomingLoss set to 1\nLogNet: PktLossMinSize set to 1\n")
        self.assertEqual(movement.packet_log_values(lookalike), {"PktLag": [], "PktLagVariance": [], "PktLoss": []})
        self.assertIsNotNone(movement.emulation_problem(self.logs(server=lookalike), self.requested))


class NetSummaryTests(unittest.TestCase):
    server_log = ('p.NetShowCorrections = "1"\n'
                  "LogNetPlayerMovement: Warning: *** Server: Error for BP_VSCharacter_C_0 at Time=12.345 is 3.250 "
                  "LocDiff(X=1) ClientLoc(X=2) ServerLoc(X=3) Base: None Bone: None Accel(X=0) Velocity(X=0)\n"
                  "LogNetPlayerMovement: Warning: *** Server: Error for BP_VSCharacter_C_0 at Time=13.000 is 41.500 "
                  "LocDiff(X=1)\n")

    def test_corrections_are_counted_only_when_logging_is_confirmed(self):
        self.assertEqual(movement.corrections(self.server_log),
                         {"count": 2, "max_error_cm": 41.5, "by_character": {"BP_VSCharacter_C_0": 2}})
        self.assertEqual(movement.corrections('p.NetShowCorrections = "1"\n'),
                         {"count": 0, "max_error_cm": 0.0, "by_character": {}})
        self.assertEqual(movement.corrections(self.server_log.split("\n", 1)[1]), "unavailable")
        client = ('p.NetShowCorrections = "1"\nLogNetPlayerMovement: Warning: *** Client: Error for '
                  "BP_VSCharacter_C_1 at Time=5.000 is 7.000 LocDiff(X=1)\n")
        self.assertEqual(movement.corrections(client)["count"], 1)

    def test_summary_without_emulation(self):
        args = parse()
        summary = movement.net_summary(args, None, {})
        self.assertEqual((summary["profile"], summary["emulation"], summary["corrections"], summary["rtt_ms"]),
                         (None, "off", "unavailable", "unavailable"))
        self.assertIn("no artificial latency/loss", movement.conditions(None))

    def test_summary_with_profile_records_values_rtt_and_corrections(self):
        args = parse("--net-profile", "typical", "--pkt-loss", "2")
        _switches, requested = movement.net_emulation(args)
        logs = {"server": packet_log(40, 10, 2) + self.server_log, "driver": packet_log(40, 10, 2),
                "observer": packet_log(40, 10, 2) + 'p.NetShowCorrections = "1"\n'}
        summary = movement.net_summary(args, requested, logs)
        self.assertEqual(summary["profile"], "typical")
        self.assertEqual(summary["overrides"], {"pkt_loss": 2})
        self.assertEqual(summary["requested"], {"PktLag": 40, "PktLagVariance": 10, "PktLoss": 2})
        self.assertEqual(summary["observed"]["driver"], {"PktLag": [40], "PktLagVariance": [10], "PktLoss": [2]})
        self.assertEqual(summary["corrections"]["server"]["count"], 2)
        self.assertEqual(summary["corrections"]["driver"], "unavailable")
        self.assertEqual(summary["corrections"]["observer"]["count"], 0)
        self.assertEqual(set(summary["rtt_ms"]), {"driver", "observer"})
        self.assertTrue(all(value.startswith("unavailable") for value in summary["rtt_ms"].values()))
        self.assertIn("lag 40 ms +/- 10 ms, loss 2%", movement.conditions(requested))
        explicit = parse("--pkt-lag", "50")
        summary = movement.net_summary(explicit, movement.net_emulation(explicit)[1], {})
        self.assertEqual((summary["profile"], summary["base_profile"]), ("custom", "lan"))


if __name__ == "__main__":
    unittest.main()
