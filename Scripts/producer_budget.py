#!/usr/bin/env python3
"""Atomic admission ledger for producer-assigned Meshy jobs; never calls Meshy."""

import argparse
from datetime import datetime
import fcntl
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
BERLIN = ZoneInfo("Europe/Berlin")


def totals(jobs, day, limit):
    used = sum(
        job["credits"] if job["actual"] is None else job["actual"]
        for job in jobs.values()
        if job["actual"] is None or job.get("submitted_day", job["day"]) == day
    )
    overrun = any(job["actual"] is not None and job["actual"] > job["credits"]
                  for job in jobs.values())
    return {"day": day, "limit": limit, "committed": used,
            "available": max(0, limit - used), "blocked_overrun": overrun}


def transact(path, limit, action, *, job_id=None, task=None, credits=None,
             task_id=None, actual=None, now=None):
    """Serialize read/modify/write; unresolved reservations consume today's cap."""
    day = (now or datetime.now(BERLIN)).astimezone(BERLIN).date().isoformat()
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.with_suffix(".lock").open("a+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if action == "init":
            if path.exists():
                raise ValueError("Ledger already exists; never reset it to obtain more allowance")
            data = {"schema_version": 1, "jobs": {}}
        else:
            if not path.exists():
                raise ValueError("Ledger missing: reconcile spending before explicit initialization")
            data = json.loads(path.read_text())
            if data.get("schema_version") != 1 or not isinstance(data.get("jobs"), dict):
                raise ValueError("Invalid ledger; stop paid dispatch and recover the recorded state")
        jobs = data["jobs"]
        before = totals(jobs, day, limit)
        if action == "reserve":
            if not job_id or not re.fullmatch(r"[A-Za-z0-9_.-]+", job_id) or not task:
                raise ValueError("A stable job ID and assigned task are required")
            if type(credits) is not int or not 0 < credits <= limit:
                raise ValueError("Reservation must be a positive whole credit amount within the cap")
            if job_id in jobs:
                if jobs[job_id]["task"] != task or jobs[job_id]["credits"] != credits:
                    raise ValueError("Job ID already belongs to a different allocation")
            else:
                if before["blocked_overrun"]:
                    raise ValueError("An actual charge exceeded its allocation; producer review required")
                if credits > before["available"]:
                    raise ValueError("Daily allowance exhausted; pending jobs remain reserved")
                jobs[job_id] = {"task": task, "credits": credits, "day": day,
                                "task_id": None, "actual": None}
        elif action in ("bind", "settle"):
            if job_id not in jobs:
                raise ValueError("Unknown reservation; do not invent or recreate an API job")
            job = jobs[job_id]
            if action == "bind":
                if not task_id:
                    raise ValueError("Meshy task ID required")
                if job["task_id"] not in (None, task_id):
                    raise ValueError("Reservation already has a different Meshy task ID")
                if job["actual"] is not None and job["task_id"] is None:
                    raise ValueError("Cannot submit a settled unsubmitted job")
                if job["task_id"] is None:
                    job["task_id"] = task_id
                    job["submitted_day"] = day
            else:
                if type(actual) is not int or actual < 0:
                    raise ValueError("Actual charge must be confirmed nonnegative whole credits")
                if actual > 0 and not job["task_id"]:
                    raise ValueError("Bind the actual Meshy task before recording a nonzero charge")
                if job["actual"] not in (None, actual):
                    raise ValueError("Already settled with a different actual charge")
                job["actual"] = actual
        elif action not in ("init", "status"):
            raise ValueError("Unknown ledger action")
        if action != "status":
            fd, name = tempfile.mkstemp(prefix=".ledger-", dir=path.parent)
            try:
                with os.fdopen(fd, "w") as output:
                    json.dump(data, output, indent=2)
                    output.write("\n")
                    output.flush()
                    os.fsync(output.fileno())
                os.replace(name, path)
            finally:
                if os.path.exists(name):
                    os.unlink(name)
        return {**totals(jobs, day, limit), "jobs": jobs}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", type=Path,
                        help="Producer ledger; override only for an explicit transfer or isolated test")
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("init")
    sub.add_parser("status")
    reserve = sub.add_parser("reserve")
    reserve.add_argument("--job", required=True)
    reserve.add_argument("--task", required=True)
    reserve.add_argument("--credits", required=True, type=int)
    bind = sub.add_parser("bind")
    bind.add_argument("--job", required=True)
    bind.add_argument("--task-id", required=True)
    settle = sub.add_parser("settle")
    settle.add_argument("--job", required=True)
    settle.add_argument("--actual", required=True, type=int)
    args = parser.parse_args()
    try:
        policy = json.loads((ROOT / "docs/producer/control.json").read_text())
        path = args.ledger or Path(policy["producer_checkout"]) / policy["meshy_ledger"]
        result = transact(path, policy["meshy_new_credits_per_day"], args.action,
                          job_id=getattr(args, "job", None), task=getattr(args, "task", None),
                          credits=getattr(args, "credits", None), task_id=getattr(args, "task_id", None),
                          actual=getattr(args, "actual", None))
        print(json.dumps(result, indent=2))
        return 0
    except (ValueError, KeyError, OSError) as error:
        print(f"Producer budget: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
