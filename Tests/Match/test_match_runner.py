"""Match runner contract/process tests. These tests never launch Unreal."""

from contextlib import redirect_stderr, redirect_stdout
from copy import deepcopy
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch


SCRIPTS = Path(__file__).resolve().parents[2] / "Scripts"
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location("match_runner", SCRIPTS / "test_match.py")
match = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(match)


def valid_reports():
    return {
        role: {"schema_version": 1, "role": role, "completed": True, "error": None,
               "checks": [{"name": name, "passed": True, "detail": {"observed": 1}}
                          for name in match.REQUIRED_CHECKS[role]]}
        for role in match.ROLES
    }


class MatchReportTests(unittest.TestCase):
    def setUp(self):
        self.reports = valid_reports()

    def test_required_scenarios_and_measured_details(self):
        checks = match.evaluate(self.reports)
        self.assertEqual(len(checks), 43)
        self.assertEqual(checks[0], {"check": "server: map_contract", "passed": True, "detail": {"observed": 1}})
        self.assertIn("deadline_gate", match.REQUIRED_CHECKS["server"])
        self.assertIn("twelve_rounds", match.REQUIRED_CHECKS["server"])
        for role in ("driver", "observer"):
            self.assertIn("replicated_match_complete", match.REQUIRED_CHECKS[role])
            self.assertIn("replicated_rematch", match.REQUIRED_CHECKS[role])

    def test_missing_extra_and_absent_roles_fail(self):
        for role, report in (("observer", None), ("server", [])):
            reports = deepcopy(self.reports)
            reports[role] = report
            with self.subTest(role=role), self.assertRaisesRegex(RuntimeError, "report"):
                match.evaluate(reports)
        self.reports["extra"] = self.reports["server"]
        with self.assertRaisesRegex(RuntimeError, "exactly"):
            match.evaluate(self.reports)
        del self.reports["extra"]
        del self.reports["driver"]
        with self.assertRaisesRegex(RuntimeError, "exactly"):
            match.evaluate(self.reports)

    def test_schema_identity_and_completion_are_strict(self):
        for field, value in (("role", "server"), ("schema_version", True), ("schema_version", 2),
                             ("completed", False), ("completed", 1), ("completed", "true")):
            reports = deepcopy(self.reports)
            reports["driver"][field] = value
            with self.subTest(field=field, value=value), self.assertRaisesRegex(RuntimeError, "identity, schema or completion"):
                match.evaluate(reports)

    def test_errors_and_missing_error_field_fail(self):
        for value in ("scenario timed out", 1, [], False):
            reports = deepcopy(self.reports)
            reports["server"]["error"] = value
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                match.evaluate(reports)
        del self.reports["server"]["error"]
        with self.assertRaisesRegex(RuntimeError, "error field"):
            match.evaluate(self.reports)

    def test_missing_duplicate_and_malformed_checks_fail(self):
        original = self.reports["server"]["checks"]
        for items in ([], None, original[:-1], original + original[:1], original + [None]):
            reports = deepcopy(self.reports)
            reports["server"]["checks"] = items
            with self.subTest(items=items), self.assertRaises(RuntimeError):
                match.evaluate(reports)
        for key, value in (("name", ""), ("name", " padded "), ("name", 1),
                           ("passed", 1), ("passed", "true"), ("passed", None)):
            reports = deepcopy(self.reports)
            reports["driver"]["checks"][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaisesRegex(RuntimeError, "Invalid or duplicate"):
                match.evaluate(reports)
        del self.reports["driver"]["checks"][0]["detail"]
        with self.assertRaisesRegex(RuntimeError, "Invalid or duplicate"):
            match.evaluate(self.reports)

    def test_false_and_extra_checks_are_retained(self):
        self.reports["server"]["checks"][0]["passed"] = False
        self.reports["driver"]["checks"].append({"name": "additional", "passed": False, "detail": "observed failure"})
        checks = match.evaluate(self.reports)
        self.assertEqual(len(checks), 44)
        self.assertFalse(checks[0]["passed"])
        self.assertFalse(all(check["passed"] for check in checks))

    def test_nonfinite_or_non_json_details_fail(self):
        for value in (float("nan"), {"nested": [float("inf")]}, object()):
            self.reports["server"]["checks"][0]["detail"] = value
            with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, "measured detail"):
                match.evaluate(self.reports)

    def test_report_loading_rejects_partial_duplicate_nonfinite_and_bad_encoding(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "server.json"
            self.assertIsNone(match.load_report(path))
            for content in ('{"partial":', '{"n":NaN}', '{"n":Infinity}', '{"n":1e999}',
                            '{"role":"driver","role":"server"}', '{"nested":{"a":1,"a":2}}'):
                path.write_text(content)
                with self.subTest(content=content), self.assertRaisesRegex(RuntimeError, "Malformed"):
                    match.load_report(path)
            path.write_bytes(b"\xff")
            with self.assertRaisesRegex(RuntimeError, "Malformed"):
                match.load_report(path)
            path.write_text(json.dumps(self.reports["server"]))
            self.assertEqual(match.load_report(path), self.reports["server"])


class MatchLaunchTests(unittest.TestCase):
    def test_arguments_are_bounded_to_assigned_ports_and_finite_timeouts(self):
        defaults = match.arguments([])
        self.assertEqual(defaults.port, 7831)
        self.assertEqual(defaults.timeout, 240)
        self.assertFalse(defaults.rendered)
        for port in range(7831, 7835):
            self.assertEqual(match.arguments(["--port", str(port)]).port, port)
        for argv in (["--port", "7830"], ["--port", "7835"], ["--port", "7831?game=x"],
                     ["--timeout", "nan"], ["--timeout", "inf"], ["--timeout", "0"], ["--timeout", "-1"]):
            with self.subTest(argv=argv), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                match.arguments(argv)

    def test_launch_identity_fixed_map_mode_and_literal_paths(self):
        output = Path("/tmp/project $(literal)/match output")
        commands = {role: match.launch_command(Path("/tmp/UE editor"), Path("/tmp/Game.uproject"),
                                               output, role, 7831) for role in match.ROLES}
        for role, command in commands.items():
            self.assertIn(f"-VSMatchOutput={output / (role + '.json')}", command)
            self.assertIn(f"-VSMatchSmoke={role}", command)
            self.assertIn("-nullrhi", command)
            self.assertIn("-nosound", command)
        self.assertEqual(commands["server"][2], "/Game/Maps/Site01/VS_Site01?game=/Script/ValStrike.VSMatchGameMode")
        self.assertIn("-multihome=127.0.0.1", commands["server"])
        self.assertIn("127.0.0.1:7831?Name=VSMatchDriver", commands["driver"])
        self.assertIn("127.0.0.1:7831?Name=VSMatchObserver", commands["observer"])
        self.assertNotIn("-server", commands["driver"])
        with self.assertRaisesRegex(ValueError, "Unknown"):
            match.launch_command(Path("/UE"), Path("/Game"), output, "unexpected", 7831)

    def test_rendered_option_only_changes_client_rendering(self):
        for role in match.LAUNCH_ROLES:
            command = match.launch_command(Path("/UE"), Path("/Game"), Path("/out"), role, 7832, True)
            self.assertEqual("-nullrhi" in command, role == "server")
            self.assertEqual("-windowed" in command, role != "server")
            if role != "server":
                self.assertIn("-ResX=960", command)
                self.assertIn("-ResY=540", command)

    def test_any_unexpected_exit_fails_even_with_success_exit_code(self):
        running = Mock()
        running.poll.return_value = None
        for code in (0, 1, -11):
            exited = Mock()
            exited.poll.return_value = code
            with self.subTest(code=code), self.assertRaisesRegex(RuntimeError, "observer process exited"):
                match.check_processes_running({"server": running, "observer": exited})

    def test_late_client_identity_has_no_required_report(self):
        command = match.launch_command(Path("/UE"), Path("/Game"), Path("/out"), "late", 7831)
        self.assertEqual(command[2], "127.0.0.1:7831?Name=VSMatchLate")
        self.assertIn("-VSMatchSmoke=late", command)
        self.assertIn("-game", command)
        self.assertIn("-nullrhi", command)
        self.assertNotIn("late", match.ROLES)
        self.assertNotIn("late", match.REQUIRED_CHECKS)

    def test_only_late_exit_after_teardown_request_is_expected(self):
        running = Mock()
        running.poll.return_value = None
        exited = Mock()
        exited.poll.return_value = -15
        with self.assertRaisesRegex(RuntimeError, "late process exited"):
            match.check_processes_running({"server": running, "late": exited})
        match.check_processes_running({"server": running, "late": exited}, late_teardown_requested=True)
        with self.assertRaisesRegex(RuntimeError, "server process exited"):
            match.check_processes_running({"server": exited, "late": exited}, late_teardown_requested=True)

    @unittest.skipUnless(os.name == "posix", "process-group cleanup is POSIX-specific")
    def test_cleanup_leaves_unrelated_process_alive(self):
        command = [sys.executable, "-c", "import time; time.sleep(20)"]
        owned = subprocess.Popen(command, start_new_session=True)
        unrelated = subprocess.Popen(command, start_new_session=True)
        try:
            match.stop([owned])
            self.assertIsNotNone(owned.poll())
            self.assertIsNone(unrelated.poll())
        finally:
            for process in (owned, unrelated):
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=3)


class MatchMetadataTests(unittest.TestCase):
    def test_source_module_and_map_hashes_use_actual_files_and_unknown_git_is_not_clean(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            files = {"Source/ValStrike/Match/Example.cpp": b"source", "Scripts/test_match.py": b"runner",
                     "Binaries/Mac/libUnrealEditor-ValStrike.dylib": b"module", "Content/Maps/Site01/VS_Site01.umap": b"map"}
            for relative, content in files.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)
            results = [subprocess.CompletedProcess([], 0, "revision123\n", ""),
                       subprocess.CompletedProcess([], 128, "", "LFS metadata unavailable")]
            with patch.object(match.subprocess, "run", side_effect=results):
                metadata = match.evidence_metadata(root)
            self.assertEqual(metadata["revision"], "revision123")
            self.assertIsNone(metadata["dirty_checkout"])
            self.assertEqual(metadata["git_status_error"], "LFS metadata unavailable")
            self.assertEqual(metadata["source_sha256"]["Source/ValStrike/Match/Example.cpp"], hashlib.sha256(b"source").hexdigest())
            self.assertEqual(metadata["editor_module_sha256"], hashlib.sha256(b"module").hexdigest())
            self.assertEqual(metadata["map_sha256"], hashlib.sha256(b"map").hexdigest())


class MatchLifecycleTests(unittest.TestCase):
    def run_fixture(self, *, exit_code=None, timeout=False, malformed=False, failed_check=False,
                    changed_source=False, late_exit_code=None, late_markers=True, leave_without_join=False,
                    spectate_stage=False):
        """Exercise runner orchestration with fake editor handles and real report files."""
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in ("ValStrike.uproject", "Content/Maps/Site01/VS_Site01.umap",
                             "Binaries/Mac/libUnrealEditor-ValStrike.dylib"):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("fixture")
            engine = root / "Engine"
            (engine / "Build").mkdir(parents=True)
            (engine / "Build/Build.version").write_text('{"MajorVersion":5,"MinorVersion":8,"PatchVersion":4}')
            reports = valid_reports()
            if failed_check:
                reports["server"]["checks"][0]["passed"] = False
            handles = []
            output = None
            cleanup_batches = []

            def publish_reports():
                for key, value in reports.items():
                    (output / f"{key}.json").write_text(json.dumps(value))
                if malformed:
                    (output / "observer.json").write_text('{"incomplete":')

            def launch(command, **kwargs):
                nonlocal output
                self.assertTrue(kwargs["start_new_session"])
                self.assertEqual(kwargs["cwd"], root)
                role = next(arg.split("=", 1)[1] for arg in command if arg.startswith("-VSMatchSmoke="))
                report_path = Path(next(arg.split("=", 1)[1] for arg in command if arg.startswith("-VSMatchOutput=")))
                output = report_path.parent
                if role == "server":
                    marker = match.LATE_LEAVE_MARKER if leave_without_join else match.LATE_JOIN_MARKER
                    (output / "server.log").write_text("LogNet: listening on port 7831\n" + (marker if late_markers else ""))
                    if spectate_stage:
                        with (output / "server.log").open("a") as stream:
                            stream.write("\n" + match.SPECTATE_JOIN_MARKER + "\n" + STAGE + "\n")
                process = Mock(pid=100 + len(handles))
                process.fixture_role = role
                process.poll.return_value = exit_code if role == "driver" else late_exit_code if role == "late" else None
                handles.append(process)
                if role == "late":
                    with (output / "server.log").open("a") as stream:
                        stream.write("\n" + match.LATE_LEAVE_MARKER)
                elif role == "driver" and not late_markers and not timeout:
                    publish_reports()
                return process

            def cleanup(processes):
                processes = list(processes)
                cleanup_batches.append([process.fixture_role for process in processes])
                for process in processes:
                    if process.poll() is None:
                        process.poll.return_value = -15
                if len(processes) == 1 and processes[0].fixture_role == "late" and not timeout:
                    publish_reports()

            before = {"source_sha256": {"source": "hash"}, "editor_module_sha256": "module", "map_sha256": "map"}
            after = deepcopy(before)
            if changed_source:
                after["source_sha256"]["source"] = "changed"
            with patch.object(match.ue, "ROOT", root), patch.object(match.ue, "host_platform", return_value="Mac"), \
                    patch.object(match.ue, "engine_path", return_value=engine), \
                    patch.object(match.ue, "editor_binary", return_value=Path("/fake/editor")), \
                    patch.object(match.socket, "socket"), patch.object(match, "evidence_metadata", side_effect=[before, after]), \
                    patch.object(match.subprocess, "Popen", side_effect=launch), patch.object(match, "stop", side_effect=cleanup), \
                    patch.object(match.time, "monotonic", side_effect=[0, 241, 242] if timeout else None, return_value=0), \
                    patch.object(match.time, "sleep"), \
                    redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                status = match.main([])
            paths = list((root / "Saved/Tests/Match").glob("*/result.json"))
            self.assertEqual(len(paths), 1)
            result = json.loads(paths[0].read_text())
            result["fixture_cleanup_batches"] = cleanup_batches
            return status, result

    def test_success_requires_all_reports_and_records_processes_and_commands(self):
        status, result = self.run_fixture()
        self.assertEqual(status, 0)
        self.assertTrue(result["passed"])
        self.assertEqual(set(result["commands"]), set(match.LAUNCH_ROLES))
        self.assertEqual(result["exit_before_cleanup"], {**{role: None for role in match.ROLES}, "late": -15})
        self.assertEqual(result["exit_after_cleanup"], {role: -15 for role in match.LAUNCH_ROLES})
        self.assertEqual(result["engine_build"]["PatchVersion"], 4)
        self.assertTrue(result["late_client"]["teardown_completed"])
        self.assertEqual(result["fixture_cleanup_batches"][0], ["late"])
        self.assertEqual(set(result["fixture_cleanup_batches"][1]), set(match.ROLES))
        self.assertNotIn("late", result["reports"])

    def test_completed_reports_do_not_hide_an_unexpected_exit(self):
        status, result = self.run_fixture(exit_code=7, late_markers=False)
        self.assertEqual(status, 1)
        self.assertFalse(result["passed"])
        self.assertIn("driver process exited", result["error"])

    def test_auxiliary_exit_before_requested_teardown_fails(self):
        status, result = self.run_fixture(late_exit_code=0)
        self.assertEqual(status, 1)
        self.assertIn("late process exited", result["error"])
        self.assertFalse(result["late_client"]["teardown_requested"])

    def test_leave_without_join_and_completed_reports_without_probe_fail(self):
        for options, message in (({"leave_without_join": True}, "before requesting its join"),
                                 ({"late_markers": False}, "lack a real requested late-client")):
            with self.subTest(options=options):
                status, result = self.run_fixture(**options)
                self.assertEqual(status, 1)
                self.assertIn(message, result["error"])

    def test_timeout_and_malformed_report_fail_with_retained_evidence(self):
        for options, message in (({"timeout": True}, "Timed out"), ({"malformed": True}, "Malformed")):
            with self.subTest(options=options):
                status, result = self.run_fixture(**options)
                self.assertEqual(status, 1)
                self.assertFalse(result["passed"])
                self.assertIn(message, result["error"])

    def test_failed_native_check_and_mid_run_source_change_fail(self):
        status, result = self.run_fixture(failed_check=True)
        self.assertEqual(status, 1)
        self.assertFalse(result["checks"][0]["passed"])
        status, result = self.run_fixture(changed_source=True)
        self.assertEqual(status, 1)
        self.assertIn("changed during", result["error"])


def spectate_line(event, viewer, target, round_id):
    payload = json.dumps({"event": event, "viewer": viewer, "target": target, "round": round_id}, separators=(",", ":"))
    return f"[2026.11.04-10.00.00:000][  1]LogValStrike: Display: VSSPECTATE {payload}"


class MatchSpectateTests(unittest.TestCase):
    """GPT-13 sample VSSPECTATE lines; driver is id 1 on team 1, observer id 2 on team 2."""

    def setUp(self):
        self.reports = valid_reports()
        for role, team, ident in (("driver", 1, 1), ("observer", 2, 2)):
            check = next(item for item in self.reports[role]["checks"] if item["name"] == "replicated_match")
            check["detail"] = f"team={team} id={ident} round=1"

    def statuses(self, driver="", observer="", server=None):
        return {check["check"]: check["status"]
                for check in match.evaluate_spectate(self.reports, {"driver": driver, "observer": observer}, server)}

    def test_parse_accepts_structured_lines_and_ignores_others(self):
        text = "LogNet: unrelated\n" + spectate_line("start", 1, 3, 4) + "\n" + spectate_line("stop", 1, 0, 5)
        self.assertEqual(match.parse_spectate(text), [{"event": "start", "viewer": 1, "target": 3, "round": 4},
                                                      {"event": "stop", "viewer": 1, "target": 0, "round": 5}])

    def test_parse_rejects_malformed_events(self):
        for payload in ('{"event":"start"', '{"event":"jump","viewer":1,"target":3,"round":1}',
                        '{"event":"start","viewer":1,"target":0,"round":1}', '{"event":"stop","viewer":1,"target":3,"round":1}',
                        '{"event":"start","viewer":0,"target":3,"round":1}', '{"event":"start","viewer":1,"target":3}',
                        '{"event":"start","viewer":1,"target":true,"round":1}', '{"event":"start","viewer":1,"target":3,"round":1,"x":1}',
                        '{"event":"start","viewer":1,"viewer":1,"target":3,"round":1}'):
            with self.subTest(payload=payload), self.assertRaisesRegex(RuntimeError, "VSSPECTATE"):
                match.parse_spectate("LogValStrike: Display: VSSPECTATE " + payload)

    def test_no_spectate_is_not_reached_never_a_pass(self):
        statuses = self.statuses()
        self.assertEqual(set(statuses.values()), {"not_reached"})
        self.assertEqual(len(statuses), 3)
        absent = match.evaluate_spectate(self.reports, {"driver": None, "observer": None})
        self.assertEqual({check["status"] for check in absent}, {"not_reached"})
        self.assertIn("log absent", absent[0]["detail"]["reason"])

    def test_teammate_start_then_reset_stop_passes(self):
        driver = "\n".join((spectate_line("start", 1, 3, 4), spectate_line("stop", 1, 0, 5)))
        self.assertEqual(set(self.statuses(driver).values()), {"pass"})

    def test_enemy_self_or_foreign_viewer_target_fails(self):
        for line in (spectate_line("start", 1, 2, 4), spectate_line("start", 1, 1, 4), spectate_line("start", 2, 3, 4)):
            with self.subTest(line=line):
                statuses = self.statuses(line + "\n" + spectate_line("stop", 1, 0, 5))
                self.assertEqual(statuses["spectate: teammate_start"], "fail")
                self.assertEqual(statuses["spectate: no_enemy_or_dead_target"], "fail")
        statuses = self.statuses("\n".join((spectate_line("start", 1, 3, 4), spectate_line("cycle", 1, 2, 4),
                                            spectate_line("stop", 1, 0, 5))))
        self.assertEqual(statuses["spectate: teammate_start"], "pass")
        self.assertEqual(statuses["spectate: no_enemy_or_dead_target"], "fail")

    def test_missing_stop_or_bad_sequence_fails_and_same_round_stop_is_not_reached(self):
        for driver in (spectate_line("start", 1, 3, 4),
                       "\n".join((spectate_line("start", 1, 3, 4), spectate_line("start", 1, 3, 4), spectate_line("stop", 1, 0, 5))),
                       "\n".join((spectate_line("start", 1, 3, 4), spectate_line("cycle", 1, 3, 4), spectate_line("stop", 1, 0, 5)))):
            with self.subTest(driver=driver):
                self.assertEqual(self.statuses(driver)["spectate: stop_on_round_reset"], "fail")
        statuses = self.statuses("\n".join((spectate_line("start", 1, 3, 4), spectate_line("stop", 1, 0, 4))))
        self.assertEqual(statuses["spectate: stop_on_round_reset"], "not_reached")
        self.assertEqual(statuses["spectate: teammate_start"], "pass")

    def test_missing_identity_with_events_fails(self):
        next(item for item in self.reports["driver"]["checks"] if item["name"] == "replicated_match")["detail"] = {"observed": 1}
        statuses = self.statuses("\n".join((spectate_line("start", 1, 3, 4), spectate_line("stop", 1, 0, 5))))
        self.assertEqual(statuses["spectate: teammate_start"], "fail")

    def test_runner_records_not_reached_without_failing_and_fails_on_enemy_target(self):
        status, result = MatchLifecycleTests.run_fixture(self)
        self.assertEqual(status, 0)
        self.assertEqual(len(result["checks"]), 43)
        self.assertEqual({check["status"] for check in result["spectate_checks"]}, {"not_reached"})
        original = match.evaluate_spectate
        enemy = "\n".join((spectate_line("start", 1, 2, 4), spectate_line("stop", 1, 0, 5)))
        with patch.object(match, "evaluate_spectate",
                          side_effect=lambda reports, logs, server: original(self.reports, {**logs, "driver": enemy}, server)):
            status, result = MatchLifecycleTests.run_fixture(self)
        self.assertEqual(status, 1)
        self.assertFalse(result["passed"])
        self.assertIn("fail", {check["status"] for check in result["spectate_checks"]})


STAGE = "[2026.11.04-10.00.00:000][  1]LogValStrike: Display: VSSPECTATE_STAGE " + \
    json.dumps({"viewer": 1, "teammate": 4, "enemy": 2, "round": 7}, separators=(",", ":"))


class MatchSpectateStageTests(unittest.TestCase):
    """GPF-03 stage: driver (1) dies while mate (4) lives; observer (2) is the enemy."""

    setUp = MatchSpectateTests.setUp
    statuses = MatchSpectateTests.statuses

    def stage_run(self, *lines):
        return self.statuses("\n".join(lines), server="LogNet: listening\n" + STAGE)

    def test_stage_line_is_strict(self):
        self.assertEqual(match.parse_spectate_stage(STAGE), {"viewer": 1, "teammate": 4, "enemy": 2, "round": 7})
        self.assertIsNone(match.parse_spectate_stage(None))
        self.assertIsNone(match.parse_spectate_stage(spectate_line("start", 1, 4, 7)))
        for payload in ('{"viewer":1', '{"viewer":1,"teammate":4,"enemy":2}', '{"viewer":1,"teammate":1,"enemy":2,"round":7}',
                        '{"viewer":0,"teammate":4,"enemy":2,"round":7}', '{"viewer":1,"teammate":4,"enemy":2,"round":7,"x":1}'):
            with self.subTest(payload=payload), self.assertRaisesRegex(RuntimeError, "VSSPECTATE_STAGE"):
                match.parse_spectate_stage("VSSPECTATE_STAGE " + payload)
        with self.assertRaisesRegex(RuntimeError, "More than one"):
            match.parse_spectate_stage(STAGE + "\n" + STAGE)

    def test_stage_start_on_teammate_and_reset_stop_pass(self):
        statuses = self.stage_run(spectate_line("start", 1, 4, 7), spectate_line("stop", 1, 0, 8))
        self.assertEqual(set(statuses.values()), {"pass"})

    def test_stage_without_spectate_or_reset_is_a_failure_not_not_reached(self):
        statuses = self.stage_run()
        self.assertEqual(statuses["spectate: teammate_start"], "fail")
        self.assertEqual(statuses["spectate: stop_on_round_reset"], "fail")
        self.assertEqual(statuses["spectate: no_enemy_or_dead_target"], "not_reached")
        statuses = self.stage_run(spectate_line("start", 1, 4, 7), spectate_line("stop", 1, 0, 7))
        self.assertEqual(statuses["spectate: teammate_start"], "pass")
        self.assertEqual(statuses["spectate: stop_on_round_reset"], "fail")

    def test_stage_wrong_round_viewer_or_dead_enemy_target_fails(self):
        for lines in ((spectate_line("start", 1, 4, 6), spectate_line("stop", 1, 0, 8)),
                      (spectate_line("start", 1, 2, 7), spectate_line("stop", 1, 0, 8))):
            with self.subTest(lines=lines):
                self.assertEqual(self.stage_run(*lines)["spectate: teammate_start"], "fail")
        statuses = self.stage_run(spectate_line("start", 1, 4, 7), spectate_line("cycle", 1, 2, 7), spectate_line("stop", 1, 0, 8))
        self.assertEqual(statuses["spectate: no_enemy_or_dead_target"], "fail")
        next(item for item in self.reports["driver"]["checks"] if item["name"] == "replicated_match")["detail"] = "team=1 id=9 round=1"
        self.assertEqual(self.stage_run()["spectate: teammate_start"], "fail")


class MatchSpectateClientTests(unittest.TestCase):
    def test_mate_launch_identity_has_no_required_report(self):
        command = match.launch_command(Path("/editor"), Path("/p.uproject"), Path("/out"), match.MATE_ROLE, 7831)
        self.assertIn("-VSMatchSmoke=mate", command)
        self.assertIn("127.0.0.1:7831?Name=VSMatchMate", command)
        self.assertNotIn(match.MATE_ROLE, match.ROLES)
        self.assertNotIn(match.MATE_ROLE, match.REQUIRED_CHECKS)
        self.assertNotIn(match.SPECTATE_JOIN_MARKER, (match.LATE_JOIN_MARKER, match.LATE_LEAVE_MARKER))

    def test_runner_launches_mate_on_marker_and_a_reached_stage_needs_real_spectate(self):
        status, result = MatchLifecycleTests.run_fixture(self, spectate_stage=True)
        self.assertTrue(result["spectate_client"]["launched"])
        self.assertIn(match.MATE_ROLE, result["commands"])
        self.assertEqual(result["exit_after_cleanup"][match.MATE_ROLE], -15)
        self.assertIn(match.MATE_ROLE, result["fixture_cleanup_batches"][-1])
        # The fixture's client logs carry no VSSPECTATE lines, so a reached stage fails the run.
        self.assertEqual(status, 1)
        self.assertEqual(len(result["checks"]), 43)
        statuses = {check["check"]: check["status"] for check in result["spectate_checks"]}
        self.assertEqual(statuses["spectate: teammate_start"], "fail")
        self.assertEqual(result["spectate_checks"][0]["detail"]["stage"]["teammate"], 4)


if __name__ == "__main__":
    unittest.main()
