# Task INT-04: verify main `82d7a4c` after the GPF-01 harness fix (#52), settings only

- Status: In progress
- Owner/session and provider: the INT QA session (integration/QA owner)
- Checkout: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-integration`
- Candidate: `origin/main` `82d7a4c`, checked out detached or on a disposable unpushed `claude/integration-04`. It contains everything INT-03 verified (#44, #45, #46) plus #48 (`a0b7562`) and #52 (`4a83116`), and Meshy docs PRs #53–#56 that touch no `Source/`, `Config/`, `Scripts/` or `Tests/`. Confirm that with `git diff --stat 4a83116 82d7a4c -- Source Config Scripts Tests` before building; if anything else changed, stop and report.
- What changed since INT-03b: `UI/VSSettingsSmoke.cpp` `Click()` now enables `SetHandleDeviceInputWhenApplicationNotActive(true)` for its synthesized press/release only, logs `app_active` in `VSSET_CLICK_TRACE`, and the `SVSSessionMenu::Tick` capture reclaim from `a0b7562` is removed. The owner used its 3 cycles; the last one failed to link (an approach since removed), so **this exact code has never been compiled**.
- Owns: `docs/workstreams/integration-04.md` only, on a branch `claude/int-04-report` from `origin/main` with a docs-only PR. No source fixes.
- Validation tier: V2

## Suites (each under `python3 Scripts/with_ue_slot.py --`)

| # | Command | Expected |
| --- | --- | --- |
| 0 | `ue.py build --target Editor --platform Mac` | exit 0. On failure, list the errors and stop; owner GPT-06/GPF-01 |
| 1 | `Tests/UI/test_settings.py` (rendered) | all PASS, in particular `settings_fit_without_scrolling` and `arrow_selects_windowed`. Quote the arrow's `VSSET_CLICK_TRACE` line (expect `app_active` recorded, `captor_after_down:"SButton:VS.WindowMode.Next"`, mode 1→2). Run twice, because D1 was intermittent with app activation |
| 2 | native `Automation RunTests ValStrike.UI` | 4/4 `Result={Success}` |
| 3 | `Tests/UI/test_session.py --rendered` | 72/72, because #52 removed the menu `Tick` reclaim |

Nothing else: INT-03 already covers the other inputs, and no other source changed.

## Report

Same format as `integration-03.md`: candidate SHA, table, `INT-04-D<n>` defects with owner, limits. If both settings runs pass, say that D1 is closed for Editor on this Mac (two monitors, 2560×1440), and that players were never affected (harness defect).
