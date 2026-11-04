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

The kickoff's limit of two implementation workers was raised to four by the user on 2026-11-04 ("more"). All four have disjoint paths, and none uses the engine slot.

Paths are relative to `/Users/webproduktion3/Documents/ValStrike`. Engine: `/Users/webproduktion3/UE_5.8/UE_5.8` (UE 5.8.4, Xcode 26.1.1).

## Task registry

| ID | Result | Owner / checkout | Exclusive paths | State / validation |
| --- | --- | --- | --- | --- |
| INT-01 | One combined build of `origin/main` `90849f5` plus the affected suites once; per-suite pass/fail with evidence and owner-addressed defects | INT-01 QA session; `team-integration` | `docs/workstreams/integration-01.md`; its own ignored `Saved/`, `Binaries/`, `Intermediate/`. No source edits | In progress; V2. [Brief](../tasks/gameplay/INT-01-baseline-verification.md) |

| GPT-01 | GUN-03: server-confirmed hit marker, damage-direction indicator, death feedback | GPT-01 session; `gpt-01-hit-feedback` | new `Feedback/**`, `VSHUD.h/.cpp`, `VSHUDData.h`, `VSPlayerController.h/.cpp` (feedback hosting only), new `Tests/Feedback/**`, `docs/workstreams/hit-feedback.md` | In progress; V2. [Brief](../tasks/gameplay/GPT-01-hit-feedback.md) |
| GPT-02 | GUN-03: deterministic recoil, server-authoritative spread, rifle and pistol | GPT-02 session; `gpt-02-recoil-spread` | `Combat/VSWeaponComponent.*`, `Combat/VSRifleComponent.*`, `Combat/VSPistolComponent.*`, new `Combat/VSRecoilRules.*`, new `Tests/Weapons/recoil_rules_test.cpp`, `docs/workstreams/recoil-spread.md` | In progress; V2. [Brief](../tasks/gameplay/GPT-02-recoil-spread.md) |
| GPT-03 | MOVE-03 enabler: `ue.py --net-profile` latency/loss options plus measurement procedure | GPT-03 session; `gpt-03-net-emulation` | `Scripts/ue.py` (server/client flags only), `Tests/test_ue.py` (new cases), new `docs/playtest/net-emulation.md`, `docs/workstreams/net-emulation.md` | In progress; V1. [Brief](../tasks/gameplay/GPT-03-net-emulation.md) |
| GPT-04 | Client-predicted weapon switching with server reconciliation | GPT-04 session; `gpt-04-switch-prediction` | `Combat/VSWeaponLoadoutComponent.*`, `Combat/VSWeaponLoadoutRules.h`, `Tests/Weapons/weapon_loadout_rules_test.cpp`, `Presentation/VSWeaponPresentationComponent.*`, `docs/workstreams/switch-prediction.md` | In progress; V2. [Brief](../tasks/gameplay/GPT-04-switch-prediction.md) |

Path conflicts with INT-01 findings: a fix in GPT-01/GPT-02-owned files goes to that owner, or waits until its PR is handed off. `VSCharacter.*`, `Damage/**`, `Effects/**`, `Match/**`, `UI/**` and `Scoreboard/**` stay unclaimed for fix tasks. GPT-02 lands before the next INT batch, which reruns combat/weapons/damage/shot-effects for both workers together.

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
