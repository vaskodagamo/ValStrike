# INT-02: combined GPT-01..06 candidate verification

- Date: 2026-11-04 (Europe/Berlin), runs 10:13–10:31
- Task brief: `docs/tasks/gameplay/INT-02-gpt-batch.md` (on `claude/gameplay-team-kickoff`)
- Owner: Claude session "INT-01 baseline verification", continued as the integration/QA owner and assigned by the Claude gameplay lead
- Candidate SHA: `c004369c4d78374ab2df9347489eb586c8c7747b`. This is the local disposable branch `claude/integration-02` (not pushed), built from `90849f5` by `--no-ff` merges in the briefed order. There were no conflicts.

| Order | PR | Task | Tip |
| --- | --- | --- | --- |
| 1 | #34 | GPT-01 hit feedback | `0cf1115` |
| 2 | #35 | GPT-02 recoil/spread | `a64dfa5` |
| 3 | #36 | GPT-03 net emulation | `4194a16` |
| 4 | #37 | GPT-04 switch prediction | `b674a56` |
| 5 | #40 | GPT-05 spatial audio | `a0a8a38` |
| 6 | #39 | GPT-06 volume setting | `b5b8523` |

- Relation to main: while this batch ran, the user merged all six PRs. The merges ran in a different order: #34–#37 into `36b3698`, then #39 and #40 into `5b13856`. `git diff origin/main c004369 -- Source Scripts Tests Config Content` is empty, so these results apply to the code of `origin/main` `5b13856`. Only docs differ.
- Engine/toolchain: UE 5.8.4 (CL 58902052), Xcode 26.1.1 (17B100), Mac SDK 26.1, Apple clang 17.0.0
- Machine: Apple M4 (Mac16,10), 24 GB, macOS 15.6 (24G84). This is the same machine as INT-01.
- Build identity: `ValStrikeEditor Mac Development`, unity on (`Module.ValStrike.1–3.cpp`), 0 errors and 0 warnings. Editor receipt `470a61f508318c72139e4bb715f5cfbb7aa304b09571089eec8447cdec1fee48`. `test_session.py` reports `revision=c004369…` and `dirty_source_checkout=false`.
- Engine slot: every command ran under `python3 Scripts/with_ue_slot.py --`. None returned 75.

## Results

Evidence lives in this checkout's ignored `Saved/`. `Saved/INT-02/NN.log` is each command's console output.

| # | suite | result | counts | evidence path |
| --- | --- | --- | --- | --- |
| 0 | `Scripts/ue.py build --target Editor --platform Mac` | PASS | 4/4 actions, unity, receipt verified | `Saved/INT-02/00_build.log` |
| 1 | `Tests/Damage/run_runtime.py` | PASS | 46/46 (server 23, driver 11, observer 12); first-shot damage unchanged (GPT-02) | `Saved/Tests/DamageRuntime/20261104T091455Z_7f22b9/` |
| 2 | `Scripts/test_combat.py` | PASS | 55/55 | `Saved/Tests/Combat/20261104T091643Z_1b9242/` |
| 3a | `Scripts/test_weapons.py --scenario practice` | PASS | 74/74; the only equip rejections are the deliberate `reject_equip_time` / `reject_duplicate` checks, with no unexpected first-shot rejection (GPT-04) | `Saved/Tests/Weapons/20261104T091808Z_practice_0216d8/` |
| 3b | `Scripts/test_weapons.py --scenario match` | PASS | 29/29, with no equip rejections | `Saved/Tests/Weapons/20261104T091900Z_match_b840af/` |
| 4 | `Scripts/test_shot_effects.py` (rendered) | PASS | 73/73 | `Saved/Tests/ShotEffects/20261104T092006Z_342b7d/` |
| 5 | `Scripts/test_match.py` | PASS | 43/43 | `Saved/Tests/Match/20261104T092145Z_c69798/` |
| 6 | `Scripts/test_movement.py` | PASS | 45/45 | `Saved/Tests/Movement/20261104T092347Z_c3ede2/` |
| 7 | UnrealEditor `-nullrhi -ExecCmds="Automation RunTests ValStrike.UI"` | PASS | 4/4 `Result={Success}` (JoinKey, RoundPresentation, ServerAddress, Settings); only the 2 known startup `Condition failed` lines | `Saved/INT-02/07_ui_automation.log` |
| 8 | `Tests/UI/test_settings.py` (rendered), run 1 | **FAIL** | 57 pass, 20 fail. See INT-02-D1. Look-ratio failures in this run show input contamination | `Saved/Tests/Settings/20261104T092513Z_b8f7e4/` |
| 8 | `Tests/UI/test_settings.py` (rendered), rerun | **FAIL** | 63 pass, 14 fail. See INT-02-D1. Same first failure; contamination checks now pass | `Saved/Tests/Settings/20261104T092738Z_083123/` |
| 9 | `Tests/UI/test_session.py --rendered` | PASS | 72/72 (session-page Tab order intact with GPT-06) | `Saved/Tests/UI/20261104T092602Z_81affb/` |
| 10 | Observational: `ue.py server --mode practice` plus 2× `ue.py client`, all `--net-profile typical`, client audio on, 90 s, no input | Observed (partial) | See the suite 10 section | `Saved/INT-02/10/` |

Rerun justification for suite 8: run 1 showed the README's documented signature of OS input reaching the window. Look yaw and pitch disagreed: expected 25.2° each, measured yaw 9.374° and pitch 28.980°. A mouse-capture change also occurred mid-run (`first.log:1467`). Note that `HandleMouseConnected` lines also appear at every startup, so they do not prove physical input. The rerun removed the contamination failures but reproduced the window-mode failure exactly. So that failure is not environmental.

### Suite 10 observations (observational, not a pass/fail gate)

Observed in the logs:

- GPT-03: the server and both clients log `LogNet: PktLag set to 40`, `PktLagVariance set to 10`, `PktLoss set to 1`. See `VS_Server_7777.log:1334-1339`, `VS_Client_85406.log:1394-1399` and `VS_Client_85658.log:1394-1399`.
- The server reports `VSSERVER_MAP map=graybox mode=practice` (`VS_Server_7777.log:1352`). Both clients got `Join succeeded` / `Welcomed by server` under emulation (`VS_Server_7777.log:1400,1419`).
- GPT-05: both clients log `Gameplay audio active (client-only; 0/10 cues from art, rest placeholder)` (`VS_Client_85406.log:1464`, `VS_Client_85658.log:1463`).
- There were 0 `Error:` lines and 0 `LogValStrike: Warning` lines in all three logs. All processes stopped cleanly with nothing left running.

Not observed, so still pending a human playtest:

- GPT-01: shooter-only hit marker, victim-only damage arcs and death text.
- GPT-04: predicted 1/2 switch with no snap-back under lag.
- GPT-05: audible, spatialized shot and footstep cues.
- GPT-06: listening check at 0 % and 50 %.

No gameplay input was injected and the brief allows no new harness. The integration owner also cannot listen to audio. Automated coverage of the HUD and switching paths is limited to suites 2–4 and 9 above.

## Defects

### INT-02-D1: settings window-mode arrow click does not change the mode

- Suite: 8, `Tests/UI/test_settings.py` (rendered). Reproduced in 2/2 runs.
- Owner for triage: **GPT-06 (volume setting)**, area UI. Attribution is unconfirmed. GPT-06's diff to `Source/ValStrike/UI/SVSSessionMenu.cpp` adds the volume row and tightens vertical padding. It does not touch the window-mode selector or its handler. This runner had not run on `90849f5` (INT-01 did not include it), and GPT-06 deferred it to integration. So there is no baseline showing whether the failure predates GPT-06.
- Expected: after Windowed Fullscreen is applied, a synthesized click on `VS.WindowMode.Next` sets the pending mode to Windowed (2). The run then selects 1280×720, applies and saves it, and the restart restores it.
- Actual: the click is delivered at the arrow's centre with `hit_testable=true`. The pending mode stays Windowed Fullscreen (1). All later window checks cascade: no 1280×720 option, Tab lands on Apply/Back, `windowed_1280x720` is neither applied nor saved, and the restart restores 2560×1440. Other synthesized clicks in the same run work (the `VS.Settings` button). Volume and sensitivity checks all pass, including disk and restart: `volume_saved`, `restart_loads_saved_volume: 95`.
- Key log lines (rerun `Saved/Tests/Settings/20261104T092738Z_083123/first.log`; run 1 has the same lines):
  - `:1457` `VSSET_CLICK VS.WindowMode.Next at (1571.8, 796.1) hit_testable=true`
  - `:1459` `VSSET_CHECK arrow_selects_windowed FAIL mode=1`
  - Screenshot `screenshots/first/settings-pending-windowed.png` shows the `›` arrow at the clicked point and the mode still "Windowed Fullscreen".
- Suggested first step for the owner: run the same runner on `90849f5` and on `b5b8523` to settle attribution. Then check whether the selector's `OnClicked` is reached when the window is Windowed Fullscreen at 2560×1440.

No other suite failed.

## Limits

- Editor Development binaries on one Mac over loopback. GPT-03 emulation (`typical`) applied only in suite 10. All scripted suites ran without added latency or loss. MOVE-03 measurements were not taken.
- No packaged builds, LAN, four-player/2v2, Windows/Linux or Exclusive Fullscreen.
- Suite 10 is observational, used no gameplay input and included no listening. GPT-01/04/05 presentation and GPT-06 audible volume need a human playtest.
- Engine-free tests for each PR are covered by their worker sessions and CI, and were not repeated here.

## Completion handoff

- Commit/PR: this report only, on `claude/int-02-report` (docs-only PR). The disposable `claude/integration-02` stays local.
- Changed: `docs/workstreams/integration-02.md` (new)
- Checks actually run and result: build plus suites 1–9 pass, except suite 8, which failed in 2/2 runs (D1). Suite 10 was observed partially.
- Deferred: human playtest items listed in the suite 10 section; MOVE-03 emulation measurements; LAN, packaged and multi-player acceptance
- Remaining defect/dependency: INT-02-D1 (now on `main` `5b13856`)
- Next action/owner: the gameplay lead triages D1 into a GPF task (GPT-06 or UI owner). After the fix, integration reruns only `Tests/UI/test_settings.py`.
