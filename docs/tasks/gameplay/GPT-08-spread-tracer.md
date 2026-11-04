# Task GPT-08: tracers and impact marks follow the recoil/spread shot direction

- Status: In progress
- Backlog ID: GUN-03 (follow-up from the GPT-02 handoff: "Tracers ignore spread")
- Owner/session and provider: Claude session "GPT-08 spread tracer"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-08-spread-tracer` from `a64dfa5`, the GPT-02 tip, stacked on PR #35. The PR must say that it merges after #35.
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-08-spread-tracer`
- Owns:
  - `Source/ValStrike/Effects/VSShotEffectsSubsystem.h/.cpp` and `Effects/VSShotEffectsState.h`
  - `Tests/Effects/shot_effects_state_test.cpp`
  - `docs/workstreams/shot-effects.md`: append a dated section
- Read-only dependencies / other owners:
  - `Combat/**`, including GPT-02's `GetLastPredictedShot()`/`GetLastServerShot()` accessors. Do not modify; describe needed accessor changes to the lead.
  - `Presentation/**` (GPT-04, GPT-09), the `M_VSShotTracer`/`M_VSImpactMark` assets and all Content: read-only
  - `Scripts/test_shot_effects.py` and `Tests/Effects/test_shot_effects_runner.py`: read-only unless an expectation must change; then stop and ask
- Result:
  - The local shooter's tracer uses the predicted spread direction for its accepted shot id.
  - Observers' tracers and impacts use the server-resolved direction or impact point already replicated. No new replication unless unavoidable: ask first.
  - If the predicted and server directions differ, the local view still shows exactly one tracer per shot, so prediction never duplicates.
  - Existing once-only, pool-cap and no-replay behavior is unchanged.
  - The reducer test is extended for the direction choice: predicted versus server, fallback to base aim when no prediction exists for that shot id.
  - Stop when committed, the PR is open (stacked on #35) and the handoff is written.
- Out of scope: muzzle flash (GPT-09), new materials, audio, recoil logic.
- Validation tier: V1 (client presentation)
- Contributor checks: the extended `shot_effects_state_test.cpp` (clang `-Wall -Wextra -Werror -pedantic`); `git diff --check`. No Unreal build or runtime.
- Integration-owned checks: combined build including #35, then `Scripts/test_shot_effects.py` (73/73 baseline; it uses first-shot-accurate aimed shots) and a sustained-fire visual observation.
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR whose description names #35 as a prerequisite; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
