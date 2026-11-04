# Task INT-06: verify main `09347c2` (GPT-12/13/14 merged) plus PR #74 (GPT-15 movement feel)

- Status: In progress (waits for the GPT-15 tip that adds the runner bounds; the lead forwards the SHA)
- Owner/session and provider: the INT QA session
- Checkout: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-integration`
- Branch: a disposable, unpushed `claude/integration-06` from `origin/main` `09347c2` (or newer, if only docs were added). Merge `origin/claude/gpt-15-movement-feel` at the SHA the lead gives you, with `--no-ff`. If the tip differs, stop and report.
- Inputs already on main and never built: #69 GPT-13 (`VSSPECTATE` log and match checks), #70 GPT-12 (`test_movement.py --net-profile`), #71 GPT-14 (kill feed: `Scoreboard/*`, `VSHUD.cpp`). Then #74 GPT-15: weapon weight, gravity scale 1.9955, jump 738.2, crouch-jump tuck, new `UVSCharacterMovementComponent`, changed `AVSCharacter` constructor, new movement smoke stages and runner bounds.
- Owns: `docs/workstreams/integration-06.md` on `claude/int-06-report` from `origin/main`, docs-only PR. No source fixes.
- Validation tier: V2

## Suites (each under `python3 Scripts/with_ue_slot.py --`)

| # | Command | Why | Expected |
| --- | --- | --- | --- |
| 0 | `ue.py build --target Editor --platform Mac` | all | exit 0; on failure, list the errors by owner and stop |
| 1 | `Scripts/test_movement.py` (lan) | GPT-15, GPT-12 | all pass with the new bounds; record the jump height, crouch-jump clearance, run speeds (rifle about 387, pistol about 432) |
| 2 | `Scripts/test_movement.py --net-profile typical` | GPT-12, GPT-15 | the run is valid (PktLag lines present); record the `net` block and correction count; steady running should cause no corrections, a switch at most one each |
| 3 | `Scripts/test_match.py` | GPT-13 | 43 plus the new spectate checks; record pass/fail/`not_reached` for each |
| 4 | `Scripts/test_damage.py` (run_runtime) | GPT-14, GPT-15 | 46/46; quote one kill-feed log line or entry if visible |
| 5 | `Scripts/test_combat.py` | GPT-15 speeds | 55/55 (the observer target now walks 57 cm less) |
| 6 | `Scripts/test_weapons.py --scenario practice` | GPT-15 spread classification | 74/74 |
| 7 | `Tests/UI/test_session.py --rendered` | GPT-14 HUD | 72/72; check the reticle and HUD screenshots for a broken layout |

One rerun is allowed per suite for documented unscripted fire (watch item). If it recurs while nobody is at the machine, raise it as an `INT-06-D` harness defect.

## Report

Same format as `integration-05.md`. Kill-feed visuals and movement feel are for human playtest unless a screenshot shows them.
