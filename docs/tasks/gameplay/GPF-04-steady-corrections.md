# Task GPF-04: fix INT-06-D3, where weapon-weighted movement corrects while running steadily under latency

- Status: **Queued via the producer** (not started; the active-contributor cap applies)
- Defect: INT-06-D3, attributed by QA in [PR #82](https://github.com/vaskodagamo/ValStrike/pull/82). Under `test_movement.py --net-profile typical`:
  - `09347c2` (without #74): 15 server corrections, all zero-velocity setup teleports. None in motion, including a steady 450 cm/s boundary run.
  - `a935f0c` (with #74): 38 corrections, 10 with non-zero velocity. Of those, 7 come while accelerating (2.0–6.8 cm) and 3 at a steady 387 cm/s (7.176 cm each). None coincide with the weapon switch.
- So the weight path in #74 (merged) makes server and client disagree on max speed or acceleration in motion. Suspects: `UVSCharacterMovementComponent::GetMaxSpeed` reading the loadout slot differently on the server (replicated slot) and the owning client (predicted slot) at the start, or during replay after a correction; or the weight being applied to the falling/acceleration path and not to the saved-move data.
- Owner: the GPT-15 session (it wrote #74), in its worktree `gpt-15-movement-feel`, on a new branch `claude/gpf-04-steady-corrections` from current `origin/main`.
- Owns: `VSCharacterMovementComponent.*`, the movement-weight parts of `VSCharacter.*`, `VSMovementRules.h`, `Tests/Movement/**`, and an append to `docs/workstreams/movement-feel.md`.
- Result: no non-zero-velocity corrections in steady movement on one weapon under `typical`, matching the 09347c2 baseline. Acceleration-phase corrections drop to the baseline (0). Keep the speeds and the feel unchanged. If the fix needs the max speed in the saved move (`FSavedMove_Character` / compressed flags), keep it minimal.
- Validation tier: V2. Contributor checks: the engine-free tests, `-Wshadow` included; `git diff --check`. No Unreal build.
- Integration-owned checks: Editor build, `test_movement.py` lan and `--net-profile typical` (69/69, corrections at baseline).
