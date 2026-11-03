"""SET-05 integrity, evidence freshness and owned-process checks; no Unreal runs.

Fixtures use real temporary Git repositories containing LFS pointer blobs. The
native subprocess is mocked except the cancellation test's ordinary Python child.
"""
from contextlib import redirect_stderr, redirect_stdout
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).resolve().parents[2] / "Scripts/check_playtest.py"
SPEC = importlib.util.spec_from_file_location("playtest_preflight_under_test", SCRIPT)
check = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check)


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="ValStrike-preflight-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve() / "project spaces $(literal) `literal`"
        self.root.mkdir()
        self.engine = Path(self.temporary.name) / "engine spaces/Engine"
        self.body = check.PACKAGE_MAGIC[0] + bytes(range(128))
        self.paths = set(check.MAP_DEPENDENCIES["graybox"])
        self.paths.add("Content/Extra/UnselectedDependency.uasset")
        self.git("init", "-q")
        self.git("config", "user.name", "Preflight Fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.write("ValStrike.uproject", "{}")
        for relative in self.paths:
            self.write(relative, self.pointer(self.body))
        self.git("add", "--", ".")
        self.git("commit", "-qm", "LFS pointer fixture")
        for relative in self.paths:
            self.write(relative, self.body)

    def git(self, *args):
        environment = {**os.environ, "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull}
        return subprocess.run(["git", *args], cwd=self.root, env=environment,
                              check=True, capture_output=True, timeout=10).stdout

    def write(self, relative, value):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(value, bytes):
            path.write_bytes(value)
        else:
            path.write_text(value)
        return path

    @staticmethod
    def pointer(body, newline="\n"):
        return newline.join((check.LFS_VERSION, "oid sha256:" + hashlib.sha256(body).hexdigest(),
                             f"size {len(body)}", "")).encode("ascii")

    def compiled_fixture(self):
        self.write("Source/ValStrike/Fixture.cpp", "// fixture source, never compiled\n")
        self.write("Config/DefaultEngine.ini", "; fixture configuration\n")
        self.write("Binaries/Mac/UnrealEditor.modules", json.dumps({"BuildId": "fixture",
                    "Modules": {"ValStrike": "libUnrealEditor-ValStrike.dylib"}}))
        self.write("Binaries/Mac/libUnrealEditor-ValStrike.dylib", b"fixture compiled module, never loaded")
        version = self.engine / "Build/Build.version"
        version.parent.mkdir(parents=True)
        version.write_text(json.dumps({"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4}))
        editor = self.engine / "Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
        editor.parent.mkdir(parents=True)
        editor.write_text("fixture executable is mocked")
        check.record_editor_build(self.root, self.engine, check.build_inputs(self.root, self.engine))
        return editor

    def test_basic_check_has_no_git_subprocess_or_output_side_effects(self):
        with patch.object(check.subprocess, "run") as run, patch.object(check.subprocess, "Popen") as popen:
            result = check.basic_map_assets(self.root, "graybox")
        self.assertTrue(result["passed"])
        self.assertEqual(len(result["assets"]), len(check.MAP_DEPENDENCIES["graybox"]))
        run.assert_not_called()
        popen.assert_not_called()
        self.assertFalse((self.root / "Saved").exists())

    def test_map_and_mode_names_are_allowlisted(self):
        for name in ("/Game/Maps/VS_Graybox", "site01?game=Other", "../site01", "-game", "site01\n"):
            with self.subTest(name=name), self.assertRaises(check.PreflightError):
                check.basic_map_assets(self.root, name)
        for mode in ("match?listen", "/Script/ValStrike.VSMatchGameMode", "practice\n"):
            with self.subTest(mode=mode), self.assertRaises(check.PreflightError):
                check.probe_native(self.engine, self.root, "graybox", mode)

    def test_missing_known_dependency_is_rejected(self):
        (self.root / "Content/Materials/Bootstrap/M_Floor.uasset").unlink()
        with self.assertRaisesRegex(check.PreflightError, "Missing native asset.*M_Floor"):
            check.basic_map_assets(self.root, "graybox")

    def test_lfs_pointer_lf_crlf_and_truncated_package_are_rejected(self):
        relative = check.MAP_DEPENDENCIES["graybox"][0]
        for newline in ("\n", "\r\n"):
            self.write(relative, self.pointer(self.body, newline))
            with self.assertRaisesRegex(check.PreflightError, "Unresolved Git LFS pointer"):
                check.basic_map_assets(self.root, "graybox")
        for body in (b"", check.PACKAGE_MAGIC[0], b"not a native package" * 8):
            self.write(relative, body)
            with self.assertRaisesRegex(check.PreflightError, "Invalid or truncated"):
                check.basic_map_assets(self.root, "graybox")

    def test_native_asset_symlink_cannot_escape_checkout(self):
        relative = check.MAP_DEPENDENCIES["graybox"][0]
        external = Path(self.temporary.name) / "external.umap"
        external.write_bytes(self.body)
        target = self.root / relative
        target.unlink()
        target.symlink_to(external)
        with self.assertRaisesRegex(check.PreflightError, "escapes checkout"):
            check.basic_map_assets(self.root, "graybox")

    def test_complete_hydrated_native_inventory_matches_head_lfs(self):
        result = check.validate_assets(self.root, "graybox")
        self.assertTrue(result["passed"])
        self.assertEqual(result["committed_native_count"], len(self.paths))
        self.assertEqual({a["path"] for a in result["assets"]}, self.paths)

    def test_missing_unselected_tracked_directory_is_not_hidden_by_disk_glob(self):
        path = self.root / "Content/Extra/UnselectedDependency.uasset"
        path.unlink()
        path.parent.rmdir()
        with self.assertRaisesRegex(check.PreflightError, "Missing native asset.*UnselectedDependency"):
            check.validate_assets(self.root, "graybox")

    def test_same_size_corruption_and_size_mismatch_fail_integrity(self):
        relative = "Content/Extra/UnselectedDependency.uasset"
        self.write(relative, self.body[:-1] + b"\xff")
        with self.assertRaisesRegex(check.PreflightError, "SHA256 mismatch"):
            check.validate_assets(self.root, "graybox")
        self.write(relative, self.body + b"extra")
        with self.assertRaisesRegex(check.PreflightError, "size mismatch"):
            check.validate_assets(self.root, "graybox")

    def test_malformed_committed_pointer_is_not_accepted_as_native_evidence(self):
        relative = "Content/Extra/UnselectedDependency.uasset"
        self.write(relative, (check.LFS_VERSION + "\noid sha256:not-a-hash\nsize 132\n").encode())
        self.git("add", "--", relative)
        self.git("commit", "-qm", "Malformed pointer fixture")
        self.write(relative, self.body)
        with self.assertRaisesRegex(check.PreflightError, "Malformed LFS"):
            check.validate_assets(self.root, "graybox")

    def test_hydrated_but_uncommitted_selected_dependency_is_rejected(self):
        relative = "Content/Materials/Bootstrap/M_Floor.uasset"
        self.git("rm", "-q", "--cached", "--", relative)
        self.git("commit", "-qm", "Remove tracked dependency")
        with self.assertRaisesRegex(check.PreflightError, "not committed in HEAD"):
            check.validate_assets(self.root, "graybox")

    def test_revision_identifies_head_and_literal_untracked_names(self):
        special = "bug notes $(literal) `literal`\nsecond line.txt"
        self.write(special, "fixture")
        result = check.checkout_revision(self.root)
        self.assertEqual(result["head"], self.git("rev-parse", "HEAD").decode().strip())
        self.assertTrue(result["dirty"])
        self.assertIn({"status": "??", "path": special}, result["changes"])

    def test_module_manifest_is_required_and_cannot_escape_binaries(self):
        with self.assertRaisesRegex(check.PreflightError, "build receipt is missing"):
            check.native_identity(self.root, "graybox")
        self.compiled_fixture()
        self.write("Binaries/Mac/UnrealEditor.modules", json.dumps({"Modules": {"ValStrike": "../../wrong.dylib"}}))
        with self.assertRaisesRegex(check.PreflightError, "Unsafe or unexpected"):
            check.native_identity(self.root, "graybox")

    def test_build_receipt_rejects_source_changes_and_missing_compiled_outputs(self):
        self.compiled_fixture()
        expected = check.build_inputs(self.root, self.engine)
        self.write("Source/ValStrike/Fixture.cpp", "// concurrent source edit\n")
        with self.assertRaisesRegex(check.PreflightError, "changed during Editor build"):
            check.record_editor_build(self.root, self.engine, expected)
        with self.assertRaisesRegex(check.PreflightError, "receipt is stale"):
            check.native_identity(self.root, "graybox")
        (self.root / "Binaries/Mac/libUnrealEditor-ValStrike.dylib").unlink()
        with self.assertRaisesRegex(check.PreflightError, "module is missing or empty"):
            check.record_editor_build(self.root, self.engine, check.build_inputs(self.root, self.engine))

    def test_receipt_detects_added_source_config_and_engine_changes(self):
        self.compiled_fixture()
        self.write("Source/ValStrike/NewClass.h", "// new reflected class\n")
        with self.assertRaisesRegex(check.PreflightError, "receipt is stale"):
            check.native_identity(self.root, "graybox")
        check.record_editor_build(self.root, self.engine, check.build_inputs(self.root, self.engine))
        self.write("Config/DefaultEngine.ini", "; changed configuration\n")
        with self.assertRaisesRegex(check.PreflightError, "receipt is stale"):
            check.native_identity(self.root, "graybox")
        check.record_editor_build(self.root, self.engine, check.build_inputs(self.root, self.engine))
        version = self.engine / "Build/Build.version"
        data = json.loads(version.read_text())
        data["Changelist"] = 1234
        version.write_text(json.dumps(data))
        with self.assertRaisesRegex(check.PreflightError, "receipt is stale"):
            check.native_identity(self.root, "graybox")

    def fake_probe(self, transform=lambda value: value, status=0, produce=True):
        """Create a fresh nonce-matching result without ever executing Unreal."""
        def launch(argv, **kwargs):
            request_path = Path(kwargs["env"][check.PROBE_ENV])
            request = json.loads(request_path.read_text())
            if produce:
                result = {**request, "passed": True, "engine_version": "5.8.4-fixture",
                          "dependencies": [request["map_asset"]]}
                Path(request["result_path"]).write_text(json.dumps(transform(result)))
            class Child:
                pid = 987654321
                def wait(self, timeout):
                    return status
            return Child()
        return launch

    def test_native_probe_requires_fresh_nonce_and_uses_literal_argv(self):
        self.compiled_fixture()
        previous = {s: signal.getsignal(s) for s in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
        with patch.object(check.subprocess, "Popen", side_effect=self.fake_probe()) as launch, \
                patch.object(check, "stop_owned_process") as stop:
            result = check.probe_native(self.engine, self.root, "graybox", "practice")
        self.assertTrue(result["passed"])
        argv = launch.call_args.args[0]
        self.assertIsInstance(argv, list)
        self.assertEqual(argv[1], str(self.root / "ValStrike.uproject"))
        self.assertIn("-run=pythonscript", argv)
        self.assertNotIn("shell", launch.call_args.kwargs)
        stop.assert_called_once()
        self.assertEqual(previous, {s: signal.getsignal(s) for s in previous})

    def test_zero_exit_without_report_cannot_reuse_an_old_success(self):
        self.compiled_fixture()
        self.write("Saved/Playtest/native/old/result.json", '{"passed":true}')
        with patch.object(check.subprocess, "Popen", side_effect=self.fake_probe(produce=False)), \
                patch.object(check, "stop_owned_process") as stop:
            with self.assertRaisesRegex(check.PreflightError, "no nonce-bound result"):
                check.probe_native(self.engine, self.root, "graybox", "practice")
        stop.assert_called_once()

    def test_absent_reflected_match_class_is_a_failure_despite_source_presence(self):
        self.compiled_fixture()
        self.write("Source/ValStrike/Match/VSMatchGameMode.h", "class AVSMatchGameMode {};")
        check.record_editor_build(self.root, self.engine, check.build_inputs(self.root, self.engine))
        with patch.object(check.subprocess, "Popen", side_effect=self.fake_probe(
                lambda r: {**r, "passed": False, "error": "Compiled game mode is unavailable"})), \
                patch.object(check, "stop_owned_process"):
            with self.assertRaisesRegex(check.PreflightError, "Compiled game mode is unavailable"):
                check.probe_native(self.engine, self.root, "graybox", "match")

    def test_report_mismatches_and_nonzero_exit_are_rejected(self):
        self.compiled_fixture()
        for key in ("nonce", "project_root", "map_key", "map_asset", "mode", "game_mode_class", "identity"):
            with self.subTest(key=key), patch.object(check.subprocess, "Popen", side_effect=self.fake_probe(
                    lambda r, field=key: {**r, field: "wrong"})), patch.object(check, "stop_owned_process"):
                with self.assertRaisesRegex(check.PreflightError, "stale or mismatched"):
                    check.probe_native(self.engine, self.root, "graybox", "practice")
        with patch.object(check.subprocess, "Popen", side_effect=self.fake_probe(status=7)), \
                patch.object(check, "stop_owned_process"):
            with self.assertRaisesRegex(check.PreflightError, "exited 7"):
                check.probe_native(self.engine, self.root, "graybox", "practice")

    def test_changing_compiled_module_during_probe_invalidates_result(self):
        self.compiled_fixture()
        def mutate(result):
            self.write("Binaries/Mac/libUnrealEditor-ValStrike.dylib", b"changed native module")
            return result
        with patch.object(check.subprocess, "Popen", side_effect=self.fake_probe(mutate)), \
                patch.object(check, "stop_owned_process"):
            with self.assertRaisesRegex(check.PreflightError, "differs from its build receipt"):
                check.probe_native(self.engine, self.root, "graybox", "practice")

    def test_native_timeout_cleans_owned_child(self):
        self.compiled_fixture()
        with patch.object(check.subprocess, "Popen", side_effect=self.fake_probe(produce=False)), \
                patch.object(check, "PROBE_TIMEOUT_SECONDS", 0), patch.object(check, "stop_owned_process") as stop:
            with self.assertRaisesRegex(check.PreflightError, "timed out"):
                check.probe_native(self.engine, self.root, "graybox", "practice")
        stop.assert_called_once()

    def test_cli_rejects_report_escape_without_creating_it(self):
        outside = Path(self.temporary.name) / "outside-report.json"
        with redirect_stderr(io.StringIO()), redirect_stdout(io.StringIO()):
            self.assertEqual(check.main(["--report", str(outside)], root=self.root), 1)
        self.assertFalse(outside.exists())

    def test_lightweight_cli_reports_tools_and_revision_without_native_process(self):
        ue_stub = SimpleNamespace(host_platform=lambda: "Mac", engine_path=lambda value: self.engine,
                                  check_xcode=lambda: "Xcode 26.1.1; Metal compiler runs",
                                  editor_binary=lambda *args: self.engine / "FakeEditor",
                                  probe=lambda argv: "26.1" if "--show-sdk-version" in argv else "git-lfs/fixture")
        output = io.StringIO()
        with patch.dict(sys.modules, {"ue": ue_stub}), patch.object(check.platform, "machine", return_value="arm64"), \
                patch.object(check, "probe_native") as probe, redirect_stdout(output), redirect_stderr(io.StringIO()):
            self.assertEqual(check.main(["--map", "graybox"], root=self.root), 0)
        result = json.loads(output.getvalue())
        self.assertTrue(result["passed"])
        self.assertEqual(result["architecture"], "arm64")
        self.assertEqual(result["macos_sdk_version"], "26.1")
        self.assertFalse(result["native"]["checked"])
        self.assertEqual(result["assets"]["checked_head"], result["revision"]["head"])
        probe.assert_not_called()
        self.assertFalse((self.root / "Saved").exists())

    def test_cli_failure_invalidates_requested_old_success_report(self):
        report = self.write("Saved/Playtest/report.json", '{"passed":true}')
        def wrong_toolchain():
            raise ValueError("Expected Xcode 26.1.1")
        ue_stub = SimpleNamespace(host_platform=lambda: "Mac", engine_path=lambda value: self.engine,
                                  check_xcode=wrong_toolchain)
        with patch.dict(sys.modules, {"ue": ue_stub}), redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(check.main(["--report", "Saved/Playtest/report.json"], root=self.root), 1)
        self.assertFalse(json.loads(report.read_text())["passed"])

    @unittest.skipUnless(os.name == "posix", "POSIX owned-process-group cancellation")
    def test_sigterm_cancels_real_python_child_within_slot_cleanup_budget(self):
        editor = self.compiled_fixture()
        editor.unlink()
        editor.symlink_to(sys.executable)  # Fake Editor is Python, never Unreal.
        pid_file = Path(self.temporary.name) / "ordinary-python-child.pid"
        self.write("ValStrike.uproject", "import os,time\nfrom pathlib import Path\n"
                   f"Path({str(pid_file)!r}).write_text(str(os.getpid()))\n"
                   "time.sleep(60)\n")
        check.record_editor_build(self.root, self.engine, check.build_inputs(self.root, self.engine))
        worker = Path(self.temporary.name) / "probe-worker.py"
        worker.write_text("import importlib.util,sys\nfrom pathlib import Path\n"
                          f"spec=importlib.util.spec_from_file_location('p',{str(SCRIPT)!r})\n"
                          "p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)\n"
                          "try:\n"
                          f" p.probe_native(Path({str(self.engine)!r}),Path({str(self.root)!r}),'graybox','practice')\n"
                          "except p.PreflightError as error:\n print(error,flush=True);sys.exit(23)\n"
                          "sys.exit(0)\n")
        process = subprocess.Popen([sys.executable, str(worker)], stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, start_new_session=True)
        child_pid = None
        try:
            deadline = time.monotonic() + 5
            while not pid_file.is_file() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.02)
            self.assertTrue(pid_file.is_file(), "Fixture child did not start")
            child_pid = int(pid_file.read_text())
            started = time.monotonic()
            process.send_signal(signal.SIGTERM)
            output, _ = process.communicate(timeout=7)
            self.assertEqual(process.returncode, 23, output)
            self.assertIn("cancelled by signal", output)
            self.assertLess(time.monotonic() - started, 6)
            with self.assertRaises(ProcessLookupError):
                os.kill(child_pid, 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=2)
            if child_pid is not None:
                try:
                    os.kill(child_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass


if __name__ == "__main__":
    unittest.main()
