# Spread crosshair workstream (GPT-10)

Append-only. Dates and times are Europe/Berlin. Task brief: `docs/tasks/gameplay/GPT-10-spread-crosshair.md` on the lead's branch `claude/gameplay-board-int04`. Backlog: GUN-03 (crosshair part). Branch `claude/gpt-10-spread-crosshair`, base `origin/main` `ee25083` (source-identical to the brief's `82d7a4c`; the commits between change Meshy docs only). Validation tier V1 (client presentation).

## 2026-11-04 — Implementation ready, verification pending

### Completion handoff

- **Commit/PR:** see the PR to `main` from `claude/gpt-10-spread-crosshair`. Implementation and this handoff are in one commit.
- **Observable change:** the reticle's four arms now move outward with the equipped weapon's current spread cone. They open on firing, moving or jumping, and close again during recovery. The arm length stays 6 px × scale. Standing still with a fresh rifle or pistol (spread 0) shows the static 4 px / 10 px reticle, as before.
- **Changed (owned paths only):**
  - New `Source/ValStrike/Feedback/VSCrosshairRules.h`: header-only and engine-free.
    - `ProjectGapPx(spread, hfov, width)` = `tan(spread) / tan(hfov/2) × width/2`. It returns 0 for invalid input: spread ≤ 0, NaN, ≥ 45°, FOV outside (1°, 179°), or a non-positive width.
    - `TargetGapPx` clamps the projection to [static gap, max gap], and returns exactly the static gap whenever the spread cannot be shown.
    - `Step` is an exponential smoothing step: it opens at 30/s and closes at 12/s. It is frame-rate independent and never overshoots. The frame time is clamped to [0, 0.25 s]. It snaps exactly onto the target within 0.05 px, and the first step after a reset starts at the static gap. It has no randomness.
  - `VSHUD.h`: private `ReticleGap(...)` helper, a `VSCrosshairRules::State` member and its last-frame time.
  - `VSHUD.cpp`, reticle block only:
    - The gap is `ReticleGap(character, 4 × scale, scale)` and the outer end is `10 × scale + (gap − static gap)`, so the static gap reproduces today's numbers bit for bit.
    - `ReticleGap` reads `GetEquippedWeapon()` and its `GetReadiness`. A holstered weapon or no weapon returns the static gap immediately and resets the smoothing. Otherwise it reads `GetCurrentSpreadDegrees()`, which is the local client's predicted recoil state, and `PlayerCameraManager->GetFOVAngle()`. It projects onto `Canvas->SizeX` and smooths on world time. The maximum gap is 60 px × scale. Once fully recovered, it hands back the exact static value and resets.
    - The smoothing also resets when the reticle is hidden (dead, spectating, result) or there is no pawn.
  - New `Tests/Feedback/crosshair_rules_test.cpp`: 63 checks. They cover the projection formula and how it scales with FOV and width, invalid spread/FOV/width, the clamps (including max < min), monotonic no-overshoot opening, slower closing, settling exactly on the target and on the static gap, equality of one 100 ms step with ten 10 ms steps, zero/negative/NaN time, the hitch bound, NaN targets, a corrupted state, a scale change, and reset.
- **No gameplay change:** draw only. There is no replication, RPC, server or Combat change, and no Content, art or map touched.
- **Checks actually run (macOS, Apple clang):**
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Feedback/crosshair_rules_test.cpp -o Saved/Tests/Feedback/crosshair_rules_test && Saved/Tests/Feedback/crosshair_rules_test`: `63/63 crosshair rule checks passed`.
  - The existing `Tests/Feedback/hit_feedback_state_test.cpp`, built the same way: `58/58` passed (sanity check; this task did not change it).
  - `git diff --check`: clean.
- **Not run:** any Unreal build, Editor or runtime check. `VSHUD.cpp` has not been compiled.
- **Deferred to integration:**
  - Editor build of the combined candidate.
  - `Tests/UI/test_session.py --rendered`, for the HUD screenshots. The idle reticle must match the previous screenshots.
  - `Scripts/test_weapons.py --scenario practice`.
  - A visual gap check: in practice, the gap opens while firing a burst and while moving or jumping, closes smoothly on recovery, never drops inside the static gap, and stays the static reticle after a switch or with no weapon.
- **Risks / notes:**
  - The projection assumes the camera's horizontal FOV spans `Canvas->SizeX` (the default maintain-X-FOV aspect setting).
  - The 60 px × scale cap is a proposal: at 90° FOV and 1280 px, the rifle's 8° cap would project to about 90 px.
  - Both new tests (this one and `hit_feedback_state_test`) are not in `.github/workflows/checks.yml`. Adding `Tests/Feedback` to the engine-free CI job is a follow-up for that file's owner.
- **Next owner:** the gameplay lead for review, then integration for the deferred checks.
