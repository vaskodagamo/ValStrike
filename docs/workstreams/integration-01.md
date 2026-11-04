# INT-01: combined gameplay baseline verification on main

- Date: 2026-11-04 (Europe/Berlin), runs 09:41–10:03
- Task brief: `docs/tasks/gameplay/INT-01-baseline-verification.md` (on `claude/gameplay-team-kickoff`)
- Owner: Claude session "INT-01 baseline verification" (integration/QA), assigned by the Claude gameplay lead
- Candidate SHA: `90849f578db36c5f23fdefe6cb3c36ed2c1e087b` (`origin/main`, fast-forwarded from `d4183ca`; clean checkout)
- Engine: Unreal Engine 5.8.4, changelist 58902052, `++UE5+Release-5.8`, at `/Users/webproduktion3/UE_5.8/UE_5.8`
- Toolchain: Xcode 26.1.1 (17B100), Mac SDK 26.1, Apple clang 17.0.0 (clang-1700.4.4.1)
- Machine: Apple M4 (Mac16,10), 10 cores, 24 GB, macOS 15.6 (24G84)
- Build: `ValStrikeEditor Mac Development`, unity on (`Module.ValStrike.1–3.cpp`; clean tree, so the adaptive non-unity working set was empty). Editor build receipt `6d8851bd0d838eac8daa3a6465988cb845f5627313ee280709e4a250b1bc4aba`. Runners report build identity `revision=90849f5…`, `engine=5.8.4-58902052+++UE5+Release-5.8`, `configuration=Development`, `target=Editor`, `net_version=1575324713`.
- Engine slot: every command ran under `python3 Scripts/with_ue_slot.py --`. None returned 75. No other `UnrealEditor`/`UnrealBuildTool` process was running beforehand.

## Results

Each suite ran once, in order. There were no reruns. Evidence paths are this checkout's ignored `Saved/` directory. `Saved/INT-01/NN.log` holds each command's console output.

| # | suite | result | counts | evidence path |
| --- | --- | --- | --- | --- |
| 0 | `Scripts/ue.py build --target Editor --platform Mac` | PASS (exit 0) | 6/6 actions, unity, receipt verified | `Saved/INT-01/00_build.log` |
| 1 | `Tests/Damage/run_runtime.py` | PASS | 46/46: server 23, driver 11, observer 12. `stats_final` K1 D1 HS1 DMG100 for both players, score 1:1 | `Saved/Tests/DamageRuntime/20261104T084256Z_d2cd80/` |
| 2 | `Scripts/test_combat.py` | PASS | 55/55 (first run that includes yaw fix `3952d4b`) | `Saved/Tests/Combat/20261104T084519Z_926322/` |
| 3 | `Scripts/test_economy.py --network` | PASS | 35/35. Post-result ledger 2800/4000, 3300/3000, 5000/3500, 2500/4000, 4700/7200, 7400/9000; rematch 800/800; replicated checkpoints 26/26 for both clients | `Saved/Tests/Economy/network_20261104T084644Z_39b4c7/` |
| 4 | `Scripts/test_shot_effects.py` (rendered) | PASS | 73/73, including `first_effect_no_stall` for shooter (0.079 s) and observer (0.078 s); first compiled run of `40a7a83`/`22b1d98` | `Saved/Tests/ShotEffects/20261104T085036Z_8dfeb2/` |
| 5 | `Scripts/test_weapons.py --scenario practice` | PASS | 74/74 | `Saved/Tests/Weapons/20261104T085216Z_practice_9b9c87/` |
| 6 | `Scripts/test_weapons.py --scenario match` | PASS | 29/29 | `Saved/Tests/Weapons/20261104T085309Z_match_d96f45/` |
| 7 | `Scripts/test_match.py` | PASS | 43/43 | `Saved/Tests/Match/20261104T085419Z_4ff7a7/` |
| 8 | `Scripts/test_movement.py` | PASS | 45/45 | `Saved/Tests/Movement/20261104T085624Z_459280/` |
| 9 | `Scripts/test_combat_look.py` (rendered) | PASS | 8/8 | `Saved/Tests/CombatLook/20261104T085736Z_134b78/` |
| 10 | `Scripts/test_server_lifecycle.py` | PASS | 36/36; all 4 scenarios (invalid, empty, match, joinkey) PASS; reported revision equals HEAD in each | `Saved/Tests/ServerLifecycle/20261104T085826Z_c4707b/` |
| 11 | UnrealEditor `-nullrhi -ExecCmds="Automation RunTests ValStrike.UI"` | PASS | 4/4 `Result={Success}`: JoinKey, RoundPresentation, ServerAddress, Settings ("4 tests performed"). Only the two known startup `Condition failed` lines (log lines 1498–1499) | `Saved/INT-01/11_ui_automation.log` |
| 12 | `Tests/UI/test_join_key.py` (rendered) | PASS | 31/31; keyless join refused by server, keyed join admitted; no key in ValStrike log lines (7 Unreal-written occurrences redacted, as documented) | `Saved/Tests/JoinKey/20261104T090127Z_a962f0/` |
| 13 | `Tests/UI/test_session.py --rendered` | PASS | 72/72; 8 HUD snapshots, 0 snapshot errors; `dirty_source_checkout=false` | `Saved/Tests/UI/20261104T090155Z_86f051/` |

Note on suite 13: the brief expected 59/59, from the UI branch. On combined main the runner has 72 required checks, and all passed (`passed: true`). The higher count reflects checks added by other merged streams. It is not a deviation in behavior.

Rendered suites 4, 9, 12 and 13: the integration owner sent no desktop input during the runs. The look and click checks show no sign of contamination; for example, suite 9 measured exact 8.4° steps.

## Defects

None. No suite failed, so no `INT-01-D<n>` entries exist.

## Limits

- Editor Development binaries only. Not packaged client, packaged dedicated server, Shipping or Test configurations.
- One Mac (Apple M4, macOS 15.6), loopback only, with no artificial latency or loss. No two-machine LAN, four-player/2v2, Windows or Linux acceptance.
- Exclusive Fullscreen was not exercised. The `Tests/UI/test_settings.py` rendered runner was not part of this batch; suite 11 covers only the `ValStrike.UI.Settings` unit automation.
- Engine-free suites were not repeated here; GitHub CI covers them.
- No manual playtest. The checks are the automated runners listed above.

## Completion handoff

- Commit/PR: this report only, on `claude/int-01-report` (docs-only PR to `main`)
- Changed: `docs/workstreams/integration-01.md` (new)
- Checks actually run and result: build plus suites 1–13 above, all PASS on `90849f5`
- Deferred checks and integration owner: packaged, LAN, four-player and cross-platform acceptance; rendered settings runner; Exclusive Fullscreen (not scheduled)
- Remaining defect/dependency: none from this batch
- Next action/owner: the gameplay lead triages the result (no defects) and the coordinator records the baseline as Verified (Editor, single Mac, loopback)
