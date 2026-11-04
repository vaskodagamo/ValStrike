# Team board

## ART-09 — Modular reference expansion (2026-11-04)

The user requested isolated modular walls, corners, doors/garage entrances, roofs, ground tiles and crates for image-to-3D conversion. This coordinator owns [ART-09](tasks/art/ART-09-modular-environment-references.md) on `codex/modular-environment-references` in the root checkout, base `887d349`. Exclusive output: [sixteen-image pack](../Art/References/Environment/ModularIndustrialV1/README.md); implementation ready, V0 reference inspection only. Nominal dimensions and mesh cleanup notes accompany the images; no native-model/runtime verification is claimed. No engine slot or additional worker dispatch. Existing art ownership is unchanged.

Updated 2026-11-04 (Europe/Berlin). Coordinator-owned. Operating rules: [TEAM_WORKFLOW.md](TEAM_WORKFLOW.md). This board replaces old workstream ownership tables; old logs retain their evidence. No row silently reactivates a completed chat.

## Roles

| Responsibility | Current owner | Authority |
| --- | --- | --- |
| Game director | User | Scope, art direction, priorities, PR merges |
| Coordinator + art lead | This Codex coordinating chat | Central policy/board, art assignments and review, cross-team contracts |
| Engineering lead | User-started Claude session (reserved, not launched here) | Delegate engineering tasks and INT-01 through `docs/teams/GAMEPLAY_BOARD.md`; preserve art ownership |
| Coding contributors | Assigned by Claude gameplay lead | Isolated task worktrees, exact source paths, lightweight validation |
| Asset contributors | Four Codex sessions: ART-05 through ART-08 | Separate source-asset groups; native import deferred to integration |
| Integration / QA | Slot reserved for Claude lead to assign one INT-01 owner | One combined build/runtime batch; no worker build loops |

## Prepared worktree slots

Paths below are relative to `/Users/webproduktion3/Documents/ValStrike`. The art kickoff advances reserved idle slots to the assignment commit based on merged-policy main `66428e5`. Slots are not department-wide write permission. One concurrent writer per slot; additional contributors get additional task worktrees. Existing feature worktrees are preserved.

| Slot | Branch | Path | State |
| --- | --- | --- | --- |
| Gameplay lead | `codex/team-gameplay` | `.parallel-worktrees/team-gameplay` | Reserved for user-started Claude; kickoff below |
| First-person arms | `codex/team-art` | `.parallel-worktrees/team-art` | ART-05 dispatched; exclusive active owner |
| Site01 signs | `codex/art-site-signage` | `.parallel-worktrees/art-site-signage` | ART-06 dispatched; exclusive active owner |
| Third-person player | `codex/art-player-model` | `.parallel-worktrees/art-player-model` | ART-07 dispatched; High reasoning |
| HUD icons | `codex/art-hud-icons` | `.parallel-worktrees/art-hud-icons` | ART-08 dispatched; High reasoning |
| Integration / QA | `codex/team-integration` | `.parallel-worktrees/team-integration` | Reserved for one Claude-assigned INT-01 owner; no run started here |

The user selected Codex for art and Claude for gameplay on 2026-11-04. These assignments now govern the slots; branch prefixes do not define permissions. New Claude workers receive separate `claude/<task-id>` branches. [Claude lead kickoff](teams/CLAUDE_GAMEPLAY_KICKOFF.md).

## Task registry

| ID | Result | Owner / checkout | Exclusive paths | State / validation |
| --- | --- | --- | --- | --- |
| TEAM-01 | Shared hierarchy, validation policy, provider entry points and isolated workspaces | Coordinator; root `codex/team-workflow`, base `95a0ba7` | `AGENTS.md`, `CLAUDE.md`, `.github/pull_request_template.md`, `README.md`, `docs/TEAM_WORKFLOW.md`, `docs/TEAM_BOARD.md`, `docs/TASK_TEMPLATE.md`, current-policy notices in `docs/PARALLEL_WORK.md`, `docs/PLAYTEST_WORKSTREAMS.md`, `docs/SETUP.md`, workflow paragraphs in `docs/MVP_PLAN.md`, `docs/BACKLOG.md`, append to `docs/WORK_LOG.md` | Merged in PR #30 (`66428e5`); V0 checks passed, no Unreal build |
| INT-01 | Verify the combined current-main gameplay batch and report actionable failures | Delegated to user-started Claude engineering lead; integration slot reserved | Integration report `docs/workstreams/integration-01.md`; source fixes require exact owner in gameplay sub-board | Ready for one QA assignment; V2; use the batch below |
| ART-05 | First-person arms/gloves source-ready asset | Codex “Art — First-person arms and gloves” (`01a1a5af-02f5-7822-85ed-f2b741e45cf5`); team-art slot | Exact source/docs paths in [ART-05 brief](tasks/art/ART-05-first-person-arms.md) | Visual prototype rejected by user; modeling paused pending replacement brief/model. Selected concept A is recorded below |
| ART-06 | Two Site01 signs and additive placement recipe | Codex “Art — Site01 navigation signs” (`01a1a5af-0f13-78f1-8aaa-1903c1392df7`); art-site-signage slot | Exact source/docs paths in [ART-06 brief](tasks/art/ART-06-site01-signage.md) | Dispatched from `d4183ca`; V1 source only, no Unreal build |
| ART-07 | Third-person player model, source-ready | Codex “Art — Third-person player model” (`01a1a5ba-709c-7dd3-bc9e-d70ee4f951bc`), High reasoning | Exact paths in [ART-07 brief](tasks/art/ART-07-player-model.md) | Visual prototype rejected by user; modeling paused pending replacement brief/model. Selected concept A is recorded below |
| ART-08 | Eight tactical HUD icons | Codex “Art — Tactical HUD icons” (`01a1a5ba-7e9f-7c41-9b0f-38db0ea9a923`), High reasoning | Exact paths in [ART-08 brief](tasks/art/ART-08-hud-icons.md) | Dispatched from `d46a27c`; V1 source only |

Claude owns only its delegated sub-board, task briefs under `docs/tasks/gameplay/` and `docs/workstreams/gameplay-lead.md`; Codex retains central docs/policy. Engineering can allocate task-scoped `Source/`, `Config/`, `Scripts/`, `Tests/` paths. All art/native Content/maps and art asset documentation are read-only to engineering. ART-05 through ART-08 own no gameplay or Content paths in this source-only batch. No workers are authorized to create more art chats.

Before dispatch, replace unassigned fields with one actual session/provider and exact revision. Give the worker a brief from TASK_TEMPLATE.md. Contributors report status in their own handoff; the coordinator updates this table. Do not use previous broad ownership tables to claim Character, HUD, maps or shared test files.

## Next integration batch (queued, not running)

Baseline inspected: `origin/main` at `66428e5`. Select/record a fresh exact candidate at dispatch; remote main can change. Several implementation PRs have merged since the old playtest assignments, but their latest workstream logs leave combined verification pending. Merge state is not acceptance.

- Review the latest handoffs for damage zones/scoreboard, economy kill rewards, shot effects, settings/join-key UI and server lifecycle. Reuse their recorded commands/evidence; identify which final changes were not verified.
- One standard Editor build of the combined candidate, then affected damage/economy/shot-effects/join-key checks. Run shared movement/combat/match regressions once for this batch; use the relevant rendered UI check. Do not repeat each stream's whole historical test sequence.
- Keep real two-Mac LAN, four-player and packaged acceptance separate. Do not claim them from local editor runs.
- Return failures to the owning discipline with a small reproduction; coordinator assigns exact fix paths before another writer starts.

No heavy slot is currently reserved by this board. All four Codex art sessions do source/export work only; no Unreal imports are scheduled. Claude lead may reserve one INT-01 integration job in its sub-board; future art-native work requires coordination with that owner. Before reserving, inspect the existing lock/active work; a free-looking board is not proof that a legacy process stopped. On reassigning any existing chat, send the new policy and assignment explicitly and retire its old ownership.

## Reasoning and current engineering handoff

The user explicitly requested High reasoning for the two additional ART-07/08 chats on 2026-11-04. Creation must set `thinking: high`; model remains the user-configured default. This request does not change or restart ART-05/06. Further art dispatches should use High unless the user specifies otherwise.

Read-only inspection of the Claude gameplay sub-board found INT-01 assigned to its sole QA owner for main `90849f5`. Art workers must not launch Unreal or modify those active gameplay/integration worktrees.

## Environment visual target — 2026-11-04

The user selected the environment colors/material feel of the Operator V2 first-person composition. [Industrial V2 reference pack](../Art/References/Environment/IndustrialV2/README.md) provides six derived scene/asset sheets and the shared palette/construction brief. Use it for future environment-asset assignments. It does not replace Site01 layout/collision contracts or authorize new map work. Start with a representative wall/crate/paving sample and obtain visual review before mass-producing variants; no new worker is dispatched by this reference entry.
## Character art reset — 2026-11-04

The user rejected the procedural arms/gloves and third-person model as too blocky and simple. Both owners were told to preserve their work as unapproved prototypes and stop further modeling/import/animation pending a new brief. The user then selected **A — Urban Entry** from three generated concepts. References and artist/3D-tool handoff are in [Operator V2](../Art/References/Characters/OperatorV2/README.md). This approves the concept direction only; no replacement production mesh exists or is approved. The coordinator generated the references with the built-in image tool; no new Unreal work or third-party 3D generation was launched.
