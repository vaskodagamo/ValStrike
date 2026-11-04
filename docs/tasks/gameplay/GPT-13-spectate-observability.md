# Task GPT-13: make GPT-07 death-spectate observable in the match runner

- Status: In progress
- Why: INT-03 could not verify GPT-07 (PR #46), because the spectate target changes leave no log line. The behavior is merged but unverified.
- Owner/session and provider: Claude session "GPT-13 spectate observability"
- Branch and base: `claude/gpt-13-spectate-observability` from `origin/main` `b342ca9`
- Worktree: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-13-spectate-observability`
- Owns:
  - `Source/ValStrike/Match/VSMatchPlayerController.cpp`: one development-only log line in the spectate start/cycle/stop paths, nothing else
  - `Scripts/test_match.py` and `Tests/Match/test_match_runner.py`: additive checks only
  - append to `docs/workstreams/death-spectate.md`
- Read-only: `Match/VSSpectateRules.h` and the rest of `Match/**`, `VSMatchHUDData.h`, every other runner, Content and maps.
- Result:
  - Behind `#if !UE_BUILD_SHIPPING`, a structured line `VSSPECTATE {"event":"start|cycle|stop","viewer":<id>,"target":<id|0>,"round":<n>}` on the owning client when the view target changes. One line per change, no per-frame spam.
  - `test_match.py` already reaches three players. Add checks: after a player dies while a teammate lives, a `start` targets that living teammate; a `stop` follows on round reset; no target is ever an enemy or a dead player. If the existing scenario cannot produce a death with a living teammate, record the check as `not_reached` (not a pass), explain why, and do not build a new scenario.
  - Existing 43 checks unchanged.
  - Runner unit tests parse sample `VSSPECTATE` lines, with no engine.
- Validation tier: V1. Contributor checks: runner unit tests, `Tests/Match/spectate_rules_test.cpp` still passes, `git diff --check`. No Unreal.
- Integration-owned checks: Editor build, `test_match.py` (43 plus the new checks).
- Engine slot: none. Publication: push and open a PR; the user merges.
