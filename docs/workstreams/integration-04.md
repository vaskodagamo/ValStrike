# INT-04: verify main after the GPF-01 harness fix (#52), settings path only

- Date: 2026-11-04 (Europe/Berlin), runs 14:04–14:07
- Task brief: `docs/tasks/gameplay/INT-04-main-settings.md` (on `claude/gameplay-board-int04`)
- Owner: the integration/QA session, assigned by the Claude gameplay lead
- Candidate SHA: `origin/main` `ee25083298801008291747e710b078bf6ea9563b`. It was checked out on a local disposable `claude/integration-04` (not pushed), with a clean tree.
- Relation to the brief's `82d7a4c`:
  - Since `82d7a4c`, only Meshy docs PRs (#57) have landed. `git diff --stat 82d7a4c origin/main -- Source Config Scripts Tests` is empty.
  - The briefed check `git diff --stat 4a83116 origin/main -- Source Config Scripts Tests` is **not** empty (15 files). That is expected. `4a83116` is the #52 branch tip, cut before #44, #45 and #46 merged, so the diff shows the GPT-07/08/09 code that INT-03/03b already verified.
  - The exact check is against the #52 merge, `49cf3c5`. `git diff --stat 49cf3c5 origin/main -- Source Config Scripts Tests` is empty. That merge's own first-parent delta is just the GPF-01 change: `SVSSessionMenu.cpp/.h` (the `Tick` reclaim removed), `VSSettingsSmoke.cpp` and `Tests/UI/README.md`.
- Engine/toolchain/machine: UE 5.8.4 (CL 58902052), Xcode 26.1.1, Apple M4 (Mac16,10), macOS 15.6. This is the same setup as INT-01..03, with two monitors and the main display at 2560×1440.
- Build identity:
  - `ValStrikeEditor Mac Development`, unity on (`Module.ValStrike.1–3.cpp`), 0 errors and 0 warnings.
  - Editor receipt `288c2e71b4f4c08d5f3233bd815eef1c64ae50af6ef532f82a171b5846a0af81`.
  - The settings and session runners report `revision=ee25083…` and `dirty_source_checkout=false`.
- Engine slot: every command ran under `python3 Scripts/with_ue_slot.py --`. None returned 75.

## Results

Evidence lives in this checkout's ignored `Saved/`. `Saved/INT-04/NN.log` is each command's console output.

| # | suite | result | counts | evidence path |
| --- | --- | --- | --- | --- |
| 0 | `Scripts/ue.py build --target Editor --platform Mac` | PASS | 4/4 actions, unity, receipt verified. This is the first compile of `4a83116` | `Saved/INT-04/00.log` |
| 1 | `Tests/UI/test_settings.py` (rendered), run 1 | PASS | 78/78: `settings_fit_without_scrolling` PASS (`overflow=0.0`), `arrow_selects_windowed` PASS (`mode=2`), look ratio 3.0000 | `Saved/Tests/Settings/20261104T130455Z_913223/` |
| 1 | `Tests/UI/test_settings.py` (rendered), run 2 | PASS | 78/78, same key checks as run 1 | `Saved/Tests/Settings/20261104T130544Z_705cf1/` |
| 2 | UnrealEditor `-nullrhi -ExecCmds="Automation RunTests ValStrike.UI"` | PASS | 4/4 `Result={Success}` (JoinKey, RoundPresentation, ServerAddress, Settings). Only the 2 known startup `Condition failed` lines | `Saved/INT-04/02_ui_automation.log` |
| 3 | `Tests/UI/test_session.py --rendered` | PASS | 72/72, with the menu `Tick` reclaim removed | `Saved/Tests/UI/20261104T130644Z_c72889/` |

Both settings runs are planned runs required by the brief, not reruns. Neither showed input contamination: in both, look yaw and pitch were 25.200 against an expected 25.200.

### Arrow click trace

From run 1, `first.log:1459-1462`. Run 2 shows identical lines at the same line numbers:

```text
VSSET_CLICK VS.WindowMode.Next at (1577.4, 782.1) hit_testable=true
VSSET_CLICK_TRACE {"tag":"VS.WindowMode.Next","app_active":false,"captor_before":"none","focus_before":"SButton:VS.WindowMode","under_point":"STextBlock","hovered_after_move":true,"captor_after_down":"SButton:VS.WindowMode.Next","mode_before":1,"mode_after":2,"status_after":"Press Apply display to switch."}
VSSET_CHECK arrow_selects_windowed PASS mode=2
```

- `app_active:false` in both runs. The app was not the active application when the harness clicked. This matches the GPF-01 diagnosis: before #52, Slate dropped device input to an inactive app, so the arrow never took capture.
- With `SetHandleDeviceInputWhenApplicationNotActive(true)` scoped to the synthesized press/release, `captor_after_down` is now `SButton:VS.WindowMode.Next`. In INT-03b it was `"none"`. The mode goes from 1 to 2, which is what the brief expects.

## Defects

None. Both settings runs pass, so **INT-02-D1 is closed** for the Editor on this Mac (two monitors, 2560×1440). It was a harness defect: synthesized clicks into an inactive app were ignored. Players were never affected. INT-03-D2 (resolution value clipping) stays fixed: `settings_fit_without_scrolling` passes in both runs.

## Limits

- Settings path only, as briefed. The other suites were not rerun: INT-03/03b covered them, and no other source changed since.
- Editor Development binaries on one Mac. No packaged build, Exclusive Fullscreen, Windows/Linux, or other display layouts.
- The trace shows the app was inactive in both runs. The app-active branch of the click path was not exercised here.

## Completion handoff

- Commit/PR: this report only, on `claude/int-04-report` (docs-only PR). The disposable `claude/integration-04` stays local.
- Changed: `docs/workstreams/integration-04.md` (new)
- Checks actually run and result: build plus suites 1 (×2), 2 and 3, all PASS on `ee25083`.
- Remaining defect/dependency: none from this batch.
- Next action/owner: the gameplay lead marks GPF-01 / INT-02-D1 as Verified.
