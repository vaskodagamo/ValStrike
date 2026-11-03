"""Lightweight session-runner tests; no Unreal process or engine slot is used."""

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("playtest_session", Path(__file__).with_name("run_session.py"))
session = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(session)

MAP = "/Game/Maps/Site01/VS_Site01"
WELCOME = f"LogNet: Welcomed by server (Level: {MAP}, Game: /Script/ValStrike.VSGameMode)\n"
LOAD = f"LogLoad: Took 0.120000 seconds to LoadMap({MAP})\n"
SERVER = ("LogNet: GameNetDriver listening on port 7861\n"
          "LogLoad: Game class is 'VSGameMode'\n" + LOAD)


class SessionTests(unittest.TestCase):
    def args(self, *extra):
        return session.parser().parse_args(["host", "--map", "site01", "--port", "7861", *extra])

    def test_allowlists_reject_url_or_console_injection_and_unbounded_session(self):
        invalid = [
            ["host", "--map", "site01?listen"], ["host", "--mode", "match?game=Other"],
            ["join", "--address", "192.168.1.2:7777?game=Other"],
            ["join", "--address", "192.168.1.2:7777", "--clients", "0"],
            ["host", "--duration-seconds", "nan"], ["host", "--duration-seconds", "301"],
            ["host", "--clients", "3"],
        ]
        for argv in invalid:
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                session.parser().parse_args(argv)

    def test_client_requires_correct_welcome_then_completed_map_travel(self):
        for text in ("", LOAD, LOAD + WELCOME, WELCOME, WELCOME.replace("VS_Site01", "Other") + LOAD):
            self.assertFalse(session.client_evidence(text, MAP)["map_load_completed_after_welcome"])
        self.assertTrue(session.client_evidence(WELCOME + LOAD, MAP)["map_load_completed_after_welcome"])

    def test_two_clients_need_two_separate_logs_and_server_joins(self):
        args = self.args("--clients", "2")
        logs = {"server": SERVER + "Join succeeded: same-host-name\n" * 2,
                "client1": WELCOME + LOAD, "client2": WELCOME + LOAD}
        self.assertTrue(session.evaluate_logs(logs, args)["connection_checks_passed"])
        for role in ("client1", "client2"):
            altered = dict(logs, **{role: ""})
            self.assertFalse(session.evaluate_logs(altered, args)["connection_checks_passed"])
        logs["server"] = SERVER + "Join succeeded: same-host-name\n"
        self.assertFalse(session.evaluate_logs(logs, args)["connection_checks_passed"])

    def test_server_requires_exact_port_map_and_game_mode(self):
        for text in (SERVER.replace("7861", "78610"), SERVER.replace("VS_Site01", "Other"),
                     SERVER.replace("VSGameMode", "VSMatchGameMode")):
            evidence = session.server_evidence(text, MAP, 7861, "practice")
            self.assertFalse(all(evidence[k] for k in ("listening", "map_load_complete", "expected_game_mode")))

    def test_failed_connection_invalidates_prior_success_without_matching_startup_diagnostics(self):
        args = self.args("--clients", "1")
        logs = {"server": SERVER + "Join succeeded: tester\n", "client1": WELCOME + LOAD}
        logs["client1"] += "LogAutomationTest: Error: Condition failed\n"
        self.assertTrue(session.evaluate_logs(logs, args)["connection_checks_passed"])
        logs["client1"] += "LogNet: NetworkFailure: ConnectionTimeout\n"
        self.assertFalse(session.evaluate_logs(logs, args)["connection_checks_passed"])

    def test_join_evidence_does_not_invent_unavailable_server_evidence(self):
        args = session.parser().parse_args(["join", "--map", "site01", "--address", "192.168.1.25:7777"])
        result = session.evaluate_logs({"client1": WELCOME + LOAD}, args)
        self.assertTrue(result["connection_checks_passed"])
        self.assertNotIn("server", result)

    def test_child_arguments_stay_literal_and_receive_unique_logs(self):
        args = self.args("--clients", "2", "--headless-clients")
        root = Path("/tmp/project with spaces $(not-shell)")
        log = root / "Saved/Playtest/run/client2.log"
        original = ["/tmp/editor with spaces", str(root / "ValStrike.uproject"), "127.0.0.1:7861",
                    "-game", "-abslog=/tmp/old.log"]
        with patch.object(session.ue, "command", return_value=original) as command:
            result = session.child_command(args, Path("/tmp/Engine"), root, "Mac", "client2", log)
        self.assertEqual(result[:3], original[:3])
        self.assertEqual([a for a in result if a.startswith("-abslog=")], [f"-abslog={log}"])
        self.assertIn("-nullrhi", result)
        self.assertIn("-nosound", result)
        args.headless_clients = False
        with patch.object(session.ue, "command", return_value=original):
            rendered = session.child_command(args, Path("/tmp/Engine"), root, "Mac", "client2", log)
        self.assertNotIn("-nosound", rendered)
        self.assertEqual(command.call_args.args[0].address, "127.0.0.1:7861")

    def test_missing_match_class_prevents_every_session_process(self):
        with tempfile.TemporaryDirectory(prefix="VS session failure ") as tmp:
            root = Path(tmp)
            (root / "Binaries/Mac").mkdir(parents=True)
            (root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib").write_bytes(b"unit fixture")
            engine = root / "Engine"
            (engine / "Build").mkdir(parents=True)
            (engine / "Build/Build.version").write_text("{}")
            checker = types.SimpleNamespace(checkout_revision=lambda r: {"head": "fixture"},
                validate_assets=lambda r, m: {"passed": True},
                native_identity=lambda r, m: {"module_sha256": "fixture", "engine_root": str(engine)},
                probe_native=lambda *a: (_ for _ in ()).throw(RuntimeError("VSMatchGameMode unavailable")))
            with patch.object(session.importlib, "import_module", return_value=checker), \
                 patch.object(session.ue, "host_platform", return_value="Mac"), \
                 patch.object(session.ue, "check_xcode", return_value="fixture"), \
                 patch.object(session.ue, "engine_path", return_value=engine), \
                 patch.object(session, "run_session") as run, \
                 redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                code = session.main(["host", "--map", "site01", "--mode", "match"], root)
            self.assertEqual(code, 1)
            run.assert_not_called()
            reports = list((root / "Saved/Playtest").glob("*/result.json"))
            self.assertEqual(len(reports), 1)
            report = json.loads(reports[0].read_text())
            self.assertFalse(report["passed"])
            self.assertIn("unavailable", report["error"])
            self.assertEqual(report["commands"], {})

    def test_selected_engine_must_match_build_receipt_for_practice_and_join(self):
        for action in (["host", "--map", "site01", "--mode", "practice"],
                       ["join", "--map", "site01", "--address", "192.168.1.25:7777"]):
            with self.subTest(action=action), tempfile.TemporaryDirectory(prefix="VS engine fixture ") as tmp:
                root = Path(tmp)
                engine = root / "Engine"
                (engine / "Build").mkdir(parents=True)
                (engine / "Build/Build.version").write_text("{}")
                checker = types.SimpleNamespace(checkout_revision=lambda r: {"head": "fixture"},
                    validate_assets=lambda r, m: {"passed": True},
                    native_identity=lambda r, m: {"engine_root": str(root / "OtherEngine")})
                with patch.object(session.importlib, "import_module", return_value=checker), \
                     patch.object(session.ue, "host_platform", return_value="Mac"), \
                     patch.object(session.ue, "check_xcode", return_value="fixture"), \
                     patch.object(session.ue, "engine_path", return_value=engine), \
                     patch.object(session, "run_session") as run, \
                     redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                    code = session.main(action, root)
                self.assertEqual(code, 1)
                run.assert_not_called()
                report_path, = (root / "Saved/Playtest").glob("*/result.json")
                report = json.loads(report_path.read_text())
                self.assertEqual(report["status"], "failed")
                self.assertFalse(report["passed"])
                self.assertIn("Selected engine differs", report["error"])
                self.assertEqual(report["commands"], {})

    def test_stop_during_startup_cleans_up_the_started_server(self):
        with tempfile.TemporaryDirectory(prefix="VS stop fixture ") as tmp:
            root = Path(tmp)
            child = types.SimpleNamespace(pid=123456, returncode=None, poll=lambda: None)
            def requested():
                raise session.StopRequested(signal.SIGTERM)
            with patch.object(session, "child_command", return_value=["fixture"]), \
                 patch.object(session.subprocess, "Popen", return_value=child), \
                 patch.object(session.socket, "socket"), \
                 patch.object(session, "stop_children") as stop, redirect_stdout(io.StringIO()):
                result = session.run_session(self.args(), root, root, root, requested)
            stop.assert_called_once_with([child])
            self.assertEqual(result["status"], "stopped")
            self.assertEqual(result["stop_signal"], signal.SIGTERM)
            self.assertFalse(result["passed"])

    def test_early_exit_during_startup_is_failure_and_cleans_up(self):
        with tempfile.TemporaryDirectory(prefix="VS exit fixture ") as tmp:
            root = Path(tmp)
            child = types.SimpleNamespace(pid=123456, returncode=7, poll=lambda: 7)
            with patch.object(session, "child_command", return_value=["fixture"]), \
                 patch.object(session.subprocess, "Popen", return_value=child), \
                 patch.object(session.socket, "socket"), \
                 patch.object(session, "stop_children") as stop, redirect_stdout(io.StringIO()):
                result = session.run_session(self.args(), root, root, root, lambda: None)
            stop.assert_called_once_with([child])
            self.assertEqual(result["status"], "failed")
            self.assertEqual(result["exit_codes_before_cleanup"], {"server": 7})
            self.assertFalse(result["passed"])

    @unittest.skipUnless(os.name == "posix", "owned process groups are POSIX")
    def test_cleanup_stops_only_owned_group(self):
        owned = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"], start_new_session=True)
        unrelated = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"], start_new_session=True)
        try:
            session.stop_children([owned])
            self.assertIsNotNone(owned.poll())
            self.assertIsNone(unrelated.poll())
        finally:
            if unrelated.poll() is None:
                os.killpg(unrelated.pid, signal.SIGKILL)
            unrelated.wait(timeout=2)
            if owned.poll() is None:
                os.killpg(owned.pid, signal.SIGKILL)
                owned.wait(timeout=2)


if __name__ == "__main__":
    unittest.main()
