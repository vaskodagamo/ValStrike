#!/usr/bin/env python3
"""Engine-free checks for the client join-key rule (Source/ValStrike/UI/VSJoinKey).

Compiles the C++ rule with the system compiler (no Unreal), runs its assertions,
and compares it with Scripts/ue.py's join_key() on shared examples so the menu,
the launcher and the server agree. Set VS_JOIN_KEY_SANITIZE=1 for ASan/UBSan.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Scripts"))
import ue

SOURCES = [ROOT / "Source/ValStrike/UI/VSJoinKey.cpp", ROOT / "Source/ValStrike/Server/VSServerContract.cpp",
           ROOT / "Tests/UI/VSJoinKeyTests.cpp"]
EXAMPLES = ["", "   ", "a" * 7, "a" * 8, "Z" * 64, "Z" * 65, "lan-Key_0001", "  lan-Key_0001  ", "lan Key_0001",
            "lan-Key.0001", "lan-Key_0001?listen", "lan-Key=0001", "län-Key_0001", "0123456789_-AZaz"]


def compiler():
    configured = os.environ.get("VS_JOIN_KEY_COMPILER")
    if configured:
        return [configured]
    if sys.platform == "darwin" and shutil.which("xcrun"):
        return ["xcrun", "clang++"]
    found = shutil.which("c++") or shutil.which("clang++") or shutil.which("g++")
    return [found] if found else None


class JoinKeyRuleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        command = compiler()
        if not command:
            raise unittest.SkipTest("no C++ compiler found")
        cls.directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.directory.name) / "join_key_tests"
        flags = ["-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-fno-exceptions", "-fno-rtti", "-O1", "-g"]
        if os.environ.get("VS_JOIN_KEY_SANITIZE") == "1":
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        build = subprocess.run(command + flags + ["-I", str(ROOT / "Source/ValStrike")] + [str(path) for path in SOURCES]
                               + ["-o", str(cls.binary)], capture_output=True, text=True)
        if build.returncode:
            raise AssertionError("compile failed:\n" + build.stdout + build.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_cpp_assertions_pass(self):
        run = subprocess.run([str(self.binary)], capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn("failures=0", run.stdout)

    def test_cpp_rule_matches_launcher_rule(self):
        run = subprocess.run([str(self.binary), "--classify"], input="\n".join(EXAMPLES) + "\n",
                             capture_output=True, text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stderr)
        results = run.stdout.split()
        self.assertEqual(len(results), len(EXAMPLES))
        for value, result in zip(EXAMPLES, results):
            trimmed = value.strip(" \t")
            if not trimmed:
                expected = "none"
            else:
                try:
                    ue.join_key(trimmed)
                    expected = "valid"
                except argparse.ArgumentTypeError:
                    expected = "invalid"
            self.assertEqual(result, expected, repr(value))


if __name__ == "__main__":
    unittest.main()
