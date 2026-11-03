"""Unit tests for helper safety; these do not execute or validate Unreal."""

from contextlib import redirect_stderr, redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("ue", Path(__file__).resolve().parents[1] / "Scripts/ue.py")
ue = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ue)


class UnrealHelperTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ValStrike tests ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve() / "project with spaces $(no-shell)"
        self.root.mkdir()
        (self.root / "ValStrike.uproject").write_text("{}")
        self.engine = Path(self.temp.name).resolve() / "engine with spaces/Engine"
        self.make(self.engine / "Build/Build.version", json.dumps({"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4}))
        self.make(self.engine / "Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor")
        self.make(self.engine / "Build/BatchFiles/Mac/Build.sh")
        self.make(self.root / "Scripts/create_graybox.py")

    @staticmethod
    def make(path, text="fixture"):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def run_cli(self, args):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()), patch.object(ue, "host_platform", return_value="Mac"):
            return ue.main(["--engine", str(self.engine.parent)] + args, root=self.root)

    def test_engine_root_and_engine_directory_resolve_identically(self):
        self.assertEqual(ue.engine_path(self.engine.parent), self.engine)
        self.assertEqual(ue.engine_path(self.engine), self.engine)

    def test_missing_and_wrong_engine_fail_actionably(self):
        with patch.dict(ue.os.environ, {}, clear=True):
            with self.assertRaisesRegex(ue.SetupError, "UE_ROOT"):
                ue.engine_path()
        self.make(self.engine / "Build/Build.version", '{"MajorVersion":5,"MinorVersion":8,"PatchVersion":3}')
        with self.assertRaisesRegex(ue.SetupError, "differs from pinned"):
            ue.engine_path(self.engine)

    def test_dry_run_never_invokes_subprocess(self):
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "build"]), 0)
            run.assert_not_called()

    def test_spaces_and_metacharacters_stay_in_single_arguments(self):
        args = ue.parser().parse_args(["generate-map"])
        argv = ue.command(args, self.engine, self.root, "Mac")
        self.assertEqual(argv[1], str(self.root / "ValStrike.uproject"))
        self.assertIn(f"-ExecutePythonScript={self.root / 'Scripts/create_graybox.py'}", argv)
        self.assertIn("/Engine/Maps/Entry", argv)

    def test_launch_requires_generated_map_even_in_dry_run(self):
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "server"]), 1)
            run.assert_not_called()

    def test_map_generation_requires_output_even_if_process_exits_zero(self):
        with patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 0)):
            self.assertEqual(self.run_cli(["generate-map"]), 1)

    def test_partial_map_without_fresh_success_marker_is_a_failure(self):
        marker = ue.generation_marker(self.root)
        self.make(marker, json.dumps({"map": ue.MAP, "generator_version": 1, "actor_count": 20}))

        def failed_script(*args, **kwargs):
            self.assertFalse(marker.exists(), "A stale success result must be removed before execution")
            self.make(self.root / "Content/Maps/VS_Graybox.umap", "partial map")
            return ue.subprocess.CompletedProcess([], 0)

        with patch.object(ue.subprocess, "run", side_effect=failed_script):
            self.assertEqual(self.run_cli(["generate-map"]), 1)

    def test_complete_generation_requires_map_and_matching_success_marker(self):
        def successful_script(*args, **kwargs):
            self.make(self.root / "Content/Maps/VS_Graybox.umap")
            self.make(ue.generation_marker(self.root), json.dumps({"map": ue.MAP, "generator_version": 1, "actor_count": 20}))
            return ue.subprocess.CompletedProcess([], 0)

        with patch.object(ue.subprocess, "run", side_effect=successful_script):
            self.assertEqual(self.run_cli(["generate-map"]), 0)

    def test_generation_rejects_invalid_success_marker(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        valid = {"map": ue.MAP, "generator_version": 1, "actor_count": 20}
        invalid = [{**valid, "map": "/Game/Other"}, {**valid, "generator_version": 2},
                   {**valid, "actor_count": 0}, {**valid, "actor_count": True}, []]
        for result in invalid:
            with self.subTest(result=result):
                self.make(ue.generation_marker(self.root), json.dumps(result))
                with self.assertRaises(ue.SetupError):
                    ue.verify_generation(self.root)

    def test_generate_map_dry_run_does_not_remove_success_marker(self):
        marker = ue.generation_marker(self.root)
        self.make(marker, "existing marker")
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "generate-map"]), 0)
            run.assert_not_called()
        self.assertEqual(marker.read_text(), "existing marker")

    def test_existing_map_will_not_be_overwritten(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["generate-map"]), 1)
            run.assert_not_called()

    def test_build_failure_status_is_preserved(self):
        with patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 7)) as run:
            self.assertEqual(self.run_cli(["build"]), 7)
            self.assertIsInstance(run.call_args.args[0], list)
            self.assertNotIn("shell", run.call_args.kwargs)

    def test_cross_host_build_and_installed_server_build_are_rejected(self):
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["build", "--platform", "Win64"]), 1)
            self.make(self.engine / "Build/InstalledBuild.txt")
            self.assertEqual(self.run_cli(["build", "--target", "Server"]), 1)
            run.assert_not_called()

    def test_doctor_dry_run_does_not_run_tool_probes(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "doctor"]), 0)
            run.assert_not_called()

    def test_client_rejects_travel_options_and_invalid_ports(self):
        for value in ("127.0.0.1:7777?listen", "-game", "-option:7777", "localhost:0", "localhost:65536"):
            with self.subTest(address=value), self.assertRaises(ue.argparse.ArgumentTypeError):
                ue.address(value)

    def test_xcode_rejects_command_line_tools_and_wrong_version(self):
        with patch.object(ue, "probe", return_value="/Library/Developer/CommandLineTools"):
            with self.assertRaisesRegex(ue.SetupError, "Full Xcode"):
                ue.check_xcode()
        with patch.object(ue, "probe", side_effect=["/Applications/Xcode.app/Contents/Developer", "Xcode 26.4"]):
            with self.assertRaisesRegex(ue.SetupError, "Expected Xcode 26.1.1"):
                ue.check_xcode()

    def test_xcode_requires_a_real_metal_compiler_path(self):
        with patch.object(ue, "probe", side_effect=["/Applications/Xcode.app/Contents/Developer", "Xcode 26.1.1\nBuild version test", str(self.root / "missing-metal")]):
            with self.assertRaisesRegex(ue.SetupError, "Metal toolchain"):
                ue.check_xcode()

    def test_xcode_rejects_present_metal_stub_that_cannot_run(self):
        metal = self.root / "metal-stub"
        self.make(metal)
        outputs = ["/Applications/Xcode.app/Contents/Developer", "Xcode 26.1.1\nBuild version test",
                   str(metal), ue.SetupError("MetalToolchain is missing")]
        with patch.object(ue, "probe", side_effect=outputs) as probe:
            with self.assertRaisesRegex(ue.SetupError, "stub is not a working compiler"):
                ue.check_xcode()
            self.assertEqual(probe.call_args.args[0], ["xcrun", "--sdk", "macosx", "metal", "--version"])

    def test_windows_batch_build_rejects_shell_expansion_paths(self):
        self.make(self.engine / "Build/BatchFiles/Build.bat")
        unsafe_root = self.root / "percent%PATH%"
        self.make(unsafe_root / "ValStrike.uproject", "{}")
        args = ue.parser().parse_args(["build"])
        with self.assertRaisesRegex(ue.SetupError, "metacharacters"):
            ue.command(args, self.engine, unsafe_root, "Win64")


if __name__ == "__main__":
    unittest.main()
