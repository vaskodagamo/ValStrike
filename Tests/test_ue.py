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
        for material in ("M_Floor", "M_Wall", "M_Cover", "M_Spawn"):
            self.make(self.root / f"Content/Materials/Bootstrap/{material}.uasset")

    @staticmethod
    def make(path, text="fixture"):
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.suffix in (".umap", ".uasset") and text == "fixture":
            # Test-only package signature. These bytes are never loaded by Unreal.
            path.write_bytes(b"\xc1\x83\x2a\x9e" + b"fixture" * 20)
        else:
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

    def test_runtime_commands_use_separate_absolute_logs_without_console_windows(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        args = ue.parser().parse_args(["server", "--port", "7778"])
        server = ue.command(args, self.engine, self.root, "Mac")
        with patch.object(ue.os, "getpid", return_value=12345):
            client = ue.command(ue.parser().parse_args(["client"]), self.engine, self.root, "Mac")
        self.assertIn(f"-abslog={self.root / 'Saved/Logs/VS_Server_7778.log'}", server)
        self.assertIn(f"-abslog={self.root / 'Saved/Logs/VS_Client_12345.log'}", client)
        self.assertIn("-unattended", server)
        self.assertNotIn("-log", server)
        self.assertNotIn("-log", client)
        self.assertFalse((self.root / "Saved/Logs").exists())

    def test_runtime_log_directory_is_created_only_for_real_launch(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        original_check = ue.playtest_check
        def checked(name, *args):
            return {} if name == "validate_assets" else original_check(name, *args)
        with patch.object(ue, "playtest_check", side_effect=checked), \
             patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 0)) as run:
            self.assertEqual(self.run_cli(["--dry-run", "server"]), 0)
            run.assert_not_called()
            self.assertFalse((self.root / "Saved/Logs").exists())
            self.assertEqual(self.run_cli(["server"]), 0)
            self.assertTrue((self.root / "Saved/Logs").is_dir())

    def site_assets(self):
        self.make(self.root / "Content/Maps/Site01/VS_Site01.umap")
        for name in ("Paving", "Lane", "Site"):
            self.make(self.root / f"Content/Maps/Site01/Materials/M_Site01_{name}.uasset")
        for name in ("Plaster", "PaintedMetal", "Trim", "Accent"):
            self.make(self.root / f"Content/Art/Environment/Materials/M_VS_{name}.uasset")
        for name in ("Wall_400x300", "Pillar_40x300", "Cover_200x120", "Crate_100", "Vent_120x80"):
            self.make(self.root / f"Content/Art/Environment/Meshes/SM_VS_{name}.uasset")

    def test_allowlisted_server_modes_and_map_selection_do_not_change_editor_map(self):
        self.site_assets()
        for mode, cls in (("practice", "VSGameMode"), ("match", "VSMatchGameMode")):
            args = ue.parser().parse_args(["server", "--map", "site01", "--mode", mode])
            argv = ue.command(args, self.engine, self.root, "Mac")
            self.assertEqual(argv[2], "/Game/Maps/Site01/VS_Site01?game=/Script/ValStrike." + cls)
        editor = ue.command(ue.parser().parse_args(["editor", "--map", "site01"]), self.engine, self.root, "Mac")
        self.assertEqual(editor[2], "/Game/Maps/Site01/VS_Site01")
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        default = ue.command(ue.parser().parse_args(["server"]), self.engine, self.root, "Mac")
        self.assertEqual(default[2], "/Game/Maps/VS_Graybox?game=/Script/ValStrike.VSGameMode")
        with self.assertRaisesRegex(ue.SetupError, "requires --map site01"):
            ue.command(ue.parser().parse_args(["server", "--mode", "match"]), self.engine, self.root, "Mac")

    def test_server_contract_flags_map_to_allowlisted_switches(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        default = ue.command(ue.parser().parse_args(["server"]), self.engine, self.root, "Mac")
        self.assertFalse([item for item in default if item.startswith("-VSServer")])
        args = ue.parser().parse_args(["server", "--session-id", "lan-01_a", "--max-players", "2", "--exit-when-done",
                                       "--empty-timeout", "30", "--result-grace", "0"])
        argv = ue.command(args, self.engine, self.root, "Mac")
        self.assertEqual([item for item in argv if item.startswith("-VSServer")],
                         ["-VSServerSession=lan-01_a", "-VSServerMaxPlayers=2", "-VSServerExitWhenDone",
                          "-VSServerEmptyTimeout=30", "-VSServerResultGrace=0"])
        for timer in (["--empty-timeout", "30"], ["--result-grace", "5"]):
            with self.subTest(timer=timer), self.assertRaisesRegex(ue.SetupError, "require --exit-when-done"):
                ue.command(ue.parser().parse_args(["server"] + timer), self.engine, self.root, "Mac")

    def test_server_contract_flags_reject_invalid_values(self):
        for option, value in (("--session-id", "-lead"), ("--session-id", "a?game=x"), ("--session-id", "a b"),
                              ("--session-id", "x" * 65), ("--max-players", "0"), ("--max-players", "5"),
                              ("--empty-timeout", "0"), ("--empty-timeout", "86401"), ("--empty-timeout", "-1"),
                              ("--result-grace", "3601"), ("--result-grace", "1.5"), ("--max-players", "+2")):
            with self.subTest(option=option, value=value), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                ue.parser().parse_args(["server", option, value])

    def test_map_and_mode_reject_untrusted_urls_and_console_options(self):
        for option, value in (("--map", "/Game/Other"), ("--map", "site01?listen"),
                              ("--mode", "/Script/Other.Mode"), ("--mode", "match?Exec=Quit")):
            with self.subTest(option=option, value=value), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                ue.parser().parse_args(["server", option, value])

    def test_unhydrated_map_and_missing_dependency_fail_before_launch(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap", "version https://git-lfs.github.com/spec/v1\noid sha256:" + "0"*64 + "\nsize 99\n")
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "server"]), 1)
            run.assert_not_called()
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        (self.root / "Content/Materials/Bootstrap/M_Floor.uasset").unlink()
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "server"]), 1)
            run.assert_not_called()

    def test_match_dry_run_does_not_probe_or_create_output(self):
        self.site_assets()
        with patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["--dry-run", "server", "--map", "site01", "--mode", "match"]), 0)
            run.assert_not_called()
        self.assertFalse((self.root / "Saved").exists())

    def test_missing_compiled_match_class_prevents_server_launch(self):
        self.site_assets()
        calls = []
        def check(name, *args):
            calls.append(name)
            if name == "probe_native":
                raise ue.SetupError("Compiled /Script/ValStrike.VSMatchGameMode is unavailable; integrate and rebuild.")
            return {}
        with patch.object(ue, "playtest_check", side_effect=check), patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["server", "--map", "site01", "--mode", "match"]), 1)
            run.assert_not_called()
        self.assertIn("validate_assets", calls)
        self.assertIn("probe_native", calls)

    def test_map_generation_requires_output_even_if_process_exits_zero(self):
        with patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 0)):
            self.assertEqual(self.run_cli(["generate-map"]), 1)

    def test_partial_map_without_fresh_success_marker_is_a_failure(self):
        marker = ue.generation_marker(self.root)
        self.make(marker, json.dumps({"map": ue.MAP, "generator_version": ue.GENERATOR_VERSION, "actor_count": 20}))

        def failed_script(*args, **kwargs):
            self.assertFalse(marker.exists(), "A stale success result must be removed before execution")
            self.make(self.root / "Content/Maps/VS_Graybox.umap", "partial map")
            return ue.subprocess.CompletedProcess([], 0)

        with patch.object(ue.subprocess, "run", side_effect=failed_script):
            self.assertEqual(self.run_cli(["generate-map"]), 1)

    def test_complete_generation_requires_map_and_matching_success_marker(self):
        def successful_script(*args, **kwargs):
            self.make(self.root / "Content/Maps/VS_Graybox.umap")
            self.make(ue.generation_marker(self.root), json.dumps({"map": ue.MAP, "generator_version": ue.GENERATOR_VERSION, "actor_count": 20}))
            return ue.subprocess.CompletedProcess([], 0)

        with patch.object(ue.subprocess, "run", side_effect=successful_script):
            self.assertEqual(self.run_cli(["generate-map"]), 0)

    def test_generation_rejects_invalid_success_marker(self):
        self.make(self.root / "Content/Maps/VS_Graybox.umap")
        valid = {"map": ue.MAP, "generator_version": ue.GENERATOR_VERSION, "actor_count": 20}
        invalid = [{**valid, "map": "/Game/Other"}, {**valid, "generator_version": ue.GENERATOR_VERSION + 1},
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
        with patch.object(ue, "check_xcode"), patch.object(ue, "playtest_check", return_value={}) as check, \
             patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 7)) as run:
            self.assertEqual(self.run_cli(["build"]), 7)
            self.assertIsInstance(run.call_args.args[0], list)
            self.assertNotIn("shell", run.call_args.kwargs)
            self.assertEqual([call.args[0] for call in check.call_args_list], ["build_inputs"])

    def test_editor_build_records_only_successful_compilation_and_unchanged_inputs(self):
        snapshot = {"source_fingerprint": "before"}
        with patch.object(ue, "check_xcode"), \
             patch.object(ue, "playtest_check", side_effect=[snapshot, {"inputs": snapshot}]) as check, \
             patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 0)):
            self.assertEqual(self.run_cli(["build"]), 0)
            self.assertEqual(check.call_args_list[1].args, ("record_editor_build", self.root, self.engine, snapshot))
        with patch.object(ue, "check_xcode"), \
             patch.object(ue, "playtest_check", side_effect=[snapshot, ue.SetupError("Source changed during build")]), \
             patch.object(ue.subprocess, "run", return_value=ue.subprocess.CompletedProcess([], 0)):
            self.assertEqual(self.run_cli(["build"]), 1)

    def test_wrong_xcode_prevents_real_build(self):
        with patch.object(ue, "check_xcode", side_effect=ue.SetupError("wrong Xcode")), \
             patch.object(ue.subprocess, "run") as run:
            self.assertEqual(self.run_cli(["build"]), 1)
            run.assert_not_called()

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
        for value in ("127.0.0.1:7777?listen", "-game", "-option:7777", "localhost:0", "localhost:65536",
                      "999.1.2.3:7777", "127..1:7777", "host..local:7777", "host.-bad:7777",
                      "0.0.0.0:7777", "255.255.255.255:7777", "224.0.0.1:7777"):
            with self.subTest(address=value), self.assertRaises(ue.argparse.ArgumentTypeError):
                ue.address(value)
        for value in ("127.0.0.1:7777", "192.168.1.42:7777", "localhost:7777", "host-mac.local:7861"):
            self.assertEqual(ue.address(value), value)

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
