# Task GPT-05: basic spatial gameplay audio with code-generated placeholder sounds

- Status: In progress
- Backlog ID: PLAY-03 audio part. The MVP feedback row includes "basic spatial audio"; GUN-03 lists firing audio.
- Owner/session and provider: Claude session "GPT-05 spatial audio"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-05-spatial-audio` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-05-spatial-audio`
- Owns:
  - new `Source/ValStrike/Audio/**`
  - new `Tests/Audio/**`
  - `docs/workstreams/spatial-audio.md`
- Read-only dependencies / other owners:
  - everything else in `Source/`
  - `ValStrike.Build.cs`: if a module dependency is truly required, stop and ask the lead; prefer the `Engine` module's `USoundWaveProcedural`
  - `Config/`, all Content, art and maps
  - Other workers own `Combat/` weapon components (GPT-02, GPT-04), `VSHUD`/`VSPlayerController`/`Feedback` (GPT-01), and `UI/VSUserSettings`/settings menu (GPT-06).
- Result:
  - A client-only `UVSGameplayAudioSubsystem` (world or local-player subsystem). It is absent on dedicated servers and never replicates.
  - It observes existing replicated state through public read-only accessors, the way `Effects/VSShotEffectsSubsystem` observes accepted shots:
    - accepted shots per weapon (rifle and pistol differ; first-person shots non-spatial, others 3D);
    - footsteps from each pawn's ground speed and stance (walk is quiet or silent and crouch silent, CS-style: running is audible);
    - jump landing;
    - own damage taken and death;
    - objective plant/defuse start, the device beep that speeds up near detonation, and defuse complete.
  - Sounds are short procedural placeholders: `USoundWaveProcedural` filled with synthesized clicks, noise bursts and tones from a pure, engine-free generator. The generator is clearly marked as placeholder, to be replaced by Codex art assets through one `UPROPERTY` soft-reference slot per cue (null means the placeholder).
  - Spatialization uses a code-built `USoundAttenuation` with distance falloff. Occlusion is out of scope.
  - Each event plays once: no replay on respawn, rejoin or replication catch-up (same rule as GUN-03).
  - A `-VSNoGameplayAudio` switch disables it for test runs.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope: real audio assets, music, the volume setting (GPT-06; the engine's master volume will apply), voice chat, occlusion/reverb, UI click sounds.
- Validation tier: V1 (client-only presentation, no rule change)
- Contributor checks: one engine-free C++ test of the pure parts (event dedup from counters, footstep cadence from speed/stance, beep interval curve, generator output bounded in [-1, 1] and deterministic), compiled with clang `-Wall -Wextra -Werror -pedantic`; `git diff --check`. No Unreal build or runtime.
- New test, justified: once-only event emission is the duplicate-feedback rule.
- Integration-owned checks: combined Editor build, then a rendered client with audio enabled, recording that each cue is audible and spatialized (by ear), plus the existing combat/match suites to confirm no behavior change (they run `-nosound`).
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
