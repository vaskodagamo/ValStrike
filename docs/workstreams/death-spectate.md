# Death spectate workstream (GPT-07)

## 2026-11-04 — Dead players follow living teammates

Task: GPT-07 (RULE-03 / PLAY-03), branch `claude/gpt-07-death-spectate` from `90849f5`, worktree `.parallel-worktrees/gpt-07-death-spectate`. Validation tier V2.

### Behavior

- About 2 s (`ValStrike::Spectate::DelaySeconds`) after the local participant's possessed pawn dies in an Active or Planted round, the camera moves to a living teammate's pawn (its `FirstPersonCamera`). The first target is the living teammate with the lowest connection id.
- Left mouse (raw key, read in `InputKey`) cycles living teammates in ascending connection-id order and wraps. The pawn's Enhanced Input fire action is not consumed, and the dead pawn's own fire stays rejected by its health gate.
- When the watched teammate dies, leaves or changes team, the view moves to the next eligible teammate in the same tick. If no teammate is alive, the camera goes back to the player's own death view.
- Only same-team, participating, alive players (replicated `bAlive` plus a live pawn) are eligible. Enemies, self, late joins and unassigned players never are. The team rule is checked again each time the target is used, not only when it is chosen.
- The view stays on the teammate through the Resolved result display. Preparation's fresh possession, `ClearPawns` (roster loss or match cleared) and MatchComplete restore the normal view.
- Late joins have no possessed pawn and are not participants, so they keep the existing `NAME_Spectating` behavior unchanged.
- HUD: while following a teammate, `GetMatchHUDData` sets `bSpectating = true`, the new additive `SpectatingName`, and, during Active/Planted with no map error, `StatusText = "Spectating <name>"`. The existing HUD draws this unchanged: the status line under the objective, plus the bottom "SPECTATING / SPECTATING · TEAM n" panel. The result panel keeps its own status during Resolved.

Spectating is client-local presentation. It uses only state the client already receives, so it adds no new replication or server hook. `VSMatchGameMode` is untouched.

### Completion handoff

- Commit/PR: see the PR for `claude/gpt-07-death-spectate` (the user merges).
- Changed: `Source/ValStrike/Match/VSSpectateRules.h` (new pure rule), `Source/ValStrike/Match/VSMatchPlayerController.h/.cpp` (`PlayerTick`, target tracking, LMB cycling, HUD data), `Source/ValStrike/VSMatchHUDData.h` (additive `SpectatingName`), `Tests/Match/spectate_rules_test.cpp` (new), this file.
- Checks actually run and result:
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Match/spectate_rules_test.cpp`: PASS, 21 checks, 0 failures. Also passed with `g++` using the same flags.
  - `git diff --check`: clean.
- Deferred checks (integration/QA owner): compile **verification pending**; runtime **verification pending**. Integration runs the combined Editor build, `Scripts/test_match.py`, and the scenarios below.
- Remaining defect/dependency:
  - The new test is not yet wired into `.github/workflows` CI, because that file is outside this task's owned paths. A one-step addition like the rifle-presentation step would cover it.
  - The watched pawn is a simulated proxy, so `UCameraComponent` ignores control rotation and the view follows the teammate's body yaw with level pitch, not their exact aim. Acceptable for the MVP; a camera-manager follow could use replicated `RemoteViewPitch` later.
  - Teammates' `OwnerNoSee` body/head visuals are hidden through their camera, which is the intended first-person look. Whether their first-person weapon presentation shows correctly from a non-owning viewer must be checked visually.
- Next action/owner: INT QA runs the scenarios below on the combined candidate. Defects return to the GPT-07 owner.

### Runtime scenarios for QA

Run a 2v2 match on Site01 (`?game=/Script/ValStrike.VSMatchGameMode`, `-VSMatchMinimum=2`) with at least one observing client per team, or three clients where noted.

1. **Delay**: kill player A (team 1) during Active. A's view stays on its own dead pawn for about 2 s, with the ELIMINATED banner, then moves to living teammate B. The HUD shows "Spectating <B's name>" and the SPECTATING panel.
2. **Teammates only**: with A dead and both enemies alive, A's view never lands on an enemy across repeated LMB presses. With A and B dead and enemies alive, A stays on the death view.
3. **Cycling**: the 2-per-team cap means a dead player has at most one living teammate, so live play can only check that LMB keeps the same target. Multi-target ordering and wrap are covered by the engine-free test. LMB while dead must not fire a shot, play a local shot effect or change ammo.
4. **Auto-advance and fallback**: while A watches B, kill B. A's view goes back to A's death view (no teammate alive) within a tick, and the "Spectating" status disappears.
5. **Planted**: A dies after the plant. Spectating works the same during Planted.
6. **Round reset**: after Resolved, the next Preparation gives A a fresh pawn with a normal first-person view, reticle and help bar. Also check a rematch after MatchComplete.
7. **Late join**: a third or fifth client joining mid-round stays on the existing spectator view with no teammate follow, and its HUD is unchanged.
8. **Listen-server host** (if used): the host dying behaves as in scenario 1.
9. **Information boundary** (three-client observation): across scenarios 1–6, the dead client's camera never shows an enemy pawn's point of view, and its HUD never names an enemy.

## 2026-11-04 — GPT-13: spectate observability

Task: GPT-13, branch `claude/gpt-13-spectate-observability` from `b342ca9`, worktree `.parallel-worktrees/gpt-13-spectate-observability`. Validation tier V1.

### Change

- `Source/ValStrike/Match/VSMatchPlayerController.cpp`: behind `#if !UE_BUILD_SHIPPING`, the owning client logs one `LogValStrike` Display line per view-target change: `VSSPECTATE {"event":"start|cycle|stop","viewer":<id>,"target":<id|0>,"round":<RoundId>}`. `start` is 0 → target, `cycle` is target → another target (LMB or auto-advance), `stop` is target → 0. The line is logged only when `SpectateTargetId` changes, so it does not repeat every frame. Spectate behavior itself is unchanged.
- `Scripts/test_match.py`: after the existing 43 checks, the runner reads `driver.log` and `observer.log` and records three `spectate:` checks in a new `spectate_checks` result field. Each check is `pass`, `fail` or `not_reached`:
  - `teammate_start`: a `start` exists, and its target is not zero, self or the other persistent client when that client is on the opposing team. Team and id come from each client's `replicated_match` detail.
  - `stop_on_round_reset`: every start is followed by a stop, the sequence is consistent, and at least one stop is in a later round.
  - `no_enemy_or_dead_target`: no `start` or `cycle` target is self or a known enemy.
  A `fail` fails the run. `not_reached` is printed and recorded, but it does not pass or fail. A malformed `VSSPECTATE` line counts as invalid evidence. The 43 existing checks and `REQUIRED_CHECKS` are unchanged.
- `Tests/Match/test_match_runner.py`: new `MatchSpectateTests` class (8 tests). It parses sample lines, rejects malformed ones, and covers not_reached, pass, enemy/self/foreign-viewer failures, a missing stop or a bad sequence, missing identity, and runner integration. Existing tests are unchanged.

### Expected result on the existing scenario

All three spectate checks will probably be **`not_reached`**. The only persistent combatants, driver and observer, are on opposite teams. A persistent client therefore never dies while a teammate lives. The late client can be the attacker's teammate during the disconnect-drop round, but nobody is killed in that window, and the late client keeps no report. As the brief says, no new scenario was built. A real `pass` needs a smoke stage that kills a persistent client while its late teammate lives. That would be a separate task in `Match/Tests/VSMatchSmoke.cpp`.

The log line does not carry the target's life state. The runner cannot see a dead target directly; the client's `CanWatch` re-check and `spectate_rules_test.cpp` exclude it.

### Completion handoff

- Commit/PR: see the PR for `claude/gpt-13-spectate-observability` (the user merges).
- Checks actually run: `python3 -m unittest Tests/Match/test_match_runner.py`: 30 tests OK. `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Match/spectate_rules_test.cpp`: 21 checks, 0 failures. `git diff --check`: clean.
- Deferred to integration: Editor build (compile **verification pending**), `Scripts/test_match.py` (43 checks plus the three spectate checks, expected `not_reached`), and a manual 2v2 check that `VSSPECTATE` lines appear in the client log for scenarios 1, 4 and 6 above.
- Risks: the `%llu` formatting matches existing `VSMATCH` lines. The line is client-side Display verbosity, so it appears in `-abslog` client logs.
- Next owner: INT QA.
