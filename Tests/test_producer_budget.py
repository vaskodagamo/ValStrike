"""Spending-boundary checks only; no Meshy requests or Unreal processes."""

from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))
from producer_budget import transact


class ProducerBudgetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.ledger = Path(self.temp.name) / "ledger.json"
        self.now = datetime(2026, 11, 4, 22, 59, tzinfo=timezone.utc)
        self.call("init")

    def call(self, action, **kwargs):
        kwargs.setdefault("now", self.now)
        return transact(self.ledger, 300, action, **kwargs)

    def test_same_job_is_idempotent_and_does_not_reset_settlement(self):
        for _ in range(2):
            result = self.call("reserve", job_id="one", task="ART-21", credits=30)
        self.assertEqual(result["available"], 270)
        with self.assertRaises(ValueError):
            self.call("reserve", job_id="one", task="ART-22", credits=30)
        self.call("bind", job_id="one", task_id="meshy-id")
        self.call("settle", job_id="one", actual=25)
        result = self.call("reserve", job_id="one", task="ART-21", credits=30)
        self.assertEqual(result["available"], 275)
        self.assertEqual(result["jobs"]["one"]["actual"], 25)

    def test_parallel_reservations_cannot_overallocate(self):
        self.call("reserve", job_id="existing", task="ART-21", credits=150)
        def reserve(job):
            try:
                self.call("reserve", job_id=job, task="ART-22", credits=100)
                return True
            except ValueError:
                return False
        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(reserve, ["a", "b"]))
        self.assertEqual(sum(results), 1)
        self.assertEqual(self.call("status")["available"], 50)

    def test_midnight_keeps_unknown_jobs_and_uses_submission_day(self):
        self.call("reserve", job_id="pending", task="ART-21", credits=300)
        tomorrow = datetime(2026, 11, 4, 23, 1, tzinfo=timezone.utc)
        result = self.call("status", now=tomorrow)
        self.assertEqual(result["day"], "2026-11-05")
        self.assertEqual(result["available"], 0)
        self.call("bind", job_id="pending", task_id="meshy-id", now=tomorrow)
        result = self.call("settle", job_id="pending", actual=300, now=tomorrow)
        self.assertEqual(result["available"], 0)

    def test_missing_ledger_reset_and_overrun_fail_closed(self):
        with self.assertRaises(ValueError):
            self.call("init")
        with self.assertRaises(ValueError):
            transact(Path(self.temp.name) / "missing.json", 300, "status")
        self.call("reserve", job_id="one", task="ART-21", credits=30)
        self.call("bind", job_id="one", task_id="meshy-id")
        self.assertTrue(self.call("settle", job_id="one", actual=40)["blocked_overrun"])
        with self.assertRaises(ValueError):
            self.call("reserve", job_id="two", task="ART-22", credits=30)


if __name__ == "__main__":
    unittest.main()
