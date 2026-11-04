# Recoil and spread workstream (GPT-02)

Append-only. Dates and times are Europe/Berlin. Task brief: `docs/tasks/gameplay/GPT-02-recoil-spread.md` on the lead's branch `claude/gameplay-team-kickoff`. Backlog: GUN-03 (recoil part). Branch `claude/gpt-02-recoil-spread`, base `90849f5`. Validation tier V2.

## 2026-11-04 — Implementation ready, verification pending

### Completion handoff

- **Commit/PR:** implementation `aebf913`; this handoff in the following commit. PR to `main`: see the PR opened from `claude/gpt-02-recoil-spread`.
- **Changed (owned paths only):**
  - New `Source/ValStrike/Combat/VSRecoilRules.h`: header-only and engine-free, like `VSWeaponLoadoutRules.h`. No `.cpp` was needed. It contains:
    - **Heat:** each accepted shot adds 1. After `recoveryDelay` with no shot, heat drains at `recoveryRate` per second. A shot's index is `floor(heat)` before the shot, and heat is capped at 64.
    - **Pattern:** rifle climb of `climbPerShot` per index, up to `maxClimb`. From `driftStartIndex` on, a sinusoidal horizontal drift bounded by `±maxDrift`. Pistol: straight kick up to its cap.
    - **Spread:** a cone half-angle = `baseSpread + min(bloomPerShot × index, maxBloom)`. Crouched multiplies it by `crouchScale`; moving adds `movingSpread` and airborne adds `airborneSpread`. The total is capped at `maxSpread`. Stance: falling → airborne, horizontal speed > 200 cm/s → moving, else crouched or standing. Crouch-walking (180 cm/s) stays crouched.
    - **Shot offset:** `ShotOffset(tuning, index, stance, seed)` = pattern plus a uniform disk sample of the cone. The seed comes from `ShotSeed(shotId, weaponSalt)` (integer hash; the salt is the damage class).
    - **View kick:** `ViewKick` (fixed pitch, yaw sign from the seed) and `RecoverView` (delay, then a straight return to zero at `viewRecoveryRate`).
    - **Tuning presets:** `RifleTuning()` and `PistolTuning()`, marked as **playtest proposals**. Both give an **exact first shot** when standing or crouched (base spread 0, index 0).

    | Value | Rifle | Pistol |
    | --- | --- | --- |
    | Recovery delay / rate | 0.18 s / 10 per s | 0 s / 3 per s |
    | Climb per shot / cap | 0.3° / 3.0° | 0.5° / 2.0° |
    | Drift | from index 5, ±1.2°, period 10 shots | none |
    | Bloom per shot / cap | 0.15° / 1.5° | 0.4° / 2.0° |
    | Crouch scale | ×0.6 | ×0.6 |
    | Moving / airborne penalty | +2.5° / +6.0° | +1.5° / +5.0° |
    | Spread cap | 8° | 7° |
    | View kick pitch / yaw | 0.1° / ±0.04° | 0.6° / ±0.15° |
    | View recovery delay / rate | 0.18 s / 6° per s | 0.05 s / 6° per s |

  - `VSWeaponComponent.h/.cpp`:
    - **Server (`ServerRequestFire`):** after every existing validation and the phase gate, it registers the shot in the server's own recoil state. It resolves the direction from the **validated client aim** plus `ShotOffset(index from server time, the server's view of the stance, ShotSeed(ShotId, class))`, then traces along that direction. The RPC signature is unchanged and the client never sends an offset. Rejected requests do not advance recoil.
    - **Client (`SendShot`):** predicts with the same `ResolveShot` using its own recoil state and clock, and stores the result in `GetLastPredictedShot()`. It sends the unmodified aim, then applies the view kick once through the owning `APlayerController` (`SetControlRotation`). The kick happens only in `SendShot`, so server replication or `DevelopmentRequest` never kick.
    - **Kick recovery:** a tick that is off by default and enabled only on the owning client while a kick is outstanding. It removes only the outstanding kick, so the player's own aim changes are kept. On a listen-server host, prediction and server state are kept separate.
    - **New read-only accessors:**
      - `GetCurrentSpreadDegrees()`: the next shot's cone on this machine, for the later crosshair task.
      - `GetLastPredictedShot()` and `GetLastServerShot()`: shot id, index, spread and direction.
    - `DevelopmentReset()` also clears the server recoil state.
  - `VSRifleComponent.cpp` / `VSPistolComponent.cpp`: assign `RifleTuning()` / `PistolTuning()`.
  - New `Tests/Weapons/recoil_rules_test.cpp`: 108 checks, following the pattern of `weapon_loadout_rules_test.cpp`:
    - determinism for the same seed and index, and variation across shot ids;
    - exact first shot for both weapons;
    - index climb, no recovery during the delay, partial and full recovery, time running backwards, NaN time, heat cap;
    - the weapon smoke's pistol taps 0.3 s apart stay at index 0;
    - bounded climb and drift, and the pistol's straight kick;
    - spread monotonic in the index, stance order, and every sample inside the cone;
    - worst-case offset < 15°, inside the server's 20° aim-validation cone;
    - stance classification;
    - view kick magnitude and sign, recovery delay, and exact return to zero.

    `test_weapons_runner.py` is unchanged: the existing C++ test is not registered there either.
- **Checks actually run and result:**
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/recoil_rules_test.cpp`: 108/108 passed. Also passed with `-Wshadow -Wconversion` and with `-fsanitize=address,undefined`.
  - Existing `weapon_loadout_rules_test.cpp`: 20/20 passed.
  - `python3 -m unittest discover -s Tests/Weapons`: OK.
  - `git diff --check`: clean.
- **Deferred checks (integration, INT-01). Compile and runtime are verification pending:**
  - **Combined Editor build.** This is the first compile of the UE side. Points to watch:
    - the `TickComponent` override;
    - `FRotationMatrix` / `FRotator(double…)` use;
    - the non-UPROPERTY engine-free members in the `UCLASS`, the same as `DamageClass`;
    - the `GameFramework/CharacterMovementComponent.h` include.
  - `Scripts/test_combat.py`:
    - `body_damage` (1.0 s) and `head_death` (6.0 s) are isolated taps at index 0 while standing, so they should be **exact and unchanged**.
    - `cover_blocks_damage` and `empty_magazine` (held fire at 14–16 s and 17–23 s) now climb up to 3° plus drift and up to 1.5° of bloom. The cover blocks about ±18° horizontally and about 24° upward from the driver, so health 100 is expected to hold. Check it.
    - The driver's aim is reset every tick, so the view kick does not accumulate there.
    - `validation_no_mutation` uses `DevelopmentRequest` after `DevelopmentReset()`, so index 0 and no change.
  - `Scripts/test_weapons.py` (practice and match):
    - `pistol_semi_auto`, `pistol_damage` (taps 0.3 s apart → index 0, exact) and `per_weapon_ammo` should be unchanged.
    - The rifle tap after equip is index 0.
  - `Tests/Damage/run_runtime.py`: one aimed tap per stage, more than 0.6 s apart, standing or crouched, so zone damage is expected to be unchanged. A crouched index 0 shot is also exact.
  - `Scripts/test_shot_effects.py`: the cosmetic tracer and mark still trace **base aim** (`Effects/` is read-only for this task), so effect counts are unaffected. Impact points can differ from the server's hit during sprays, which already applies to moving targets. The view kick changes control rotation between planned shots, but each shot re-sets its aim and the rifle kick (0.1°) recovers within about 0.2 s.
  - **Also affected:** `Scripts/test_rifle_presentation.py` `aim_unchanged`. It compares control rotation 0.001° before the shot and at 4.2 s after it. The kick is now applied and then recovered exactly to zero, so it should still pass. If it fails by a small residual, this kick is the cause.
  - **Network acceptance:** real server/client play to confirm that held rifle fire climbs and drifts, that pistol spam kicks, and that moving or jumping shots spread. Confirm that the listen-server host and remote client both kick once per shot.
- **Remaining defect/dependency and known limits:**
  - **Prediction can diverge.** The client predicts the index from its own clock and stance. If arrival jitter crosses a heat boundary or the server rejects a shot, the predicted direction can differ from the server's. The server stays authoritative; only `GetLastPredictedShot()` is affected.
  - **Cheat resistance is limited.** The seed is predictable by design, so that client prediction is possible. A modified client could pre-compensate its aim, or skip shot ids (allowed up to 1024) to pick favourable seeds. This is the usual shared-seed trade-off. Lag compensation (NEXT-02) is out of scope.
  - **Tracers ignore spread.** The `Effects/` tracer still uses base aim. Its owner could use `GetLastPredictedShot()` for the local shooter's tracer in a follow-up.
  - The weapon tests are not in CI. Neither C++ test in `Tests/Weapons` is in the CI `cpp` job, and `.github/` is not owned by this task. The lead or integration may add `recoil_rules_test.cpp` there.
  - Recoil heat is not reset on holster. It recovers over time, and equip time already separates the shots.
- **Next action/owner:**
  - Gameplay lead: review the PR.
  - INT-01: include it in the next combined batch with the checks above.
  - Playtest: tune the proposed values.
  - The user merges.
