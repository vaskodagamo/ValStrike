# Gameplay lead handoff

Append-only. Dates are Europe/Berlin. Report for the Codex coordinator.

## 2026-11-04 09:40 — Team kickoff

- Lead checkout `team-gameplay`, `codex/team-gameplay`: clean at `d4183ca`, fast-forwarded to `origin/main` `90849f5`.
- Team: the lead plus one INT-01 integration/QA session in `team-integration` (base `90849f5`). No implementation workers, because no concrete defect exists yet. Fix tasks follow INT-01 findings. See [GAMEPLAY_BOARD.md](../teams/GAMEPLAY_BOARD.md).
- INT-01 candidate: `90849f5`. Source is identical to `95a0ba7` and includes PRs #16–#29. Suite list and expected counts come from the damage, economy, shot-effects, settings/join-key, weapons, match-runtime, combat and server handoffs: [brief](../tasks/gameplay/INT-01-baseline-verification.md).
- Unverified final changes found in the handoffs: combat yaw fix `3952d4b` (never rerun), shot-effects `40a7a83`/`22b1d98` (never compiled), economy network run (interrupted, so it is a FAIL rather than a pass), join-key rendered and native UI automation (queued, never run), and movement on damage Phase 2 (interrupted).
- Engine slot: kernel lock free at assignment. The lock file carries stale `match-runtime` metadata (dead PID). Reserved for the INT-01 job only.
- Central items proposed to the coordinator (no edits made):
  - Join-key exposure in engine logs: decide on a `ULocalPlayer` login-options subclass plus a `DefaultEngine.ini` entry.
  - Add `Tests/Weapons` to engine-free CI, as asked in the weapons handoff.
- Art contracts needed: none for INT-01.
- Verification: none claimed. INT-01 is running.

## 2026-11-04 — Two implementation workers started

At the user's request to continue building game logic while INT-01 runs, two disjoint GUN-03 tasks are assigned. GPT-01 (hit/damage/death feedback, HUD and controller) and GPT-02 (recoil and spread, Combat weapon components) own no common paths. Neither uses the engine slot. Their combined build and runtime checks go to the next INT batch, alongside any INT-01 fixes.

## 2026-11-04 — INT-01 result and INT-02 dispatch

- INT-01 ([PR #38](https://github.com/vaskodagamo/ValStrike/pull/38)): main `90849f5` built and passed all 13 suites, with no defects. That makes the merged baseline (PRs #16–#29) Verified for Editor, one Mac, loopback. Packaged, LAN, 2v2 and cross-platform acceptance remain open. Coordinator: please record this centrally.
- GPT-01..06 are implementation-ready as PRs #34, #35, #36, #37, #40 and #39. Each changes only its owned paths, and CI is green on all six. A `git merge-tree` dry run combines all six on `90849f5` without conflicts.
- INT-02 is dispatched to the same QA session to verify the combined candidate before the user merges ([brief](../tasks/gameplay/INT-02-gpt-batch.md)).
- Proposed to the coordinator (CI is central): add the new engine-free tests to `.github/workflows/checks.yml`:
  - `Tests/Feedback/hit_feedback_state_test.cpp`
  - `Tests/Weapons/recoil_rules_test.cpp`
  - `Tests/Weapons/weapon_loadout_rules_test.cpp`
  - `Tests/Audio/gameplay_audio_state_test.cpp`
- Follow-ups noted for after INT-02:
  - local tracer should use the spread direction (Effects plus GPT-02 `GetLastPredictedShot()`);
  - spread-driven crosshair (HUD, after #34/#35 merge);
  - equip-boundary tolerance, only if INT-02 observes it;
  - art slots for audio cues (Codex).

## 2026-11-04 — Merges and board resync

- The user merged #32 and #34–#40. PR #32 merged before the lead's later board commits, so this resync PR replays them onto main.
- GPT-01..06 are merged with verification still pending. INT-02 tests the same six tips combined; its result now covers main's source for those PRs, but the main revision itself must be rechecked if any later change lands.
- GPT-09 found that muzzle flash and body kick already exist in the base `VSWeaponPresentationComponent`. A shared-file exception for that file was approved, because GPT-04 had released it. GPT-07, GPT-08 and GPT-09 were told to merge current main before opening PRs.

## 2026-11-04 — INT-02 result and triage

- INT-02 ([PR #43](https://github.com/vaskodagamo/ValStrike/pull/43)) tested the six GPT tips combined, which is source-equivalent to main `5b13856`. Build and suites 1–9 pass, except the rendered settings runner: D1, 2/2 runs, window-mode arrow click. GPT-03 emulation and GPT-05 audio activation were seen in the logs. HUD feedback, predicted switching and audible cues still need a human playtest.
- D1 goes to GPF-01 in the GPT-06 session. The QA session runs the settings runner on `90849f5` and `b5b8523` to confirm which change caused it.
- GPT-07/08/09 are implementation-ready as PRs #46, #44 and #45. Each stays in its owned paths, and CI is green. These are the next integration batch, together with the GPF-01 fix.

## 2026-11-04 — D1 attribution and INT-03

- The QA bisect, with no source edits, ran the rendered settings runner on two revisions. Base `90849f5` fails `arrow_selects_windowed mode=1` (`Saved/Tests/Settings/20261104T093620Z_3cdb74`), as does `b5b8523` (GPT-06, `…093857Z_a89177`). So D1 predates GPT-06: it lies in the original settings window-mode path (PR #19), whose rendered runner had not been run on this machine's 2560×1440 desktop.
- PR #48 (GPF-01) fixes a real overflow that GPT-06 caused and adds a fit check. It does not claim to fix the click. GPF-01 stays open with GPT-06's session, now pointed at the click after the Windowed Fullscreen switch, and waits for INT-03's run of #48.
- The user merged #42, #43, #44 and #46. INT-03 verifies main `887d349` plus #45 and #48 ([brief](../tasks/gameplay/INT-03-batch.md)).

## 2026-11-04 — D1 root cause, all gameplay merged, INT-04, GPT-10/11

- The GPF-01-diag job used 3 of its 3 cycles. Root cause of D1: Slate ignores capture requests while the app is not the active macOS app, so the harness's synthesized press never completes a click. This only shows on the two-monitor Mac, and players are not affected. The fix ([PR #52](https://github.com/vaskodagamo/ValStrike/pull/52), merged by the user) scopes `SetHandleDeviceInputWhenApplicationNotActive` to `Click()`. Its final code has not been compiled.
- The user merged #45, #48, #49, #50 and #52. Main `82d7a4c` contains every gameplay PR. INT-04 builds it and reruns the settings path only ([brief](../tasks/gameplay/INT-04-main-settings.md)).
- New workers, with disjoint paths and no engine use: GPT-10, the spread-driven reticle ([brief](../tasks/gameplay/GPT-10-spread-crosshair.md)); GPT-11, GUN-04 fire-request rules ([brief](../tasks/gameplay/GPT-11-fire-request-rules.md)).
- Still for the coordinator: put the engine-free tests in CI (Feedback, Weapons, Audio, Match spectate, Presentation muzzle flash); the join-key log decision; and a human playtest for HUD feedback, spectate, audio and volume.

## 2026-11-04 — INT-04/05 results

- INT-04 ([PR #61](https://github.com/vaskodagamo/ValStrike/pull/61)): main `ee25083`: build, settings 78/78 ×2, UI 4/4, session 72/72. D1 is closed for Editor on this Mac.
- The user merged #58, #59 and #60. INT-05 ([PR #64](https://github.com/vaskodagamo/ValStrike/pull/64)) verified the same source as candidate `40d10a3`: everything passes, and no defect is attributed to GPT-10/11. Main `b8183db` has the same source.
- Watch item: unscripted fire in rendered runners (INT-03, INT-05). Human playtest still needed: HUD feedback, spectate, audio and volume, and the crosshair gap while firing or moving.

## 2026-11-04 — INT-06 record and handoff to the producer

The user appointed Direktor (2) (Codex thread `01a1a67c-4861-7e62-a15e-505444092624`) as sole producer. The gameplay lead acknowledges this. From now on, new gameplay batches are queued through the producer. The lead keeps this board and these handoffs current, but does not write the master board or the central work log. The user merges.

**INT-06 exact candidate.** Disposable `claude/integration-06` `a935f0c` = `origin/main` `09347c2` + `origin/claude/gpt-15-movement-feel` `085f09a`, `--no-ff`, no conflicts. Report: [PR #77](https://github.com/vaskodagamo/ValStrike/pull/77).

| # | Suite | Result |
| --- | --- | --- |
| attempt 1 (`1b01540`, GPT-15 `c52385e`) | Editor build | FAIL: INT-06-D1, `-Wshadow` in `VSMovementRules.h:103` (GPT-15); fixed in `085f09a` |
| 0 | Editor build | PASS, 0 warnings |
| 1 | movement lan | PASS 69/69 (rifle 387, pistol 432 cm/s; jump about 138 cm; crouch-jump about 161 cm) |
| 2 | movement `--net-profile typical` | PASS 69/69, valid (PktLag 40/10/1 in all three processes). Corrections: server 38, driver 12, observer 2; 28 are setup teleports; D3 below |
| 3 | match | PASS 43/43; the 3 GPT-13 spectate checks were `not_reached` (driver and observer are on opposite teams) |
| 4 | damage | PASS 46/46; GPT-14 `VSFEED` entries logged, cleared on round reset |
| 5 | combat | PASS 55/55 |
| 6 | weapons practice | PASS 74/74 |
| 7 | session `--rendered` | run 1 FAIL 65/72 (INT-06-D2, not unscripted fire); rerun 72/72 for information only. D2 stays open |

Open from INT-06:
- **D2**: an unreachable connect can close without a failure broadcast, leaving the menu stuck "Connecting…". It predates recent work. GPF-02 has it (new session, `gpf-02-session-connect-failure`).
- **D3**: 3 steady-state corrections of 7.2 cm at 387 cm/s under `typical`. QA's attribution run (movement `typical` on main without #74) holds the engine slot now.
- **Spectate not reached**: GPF-03 (GPT-13 session, `claude/gpf-03-spectate-scenario`).

User merges since: #69, #70, #71 and #74. Verified: Editor, one Mac, loopback, plus the `typical` emulation. Kill feed on screen, spectate and movement feel still need a human playtest.

**Engine slot:** **not yet released.** The INT QA session holds it for the single D3 attribution run (`test_movement.py --net-profile typical`). After that run the slot is released and goes to the producer's queued INT-ART-01 (Meshy playtest integration). The slot then waits for GPF-02 and GPF-03 to be ready; their checks follow through the producer queue.

**Active contributors after this batch:** QA (D3 run, then INT-ART-01 per producer), GPF-02, GPF-03. That is three; no new batch until the producer dispatches.

## 2026-11-04 — D3 attributed, engine slot released

- QA's attribution ([PR #82](https://github.com/vaskodagamo/ValStrike/pull/82), which also carries the INT-06 attempt-2 record that missed #77): detached `09347c2` (main had already absorbed #74) builds and passes movement `typical` 45/45, with only zero-velocity setup corrections. `a935f0c` has 10 in motion, including 3 at a steady 387 cm/s. **D3 belongs to GPT-15 (#74, merged).** The fix task GPF-04 goes to the producer queue rather than started, because of the three-contributor cap. Players may notice small corrections when moving under latency until it lands.
- **Engine slot released** by the INT QA session. Next: INT-ART-01 (producer).
- Active gameplay writers: GPF-02, GPF-03. Queued: GPF-04, then the GPF-02/03/04 integration checks.

## 2026-11-04 — GPF-02 and GPF-03 ready

- GPF-02 ([PR #84](https://github.com/vaskodagamo/ValStrike/pull/84)): INT-06-D2 was an unsent menu cancel (Escape or Cancel button), inferred from logs, together with a physical-mouse event. That points to physical input on the visible test window, the same family as the unscripted-fire watch item. The fix makes the game robust to silent connect teardown and logs the cancel source. Not built yet.
- GPF-03 ([PR #86](https://github.com/vaskodagamo/ValStrike/pull/86)): the spectate checks become reachable through a fourth `mate` client. A whitespace CI failure has gone to the owner. Not built yet.
- Integration queue for the producer, after INT-ART-01: one combined INT run of #84 + #86 (build; `test_session.py --rendered` ×3; `test_match.py`). Then GPF-04 (D3, queued), followed by its movement lan and `typical` check.

## 2026-11-04 — GPF-04 dispatched (producer release)

- Producer released GPF-04 after #93 (INT-ART-01 native) merged at `a1c2371`. The movement source paths and capacity are free. Dispatched to the **existing** GPT-15 session in its clean `gpt-15-movement-feel` worktree, on the new branch `claude/gpf-04-steady-corrections` from `a1c2371`. Scope: movement files plus the movement-weight sections of `VSCharacter.*`, keeping the #93 presentation code. Checks: engine-free only, including -Wshadow. One main-targeted PR with an exact-head handoff, verification pending.
- The earlier queue order is superseded. GPF-04 verification combines with the still-pending GPF-02/03 checks (both merged, #84/#86) in one native/QA candidate that the producer releases. The lead starts no QA or new batch on its own.
