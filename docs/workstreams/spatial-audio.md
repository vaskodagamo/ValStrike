# Spatial gameplay audio (GPT-05)

Owner: Claude session "GPT-05 spatial audio", branch `claude/gpt-05-spatial-audio` from `90849f5`. Backlog: PLAY-03 audio part, GUN-03 firing audio. Tier V1.

## What it does

`UVSGameplayAudioSubsystem` (`Source/ValStrike/Audio/`) is a client-only tickable world subsystem. It is not created on dedicated servers, sends no RPCs and writes no gameplay state. Like `UVSShotEffectsSubsystem` it reads replicated state through public accessors each frame and plays each event once.

| Cue | Source of truth | Where it plays |
| --- | --- | --- |
| Rifle / pistol shot | each weapon's `GetState().ShotsFired`, same `VSShotEffects::ShotCounter` replay rule as the tracers (a coalesced batch = one report) | own first-person: 2D; everyone else: 3D at the shooter's view location |
| Footstep | ground speed (`Velocity.Size2D()`) vs `MaxWalkSpeed`, `IsMovingOnGround()`, `bIsCrouched` | 3D at the feet, attached to the pawn; own: quiet 2D |
| Land | falling → grounded after at least 300 cm/s downward speed | same as footsteps |
| Hurt / death | local controller's pawn health (`UVSHealthComponent::GetHealth()`) decreasing / reaching 0 | 2D |
| Plant start / defuse start | new nonzero `FVSMatchRepState::InteractionToken`; defuse when the device state is Planted | 3D at the interacting player's pawn (via `AVSMatchPlayerState::ConnectionId`), else at the device; own: 2D |
| Device beep | device state Planted, server time vs `PhaseDeadlineSeconds`; 1 s at plant, quadratically down to 0.12 s | 3D at the device |
| Defuse complete | device state Planted → Defused within one round | 3D at the device |

CS-style footsteps: steps are audible only above 60% of run speed (270 of 450 cm/s). Walking (0.45 input scale, about 202 cm/s) and crouching are silent. Cadence is one step per 140 cm on the ground, so running produces about 3.2 steps per second. Quiet movement never accumulates toward a later step.

Once-only rules: the first sight of every counter or state is a silent baseline. This covers joining, respawn (new pawn and components), relevancy return and a late first replication. A shot jump larger than the elapsed time allows is also a baseline. Repeated snapshots are quiet, heals and resets re-baseline silently, and joining mid-plant or after a defuse replays nothing. The device beep is current state, not history, so a client that joins during a plant hears the beep.

Spatialization uses three code-built `USoundAttenuation` objects (sphere, NaturalSound falloff, -60 dB at max, no occlusion). In cm: shots 400 inner / 7000 falloff, footsteps 150 / 2200, objective 250 / 3500. Art that carries its own `AttenuationSettings` keeps them. At most 24 voices play; the oldest is stopped first. The engine's master volume applies (GPT-06 owns the setting).

Disabled by `-VSNoGameplayAudio`, by a process without audio (`-nosound`, no audio device) and in non-game worlds. The log line `Gameplay audio active (...)`, `disabled (-VSNoGameplayAudio)` or `inactive: no audio device` says which applies.

## Placeholder sounds

`VSGameplayAudio::Placeholder::Generate` (`Audio/VSGameplayAudioState.h`) is a pure, engine-free generator, marked PLACEHOLDER. It produces deterministic mono samples within [-1, 1], each cue under one second: noise bursts plus a low thump for shots and steps, buzzy sweeps for hurt/death and sine blips for objective cues. Footsteps have four variants. The subsystem converts each to PCM16 at 48 kHz once and plays every instance through its own `USoundWaveProcedural`, because a procedural wave streams its queue to a single playback. Each voice is stopped after its known length.

## Art contract (Codex)

One soft-reference slot per cue on `UVSGameplayAudioSubsystem` (`UCLASS(Config=Game)`). An empty slot, or one that does not load, keeps the placeholder. Slots are filled in `Config/DefaultGame.ini` (owner: coordinator/lead), not by engineering:

```ini
[/Script/ValStrike.VSGameplayAudioSubsystem]
RifleShotSound=/Game/Audio/<asset>.<asset>
```

| Slot | Cue | Notes for the asset |
| --- | --- | --- |
| `RifleShotSound` | rifle shot | short, fully automatic cadence (one play per accepted shot) |
| `PistolShotSound` | pistol shot | clearly distinct from the rifle |
| `FootstepSound` | running footstep | one play per step; use a SoundCue/MetaSound with random variation |
| `LandSound` | jump landing | heavier than a step |
| `HurtSound` | own damage taken | 2D, non-spatial |
| `DeathSound` | own death | 2D, non-spatial |
| `PlantStartSound` | plant started | played at the planter |
| `DefuseStartSound` | defuse started | played at the defuser |
| `DeviceBeepSound` | device beep | very short (under 0.12 s) single beep; code sets the cadence |
| `DefuseCompleteSound` | defuse complete | played at the device |

All should be mono, non-looping and under 3 s (longer art is cut at 3 s). Leave attenuation empty to use the code falloff above, or set it on the asset to override.

## Completion handoff

- Commit/PR: branch `claude/gpt-05-spatial-audio`; PR to `vaskodagamo/ValStrike` `main` (link in the PR list).
- Changed: new `Source/ValStrike/Audio/VSGameplayAudioState.h`, `VSGameplayAudioSubsystem.{h,cpp}`, `Tests/Audio/gameplay_audio_state_test.cpp`, this file. No existing file changed; no `Build.cs` change (only `Engine` module types).
- Checks actually run and result (2026-11-04):
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Audio/gameplay_audio_state_test.cpp -o Saved/Tests/Audio/gameplay_audio_state_test && Saved/Tests/Audio/gameplay_audio_state_test` → `203 checks, 0 failures` (Apple clang 17). It covers once-only shot/objective/life emission, footstep cadence by speed/stance, landing, the beep curve and the generator (bounded, deterministic, audible).
  - The state header passes a syntax-only check with `-Wshadow -Wconversion -Wsign-conversion`.
  - `git diff --check` clean.
- Deferred checks (integration, INT-01): **verification pending** for both compile and runtime.
  1. Combined Editor build. Engine APIs used without a local compile: `USoundWaveProcedural::SetSampleRate/QueueAudio`, `FSoundAttenuationSettings` fields, `UGameplayStatics::SpawnSound2D/SpawnSoundAtLocation/SpawnSoundAttached` (overload with rotation), `UWorld::GetAudioDeviceRaw`, `FApp::CanEverRenderAudio`.
  2. Rendered client with audio and a second client or server: each cue audible once per event; remote shots, running footsteps and the device spatialized and attenuated by distance; walking and crouching silent; no shot/hurt/plant replay after respawn or rejoin; beep speeds up toward detonation; no lingering voices (`GetActiveVoiceCount()` returns toward 0 when idle).
  3. Existing combat/match suites unchanged (they run `-nosound`; the log should say `inactive: no audio device`).
- Remaining defect/dependency: the new engine-free test is not in `.github/workflows/checks.yml` (outside this task's paths). The lead or coordinator can add a step mirroring "Rifle presentation state". No detonation cue: the brief does not include one.
- Next action/owner: INT-01 builds and verifies; Codex fills the art slots later; the user merges.
