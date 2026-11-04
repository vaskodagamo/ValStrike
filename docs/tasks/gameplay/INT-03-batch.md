# Task INT-03: verify main with GPT-07/08 merged, plus open PRs #45 (GPT-09) and #48 (GPF-01)

- Status: In progress
- Owner/session and provider: the INT QA session (integration/QA owner)
- Checkout: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-integration`
- Branch: a disposable, unpushed `claude/integration-03`, created from `origin/main` `887d349`. That base already contains #44 (GPT-08) and #46 (GPT-07), merged by the user and not yet verified.
- Inputs, merged with `--no-ff` in this order (the lead's `git merge-tree` dry run found no conflicts):
  1. `origin/claude/gpt-09-muzzle-kick` `c84be81` (PR #45)
  2. `origin/claude/gpf-01-settings-window-mode` `f7d62e2` (PR #48)
  If a tip has moved, stop and report.
- Owns: `docs/workstreams/integration-03.md` only, on a separate branch `claude/int-03-report` from `origin/main` with a docs-only PR. No source fixes.
- Validation tier: V2

## Suites (each under `python3 Scripts/with_ue_slot.py --`)

| # | Command | Why | Expected |
| --- | --- | --- | --- |
| 0 | `ue.py build --target Editor --platform Mac` | all | exit 0. On failure, list errors by owning task and stop |
| 1 | `Scripts/test_shot_effects.py` (rendered) | GPT-08 | 73/73 |
| 2 | `Scripts/test_rifle_presentation.py` | GPT-09 | 39/39; inspect the flash screenshots if any are produced |
| 3 | `Scripts/test_weapons.py --scenario practice` | GPT-08, GPT-09 | 74/74 |
| 4 | `Scripts/test_match.py` | GPT-07 | 43/43 |
| 5 | `Scripts/test_combat.py` | regression | 55/55 |
| 6 | `Tests/UI/test_settings.py` (rendered) | GPF-01 | Record `settings_fit_without_scrolling` and `arrow_selects_windowed` separately. D1 predates GPT-06 (your bisect: `90849f5` fails the same way), so `arrow_selects_windowed` may still FAIL; capture the log lines around the click and the screenshots for the owner |
| 7 | native `Automation RunTests ValStrike.UI` | GPF-01 | 4/4 `Result={Success}` |
| 8 | `Tests/UI/test_session.py --rendered` | GPF-01 layout | 72/72 |

No movement, damage, economy or lifecycle runs: none of these inputs touches them. The GPT-07 teammate-spectate observation needs three clients, and the existing match runner reaches three players. Record whether its log shows spectate target changes; do not add a harness.

## Report

Same format as `integration-02.md`: candidate SHA and inputs, table, `INT-03-D<n>` defects with owner, limits.
