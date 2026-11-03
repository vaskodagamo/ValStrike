"""Real lock/process tests in temporary Git worktrees; no Unreal is launched."""

import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "Scripts/with_ue_slot.py"


@unittest.skipUnless(os.name == "posix", "slot uses POSIX flock and signals")
class UnrealSlotTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ValStrike slot ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / "repo with spaces"
        self.other = self.root / "other worktree"
        self.git("init", "-q", str(self.repo))
        # An empty fixture commit is needed for an actual linked worktree.
        self.git("-C", str(self.repo), "-c", "user.name=Slot Test", "-c",
                 "user.email=slot@example.invalid", "-c", "commit.gpgsign=false",
                 "commit", "--allow-empty", "-qm", "temporary test fixture")
        self.git("-C", str(self.repo), "worktree", "add", "-qb", "other", str(self.other))
        for checkout in (self.repo, self.other):
            (checkout / "Scripts").mkdir()
            shutil.copyfile(SCRIPT, checkout / "Scripts/with_ue_slot.py")

    @staticmethod
    def git(*args):
        subprocess.run(["git", *args], check=True, capture_output=True, timeout=10)

    def command(self, checkout, *child):
        return [sys.executable, str(checkout / "Scripts/with_ue_slot.py"), "--", *child]

    def run_slot(self, checkout, *child):
        return subprocess.run(self.command(checkout, *child), cwd=checkout,
                              capture_output=True, text=True, timeout=5)

    def start_slot(self, code, *args):
        proc = subprocess.Popen(self.command(self.repo, sys.executable, "-c", code, *args),
                                cwd=self.repo, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop_owned, proc)
        return proc

    @staticmethod
    def stop_owned(proc):
        if proc.poll() is None:
            proc.terminate()
        try:
            proc.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate(timeout=5)

    def wait_file(self, path, proc):
        deadline = time.monotonic() + 5
        while not path.exists():
            self.assertIsNone(proc.poll(), "wrapper exited before child checkpoint")
            if time.monotonic() >= deadline:
                self.fail("child checkpoint timed out")
            time.sleep(0.01)

    def assert_slot_available(self):
        result = self.run_slot(self.other, sys.executable, "-c", "pass")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_literal_arguments_and_exit_status(self):
        args = ["space here", "$(touch forbidden)", "; touch forbidden", "", "*", "--", "line\nbreak"]
        result = self.run_slot(self.repo, sys.executable, "-c",
                               "import json,sys; print(json.dumps(sys.argv[1:])); sys.exit(37)", *args)
        self.assertEqual(result.returncode, 37, result.stderr)
        self.assertEqual(json.loads(result.stdout), args)
        self.assertFalse((self.repo / "forbidden").exists())
        self.assert_slot_available()

    def test_linked_worktrees_contend_and_release(self):
        ready = self.root / "ready"
        proc = self.start_slot("import pathlib,sys; pathlib.Path(sys.argv[1]).touch(); sys.stdin.readline()",
                               str(ready))
        self.wait_file(ready, proc)
        result = self.run_slot(self.other, sys.executable, "-c", "print('must not run')")
        self.assertEqual(result.returncode, 75, result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn(f'"owner_pid": {proc.pid}', result.stderr)
        self.assertIn('"child_pid":', result.stderr)
        self.assertIn(str(self.repo), result.stderr)
        proc.communicate(input="\n", timeout=5)
        self.assertEqual(proc.returncode, 0)
        # The metadata remains; it must never act as a stale PID-file lock.
        self.assertTrue((self.repo / ".parallel-worktrees/.ue-slot.lock").is_file())
        self.assert_slot_available()

    def test_interruption_keeps_slot_until_child_finishes(self):
        ready, signalled = self.root / "ready", self.root / "signalled"
        code = ("import pathlib,signal,sys; "
                "signal.signal(signal.SIGTERM, lambda *_: pathlib.Path(sys.argv[2]).touch()); "
                "pathlib.Path(sys.argv[1]).touch(); sys.stdin.readline(); sys.exit(23)")
        proc = self.start_slot(code, str(ready), str(signalled))
        self.wait_file(ready, proc)
        proc.send_signal(signal.SIGTERM)
        self.wait_file(signalled, proc)
        result = self.run_slot(self.other, sys.executable, "-c", "pass")
        self.assertEqual(result.returncode, 75, result.stderr)
        proc.communicate(input="\n", timeout=5)
        self.assertEqual(proc.returncode, 23)
        self.assert_slot_available()

    def test_signal_exit_propagates_and_releases(self):
        for signum in (signal.SIGTERM, signal.SIGKILL):
            with self.subTest(signal=signum):
                result = self.run_slot(self.repo, sys.executable, "-c",
                                       f"import os; os.kill(os.getpid(), {int(signum)})")
                self.assertEqual(result.returncode, -signum, result.stderr)
                self.assert_slot_available()

    def test_missing_executable_releases_slot(self):
        result = self.run_slot(self.repo, str(self.root / "missing executable"))
        self.assertEqual(result.returncode, 127, result.stderr)
        self.assertIn("not found", result.stderr)
        self.assert_slot_available()


if __name__ == "__main__":
    unittest.main()
