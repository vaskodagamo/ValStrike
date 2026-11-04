# Task INT-01: verified status of the combined gameplay baseline on main

- Status: In progress
- Backlog ID: INT-01 ([TEAM_BOARD.md](../../TEAM_BOARD.md))
- Owner/session and provider: Claude session "INT-01 baseline verification" (integration/QA owner)
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `codex/team-integration`, fast-forward to `origin/main` `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-integration`
- Owns: `docs/workstreams/integration-01.md` (new), plus this checkout's ignored `Saved/`, `Binaries/` and `Intermediate/`
- Read-only dependencies / other owners: all of `Source/`, `Config/`, `Scripts/`, `Tests/`, `Content/`, `Art/` and the map. Do not edit them. Central docs belong to the coordinator. Art belongs to Codex (ART-05/06, no imports scheduled).
- Result and stopping condition: one standard (unity) Mac Development Editor build of `90849f5`, then each suite below run **once**. Stop when `integration-01.md` records, for each suite: command, PASS/FAIL/not run, counts and evidence path. Each failure gets a short defect (expected/actual, log lines, suspected owning area). Commit the report and open one docs-only PR. Do not fix source.
- Out of scope: source fixes, new tests or harnesses, reruns of passing suites, packaged/LAN/four-player/Windows/Linux acceptance, Exclusive Fullscreen
- Validation tier: V2 (central acceptance)

## Procedure

1. Confirm the checkout is clean, then `git fetch origin && git merge --ff-only origin/main`. Record `git rev-parse HEAD`; it must be `90849f5`. If main has moved, stop and report to the lead.
2. Confirm there are no `UnrealEditor`/`UnrealBuildTool` processes from other trees. Do not kill processes you do not own.
3. Run every command below from the worktree, prefixed with `python3 Scripts/with_ue_slot.py --`, one at a time, in this order. `UE=/Users/webproduktion3/UE_5.8/UE_5.8`. **Exit 75 means stop:** record "waiting for engine slot" and report to the lead. Do not sleep or retry.
4. If the build fails, record it and stop; nothing else is meaningful.
5. A suite that fails is not rerun unless the failure is clearly environmental (for example OS input reaching a rendered window, or a port in use). In that case, rerun once and record both runs.

| # | Command (after the slot wrapper) | Expected (from stream handoffs) |
| --- | --- | --- |
| 0 | `python3 Scripts/ue.py --engine $UE build --target Editor --platform Mac` | Exit 0, unity on |
| 1 | `python3 Tests/Damage/run_runtime.py --engine $UE` | PASS: server 23, driver 11, observer 12; `stats_final` K1 D1 HS1 DMG100 on both sides |
| 2 | `python3 Scripts/test_combat.py --engine $UE` | 55/55. The yaw fix `3952d4b` was never rerun |
| 3 | `python3 Scripts/test_economy.py --network --engine $UE` | PASS, 35 checks; ledger 2800/4000, 3300/3000, 5000/3500, 2500/4000, 4700/7200, 7400/9000, 800/800 |
| 4 | `python3 Scripts/test_shot_effects.py --engine $UE` | All checks, including `first_effect_no_stall`. Fixes `40a7a83`/`22b1d98` were never compiled |
| 5 | `python3 Scripts/test_weapons.py --engine $UE --scenario practice` | 74/74 |
| 6 | `python3 Scripts/test_weapons.py --engine $UE --scenario match` | 29/29 |
| 7 | `python3 Scripts/test_match.py --engine $UE` | 43/43 |
| 8 | `python3 Scripts/test_movement.py --engine $UE` | 45/45 |
| 9 | `python3 Scripts/test_combat_look.py --engine $UE` | 8/8 |
| 10 | `python3 Scripts/test_server_lifecycle.py --engine $UE` | All 4 scenarios PASS; revision equals HEAD |
| 11 | Native UI automation (command form in `Tests/UI/README.md`) with `-ExecCmds=Automation RunTests ValStrike.UI` | `Result={Success}` for ServerAddress, Settings, RoundPresentation, JoinKey. Ignore the two known startup `Condition failed` lines |
| 12 | `python3 Tests/UI/test_join_key.py --engine $UE` | Refused without the key, joined with it |
| 13 | `python3 Tests/UI/test_session.py --engine $UE --rendered` | 59/59 on the UI branch. Null-RHI showed 24/32 on `93b87c8`; classify any failure |

Engine-free suites are covered by the GitHub CI and are not repeated here.

Rendered runs (4, 9, 12, 13) receive real desktop input. Do not use the mouse/keyboard on their windows; record any contamination.

## Report format (`docs/workstreams/integration-01.md`)

Header: date, candidate SHA, engine/Xcode, machine, and the build's module identity if a runner reports it. Then one table: `# | suite | result | counts | evidence path`. Then one defect entry per failure: `INT-01-D<n>`, suite, expected/actual, key log lines (file:line, no full logs), suspected area (Damage/Combat/Economy/Effects/Weapons/Match/Server/UI). Then limits: Editor binaries, one Mac, loopback, no latency/loss, no packaged/LAN/2v2. Next action: lead triage.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner: lead triages defects into GPF tasks on the gameplay board
