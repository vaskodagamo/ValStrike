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

## 2026-11-04 — GPF-03: spectate stage in the match runner

Task: GPF-03, branch `claude/gpf-03-spectate-scenario` from `origin/main` `30997f4`, same worktree. Validation tier V1. INT-06 passed 43/43, but all three GPT-13 spectate checks came back `not_reached`.

### Why a new auxiliary client and not `VSMatchLate`

`VSMatchLate` is only a participant in match 1, round 2, and it has to carry the device and disconnect there for `disconnect_drop`. A death on its team in that round would break `disconnect_drop` or the later plant that `detonation_win` needs. No development revive path exists. The stage therefore runs **after every existing check**, and the existing rounds and timings are unchanged.

### Stage (`Match/Tests/VSMatchSmoke.cpp`)

1. Second MatchComplete: the server records `fresh_pawn_reset` and `twelve_rounds` as before. It then sets stage 100 and logs `VSMATCH_REQUEST_SPECTATE_JOIN`. The runner launches a fourth process, role `mate`, named `VSMatchMate`. Like late, it writes no report. On the 1:1 roster it joins team One.
2. When the mate is in the roster, the server sets stage 101. The observer writes its report at stage 100. The driver requests a real rematch at stage 101, then writes its report. If the mate does not join within 45 s, the server sets stage 102, nobody requests a rematch, and the server finishes as before. The spectate checks then fail, because the server logged a join request but no stage line.
3. Match 3, round 1 (internal `MatchIndex` 2): the persistent teammate, the mate and the enemy are placed at the site. 0.3 s into Active, the server logs `VSSPECTATE_STAGE {"viewer","teammate","enemy","round"}` and kills the teammate with real `ApplyPointDamage(1000)`. 3.5 s later, after the 2 s spectate delay, it kills the enemy, so the mate's team wins by elimination.
4. The next Preparation ends spectate on the dead client (`stop` in a new round). The server finishes 2 s later.

`TeamController` now also excludes `VSMatchMate`. The auxiliary-client exceptions for late (no report, no native timeout) now also cover `mate`.

### Runner (`Scripts/test_match.py`)

- Launches `mate` on `VSMATCH_REQUEST_SPECTATE_JOIN`, keeps it alive, and stops it at cleanup. Unexpected exits fail the run, as for the other roles. `ROLES`, `LAUNCH_ROLES` and `REQUIRED_CHECKS` are unchanged, and the result now records `spectate_client.launched`.
- `evaluate_spectate` reads the server's `VSSPECTATE_STAGE` line. When the stage was reached:
  - `teammate_start` needs a `start` from that viewer, on that teammate, in that round.
  - `stop_on_round_reset` needs a stop in a later round. A missing one is `fail`, not `not_reached`.
  - The stage enemy counts as an enemy target.
  
  The only dead players in the stage are the viewer and the enemy, and both are rejected as targets. Without the stage line, the earlier behavior is kept (`not_reached`). A reset stop now means any round id that differs from the start round, which is valid after a rematch.

### Completion handoff

- Commit/PR: see the PR for `claude/gpf-03-spectate-scenario` (the user merges).
- Checks actually run: `python3 -m unittest Tests/Match/test_match_runner.py` (36 tests, OK); `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Match/spectate_rules_test.cpp` (21 checks, 0 failures); `git diff --check` (clean).
- Deferred to integration: Editor build (compile **verification pending**) and `Scripts/test_match.py`. Expected: 43 checks plus the three spectate checks as `pass`, with four client processes.
- Risks:
  - The run gets longer. Previous runs took about 125 s, and I estimate about 25–40 s more: client launch, a 3 s Preparation, about 4 s Active, then the result display and Preparation. The native limit is 220 s and the runner limit is 240 s.
  - The driver's report is now written a few seconds later, at stage 101/102 instead of 100.
  - Mate team assignment depends on the `One <= Two` balance in `VSMatchGameMode`. The stage reads the team instead of assuming it.
- Next owner: INT QA.
