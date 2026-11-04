# Task INT-02: verify the combined GPT-01..06 candidate before merge

- Status: In progress
- Owner/session and provider: the INT-01 QA session, continued (integration/QA owner)
- Lead: Claude gameplay lead
- Checkout: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-integration`
- Branch: a disposable `claude/integration-02`, created from `origin/main` `90849f5`
- Inputs, merged with `--no-ff` in this order (the lead's dry run with `git merge-tree` found no conflicts):

| Order | PR | Branch | Tip |
| --- | --- | --- | --- |
| 1 | #34 | `claude/gpt-01-hit-feedback` | `0cf1115` |
| 2 | #35 | `claude/gpt-02-recoil-spread` | `a64dfa5` |
| 3 | #36 | `claude/gpt-03-net-emulation` | `4194a16` |
| 4 | #37 | `claude/gpt-04-switch-prediction` | `b674a56` |
| 5 | #40 | `claude/gpt-05-spatial-audio` | `a0a8a38` |
| 6 | #39 | `claude/gpt-06-volume-setting` | `b5b8523` |

  If a tip has moved, stop and report to the lead.
- Owns: `docs/workstreams/integration-02.md` only, plus this checkout's ignored build/test output. No source fixes. A compile error is reported as a defect to its owning GPT task.
- Result and stopping condition: record the candidate SHA, one unity Editor build, then the suites below once each. One report goes on a docs-only PR from a separate branch `claude/int-02-report` based on `origin/main`. The disposable integration branch is not pushed or opened as a PR.
- Validation tier: V2

## Suites (each wrapped in `python3 Scripts/with_ue_slot.py --`)

| # | Command | Why | Expected |
| --- | --- | --- | --- |
| 0 | `ue.py build --target Editor --platform Mac` | all | exit 0. **If it fails, record each error by owning GPT task and stop** |
| 1 | `Tests/Damage/run_runtime.py` | GPT-01 routing, GPT-02 spread | 46/46. Any change from a now-inaccurate first shot is a GPT-02 defect |
| 2 | `Scripts/test_combat.py` | GPT-02, GPT-04 | 55/55 |
| 3 | `Scripts/test_weapons.py --scenario practice` / `--scenario match` | GPT-02, GPT-04 | 74/74, 29/29. Note any `equip` first-shot rejections (GPT-04 jitter risk) |
| 4 | `Scripts/test_shot_effects.py` (rendered) | GPT-02 | 73/73 |
| 5 | `Scripts/test_match.py` | GPT-01, GPT-05 | 43/43 |
| 6 | `Scripts/test_movement.py` | regression | 45/45 |
| 7 | native `Automation RunTests ValStrike.UI` | GPT-06 | Settings and the 3 others `Result={Success}` |
| 8 | `Tests/UI/test_settings.py` (rendered) | GPT-06 | PASS, including the volume row |
| 9 | `Tests/UI/test_session.py --rendered` | GPT-01 HUD, GPT-06 tab order | 72/72 |
| 10 | one manual-observation pass: `ue.py server --mode practice` plus two rendered `ue.py client` with `--net-profile typical` (GPT-03), audio enabled | GPT-01, 03, 04, 05 | Record observations: shooter-only hit marker and victim-only damage arcs; death text; predicted 1/2 switch with no snap-back; audible, spatialized shot/footstep cues; the server log shows the emulation settings. Label this observational and scripted only as far as the runners allow. No new harness |

Engine-free tests for each PR already ran in its worker session and CI. Do not repeat them.

## Report

Use the same format as `integration-01.md`: header (candidate SHA plus the six input tips), result table, `INT-02-D<n>` defects naming the owning GPT task and its files, limits.
