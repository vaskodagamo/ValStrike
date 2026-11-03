"""Server lifecycle runner contract tests. These tests never launch Unreal."""

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("server_lifecycle", SCRIPTS / "test_server_lifecycle.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)

REVISION = "a" * 40
PREFIX = "[2026.11.03-20.48.56:450][  0]LogValStrike: Display: "


def log(*lines):
    return "\n".join(PREFIX + line for line in lines) + "\n"


def version():
    return (f"VSSERVER_VERSION project=0.0.1 revision={REVISION} engine=5.8.4-58902052+++UE5+Release-5.8 "
            "changelist=58902052 compatible_changelist=55116800 configuration=Development target=Editor "
            "net_version=1 session=s")


def state(source, target, reason, at, players):
    return f"VSSERVER_STATE from={source} to={target} reason={reason} at_ms={at} players={players}"


def empty_log(wait_ms=30000):
    return log(version(),
               "VSSERVER_CONFIG session=s exit_when_done=true empty_timeout_s=30 result_grace_s=15 max_players=1 "
               "provider=local admission=open",
               "VSSERVER_MAP map=graybox mode=practice port=7871 max_players=1",
               state("starting", "ready", "listening", 1000, 0),
               "VSSERVER_PLAYERS count=1 at_ms=2000",
               state("ready", "allocated", "player_joined", 2000, 1),
               "PreLogin failure: Server full.",
               "VSSERVER_PLAYERS count=0 at_ms=5000",
               state("allocated", "ending", "empty_timeout", 5000 + wait_ms, 0),
               state("ending", "exited", "drained", 5000 + wait_ms, 0),
               "VSSERVER_EXIT code=0 session=s uptime_s=9.0")


def result(session="run-match"):
    return {"schema_version": 1, "session_id": session, "match_index": 1, "status": "completed",
            "map": "site01", "mode": "match", "source_revision": REVISION, "engine_version": "5.8.4",
            "build_configuration": "Development", "started_utc": "x", "ended_utc": "y", "result": "draw",
            "completed_rounds": 6, "scores": {"team_one": 3, "team_two": 3},
            "participants": {"team_one": 1, "team_two": 1}, "rounds": runner.expected_rounds()}


def match_log(record=None, drain_ms=300, players_at_exit=0, tick_rate=60, measured=("59.9", "60.0")):
    record = result() if record is None else record
    return log(version(),
               f"VSSERVER_MAP map=site01 mode=match port=7871 max_players=4 net_tick_rate={tick_rate}",
               state("starting", "ready", "listening", 1000, 0),
               state("ready", "allocated", "player_joined", 2000, 1),
               "VSSERVER_MATCH event=started match=1 at_ms=2100 session=run-match",
               state("allocated", "in_match", "match_started", 2100, 2),
               *[f"VSSERVER_HEALTH state=in_match players=2 frames=600 frame_ms_avg=1.20 frame_ms_max=4.00 "
                 f"tick_hz={rate} uptime_s=10" for rate in measured],
               "VSSERVER_MATCH event=completed match=1 at_ms=30000 session=run-match",
               "VSSERVER_RESULT " + json.dumps(record, separators=(",", ":")),
               state("in_match", "ending", "match_complete", 32000, 2),
               state("ending", "exited", "drained", 32000 + drain_ms, players_at_exit),
               "VSSERVER_EXIT code=0 session=run-match uptime_s=40.0")


def failed(checks):
    return sorted(item["name"] for item in checks if not item["passed"])


class LogParsingTests(unittest.TestCase):
    def test_events_keep_order_and_fields_and_ignore_command_line_echo(self):
        text = "LogInit: Command Line: -VSServerSession=x -VSServerTypo\n" + log(
            state("starting", "ready", "listening", 5, 0), "VSSERVER_RESULT {\"a\":\"b c\"}")
        events = runner.parse_events(text)
        self.assertEqual([e["kind"] for e in events], ["VSSERVER_STATE", "VSSERVER_RESULT"])
        self.assertEqual(events[0]["fields"]["reason"], "listening")
        self.assertEqual(events[1]["payload"], "{\"a\":\"b c\"}")
        self.assertEqual(runner.transitions(events), [("starting", "ready", "listening", 0, 5)])


class EvaluationTests(unittest.TestCase):
    def test_invalid_launch(self):
        text = log(version(), "VSSERVER_FAILURE code=2 reason=Unknown server switch -VSServerTypo; allowed: ...",
                   state("starting", "ending", "failure", 1, 0), state("ending", "exited", "drained", 1, 0),
                   "VSSERVER_EXIT code=2 session=s uptime_s=0.1")
        self.assertEqual(failed(runner.evaluate_invalid(text, 0, REVISION, "Mac")), [])
        self.assertEqual(failed(runner.evaluate_invalid(text, 0, REVISION, "Linux")), ["process_exit"])
        ready = text + log("VSSERVER_MAP map=graybox mode=practice port=7871 max_players=4")
        self.assertEqual(failed(runner.evaluate_invalid(ready, 0, REVISION, "Mac")), ["never_ready"])

    def test_empty_timeout(self):
        self.assertEqual(failed(runner.evaluate_empty(empty_log(), 0, REVISION, "Mac")), [])
        self.assertEqual(failed(runner.evaluate_empty(empty_log(29999), 0, REVISION, "Mac")), ["empty_timeout_measured"])
        self.assertEqual(failed(runner.evaluate_empty(empty_log(31001), 0, REVISION, "Mac")), ["empty_timeout_measured"])
        self.assertIn("version_line", failed(runner.evaluate_empty(empty_log(), 0, "b" * 40, "Mac")))
        refused = empty_log().replace("PreLogin failure: Server full.", "")
        self.assertEqual(failed(runner.evaluate_empty(refused, 0, REVISION, "Mac")), ["max_players_enforced"])
        self.assertIn("process_exit", failed(runner.evaluate_empty(empty_log(), None, REVISION, "Mac")))

    def test_match_completion(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "run-match_match1.json"
            path.write_text(json.dumps(result()) + "\n")
            checks = runner.evaluate_match(match_log(), 0, REVISION, "Mac", "run-match", path)
            self.assertEqual(failed(checks), [])
            slow = runner.evaluate_match(match_log(drain_ms=2000, players_at_exit=2), 0, REVISION, "Mac", "run-match", path)
            self.assertEqual(failed(slow), ["clients_closed"])
            wrong = result()
            wrong["rounds"][0]["reason"] = "elimination"
            self.assertEqual(failed(runner.evaluate_match(match_log(wrong), 0, REVISION, "Mac", "run-match", path)),
                             ["result_file", "result_json"])
            missing = runner.evaluate_match(match_log(), 0, REVISION, "Mac", "run-match", Path(folder) / "absent.json")
            self.assertEqual(failed(missing), ["result_file"])
            early = match_log().replace("at_ms=30000", "at_ms=31000")
            self.assertEqual(failed(runner.evaluate_match(early, 0, REVISION, "Mac", "run-match", path)), ["result_grace"])

    def test_tick_rate_must_match_configuration(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "run-match_match1.json"
            path.write_text(json.dumps(result()) + "\n")
            ok = runner.evaluate_match(match_log(tick_rate=30, measured=("57.0", "29.9", "30.0")), 0, REVISION, "Mac", "run-match", path)
            self.assertEqual(failed(ok), [])  # the first window (warm-up) is ignored
            slow = runner.evaluate_match(match_log(tick_rate=60, measured=("60.0", "52.0")), 0, REVISION, "Mac", "run-match", path)
            self.assertEqual(failed(slow), ["tick_rate_measured"])
            odd = runner.evaluate_match(match_log(tick_rate=45), 0, REVISION, "Mac", "run-match", path)
            self.assertEqual(failed(odd), ["tick_rate_configured", "tick_rate_measured"])

    def test_joinkey_evaluation(self):
        rejection = "VSSERVER_ADMISSION rejected state=ready admission=join_key reason=Join key missing or wrong"
        text = log(version(),
                   "VSSERVER_CONFIG session=s exit_when_done=true empty_timeout_s=30 result_grace_s=15 max_players=4 "
                   "provider=local admission=join_key",
                   state("starting", "ready", "listening", 1000, 0),
                   rejection, "PreLogin failure: Join key missing or wrong",
                   rejection, "PreLogin failure: Join key missing or wrong",
                   "VSSERVER_PLAYERS count=1 at_ms=5000", state("ready", "allocated", "player_joined", 5000, 1),
                   "VSSERVER_PLAYERS count=0 at_ms=9000", state("allocated", "ending", "empty_timeout", 39000, 0),
                   state("ending", "exited", "drained", 39000, 0), "VSSERVER_EXIT code=0 session=s uptime_s=40")
        self.assertEqual(failed(runner.evaluate_joinkey(text, 0, REVISION, "Mac")), [])
        leaked = text + log(f"LogInit: Command Line: -VSServerJoinKey={runner.JOIN_KEY}")
        self.assertEqual(failed(runner.evaluate_joinkey(leaked, 0, REVISION, "Mac")), ["key_never_logged"])
        once = text.replace(rejection + "\n", "", 1).replace("PreLogin failure: Join key missing or wrong\n", "", 1)
        self.assertEqual(failed(runner.evaluate_joinkey(once, 0, REVISION, "Mac")), ["wrong_keys_rejected"])

    def test_expected_rounds_swap_sides_and_draw(self):
        rounds = runner.expected_rounds()
        self.assertEqual([r["attacking_team"] for r in rounds], [1, 1, 1, 2, 2, 2])
        self.assertEqual([r["winning_team"] for r in rounds], [2, 2, 2, 1, 1, 1])


class CommandTests(unittest.TestCase):
    def test_server_commands_use_only_allowlisted_switches(self):
        output = Path("/tmp/out dir")
        invalid = runner.server_command("UE", "P.uproject", output, "invalid", 7871, "s-invalid")
        self.assertIn("-VSServerTypo", invalid)
        self.assertNotIn("-VSServerExitWhenDone", invalid)
        empty = runner.server_command("UE", "P.uproject", output, "empty", 7872, "s-empty")
        self.assertEqual(empty[2], "/Game/Maps/VS_Graybox")
        self.assertIn("-VSServerMaxPlayers=1", empty)
        self.assertIn("-VSServerEmptyTimeout=30", empty)
        match_command = runner.server_command("UE", "P.uproject", output, "match", 7874, "s-match")
        self.assertEqual(match_command[2], "/Game/Maps/Site01/VS_Site01?game=/Script/ValStrike.VSMatchGameMode")
        self.assertFalse([item for item in match_command if "MaxFPS" in item])
        keyed = runner.server_command("UE", "P.uproject", output, "joinkey", 7873, "s-key", 30)
        self.assertIn(f"-VSServerJoinKey={runner.JOIN_KEY}", keyed)
        self.assertIn("-ini:Engine:[/Script/OnlineSubsystemUtils.IpNetDriver]:NetServerMaxTickRate=30", keyed)
        with self.assertRaises(ValueError):
            runner.server_command("UE", "P", output, "match", 7871, "s", 45)
        client = runner.client_command("UE", "P", output, "joinkey", 7873, "VSServerProbeC", runner.JOIN_KEY)
        self.assertEqual(client[2], f"127.0.0.1:7873?Name=VSServerProbeC?JoinKey={runner.JOIN_KEY}")
        with self.assertRaises(ValueError):
            runner.client_command("UE", "P", output, "joinkey", 7873, "VSServerProbeC", "bad key")
        self.assertIn("-VSMatchFastTimings", match_command)
        self.assertIn("-VSServerResultGrace=2", match_command)
        self.assertIn(f"-abslog={output / 'match_server.log'}", match_command)
        for command in (invalid, empty, match_command):
            self.assertIn("-multihome=127.0.0.1", command)
        with self.assertRaises(ValueError):
            runner.server_command("UE", "P", output, "other", 7871, "s")

    def test_ports_and_client_names_are_bounded(self):
        for port in (7870, 7875, "x"):
            with self.subTest(port=port), self.assertRaises(Exception):
                runner.server_port(port)
        client = runner.client_command("UE", "P", Path("/tmp"), "empty", 7871, "VSServerProbeA")
        self.assertIn("-abslog=/tmp/empty_VSServerProbeA.log", client)
        self.assertEqual(client[2], "127.0.0.1:7871?Name=VSServerProbeA")
        for name in ("A?listen", "", "a b"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                runner.client_command("UE", "P", Path("/tmp"), "empty", 7871, name)


if __name__ == "__main__":
    unittest.main()
