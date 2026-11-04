# TEAM-02 — Recurring studio producer

- Owner: Codex “Direktor (2)”, thread `01a1a67c-4861-7e62-a15e-505444092624`.
- Branch/worktree: `codex/producer-workflow`, `/Users/webproduktion3/.codex/worktrees/10a1/ValStrike`; implementation base `09347c2`, fast-forwarded to `4876acc` before central edits.
- User mandate: implement the proposed single-producer workflow; check every day every 30 minutes while this Mac is available; up to 300 new Meshy credits/day; existing job approvals remain valid. User explicitly authorized messages to Direktor and the six existing art threads named in `docs/producer/control.json`. User additionally authorized coordination with the existing Claude gameplay and INT-ART-01 desktop chats. User retains merges.
- Ownership: `docs/PRODUCER.md`, `docs/producer/`, this brief, `Scripts/producer_budget.py`, `Tests/test_producer_budget.py`; coordinator-only updates to `docs/TEAM_WORKFLOW.md`, `docs/TEAM_BOARD.md`, `docs/BACKLOG.md`, `docs/WORK_LOG.md`, `README.md`, `CLAUDE.md`. Local recurring automation and Claude connection setup. No source/native asset or gameplay edits.
- Read-only: outgoing checkpoint `35e7abb` / PR #72, live art handoffs and Claude engineering board. Outgoing Direktor completed the explicit producer transfer; its later bounded ART-21 skill task is complete/merged, without restoring producer authority.
- Result: one scheduled producer that resumes this chat, reconciles actual work, assigns bounded follow-ups, reserves new Meshy spending centrally and queues one integration owner. Verified Claude command access; preserve existing external session ownership until a safe handoff.
- Validation: V1 for the spending guard; focused tests only for idempotency, budget/concurrency boundaries and unresolved reservations across midnight. V0 for policy/docs; a tools-disabled Claude smoke command. No Unreal work or new gameplay tests.
- Publication: commit/push and main-targeted PR; user merges. Do not merge main automatically.

## Handoff

Implementation ready. Automation `valstrike-studio-producer` is active in the same chat, every 30 minutes/every day. Four focused budget-boundary tests passed; subscription CLI probe and both authorized Claude UI handoffs succeeded. Exact schedule, first-cycle state and skill rollout are recorded in `docs/producer/STATUS.md` and the central work log. Implementation commit `a57cbda`; published as [PR #88](https://github.com/vaskodagamo/ValStrike/pull/88) targeting main. This publication record follows that implementation commit. User merges. Asset generation, native import and runtime acceptance remain separate tasks.
