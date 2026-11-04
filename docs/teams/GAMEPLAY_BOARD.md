# Gameplay team board

Owner: Claude gameplay/engineering lead (delegated by the coordinator in [TEAM_BOARD.md](../TEAM_BOARD.md)). This board is the only place gameplay tasks, worktrees, path claims and the engineering engine-slot job are assigned. The coordinator keeps [TEAM_BOARD.md](../TEAM_BOARD.md), central docs and art. Dates are Europe/Berlin.

## Team (2026-11-04)

| Role | Session / provider | Checkout | State |
| --- | --- | --- | --- |
| Engineering lead | User-started Claude session | `.parallel-worktrees/team-gameplay`, branch `codex/team-gameplay` (historical prefix), fast-forwarded to `origin/main` `90849f5` | Active: this board, briefs, triage |
| Integration / QA owner (INT-01) | Claude session "INT-01 baseline verification", started by the lead | `.parallel-worktrees/team-integration`, branch `codex/team-integration`, base `90849f5` | Assigned; sole engineering user of the engine slot |
| Implementation workers | None yet | — | No concrete defect yet. Workers are assigned only for INT-01 findings, one owner per path set, at most two at once |

Paths are relative to `/Users/webproduktion3/Documents/ValStrike`. Engine: `/Users/webproduktion3/UE_5.8/UE_5.8` (UE 5.8.4, Xcode 26.1.1).

## Task registry

| ID | Result | Owner / checkout | Exclusive paths | State / validation |
| --- | --- | --- | --- | --- |
| INT-01 | One combined build of `origin/main` `90849f5` plus the affected suites once; per-suite pass/fail with evidence and owner-addressed defects | INT-01 QA session; `team-integration` | `docs/workstreams/integration-01.md`; its own ignored `Saved/`, `Binaries/`, `Intermediate/`. No source edits | In progress; V2. [Brief](../tasks/gameplay/INT-01-baseline-verification.md) |

Fix tasks get IDs `GPF-<nn>` here and a brief in `docs/tasks/gameplay/` before any worker starts.

## Engine slot

| Job | Owner | Scope | Release signal |
| --- | --- | --- | --- |
| INT-01 | INT-01 QA session | One Editor build, then the suite list in the brief, each wrapped in `Scripts/with_ue_slot.py` | Report committed to `docs/workstreams/integration-01.md` |

At assignment the kernel lock was free. `.parallel-worktrees/.ue-slot.lock` holds stale metadata from `match-runtime` (PID 10316, process gone). Exit 75 means stop and report "waiting for engine slot", not retry. No other engineering session builds or runs Unreal.

## Candidate

`origin/main` `90849f5`. Its source equals `95a0ba7`: no `Source/`, `Config/`, `Scripts/` or `Tests/` change after it, only docs. It includes merged PRs #16–#29: unity-build fix, server contract and join key/tick rate, settings menu, weapons switching, damage scoreboard and zones Phase 2, economy and kill rewards, shot effects and the join-key menu. No gameplay PR is open.

## Known open items (not yet fix tasks)

- **Join key in engine logs.** Unreal logs the travel URL and login options, so a keyed join writes the key into local client and server logs. Fixing the client side needs a `ULocalPlayer` subclass and a `DefaultEngine.ini` change. That is a central config decision, proposed to the coordinator. No fix task until the decision.
- **Exclusive Fullscreen on macOS** never ran. INT-01 records it as pending. It is not a defect.
- **UI session suite null-RHI 24/32** on the weapons base (`93b87c8`), versus 59/59 rendered on the UI branch. INT-01 runs it rendered once and classifies it.
