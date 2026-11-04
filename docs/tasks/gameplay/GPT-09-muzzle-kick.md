# Task GPT-09: first-person muzzle flash for both weapons and rifle viewmodel kick

- Status: In progress
- Backlog ID: GUN-03. The MVP plan says "Give local camera, recoil, muzzle-flash, and sound feedback immediately".
- Owner/session and provider: Claude session "GPT-09 muzzle flash and kick"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-09-muzzle-kick` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-09-muzzle-kick`
- Owns:
  - `Source/ValStrike/Presentation/VSRiflePresentationComponent.h/.cpp`, `VSRiflePresentationState.h`, `VSRiflePresentationTest.h/.cpp`
  - `Source/ValStrike/Presentation/VSPistolPresentationComponent.h/.cpp`
  - a new pure header `Presentation/VSMuzzleFlashState.h`, if useful
  - new `Tests/Presentation/muzzle_flash_state_test.cpp`, or extend the existing pattern in `Tests/Presentation`
  - `docs/workstreams/rifle-presentation.md`: append a dated section
- Read-only dependencies / other owners:
  - `Presentation/VSWeaponPresentationComponent.*` (GPT-04, PR #37), `Combat/**` (GPT-02/04), `Effects/**` (GPT-08), `VSCharacter.*`, `VSHUD.*`
  - all Content, art and maps: reuse existing materials/meshes by reference only, never create or modify assets. A code-built, unlit, emissive-tinted engine primitive or the existing additive `M_VSShotTracer` is acceptable.
  - `Scripts/test_rifle_presentation.py` and its runner tests: read-only unless an expectation must change; then stop and ask
- Result:
  - On each locally accepted shot (the same accepted-shot counter the slide kick and Effects use), the owning client shows a brief muzzle flash at the weapon's `Muzzle` socket. It lasts 1–2 frames, about 40 ms, with a random-free deterministic rotation from the shot id. Observers see one on the third-person weapon.
  - The rifle gets a small first-person viewmodel kick (translate back plus pitch) with fast recovery, like the pistol's slide kick and independent of the camera recoil in GPT-02.
  - One flash per shot: no replay on respawn or rejoin, and none on a dedicated server.
  - A pure state reducer covered by one engine-free test.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope: camera recoil (GPT-02), tracers (GPT-08), audio (GPT-05), lighting/light flashes, new art assets, arms (ART-05).
- Validation tier: V1 (client presentation)
- Contributor checks: the engine-free reducer test (clang `-Wall -Wextra -Werror -pedantic`); `git diff --check`. No Unreal build or runtime.
- Integration-owned checks: combined Editor build, `Scripts/test_rifle_presentation.py` (39/39 baseline) and its screenshots, `Scripts/test_weapons.py --scenario practice`, plus a visual check of the flash.
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
