# Movement feel workstream

## 2026-11-04 — GPT-15: weapon weight, CS2-scaled jump arc and crouch-jump

**Branch/base:** `claude/gpt-15-movement-feel` from `b342ca9`. Brief: `docs/tasks/gameplay/GPT-15-movement-feel.md` (on `claude/gameplay-board-int06`). MOVE-02. Validation tier V2. Status: implementation ready, verification pending.

- **Commit/PR:** see the PR for `claude/gpt-15-movement-feel`.

### Observable change

| | Before | After |
| --- | --- | --- |
| Run speed, rifle / pistol | 450 / 450 cm/s | **387 / 432** cm/s |
| Slow-walk (Shift, 0.45 input), rifle / pistol | 202.5 / 202.5 | **174.2 / 194.4** |
| Crouch walk, rifle / pistol | 180 / 180 | **154.8 / 172.8** |
| Gravity | 980 cm/s² (scale 1) | **1955.6 cm/s²** (scale 1.9955) |
| Jump velocity | 420 cm/s | **738.2 cm/s** |
| Standing jump: feet apex / airtime | 90.0 cm / 0.857 s | **139.3 cm / 0.755 s** |
| Crouch-jump feet clearance | Ctrl+Space: no jump. Space then Ctrl: 122 cm | **161.3 cm** both ways (+22.0 cm) |
| Running jump distance (flat ground) | 386 cm | **292 cm rifle, 326 cm pistol**; same height as standing |

### Arithmetic (all in `Source/ValStrike/VSMovementRules.h`)

- **Weapon weight.** CS2 max speeds relative to knife 250 u/s: pistol 240 → 0.96, AK 215 → 0.86. `MaxWalkSpeed` and `MaxWalkSpeedCrouched` stay at the unarmed 450/180; `UVSCharacterMovementComponent::GetMaxSpeed` multiplies walking, nav-walking and falling speed. Slow-walk is the existing 0.45 analog input of that max, so it follows. 450 × 0.86 = 387, 450 × 0.96 = 432, 180 × 0.86 = 154.8, 180 × 0.96 = 172.8.
- **Height scale.** k = 176 cm (capsule 2 × 88) / 72 u = 2.4444 cm/u. Only vertical motion is scaled by height; ground speeds keep the existing 450 base (k_speed = 450 / 250 = 1.8), as the brief asks.
- **Gravity and jump.** CS2: g = 800 u/s², v = 301.993 u/s, apex = v² / 2g = 57.0 u (0.792 × 72 u), airtime = 2v / g = 0.755 s. Scaling g and v by the same k keeps 2v/g and scales v²/2g by k:
  - g = 800 × 2.4444 = 1955.6 cm/s², `GravityScale` = 1955.6 / 980 = **1.9955**.
  - `JumpZVelocity` = 301.993 × 2.4444 = **738.2** cm/s.
  - apex = 738.2² / (2 × 1955.6) = **139.3 cm** = 0.792 × 176. Airtime = 2 × 738.2 / 1955.6 = **0.755 s** (was 0.857 s, 12 % shorter; gravity is 2× stronger, which removes the floaty hang).
- **Crouch-jump.** CS2 gain: 66 − 57 = 9 u → 9 × 2.4444 = **22.0 cm**. UE crouches a falling capsule about its center (`bCrouchMaintainsBaseLocation` is false while falling, `CharacterMovementComponent.cpp:1457`), so its feet rise by the half-height difference 88 − 56 = 32 cm. `UVSCharacterMovementComponent::Crouch` takes back 32 − 22 = **10 cm** with a swept move right after an in-air crouch. Clearance = 139.3 + 22.0 = **161.3 cm**; 161.3 / 139.3 = 1.158 = 66 / 57. No vertical velocity is added; uncrouching in the air reverses the tuck (and is refused by the engine when the standing capsule does not fit, e.g. over a ledge).
- **Running jump.** Jumping only sets Z velocity; air friction is 0 and air speed is capped at the same weighted max with `AirControl` 0.15, so horizontal speed carries: distance = speed × 0.755 s (387 → 292 cm, 432 → 326 cm). Apex is unchanged. CS2 adds **no** height on a running jump either; extra height when running would be a follow-up decision for the user.
- **No bunny-hop gain.** Input cannot accelerate past the weighted max in the air, and the jump adds no horizontal speed, so the landing speed is never above the ground max. Ground friction applies as soon as the character lands. (Switching pistol → rifle mid-air leaves you above the rifle max until landing; friction then brings it down. That is not a gain.)

### Implementation

- `Source/ValStrike/VSMovementRules.h` (new, engine-free): CS2 constants, `SpeedMultiplier`/`MaxSpeed` by `WeaponKind`, `JumpApex`, `Airtime`, the derived `GravityScale`/`JumpZVelocity`, `AirTuckDrop`, `CrouchJumpClearance`, `JumpDistance`.
- `Source/ValStrike/VSCharacterMovementComponent.h/.cpp` (new): `GetMaxSpeed` (weapon weight), `CanAttemptJump` (drops the engine's `!bWantsToCrouch`), `Crouch` (in-air tuck cap).
- `Source/ValStrike/VSCharacter.h/.cpp`:
  - constructor takes `FObjectInitializer` and sets `UVSCharacterMovementComponent` as the default `CharMoveComp` class; tuning values come from the rules header.
  - `GetMovementWeaponKind()` maps `UVSWeaponLoadoutComponent::GetEquippedSlot()` (owning client: predicted slot; server and observers: replicated state).
  - `CanJumpInternal_Implementation` allows a jump while crouched.
  - `CheckJumpInput`: a jump from a **ground** crouch first un-crouches (base kept, encroachment-checked), then jumps; the held crouch tucks again in the same move. So Ctrl held before Space, Ctrl+Space together and Space then Ctrl all reach 161 cm. Under a low ceiling the un-crouch fails and the jump starts crouched with no gain.
- Authority and prediction: every change runs inside the CharacterMovement move (`CheckJumpInput`, `UpdateCharacterStateBeforeMovement`, `GetMaxSpeed`) on the owning client, on the server in `MoveAutonomous`, and in client replays. Inputs are the existing compressed jump and crouch flags; no new RPC or replicated field.
- `Source/ValStrike/Tests/VSMovementSmoke.cpp`:
  - samples carry `"weapon":"rifle"|"pistol"`.
  - the server places the driver at (200, −1100, 90) at 10.9 s, so the slower rifle still reaches the east wall well before the 16.5 s window (arrival ≈ 15.5 s, as with the old 450 cm/s; without it ≈ 16.7 s).
  - new stages after the existing ones (driver placed at (−1400, −1100, 90) at 32 s, open lane of VS_Graybox): `2` at 32.5 s (pistol); W 33.0–34.6 (pistol run); Ctrl+Space together at 35.0 (Ctrl to 36.2, standing crouch-jump); W 36.6–39.8 with Space at 37.2 (running jump) and Ctrl+Space at 38.6 (Ctrl to 39.6, running crouch-jump); Ctrl 40.2–41.7 with Space at 40.7 (jump from a held crouch). `done` moves from 33.0 to **42.5 s**.
- `Tests/Movement/movement_rules_test.cpp` (new): 61 checks on the table above, the CS2 ratios, invariants (airtime unchanged by scale, tuck never lifts) and invalid inputs.

### Proposed `Scripts/test_movement.py` bounds (runner owned by GPT-12; not edited here)

Changes to existing checks (rifle is equipped for all of them):

| Check | Current | Proposed | Expected |
| --- | --- | --- | --- |
| `run speed` (1.6–2.4 s) | 400–480 | **360–410** | 387 |
| `walk speed` (3.6–4.8 s) | 170–230 | **155–195** | 174.2 |
| `crouch move speed` (6.0–6.8 s) | 150–205 | **135–175** | 154.8 |
| `jump and land` height (8.0–9.5 s) | 50 < h < 120 | **125 < h < 150** | 139.3 (0.1 s sampling can read up to 2.4 cm low) |

Everything else (crouch capsule, feet, eye, landing 10.0–10.8 s, boundary, stairs, cover, identity checks) is unchanged. With today's runner and this branch, run speed and jump height fail by design, and walk (174 vs ≥170) and crouch (155 vs ≥150) pass only narrowly.

New checks for the new stages, per role. `feet(i) = i["z"] - i["half_height"]`, `floor_feet = floor - 88` (`floor` is the existing standing-z median), `stand = max feet over 8.0–9.5 s − floor_feet`:

| Check | Window | Condition | Expected |
| --- | --- | --- | --- |
| rifle before switch | 1.6–2.4 | all `weapon == "rifle"` | |
| pistol weight | 33.6–34.4 | ≥4 samples, all `weapon == "pistol"`, median speed 410–450 | 432 |
| Ctrl+Space crouch-jump | 35.0–36.0 | some sample crouched and not grounded; `cj = max feet − floor_feet` with 145 < cj < 170 and 15 < cj − stand < 30 | 161.3, +22 |
| running jump | 37.2–38.1 | `max z − floor` within 8 cm of the standing-jump height; airborne samples in 37.3–37.9 ≥3, median speed ≥ 410, max ≤ 445 | 139.3, 432 |
| running crouch-jump | 38.6–39.5 | 15 < (max feet − floor_feet) − stand < 30; airborne max speed ≤ 445 | +22, 432 |
| no landing gain | 38.0–38.5 | grounded samples' max speed ≤ 445 | ≤ 432 |
| held-crouch jump | crouched and grounded at 40.45–40.65; jump 40.7–41.6 | 145 < max feet − floor_feet < 170 | 161.3 |
| completed | | `done` at ≈42.5 s (was 33.0); 120 s default timeout still fits | |

The observer sees the driver about two one-way lags late; the windows above leave ≥0.15 s after each event for `lan`/`good`. GPT-12's latency notes apply to the new windows too.

### Reachable heights (feet clearance on flat ground; read-only)

Landing needs the feet above the top while moving over it; margins below are clearance minus top. Site01 from `Art/Source/Maps/Site01/design.py` (kit sizes as in `Tests/Maps/Site01/test_layout.py`):

| Surface (top) | Old jump 90 / old air-crouch 122 | New jump 139.3 | New crouch-jump 161.3 |
| --- | --- | --- | --- |
| `SiteEastCrate`, `NorthCrate`, `SouthCrate` (Crate_100, 100 cm) | no / yes (+22) | **yes (+39)** | yes (+61) |
| `NorthHalfCover`, `SouthHalfCover`, `SiteWestCover` (Cover_200x120, 120 cm) | no / marginal (+2) | **yes (+19, about 0.28 s above the top)** | yes (+41) |
| `SiteEastCrateTop` (stacked, 200 cm) | no | no | no (−39); the lower crate is covered by it, and nothing ≥ 39 cm high is within reach to start from |
| `SouthTransformer` 260, `DefenderScreen`/`AttackerScreen` 360, `NorthService` 380, `RelayHouse` 420, boundaries 500 | no | no | no. Even from a 120 cover (120 + 161 = 281 ≥ 260) the nearest cover is 350 cm from the transformer, beyond a 326 cm jump that peaks at mid-flight |

So **newly reachable with a plain jump:** all three 120 cm half-covers and all 100 cm crates. **Still unreachable:** the 200 cm stacked crate and every structure. In CS2 terms a 120 cm cover is 49 u, which CS2 also clears with a standing jump. If the half-covers should stay un-climbable, that is a map decision (raise them above about 140 cm, or above 161 cm to also block crouch-jumps), not a movement change.

VS_Graybox (smoke map): the 120 cm `VS_Cover_2` becomes jumpable; the 180 cm `VS_Cover_1` and 200 cm `VS_Cover_3` stay unreachable (crouch-jump 161); the 80 cm platform is reachable.

### Checks actually run

- `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Movement/movement_rules_test.cpp` → 61/61 passed.
- `g++ -std=c++17 -Wall -Wextra -Werror -pedantic …` → 61/61 passed. On this Mac `g++` is Apple clang 17; real GCC runs only in CI, and the test is not in `.github/workflows/checks.yml` (not owned here; wiring it in is a follow-up for the workflow owner).
- `git diff --check`: clean.
- No Unreal build or launch. The UE changes were checked against UE 5.8.4 engine source only (`Character.cpp:313-345`, `CharacterMovementComponent.cpp:1140-1190, 1440-1460, 3237-3354, 3500-3530, 3587-3605, 6441-6450, 10678`).

### Deferred to integration (V2)

- Editor build of the combined candidate (new `UCLASS`, changed `AVSCharacter` constructor signature).
- `Scripts/test_movement.py` with the bounds above, after the runner edit (GPT-12 owns the runner; the lead assigns the edit after GPT-12 merges). Until then expect `run speed` and `jump and land` to fail on all three roles.
- `Scripts/test_movement.py --net-profile typical` (GPT-12): confirm zero corrections during steady running on each weapon, and that the pistol switch at 32.5 s causes at most one correction.
- `Scripts/test_combat.py`: the observer target walks 7.2–8.1 s and now covers ≈ 57 cm less with the rifle; confirm the later shots still hit.
- `Scripts/test_weapons.py --scenario practice`, `Scripts/test_damage.py`: moving and airborne spread classification at the new speeds.
- Logs: no `*** Client: Error for` / `*** Server: Error for` spam during steady movement.
- Human playtest decides the feel (jump height, how snappy the gravity is, half-cover climbing).

### Risks

- **Predicted switch:** the owning client changes speed on the key press, the server when the reliable equip RPC arrives. The equip RPC is sent from the key press, so it usually reaches the server before or with the moves that used the new slot, and they agree. If the movement RPC and the equip RPC arrive out of order, under loss, while a switch is queued by the 0.1 s rate limit, or when a rejected prediction rolls back, the server may simulate a few moves at the other speed: one small correction per switch while moving (45 cm/s difference). Client replays after a correction use the current slot, not the slot at the time of each saved move. Steady movement on one weapon is unaffected.
- **Slow-walk now classifies as standing for recoil/spread.** `VSRecoilRules` uses `movingSpeed = 200`; slow-walk was 202.5 (moving) and is now 174 (rifle) or 194 (pistol). `Combat/**` is read-only here; the combat owner should decide whether to lower `movingSpeed` (e.g. to 150) or keep walking accurate as in CS2.
- **Footsteps** use the unweighted `MaxWalkSpeed` (450) × 0.6 = 270 cm/s as the audible threshold. Rifle and pistol running stay audible and slow-walk stays silent, but the stride cadence is slower with the rifle. `Tests/Audio/gameplay_audio_state_test.cpp` still comments "MaxWalkSpeed 450", which remains the property value.
- **Held-crouch jump pop:** a jump from a full ground crouch raises the crouched capsule by 22 cm on the first frame (stand, launch, tuck). It is the same reach as CS2's crouch-jump, but the eye jumps up at once. CS2's gradual duck hides this; a playtest should judge it.
- The in-air tuck makes the camera drop 42 cm (32 engine + 10 cap) on the crouch frame, as the head moves down.
- The constructor now takes `FObjectInitializer`; a Blueprint subclass of `AVSCharacter` (none found in `Config/` or source) would get the new movement component class automatically.

### Next action / owner

Gameplay lead reviews the PR; the user merges; integration runs the deferred checks; the lead assigns the `Scripts/test_movement.py` bound edit after GPT-12 merges; the combat owner decides on `movingSpeed`.

## 2026-11-04 — GPT-15 follow-up: `test_movement.py` bounds and feel-stage checks

The lead assigned the runner to GPT-15 after GPT-12 (#70) merged. The branch merges `origin/main` `09347c2` with no conflicts.

- **Decision (lead):** `VSRecoilRules::movingSpeed` stays at 200. Slow-walk (rifle 174, pistol 194) counting as accurate matches CS2. No Combat change.
- **`Scripts/test_movement.py`:**
  - The existing bounds are now exactly the proposed ones: run 360–410, walk 155–195, crouch 135–175, jump 125 < h < 150.
  - New `feel_checks()` adds eight checks per role, as tabled above (69 checks in all, was 45): `rifle before switch`, `pistol weight`, `Ctrl+Space crouch-jump`, `running jump`, `running crouch-jump`, `no landing gain`, `held-crouch jump`, and `feel stages completed` (a `done` event at t ≥ 42.0 s).
  - The thresholds are named constants next to the function.
  - GPT-12's `--net-profile` handling, the invalid-run check and every other tolerance are unchanged. `completed` itself is unchanged; the timing requirement is a separate check, so a stale 33 s smoke binary fails only that check and the stage checks.
  - Samples without a `weapon` field (an old binary) fail the two weapon checks.
- **`Tests/Movement/test_movement_runner.py`:** 10 new engine-free tests (23 in all), built on a synthetic three-role timeline of the new tuning:
  - the full timeline passes all 69 checks;
  - each speed bound passes just inside and fails just outside (and fails at the old 450 / 202.5 / 180);
  - the old 90 cm jump fails;
  - no pistol switch, or the rifle speed at 33.6 s, fails `pistol weight`;
  - a missing `weapon` field fails the weapon checks;
  - crouch-jumps without the tuck fail all three crouch-jump checks, and an overly high crouch-jump fails;
  - a higher or faster running jump fails;
  - a held-crouch jump that doesn't start crouched fails;
  - `done` at 33 s fails `feel stages completed`.
- **Checks run:**
  - `python3 -m unittest Tests/Movement/test_movement_runner.py`: 23 OK.
  - `python3 -m unittest Tests/test_ue.py`: OK.
  - `py_compile` on both files: OK.
  - The movement rules test: 61/61 with `clang++` and `g++` (Apple clang).
  - `git diff --check`: clean.
  - The UI runners' `from test_movement import stop` is unchanged.
  - No Unreal build or launch.
- **Deferred to integration:** a real `Scripts/test_movement.py` run (expect 69/69), then the `--net-profile` matrix. The new windows are the narrowest under `poor`.

## 2026-11-04 — INT-06-D1: `-Wshadow` build fix

- **Defect:** the Editor build failed at `VSMovementRules.h:103` with `-Wshadow -Werror`: the `JumpDistance` parameter `Gravity` shadowed the namespace constant `VSMovementRules::Gravity`.
- **Fix:** renamed the gravity parameter of `JumpApex`, `Airtime` and `JumpDistance` to `GravityCmS2`. No behavior change.
- **Other new code:** I compared the locals in `VSCharacterMovementComponent.cpp`, the `VSCharacter` changes and the smoke harness against the `UCharacterMovementComponent`, `ACharacter`, `APawn` and `AActor` members, and found no collisions.
- **Checks run:**
  - The rules test with `clang++` and `g++` (Apple clang) at `-std=c++17 -Wall -Wextra -Wshadow -Werror -pedantic`: 61/61. The test's compile line now includes `-Wshadow`.
  - The runner unit tests: 23 OK.
  - `git diff --check`: clean.
  - No Unreal build.

## 2026-11-04 — GPF-04 / INT-06-D3: in-motion corrections under `typical`

- **Branch:** `claude/gpf-04-steady-corrections`, from `origin/main` `a1c2371`. The PR description has the exact head SHA.
- **Verdict:** D3 is a harness artifact. The boundary run started while a teleport correction was still in flight. **No movement code changed.** `VSCharacterMovementComponent.*`, `VSCharacter.*` (including #93's presentation code) and `VSMovementRules.h` are untouched.
- **Evidence:** QA run `team-integration/Saved/Tests/Movement/20261104T140018Z_14b888`, read only.
  - **One desync episode.** All 10 server errors in motion fall in a single 0.22 s stretch at the start of the boundary run (server Time 34.042–34.265).
    - The error grows 2.0 → 7.176 cm while accelerating, then stays at 7.176 cm.
    - The driver applied one correction (6.105 cm at 34.162) and then a 0.000 cm confirmation at 34.265. After that it agreed with the server for the rest of the 4.4 s run at 387 cm/s.
    - The "3 steady" errors are the same offset, logged again within one round trip, before the correction reached the client.
  - **What the offset means.** 7.176 cm = 387 cm/s × 18.5 ms: one client frame of extra acceleration on the server.
    - The server runs each `ServerMove` for `ClientTimeStamp − CurrentClientTimeStamp` (`FNetworkPredictionData_Server_Character::GetBaseServerMoveDeltaTime`, `CharacterMovementComponent.cpp:12636`).
    - So an idle client frame that the server doesn't simulate on its own is run with the next move's acceleration. A time shift Δ at the start of acceleration ends as an offset of v_max × Δ.
    - The engine ran this movement under a correction cycle: the 481 cm teleport correction at Time 33.804 reached the client at 14:01:07.562, and W followed about 100 ms later.
    - The resulting mismatch is a one-time timing offset at the start of the run. It is not a disagreement about max speed: GetMaxSpeed was 387 on both sides.
  - **Weight agrees everywhere else.** The same build under `typical` logged **zero** in-motion errors in:
    - stairs (21–24 s) and cover (28–31 s): rifle 387 cm/s, accelerating from rest;
    - the feel stage (33–41.7 s): pistol 432 cm/s, a switch, running jumps and crouch-jumps.
  - **Cause.** #74 added the boundary teleport at 10.9 s with W pressed at 11.0 s. That 0.1 s gap is shorter than one round trip under `typical` (40 ± 10 ms each way). Every other stage leaves 1.0 s. `09347c2` has no boundary teleport, so its baseline shows 0.
- **Fix (timing only, granted by the lead outside my owned paths):**
  - `VSMovementSmoke.cpp`: the boundary placement moves from 10.9 s to **10.85 s**, and the boundary W window from 11.0–18.0 s to **11.8–18.0 s**.
  - `Scripts/test_movement.py`: the wall window moves from 16.5–17.5 to **17.0–18.0**, and the no-pass-through window from 11.0–18.0 to **11.8–18.0**. Bounds are unchanged (x 1905–1920, median speed < 5, at least 5 samples).
  - **Timing arithmetic:**
    - (a) The placement at 10.85 s lies outside every sampled window: after `landed` 10.0–10.8 (and `jump` 8.0–9.5), and before the boundary window at 11.8. Clients see the teleport at ≥ 10.9 s.
    - (b) Teleport to W is 0.95 s, close to the other stages' 1.0 s and well above one round trip (about 0.1 s). The QA run settled the correction in about 0.12 s.
    - (c) The QA run reached the wall (x 1915.9) at 15.65 s with W at 11.0, so 4.65 s from W to the wall, latency included. With W at 11.8 it arrives at about 16.45 s, 0.55 s before the wall window opens. The stairs placement at 20.0 s is unaffected.
    - Why not 10.0 s: it falls inside `landed`. Why not 9.6 s: it would also hide a failed landing (the teleport would put the pawn on the ground).
- **Runner unit tests:**
  - The synthetic timeline follows the new timing.
  - New `test_boundary_run_timing`:
    - landing samples stay at the jump spot;
    - a driver still running at 17.0–17.5 fails `boundary blocks movement`;
    - passing x 1920 fails it too.
  - Fixed a test-entrypoint bug from my earlier follow-up: `if __name__ == "__main__"` sat mid-file, so `python3 Tests/Movement/test_movement_runner.py` ran only 13 tests. It now runs all 24.
- **Checks run:**
  - Runner unit tests: 24 OK, both as a script and via `unittest discover`.
  - The rules test with `clang++` and `g++` (Apple clang) at `-Wall -Wextra -Wshadow -Werror -pedantic`: 61/61.
  - `git diff --check`: clean.
  - `git diff origin/main -- Source/ValStrike/VSCharacter.*`: empty.
  - No Unreal build.
- **Verification pending (integration):** Editor build, then `Scripts/test_movement.py` lan and `--net-profile typical`. Expect 69/69 and no server corrections with non-zero velocity or acceleration; only the zero-velocity `setup` teleports remain.
- **Risk:** if integration still sees in-motion corrections under `typical` after this change, the harness explanation is wrong and **D3 reopens against the weight path** (`UVSCharacterMovementComponent::GetMaxSpeed` or replay). One QA sample per tree; a repeat run would make the result firm.
