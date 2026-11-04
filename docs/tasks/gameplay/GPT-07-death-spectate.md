# Task GPT-07: dead players spectate living teammates until the round resets

- Status: In progress
- Backlog ID: RULE-03 / PLAY-03 (round loop death state; the MVP death/respawn state)
- Owner/session and provider: Claude session "GPT-07 death spectate"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-07-death-spectate` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-07-death-spectate`
- Owns:
  - `Source/ValStrike/Match/VSMatchPlayerController.h/.cpp`
  - new `Source/ValStrike/Match/VSSpectateRules.h`
  - `Source/ValStrike/VSMatchHUDData.h`: additive fields only
  - new `Tests/Match/spectate_rules_test.cpp`
  - `docs/workstreams/death-spectate.md`
- Read-only dependencies / other owners:
  - `Match/VSMatchGameMode.*`, `VSMatchGameState.*`, `VSMatchPlayerState.*`, `Rules/**`: if a server hook is required, describe it and ask the lead
  - `VSHUD.*`, `VSPlayerController.*` (GPT-01, PR #34): reuse the existing `bSpectating`/`StatusText` HUD data instead of drawing
  - `VSCharacter.*`, `Combat/**`, `Damage/**`, `Config/`, all Content, art and maps
- Result:
  - In match mode, about 2 s after the local player dies in an active round, the camera moves to a living teammate's first-person or close third-person view.
  - Fire (or a dedicated key bound in code without touching `Config/`) cycles living teammates.
  - The view moves on automatically when the watched teammate dies.
  - If no teammate is alive, the camera stays on the death view.
  - It never targets enemies: no information leak beyond CS-style team spectating.
  - Round reset and possession restore the normal view. Late-join spectators keep current behavior.
  - The HUD shows "Spectating <name>" through the existing match HUD data.
  - Target choice is a pure, engine-free rule (`VSSpectateRules.h`) covered by one test: ordering, cycling, skipping dead and enemy players, and the empty fallback.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope: free-fly camera, enemy spectating, kill-cam replays, HUD drawing changes, practice mode.
- Validation tier: V2 (round death state, information boundary)
- Contributor checks: the engine-free test (clang `-Wall -Wextra -Werror -pedantic`); `git diff --check`. No Unreal build or runtime.
- Integration-owned checks: combined Editor build, `Scripts/test_match.py`, plus a 2v2 or three-client observation that a dead player sees only teammates.
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
