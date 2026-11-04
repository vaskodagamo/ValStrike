# Team board

Updated 2026-11-04 (Europe/Berlin). Coordinator-owned. Operating rules: [TEAM_WORKFLOW.md](TEAM_WORKFLOW.md). This board replaces old workstream ownership tables; old logs retain their evidence. No row silently reactivates a completed chat.

## Roles

| Responsibility | Current owner | Authority |
| --- | --- | --- |
| Game director | User | Scope, art direction, priorities, PR merges |
| Coordinator + engineering/art leads | Coordinating chat for this workflow | Assign exact paths, review handoffs, select integration batch; split lead roles only when useful |
| Coding contributors | Unassigned until a task is dispatched | One assigned change and lightweight validation |
| Asset contributors | Unassigned until a task is dispatched | One assigned asset/source group and scheduled authoring |
| Integration / QA | Prepared slot, no active worker | Combined compile/runtime verification when dispatched |

## Prepared worktree slots

Paths below are relative to `/Users/webproduktion3/Documents/ValStrike`. All start at the team-workflow policy commit based on main `95a0ba7`. These are available slots, not department-wide write permission or active tasks. One concurrent writer per slot; additional contributors get additional task worktrees. Existing feature worktrees are preserved.

| Slot | Branch | Path | State |
| --- | --- | --- | --- |
| Gameplay | `codex/team-gameplay` | `.parallel-worktrees/team-gameplay` | Idle; reserve before use |
| Art | `codex/team-art` | `.parallel-worktrees/team-art` | Idle; reserve before use |
| Integration / QA | `codex/team-integration` | `.parallel-worktrees/team-integration` | Idle; reserve before use |

Codex and Claude may use any reserved slot. A future Claude task may instead receive a fresh `claude/<task-id>` branch. Provider names never override the task's ownership.

## Task registry

| ID | Result | Owner / checkout | Exclusive paths | State / validation |
| --- | --- | --- | --- | --- |
| TEAM-01 | Shared hierarchy, validation policy, provider entry points and isolated workspaces | Coordinator; root `codex/team-workflow`, base `95a0ba7` | `AGENTS.md`, `CLAUDE.md`, `.github/pull_request_template.md`, `README.md`, `docs/TEAM_WORKFLOW.md`, `docs/TEAM_BOARD.md`, `docs/TASK_TEMPLATE.md`, current-policy notices in `docs/PARALLEL_WORK.md`, `docs/PLAYTEST_WORKSTREAMS.md`, `docs/SETUP.md`, workflow paragraphs in `docs/MVP_PLAN.md`, `docs/BACKLOG.md`, append to `docs/WORK_LOG.md` | Implementation ready; V0; diff/link/worktree checks, no Unreal build |
| INT-01 | Verify the combined current-main gameplay batch and report actionable failures | Unassigned; proposed integration slot | No source ownership yet; coordinator must assign any fixes/handoff file | Ready for assignment; V2; use the batch below |

Before dispatch, replace unassigned fields with one actual session/provider and exact revision. Give the worker a brief from TASK_TEMPLATE.md. Contributors report status in their own handoff; the coordinator updates this table. Do not use previous broad ownership tables to claim Character, HUD, maps or shared test files.

## Next integration batch (queued, not running)

Baseline inspected: `origin/main` at `95a0ba7`. Select/record a fresh exact candidate at dispatch; remote main can change. Several implementation PRs have merged since the old playtest assignments, but their latest workstream logs leave combined verification pending. Merge state is not acceptance.

- Review the latest handoffs for damage zones/scoreboard, economy kill rewards, shot effects, settings/join-key UI and server lifecycle. Reuse their recorded commands/evidence; identify which final changes were not verified.
- One standard Editor build of the combined candidate, then affected damage/economy/shot-effects/join-key checks. Run shared movement/combat/match regressions once for this batch; use the relevant rendered UI check. Do not repeat each stream's whole historical test sequence.
- Keep real two-Mac LAN, four-player and packaged acceptance separate. Do not claim them from local editor runs.
- Return failures to the owning discipline with a small reproduction; coordinator assigns exact fix paths before another writer starts.

No heavy slot is currently reserved by this board. This change does not launch builds, editors, tests or new chats. Before reserving, inspect the existing lock/active work; a free-looking board is not proof that a legacy process stopped. On reassigning any existing chat, send the new policy and assignment explicitly and retire its old ownership.
