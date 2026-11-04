# INT-05: verify main `ee25083` plus PR #59 (GPT-10) and PR #60 (GPT-11)

- Date: 2026-11-04 (Europe/Berlin), runs 14:09–14:25
- Task brief: `docs/tasks/gameplay/INT-05-gpt10-11.md` (on `claude/gameplay-board-int04`)
- Owner: the integration/QA session, assigned by the Claude gameplay lead
- Candidate SHA: `40d10a32109e50c1e99a96ecd828fea1090aef55`, on the local disposable `claude/integration-05` (not pushed). It is `origin/main` `ee25083` (verified in INT-04) plus two `--no-ff` merges, in the briefed order, with no conflicts:

| Order | PR | Task | Tip |
| --- | --- | --- | --- |
| 1 | #59 | GPT-10 spread crosshair | `c3c7dd0` (branch tip unchanged) |
| 2 | #60 | GPT-11 fire-request rules | `2c3dc1e` (branch tip unchanged) |

- Engine/toolchain/machine: UE 5.8.4 (CL 58902052), Xcode 26.1.1, Apple M4 (Mac16,10), macOS 15.6. This is the same setup as INT-01..04.
- Build identity:
  - `ValStrikeEditor Mac Development`, unity on (`Module.ValStrike.1–3.cpp`), 0 errors and 0 warnings.
  - Candidate Editor receipt `ae06b6a495e1a0ff6b75aeee5c127225db5fac82a1039f293ceada626e7bea68`.
  - The runners report `revision=40d10a3…` and a clean checkout.
- Engine slot: every command ran under `python3 Scripts/with_ue_slot.py --`. None returned 75.

## Results

Evidence lives in this checkout's ignored `Saved/`. `Saved/INT-05/NN.log` is each command's console output.

| # | suite | result | counts | evidence path |
| --- | --- | --- | --- | --- |
| 0 | `Scripts/ue.py build --target Editor --platform Mac` | PASS | 4/4 actions, unity, receipt verified. First compile of both PRs | `Saved/INT-05/00.log` |
| 1 | `Scripts/test_weapons.py --scenario practice` | PASS | 74/74. Rejection checks match INT-03 (`Saved/Tests/Weapons/20261104T104033Z_practice_5c8a78/`): the same 7 server checks all PASS (`reject_equip_time`, `reject_holstered`, `reject_duplicate`, `reject_spam`, `reject_invalid`, `reject_phase`, `reject_dead`), with identical reasons | `Saved/Tests/Weapons/20261104T131036Z_practice_631cbe/` |
| 2 | `Scripts/test_combat.py` | PASS | 55/55 | `Saved/Tests/Combat/20261104T131129Z_cbadb3/` |
| 3 | `Tests/UI/test_session.py --rendered`, run 1 | FAIL (environmental) | 70/72: `initial_combat_defaults_available`, then `runtime_report_complete` as a consequence | `Saved/Tests/UI/20261104T131253Z_f732c3/` |
| 3 | `Tests/UI/test_session.py --rendered`, rerun | PASS | 72/72 | `Saved/Tests/UI/20261104T131944Z_9dec07/` |
| 4 | `Tests/Damage/run_runtime.py` (the INT-01 damage runner), run 1 | FAIL (environmental) | `native_scenario_timeout`; real-fire checks failed, see below | `Saved/Tests/DamageRuntime/20261104T131345Z_88c276/` |
| 4 | `Tests/Damage/run_runtime.py`, rerun | PASS | 46/46 (server 23, driver 11, observer 12), equal to the INT-01 count; `stats_final` score 1:1 | `Saved/Tests/DamageRuntime/20261104T132349Z_2196b1/` |
| A | Attribution only: `run_runtime.py` on `origin/main` `ee25083` (without #59/#60) | PASS | 46/46. Used only to separate code from environment, not as a candidate result | `Saved/Tests/DamageRuntime/20261104T132100Z_2aadff/` |

### Rerun justification

Both first runs show the same signature: the client's rifle fired before the harness sent any fire input. The rendered client windows can receive real desktop input, and both failures came within about 90 s of each other (log times 13:13:11 and 13:14:37 UTC). I did not send any desktop input. Both reruns passed on the same candidate binaries, so I record this as environmental. There is no direct proof of a physical click in the logs; that cause is inferred.

- **Session run 1:** the first HUD snapshot, taken before any harness input, shows `"magazine":29` (`driver.log`, `VSUI_HUD phase=initial`). So one shot had already been fired. INT-03's session run 1 had the identical signature on main without GPT-10/11.
- **Damage run 1:**
  - During stage 4, the crouched-layout check, `server.log` records four rifle hits from the driver on the observer, at 13:14:37.487 / .606 and 13:14:38.569 / .709. That is two pairs about 1 s apart, consistent with two clicks rather than held auto-fire.
  - The harness presses `LeftMouseButton` only from stage 70 onward (`VSDamageRuntimeSmoke.cpp:889-914`). The driver log has no harness fire at stage 4.
  - The shots killed the observer, ending round 1. New pawns (`VSCharacter_2/3`) spawned at their spawn points with a wall between them. Stage 6 then waited out the buy phase.
  - Every scripted shot from stage 70 hit `StaticMeshActor_7`: 45–90 shots per step, 0 rejected and no damage events. That cascaded into the `real_fire_*`, `kill_event_once` and `stats_persist_across_rounds` FAILs and the timeout.
  - In the passing runs (main and candidate rerun), the pawns stay `VSCharacter_0/1` at the placed positions, about 300 uu apart.

### GPT-10 reticle

- At rest: `screenshots/connected-hud.png` from session run 1 matches INT-04's main screenshot (`Saved/Tests/UI/20261104T130644Z_c72889/screenshots/connected-hud.png`). Both show the same static cross with a centre gap, the same size and the same position. The rerun was not inspected separately.
- While firing or moving: no existing screenshot captures the gap opening. **Needs a human playtest.** No harness was added.

## Defects

None attributable to #59 or #60. Every suite passes on the candidate. Two passes needed the one allowed rerun for the environmental signature above.

Watch item for the lead: in rendered runners, unscripted fire now appears in 2 of the last 3 batches (INT-03 session run 1, and INT-05 session and damage run 1). If this recurs when nobody is at the machine, it should become a defect for the harness owner rather than being treated as environmental.

## Limits

- Editor Development binaries on one Mac over loopback, with no artificial latency or loss.
- Only the four briefed suites ran. Other runners (shot effects, match, movement, settings, economy, lifecycle) were not rerun.
- GPT-11 is a refactor that must not change behaviour. It is covered here by weapons, combat and damage only.
- GPT-10's dynamic gap is not visually verified. Engine-free tests for each PR are covered by CI and the worker sessions, and were not repeated here.

## Completion handoff

- Commit/PR: this report only, on `claude/int-05-report` (docs-only PR). The disposable `claude/integration-05` stays local.
- Changed: `docs/workstreams/integration-05.md` (new)
- Checks actually run and result: build plus suites 1–4 PASS on `40d10a3`. Suites 3 and 4 passed on their one rerun. One attribution damage run on `ee25083` also passed.
- Deferred: human playtest of the GPT-10 crosshair gap while firing and moving.
- Remaining defect/dependency: none. There is one watch item (unscripted fire in rendered runners).
- Next action/owner: the gameplay lead decides on merging #59 and #60. The user merges.
