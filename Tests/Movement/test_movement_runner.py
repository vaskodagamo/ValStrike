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




# Synthetic driver timeline matching VSMovementSmoke.cpp and the GPT-15 tuning:
# rifle 387/174/155 cm/s, pistol 432, jump 738.2 cm/s at 1955.6 cm/s^2, tuck +22 cm.
JUMP_V, GRAVITY, TUCK = 738.2, 1955.6, 22.0
FLOOR_Z = 90.15  # Standing capsule center over the graybox floor.


def arc(t, start, offset=0.0):
    """Feet height above the floor and vertical speed for a jump launched at start."""
    dt = t - start
    height = offset + JUMP_V * dt - GRAVITY * dt * dt / 2
    return (height, JUMP_V - GRAVITY * dt) if dt >= 0 and height > 0 else None


def sample(t, **overrides):
    s = {"x": -1400.0, "y": -1100.0, "speed": 0.0, "vz": 0.0, "grounded": True, "crouched": False,
         "weapon": "pistol" if t >= 32.6 else "rifle", "feet": 0.0}
    if 1.6 <= t <= 2.4: s["speed"] = 387.0
    if 3.6 <= t <= 4.8: s["speed"] = 174.0
    if 5.5 <= t < 7.0: s.update(crouched=True, speed=155.0)
    if 10.9 <= t <= 18.0: s["x"] = 200.0  # Boundary placement at 10.85 s.
    if 11.8 <= t <= 18.0: s["x"] = min(1916.0, 200.0 + 387.0 * (t - 11.8))
    if s["x"] >= 1916.0 or 17.0 <= t <= 18.0: s["speed"] = 0.0
    elif 11.8 <= t <= 18.0: s["speed"] = 387.0
    if 22.0 <= t <= 23.0: s["feet"] = 80.0
    if 30.0 <= t <= 31.0: s["x"] = -634.0
    if 33.6 <= t <= 34.4: s["speed"] = 432.0
    if 36.6 <= t <= 39.8: s["speed"] = 432.0
    if 35.0 <= t < 36.2 or 38.6 <= t < 39.6 or 40.2 <= t < 41.7: s["crouched"] = True
    for start, offset in ((8.0, 0.0), (35.0, TUCK), (37.2, 0.0), (38.6, TUCK), (40.7, TUCK)):
        flight = arc(t, start, offset)
        if flight:
            s.update(feet=flight[0], vz=flight[1], grounded=False)
    s.update(overrides)
    half = 56.0 if s["crouched"] else 88.0
    z = FLOOR_Z - 88.0 + s.pop("feet") + half
    return dict(s, event="sample", player="VSDriver", t=round(t, 3), z=z, half_height=half,
                body_min_z=z - half, body_max_z=z - half + 2 * half, eye_z=z + (32.0 if s["crouched"] else 64.0))


def evidence(mutate=None, done_t=42.5):
    roles = {"server": (3, True), "driver": (2, False), "observer": (1, False)}
    result = {}
    for role, (net_role, authority) in roles.items():
        records = []
        if role == "server":
            records.append({"event": "ready", "role": role, "t": 0.1, "initial_spawn_distance": 1200})
        for tick in range(-10, int(done_t * 10)):
            item = sample(tick / 10 + 0.001)
            if mutate:
                item = mutate(item) or item
            records.append(dict(item, role=role, player_id=7, net_role=net_role, authority=authority))
        records.append({"event": "done", "role": role, "t": done_t})
        result[role] = records
    return result


def results(data):
    return {c["check"]: c for c in movement.evaluate(data)}


def failed(data):
    return sorted(name for name, c in results(data).items() if not c["passed"])


def retune(item, **values):
    """Rebuild one sample with overrides, keeping its pose fields consistent."""
    feet = item["z"] - item["half_height"] - (FLOOR_Z - 88.0)
    fields = {k: item[k] for k in ("x", "speed", "vz", "grounded", "crouched", "weapon")}
    fields["feet"] = feet
    fields.update(values)
    return sample(item["t"], **fields)


class EvaluateTests(unittest.TestCase):
    NEW = ("rifle before switch", "pistol weight", "Ctrl+Space crouch-jump", "running jump",
           "running crouch-jump", "no landing gain", "held-crouch jump", "feel stages completed")

    def test_new_tuning_timeline_passes_every_check(self):
        checks = results(evidence())
        self.assertEqual(failed(evidence()), [])
        for role in ("server", "driver", "observer"):
            for name in self.NEW:
                self.assertIn(f"{role}: {name}", checks)
        cj = checks["driver: Ctrl+Space crouch-jump"]["detail"]
        self.assertAlmostEqual(cj["gain_cm"], 22.0, delta=3)
        self.assertGreater(cj["feet_apex_cm"], 155)

    def test_bounds_for_rifle_speeds_and_jump_height(self):
        def scale(field, start, end, value):
            return lambda i: retune(i, **{field: value}) if start <= i["t"] <= end else None
        for label, start, end, ok, bad in (("run", 1.6, 2.4, (361, 409), (359, 411, 450)),
                                           ("walk", 3.6, 4.8, (156, 194), (154, 196, 202.5)),
                                           ("crouch move", 6.0, 6.8, (136, 174), (134, 176, 180))):
            for speed in ok:
                self.assertNotIn(f"driver: {label} speed", failed(evidence(scale("speed", start, end, speed))))
            for speed in bad:
                self.assertIn(f"driver: {label} speed", failed(evidence(scale("speed", start, end, speed))))

    def test_old_floaty_jump_fails_jump_height(self):
        def old_jump(item):
            if 8.0 <= item["t"] <= 9.5:
                dt = item["t"] - 8.0
                feet = 420 * dt - 980 * dt * dt / 2
                grounded = not (dt < 0.857 and feet > 0)
                return retune(item, feet=max(feet, 0), grounded=grounded, vz=420 - 980 * dt)
            return None
        self.assertIn("server: jump and land", failed(evidence(old_jump)))

    def test_no_switch_fails_pistol_weight_only(self):
        rifle = evidence(lambda i: retune(i, weapon="rifle"))
        self.assertEqual([n for n in failed(rifle) if n.startswith("driver")], ["driver: pistol weight"])
        slow = evidence(lambda i: retune(i, speed=387.0) if 33.6 <= i["t"] <= 34.4 else None)
        self.assertIn("driver: pistol weight", failed(slow))

    def test_missing_weapon_field_fails_weapon_checks(self):
        stale = evidence(lambda i: {k: v for k, v in i.items() if k != "weapon"})
        names = failed(stale)
        self.assertIn("observer: rifle before switch", names)
        self.assertIn("observer: pistol weight", names)

    def test_crouch_jump_without_tuck_gain_fails(self):
        def untucked(item):
            for start in (35.0, 38.6, 40.7):
                if start <= item["t"] <= start + 0.8:
                    flight = arc(item["t"], start)
                    return retune(item, feet=flight[0] if flight else 0.0, grounded=not flight)
            return None
        names = failed(evidence(untucked))
        for name in ("Ctrl+Space crouch-jump", "running crouch-jump", "held-crouch jump"):
            self.assertIn(f"driver: {name}", names)
        self.assertNotIn("driver: running jump", names)

    def test_extra_crouch_jump_height_fails(self):
        def engine_tuck(item):
            if 35.0 <= item["t"] <= 36.0:
                flight = arc(item["t"], 35.0, 32.0 + 15.0)
                return retune(item, feet=flight[0] if flight else 0.0, grounded=not flight)
            return None
        self.assertIn("server: Ctrl+Space crouch-jump", failed(evidence(engine_tuck)))

    def test_running_jump_must_not_gain_height_or_speed(self):
        higher = evidence(lambda i: retune(i, feet=arc(i["t"], 37.2)[0] + 20)
                          if 37.2 <= i["t"] <= 38.1 and arc(i["t"], 37.2) else None)
        self.assertIn("driver: running jump", failed(higher))
        faster = evidence(lambda i: retune(i, speed=470.0) if 37.2 <= i["t"] <= 39.5 else None)
        names = failed(faster)
        for name in ("running jump", "running crouch-jump", "no landing gain"):
            self.assertIn(f"driver: {name}", names)

    def test_held_crouch_must_start_crouched_on_ground(self):
        standing = evidence(lambda i: retune(i, crouched=False) if 40.45 <= i["t"] <= 40.65 else None)
        self.assertIn("driver: held-crouch jump", failed(standing))

    def test_old_smoke_ending_at_33_seconds_fails(self):
        names = failed(evidence(done_t=33.0))
        self.assertIn("server: feel stages completed", names)
        self.assertNotIn("server: completed", names)

    def test_boundary_run_timing(self):
        # The boundary placement (10.85 s) sits between the landing window and the W press.
        landing = [i for i in evidence()["server"] if i.get("event") == "sample" and 10.0 <= i["t"] <= 10.8]
        self.assertTrue(landing and all(i["x"] == -1400.0 for i in landing))
        # Still running into the wall window fails, as does passing the wall.
        late = evidence(lambda i: retune(i, x=1800.0, speed=387.0) if 17.0 <= i["t"] <= 17.5 else None)
        self.assertIn("server: boundary blocks movement", failed(late))
        through = evidence(lambda i: retune(i, x=1930.0) if 12.0 <= i["t"] <= 12.3 else None)
        self.assertIn("server: boundary blocks movement", failed(through))


if __name__ == "__main__":
    unittest.main()
