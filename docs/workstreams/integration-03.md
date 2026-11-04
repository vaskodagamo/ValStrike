# INT-03: main with GPT-07/08, plus GPT-09 (#45) and GPF-01 (#48)

- Date: 2026-11-04 (Europe/Berlin), runs 11:37–11:48
- Task brief: `docs/tasks/gameplay/INT-03-batch.md` (on `claude/gameplay-board-int03`)
- Owner: the INT QA session (integration/QA owner), assigned by the Claude gameplay lead
- Candidate SHA: `e18194ba0f4cc3eef55647f0f62f79572542a0e9`. This is the local disposable `claude/integration-03` (not pushed), built from `origin/main` `887d349` by `--no-ff` merges in the briefed order. There were no conflicts.

| Order | Input | Task | Tip |
| --- | --- | --- | --- |
| base | `origin/main` `887d349` | contains #44 GPT-08 spread tracer and #46 GPT-07 death spectate (merged, unverified until now) | `887d349` |
| 1 | PR #45 `claude/gpt-09-muzzle-kick` | GPT-09 muzzle flash and kick | `c84be81` |
| 2 | PR #48 `claude/gpf-01-settings-window-mode` | GPF-01 settings fit (INT-02-D1) | `f7d62e2` |

- `origin/main` was still `887d349` when this report was written.
- Engine/toolchain: UE 5.8.4 (CL 58902052), Xcode 26.1.1 (17B100), Mac SDK 26.1, Apple clang 17.0.0
- Machine: Apple M4 (Mac16,10), 24 GB, macOS 15.6 (24G84)
- Build identity: `ValStrikeEditor Mac Development`, unity on (`Module.ValStrike.1–3.cpp`), no errors or warnings. Editor receipt `d31910d1b836afa9faabc7451e50ff2d17ff3fad2c76654a543f933108daee1f`.
- Engine slot: every command ran under `python3 Scripts/with_ue_slot.py --`. None returned 75.

## Results

Evidence lives in this checkout's ignored `Saved/`. `Saved/INT-03/NN.log` is each command's console output.

| # | suite | result | counts | evidence path |
| --- | --- | --- | --- | --- |
| 0 | `Scripts/ue.py build --target Editor --platform Mac` | PASS | 4/4 actions, unity, receipt verified | `Saved/INT-03/00.log` |
| 1 | `Scripts/test_shot_effects.py` (rendered) | PASS | 73/73 (GPT-08) | `Saved/Tests/ShotEffects/20261104T103742Z_5191de/` |
| 2 | `Scripts/test_rifle_presentation.py` | PASS | 39/39; `flash_shader_ready` PASS for owner and observer. `owner_muzzle.png` shows the flash at the barrel tip with the tracer leaving it (GPT-09) | `Saved/Tests/RiflePresentation/20261104T103928Z_graybox_554eaa/` |
| 3 | `Scripts/test_weapons.py --scenario practice` | PASS | 74/74 (GPT-08, GPT-09) | `Saved/Tests/Weapons/20261104T104033Z_practice_5c8a78/` |
| 4 | `Scripts/test_match.py` | PASS | 43/43 (GPT-07); see the spectate note | `Saved/Tests/Match/20261104T104125Z_fa649e/` |
| 5 | `Scripts/test_combat.py` | PASS | 55/55 | `Saved/Tests/Combat/20261104T104326Z_3f222c/` |
| 6 | `Tests/UI/test_settings.py` (rendered) | **FAIL** | 64 pass, 14 fail. `settings_fit_without_scrolling` **PASS** (`overflow=0.0`). `arrow_selects_windowed` **FAIL** (`mode=1`); see INT-03-D1. Look checks clean (ratio 3.0000) | `Saved/Tests/Settings/20261104T104448Z_2bc54a/` |
| 7 | UnrealEditor `-nullrhi -ExecCmds="Automation RunTests ValStrike.UI"` | PASS | 4/4 `Result={Success}`; only the 2 known startup `Condition failed` lines | `Saved/INT-03/07_ui_automation.log` |
| 8 | `Tests/UI/test_session.py --rendered`, run 1 | FAIL (environmental) | 70 pass, 2 fail: `initial_combat_defaults_available`, `runtime_report_complete` | `Saved/Tests/UI/20261104T104555Z_474832/` |
| 8 | `Tests/UI/test_session.py --rendered`, rerun | PASS | 72/72 | `Saved/Tests/UI/20261104T104729Z_aa0a9c/` |

Rerun justification for suite 8: in run 1, the standalone `initial` HUD snapshot showed `"magazine":29` before the harness had sent any input (`driver.log:1408-1409`). No shot is logged, and none of the batch's code paths fires or spends a round at startup. The GPT-08 shot-effects change only reads predicted and server shot directions. A single round spent in a rendered window points to OS mouse input reaching it. The rerun shows `"magazine":30` and passes 72/72.

GPT-07 spectate observation: the match runner reaches three players (`server late_join_spectator PASS id=3 phase=2`). GPT-07 code writes no spectate log lines, so the logs cannot show whether the spectate target changed. **Not observed**; this needs a human run or a log line from the owner. No harness was added.

## Defects

### INT-03-D1 (continues INT-02-D1): settings window-mode arrow click does not change the mode

- Suite: 6, `Tests/UI/test_settings.py` (rendered)
- Owner: **GPF-01** (assigned to the GPT-06 session). The bisect showed the bug predates GPT-06: `90849f5` fails the same way (`Saved/Tests/Settings/20261104T093620Z_3cdb74/`).
- What GPF-01 fixed: the page now fits 960×540 (`settings_fit_without_scrolling PASS overflow=0.0`). The window mode and resolution moved to inline label rows.
- Expected: after Windowed Fullscreen is applied, a synthesized click on `VS.WindowMode.Next` sets the pending mode to Windowed (2).
- Actual: unchanged from INT-02. The pending mode stays 1. Then 1280×720 is never offered, applied or saved, and the restart restores 2560×1440. Volume, sensitivity and look checks all pass.
- Key log lines (`Saved/Tests/Settings/20261104T104448Z_2bc54a/first.log`):
  - `:1457` `VSSET_CHECK shift_tab_returns_to_window_mode PASS VS.WindowMode`
  - `:1458` `VSSET_CLICK VS.WindowMode.Next at (1577.4, 782.1) hit_testable=true`
  - `:1459` `VSSET_CHECK mouse_click_window_mode_arrow PASS`
  - `:1460` `VSSET_CHECK arrow_selects_windowed FAIL mode=1` (200 ms after the click)
- Screenshots (same directory, `screenshots/first/`):
  - `settings-pending-windowed.png`: the click point (1577, 782) is the centre of the `›` arrow of the new inline WINDOW MODE row, and the value still reads "Windowed Fullscreen".
  - `windowed-fullscreen.png` shows the state before the click.
- Observation for the owner: every run on four revisions (`90849f5`, `b5b8523`, `c004369`, `e18194b`) delivers this click at the correct centre with `hit_testable=true` and no effect. Clicks on plain buttons in the same run do work, for example `VS.Settings`. So the arrow's click handler is never reached, or it doesn't advance the pending mode in Windowed Fullscreen at 2560×1440. That handler path is where to look.

### INT-03-D2: resolution value clipped in the new inline settings row (cosmetic)

- Suite: 6 (seen in screenshots; no check covers it)
- Owner: **GPF-01**
- Expected: the resolution value is fully readable.
- Actual: in `settings-pending-windowed.png` (Windowed Fullscreen, 2560×1440) the field reads "Desktop · 2560 × 14…", cut off at the right edge. The new inline layout narrows the value box. Not checked at 960×540.
- Severity: low. It doesn't block D1 or any check.

## Limits

- Editor Development binaries on one Mac over loopback, with no emulated latency or loss in these suites.
- No packaged builds, LAN, four-player, Windows/Linux or Exclusive Fullscreen.
- GPT-07 spectate is unobserved (see above). GPT-09 flash timing (40 ms) is covered only by the runner's own checks and one screenshot.
- Movement, damage, economy and lifecycle suites were not run, per the brief.

## Completion handoff

- Commit/PR: this report only, on `claude/int-03-report` (docs-only PR). The disposable `claude/integration-03` stays local.
- Changed: `docs/workstreams/integration-03.md` (new)
- Checks actually run and result: build plus suites 1–8. All pass except suite 6 (D1 persists, D2 cosmetic). Suite 8 failed once from environmental input and passed on rerun.
- Deferred: GPT-07 spectate observation; human playtest items carried over from INT-02
- Remaining defect/dependency: INT-03-D1 and INT-03-D2, both GPF-01. PR #48 resolves only the fit part of INT-02-D1.
- Next action/owner: the gameplay lead decides whether #48 merges as a partial fix, then routes D1 and D2 back to GPF-01. After a fix, integration reruns only `Tests/UI/test_settings.py`.
