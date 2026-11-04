# Gameplay team board

Owner: Claude gameplay/engineering lead (delegated by the coordinator in [TEAM_BOARD.md](../TEAM_BOARD.md)). This board is the only place gameplay tasks, worktrees, path claims and the engineering engine-slot job are assigned. The coordinator keeps [TEAM_BOARD.md](../TEAM_BOARD.md), central docs and art. Dates are Europe/Berlin.

## Team (2026-11-04)

| Role | Session / provider | Checkout | State |
| --- | --- | --- | --- |
| Engineering lead | User-started Claude session | `.parallel-worktrees/team-gameplay`, branch `codex/team-gameplay` (historical prefix), fast-forwarded to `origin/main` `90849f5` | Active: this board, briefs, triage |
| Integration / QA owner (INT-01) | Claude session "INT-01 baseline verification", started by the lead | `.parallel-worktrees/team-integration`, branch `codex/team-integration`, base `90849f5` | Assigned; sole engineering user of the engine slot |
| Implementation worker (GPT-01) | Claude session "GPT-01 hit feedback" | `.parallel-worktrees/gpt-01-hit-feedback`, branch `claude/gpt-01-hit-feedback`, base `90849f5` | Assigned 2026-11-04 at the user's request to continue game logic; no engine use |
| Implementation worker (GPT-02) | Claude session "GPT-02 recoil and spread" | `.parallel-worktrees/gpt-02-recoil-spread`, branch `claude/gpt-02-recoil-spread`, base `90849f5` | Assigned 2026-11-04; no engine use |
| Implementation worker (GPT-03) | Claude session "GPT-03 network emulation profiles" | `.parallel-worktrees/gpt-03-net-emulation`, branch `claude/gpt-03-net-emulation`, base `90849f5` | Assigned 2026-11-04; no engine use |
| Implementation worker (GPT-04) | Claude session "GPT-04 switch prediction" | `.parallel-worktrees/gpt-04-switch-prediction`, branch `claude/gpt-04-switch-prediction`, base `90849f5` | Assigned 2026-11-04; no engine use |

| Implementation worker (GPT-05) | Claude session "GPT-05 spatial audio" | `.parallel-worktrees/gpt-05-spatial-audio`, branch `claude/gpt-05-spatial-audio`, base `90849f5` | Assigned 2026-11-04; no engine use |
| Implementation worker (GPT-06) | Claude session "GPT-06 volume setting" | `.parallel-worktrees/gpt-06-volume-setting`, branch `claude/gpt-06-volume-setting`, base `90849f5` | Assigned 2026-11-04; no engine use |
| Implementation worker (GPT-07) | Claude session "GPT-07 death spectate" | `.parallel-worktrees/gpt-07-death-spectate`, branch `claude/gpt-07-death-spectate`, base `90849f5` | Assigned 2026-11-04; no engine use |
| Implementation worker (GPT-08) | Claude session "GPT-08 spread tracer" | `.parallel-worktrees/gpt-08-spread-tracer`, branch `claude/gpt-08-spread-tracer`, base `a64dfa5` (stacked on PR #35) | Assigned 2026-11-04; no engine use |
| Implementation worker (GPT-09) | Claude session "GPT-09 muzzle flash and kick" | `.parallel-worktrees/gpt-09-muzzle-kick`, branch `claude/gpt-09-muzzle-kick`, base `90849f5` | Assigned 2026-11-04; no engine use |

The kickoff's limit of two implementation workers was raised by the user on 2026-11-04 ("more", then "kick off more and keep building"). All workers have disjoint paths, and none uses the engine slot. A finished worker's paths stay claimed until its PR merges.

Paths are relative to `/Users/webproduktion3/Documents/ValStrike`. Engine: `/Users/webproduktion3/UE_5.8/UE_5.8` (UE 5.8.4, Xcode 26.1.1).

## Task registry

| ID | Result | Owner / checkout | Exclusive paths | State / validation |
| --- | --- | --- | --- | --- |
| INT-01 | One combined build of `origin/main` `90849f5` plus the affected suites once; per-suite pass/fail with evidence and owner-addressed defects | INT-01 QA session; `team-integration` | `docs/workstreams/integration-01.md`; its own ignored `Saved/`, `Binaries/`, `Intermediate/`. No source edits | Done: 14/14 PASS (build + 13 suites), no defects, [PR #38](https://github.com/vaskodagamo/ValStrike/pull/38) (merged). Verified: Editor, one Mac, loopback. [Brief](../tasks/gameplay/INT-01-baseline-verification.md) |
| INT-02 | Combined GPT-01..06 candidate: one build plus affected suites before merge | INT-01 QA session (continued); `team-integration`, disposable `claude/integration-02` | `docs/workstreams/integration-02.md` | In progress; V2. [Brief](../tasks/gameplay/INT-02-gpt-batch.md) |

| GPT-01 | GUN-03: server-confirmed hit marker, damage-direction indicator, death feedback | GPT-01 session; `gpt-01-hit-feedback` | new `Feedback/**`, `VSHUD.h/.cpp`, `VSHUDData.h`, `VSPlayerController.h/.cpp` (feedback hosting only), new `Tests/Feedback/**`, `docs/workstreams/hit-feedback.md` | Merged, [PR #34](https://github.com/vaskodagamo/ValStrike/pull/34) (user, 2026-11-04); V2, verification pending in INT-02. [Brief](../tasks/gameplay/GPT-01-hit-feedback.md) |
| GPT-02 | GUN-03: deterministic recoil, server-authoritative spread, rifle and pistol | GPT-02 session; `gpt-02-recoil-spread` | `Combat/VSWeaponComponent.*`, `Combat/VSRifleComponent.*`, `Combat/VSPistolComponent.*`, new `Combat/VSRecoilRules.*`, new `Tests/Weapons/recoil_rules_test.cpp`, `docs/workstreams/recoil-spread.md` | Merged, [PR #35](https://github.com/vaskodagamo/ValStrike/pull/35) (user, 2026-11-04); V2, verification pending in INT-02. [Brief](../tasks/gameplay/GPT-02-recoil-spread.md) |
| GPT-03 | MOVE-03 enabler: `ue.py --net-profile` latency/loss options plus measurement procedure | GPT-03 session; `gpt-03-net-emulation` | `Scripts/ue.py` (server/client flags only), `Tests/test_ue.py` (new cases), new `docs/playtest/net-emulation.md`, `docs/workstreams/net-emulation.md` | Merged, [PR #36](https://github.com/vaskodagamo/ValStrike/pull/36) (user, 2026-11-04); V1, verification pending in INT-02. [Brief](../tasks/gameplay/GPT-03-net-emulation.md) |
| GPT-04 | Client-predicted weapon switching with server reconciliation | GPT-04 session; `gpt-04-switch-prediction` | `Combat/VSWeaponLoadoutComponent.*`, `Combat/VSWeaponLoadoutRules.h`, `Tests/Weapons/weapon_loadout_rules_test.cpp`, `Presentation/VSWeaponPresentationComponent.*`, `docs/workstreams/switch-prediction.md` | Merged, [PR #37](https://github.com/vaskodagamo/ValStrike/pull/37) (user, 2026-11-04); V2, verification pending in INT-02. [Brief](../tasks/gameplay/GPT-04-switch-prediction.md) |
| GPT-05 | PLAY-03 audio: client-only spatial gameplay cues with procedural placeholder sounds and art slots | GPT-05 session; `gpt-05-spatial-audio` | new `Audio/**`, new `Tests/Audio/**`, `docs/workstreams/spatial-audio.md` | Merged, [PR #40](https://github.com/vaskodagamo/ValStrike/pull/40) (user, 2026-11-04); V1, verification pending in INT-02. [Brief](../tasks/gameplay/GPT-05-spatial-audio.md) |
| GPT-06 | PLAY-03 settings: persistent master volume row | GPT-06 session; `gpt-06-volume-setting` | `UI/VSUserSettings*`, settings section of `UI/SVSSessionMenu.*`, `UI/VSSettingsSmoke.*`, `Tests/UI/test_settings*.py`, settings section of `Tests/UI/README.md`, append to `docs/workstreams/settings.md` | Merged, [PR #39](https://github.com/vaskodagamo/ValStrike/pull/39) (user, 2026-11-04); V1, verification pending in INT-02. [Brief](../tasks/gameplay/GPT-06-volume-setting.md) |
| GPT-07 | Dead players spectate living teammates until round reset | GPT-07 session; `gpt-07-death-spectate` | `Match/VSMatchPlayerController.*`, new `Match/VSSpectateRules.h`, `VSMatchHUDData.h` (additive), new `Tests/Match/spectate_rules_test.cpp`, `docs/workstreams/death-spectate.md` | In progress; V2. [Brief](../tasks/gameplay/GPT-07-death-spectate.md) |
| GPT-08 | Tracers follow recoil/spread direction (#35 is now merged; merge main before the PR) | GPT-08 session; `gpt-08-spread-tracer` | `Effects/VSShotEffectsSubsystem.*`, `Effects/VSShotEffectsState.h`, `Tests/Effects/shot_effects_state_test.cpp`, append to `docs/workstreams/shot-effects.md` | In progress; V1. [Brief](../tasks/gameplay/GPT-08-spread-tracer.md) |
| GPT-09 | Muzzle flash for both weapons, rifle viewmodel kick | GPT-09 session; `gpt-09-muzzle-kick` | `Presentation/VSRiflePresentation*`, `Presentation/VSPistolPresentationComponent.*`, new `Presentation/VSMuzzleFlashState.h`, new presentation test, append to `docs/workstreams/rifle-presentation.md`; **exception approved 2026-11-04:** `Presentation/VSWeaponPresentationComponent.*` (the flash and kick already live in that base class; GPT-04 released it when #37 merged) | In progress; V1. [Brief](../tasks/gameplay/GPT-09-muzzle-kick.md) |

Path conflicts with INT-01 findings: a fix in GPT-01/GPT-02-owned files goes to that owner, or waits until its PR is handed off. `VSCharacter.*`, `Damage/**`, `Scoreboard/**`, the remaining `Match/**` and the non-settings `UI/**` files stay unclaimed for fix tasks. Effects goes to GPT-08 and the match controller to GPT-07. A settings-menu finding goes to GPT-06. GPT-02 lands before the next INT batch, which reruns combat/weapons/damage/shot-effects for both workers together.

Fix tasks get IDs `GPF-<nn>` here and a brief in `docs/tasks/gameplay/` before any worker starts.

## Engine slot

| Job | Owner | Scope | Release signal |
| --- | --- | --- | --- |
| INT-01 | INT-01 QA session | Done; slot released | PR #38 |
| INT-02 | INT-01 QA session | One build of the combined GPT candidate plus the INT-02 suites | Report committed to `docs/workstreams/integration-02.md` |

At assignment the kernel lock was free. `.parallel-worktrees/.ue-slot.lock` holds stale metadata from `match-runtime` (PID 10316, process gone). Exit 75 means stop and report "waiting for engine slot", not retry. No other engineering session builds or runs Unreal.

## Candidate

`origin/main` `90849f5`. Its source equals `95a0ba7`: no `Source/`, `Config/`, `Scripts/` or `Tests/` change after it, only docs. It includes merged PRs #16–#29: unity-build fix, server contract and join key/tick rate, settings menu, weapons switching, damage scoreboard and zones Phase 2, economy and kill rewards, shot effects and the join-key menu. No gameplay PR is open.

## Known open items (not yet fix tasks)

- **Join key in engine logs.** Unreal logs the travel URL and login options, so a keyed join writes the key into local client and server logs. Fixing the client side needs a `ULocalPlayer` subclass and a `DefaultEngine.ini` change. That is a central config decision, proposed to the coordinator. No fix task until the decision.
- **Exclusive Fullscreen on macOS** never ran. INT-01 records it as pending. It is not a defect.
- **UI session suite null-RHI 24/32** on the weapons base (`93b87c8`), versus 59/59 rendered on the UI branch. INT-01 runs it rendered once and classifies it.
