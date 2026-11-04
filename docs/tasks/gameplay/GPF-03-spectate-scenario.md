# Task GPF-03: make the GPT-13 spectate checks reachable in the match runner

- Status: In progress
- Finding: INT-06 `test_match.py` passed 43/43, but all three GPT-13 spectate checks came back `not_reached`. The driver and observer are on different teams, so no client ever enters death spectate with a living teammate. That leaves GPT-07 (PR #46) still unverified.
- Owner/session: the GPT-13 session (it wrote the checks), same worktree `.parallel-worktrees/gpt-13-spectate-observability`, new branch `claude/gpf-03-spectate-scenario` from current `origin/main`
- Owns: `Source/ValStrike/Match/Tests/VSMatchSmoke.*` (the scenario only), `Scripts/test_match.py`, `Tests/Match/test_match_runner.py`, and an append to `docs/workstreams/death-spectate.md`. Every other `Match/**` file is read-only.
- Result:
  - The runner already reaches three players, including `VSMatchLate`. Add a bounded stage: two same-team players, one killed by a server-side development kill (existing dev reset/kill paths only, and no new shipping code) while the teammate lives. The spectate checks then go from `not_reached` to pass or fail: start targets the teammate, stop on round reset, never an enemy or a dead player.
  - Existing 43 checks unchanged. If the stage changes round counts or timings that other checks rely on, put it after them.
  - Runner unit tests updated.
- Validation tier: V1. Contributor checks: runner unit tests, `spectate_rules_test.cpp`, `git diff --check`. No Unreal.
- Integration-owned checks: Editor build, `test_match.py` (43 plus 3 spectate checks passing).
- Publication: push and open a PR; the user merges.
