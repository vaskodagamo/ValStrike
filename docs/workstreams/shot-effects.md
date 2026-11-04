# Shot effects workstream

## 2026-11-03 — Client-only tracers and impact marks started

**Assignment/base:** side session started by the integration chat; branch `claude/shot-effects-5daad8`, worktree `.claude/worktrees/shot-effects-5daad8`, base `claude/playtest-base` = `93b87c8` (main + PR #16). Owned paths: `Source/ValStrike/Effects/**`, `Content/Effects/**`, `Tests/Effects/`, `Scripts/test_shot_effects.py`, this log. Everything else is read-only. Loopback ports 7901–7904 (moved from 7871–7874 at the coordinator's request; those belong to the Server codebase session).

**Completion condition:** when any player fires an accepted shot, every client (shooter and observers) sees one short tracer from the weapon toward where the shot went and one small impact mark where it hit (walls, floor, cover, players). A self-contained, opt-in rendered test (server + shooter + observer) proves exactly one tracer and one impact per accepted shot on both clients, no effects on the dedicated server, no replay on join or respawn, the pool cap and fading marks, with inspected shooter/observer screenshots in neutral and dim light. The movement, combat and match suites still pass with the subsystem present. Committed, pushed, and a ready-for-review PR to `main`.

### Design

- `UVSShotEffectsSubsystem` (`Effects/VSShotEffectsSubsystem.*`) is a `UTickableWorldSubsystem` for Game/PIE worlds. `ShouldCreateSubsystem` returns false when `IsRunningDedicatedServer()`; a world whose net mode is `NM_DedicatedServer` (for example multi-process PIE) disables itself at begin play. Listen-server hosts and standalone players get effects.
- It edits no shared files and adds no RPCs, server code or build dependencies. Each tick it iterates `AVSCharacter`s and reads only public read-only state: `GetRifleComponent()->GetState().ShotsFired`, `GetFireInterval()`, `GetPawnViewLocation()`, `GetBaseAimRotation()`, `IsLocallyControlled()` and component visibility/sockets.
- **No replay:** each weapon component gets its own counter cache (`Effects/VSShotEffectsState.h`, engine-free). First sight adopts the current counter, so join, rejoin, respawn (new pawn and component) and relevancy return never replay old shots. A decrease (development reset, wrap) adopts quietly. A jump larger than the time since the last change allows (fire interval × 0.8, plus 3 shots of jitter slack) is treated as stale history and adopted quietly too. This guards against a default-valued first snapshot followed by the real counter.
- **Coalescing:** a replicated batch of N accepted shots becomes one tracer and one impact at the latest known view. The aims of older shots in the batch are unknown on that client. `RepresentedShots`/`CoalescedShots` keep the accounting exact.
- **Impact point:** a cosmetic `LineTraceSingleByChannel` that mirrors the server's rifle trace: origin `GetPawnViewLocation()`, direction `GetBaseAimRotation()` (local: camera view; remote: replicated yaw plus 16-bit `RemoteViewPitch16`), weapon range, `ECC_Visibility`, shooter ignored, simple collision. `Range` is private and not replicated on `UVSRifleComponent`, so the subsystem reads the component's reflected `Range` property and applies the server's clamp (fallback 10000 cm). **Limitation:** for moving targets, or a shooter turning between the shot and its replication, the cosmetic point can differ from the server's authoritative hit. Effects follow the replicated accepted-shot counter, so for the shooter they appear about one round trip after the click.
- **Tracer origin:** the first visible mesh on the character with a `Muzzle` socket. The owner-visibility flags select the first-person mesh for the local view and the world mesh for observers (the R01 presentation's bodies, later a pistol). Read-only lookup. Without such a mesh: a camera-relative fallback (first person (40, 12, −14) cm; observers (40, 16, −28) cm from the eye along the aim).
- **Self hits:** when this client's own view pawn is the struck character, no mark is drawn (it would sit inside the camera), and the tracer stops 60 cm short.
- **Look:** tracer is the engine cylinder at hairline radius with an unlit additive material. Its world position offset widens it with view distance (radius 0.0025 × distance, clamped 0.12–8 cm), so it reads as a thin, about 2.5 px line at 1000 px width, near or far. It lives 0.075 s, with intensity fading to 0 and width to 0.55. The world impact mark is the engine plane with an unlit translucent disc: a dark core with a pale rim, so it reads on light and dark surfaces and in dim light. It starts hot orange-white for 0.18 s, holds for 2.5 s, then fades linearly over 1.5 s. Its diameter is 7–16 cm, slightly larger far from the viewer. A player hit is a hot disc attached to the struck hit region: hold 0.12 s, fade 0.3 s.
- **Pools:** 16 tracers and 32 marks, grown lazily on the first effect. They use free slots first, then recycle the oldest. All components belong to one transient, non-replicated `VSShotEffects` actor with collision disabled: NoCollision, no overlap events, no navigation influence, no shadows or decals. Per-effect parameters use custom primitive data, not material instances.

### Pistol handoff

Pistol shots plug in without new RPCs. The Weapons session's PR #20 (not on this branch's base) adds the needed read-only accessors on `AVSCharacter`: `GetAcceptedShotCount(EVSWeaponSlot)`, `GetWeapon(EVSWeaponSlot)` (a `UVSWeaponComponent`, the shared base of rifle and pistol), `GetEquippedWeaponSlot()` and `GetWeaponPresentation(slot)->GetMuzzleTransform(FTransform&)`. After both merge, in `UVSShotEffectsSubsystem::ObserveCharacters`:

1. Replace the rifle-only block with a loop over both slots. For each, key the watch by `GetWeapon(slot)`, call `Observe(GetAcceptedShotCount(slot), Now, GetWeapon(slot)->GetFireInterval())`, then `SpawnShotEffect(*Pawn, *GetWeapon(slot), ...)`.
2. Each weapon component keeps its own cache. Counters advance only while their weapon is equipped, so switching never replays the other weapon's shots.
3. Optionally, prefer `GetMuzzleTransform` over the socket search in `TracerOrigin`. The current visible-mesh lookup already works, because `SM_VS_Pistol01_Body` has a body-space `Muzzle` socket at (16.7, 0, 4.75) cm (`docs/assets/pistol01/MANIFEST.md`) and inactive weapons are hidden.
4. `WeaponRange` reads the reflected `Range` property and includes superclass properties, so it keeps working if `Range` moves to `UVSWeaponComponent`. A public `GetRange()` would be cleaner.

The rendered fixture attaches the R01 presentation only when the shooter has no mesh with a `Muzzle` socket. It expects whichever visible `Muzzle` mesh the view has, so it keeps working once PR #20's own weapon visuals are on the character. On this base the first five shots use the camera-relative fallback and the later ones the R01 muzzle.

### Precision handoff

Observers see a remote shooter's yaw through the replicated actor rotation. By default Unreal quantizes it to 8 bits per axis (1.40625° steps), so an observer's cosmetic impact can sit up to about 12 cm sideways at 10 m from the server's hit. The shooter's own client is exact. Recommended one-line change in `AVSCharacter`'s constructor, owned by Weapons/integration: `GetReplicatedMovement_Mutable().RotationQuantizationLevel = ERotatorQuantization::ShortComponents;` (0.0055° steps). It also makes remote turning smoother. The rendered test bounds observer impacts accordingly.

### Server contract handoff

The rendered test launches the dedicated server on the normal VS_Graybox practice mode (`ue.MAP`, no `?game=`). Its switches all use the `-VSShotEffects*` prefix, so SRV-03's `ValidateMapMode`/`-VSServer*` allowlist needs no change for it.

## 2026-11-04 — Implementation committed; compile and rendered run 4 pending (integration batch)

**Status:** in progress. The completion condition is not met: the latest commits are not compiled and the rendered test has not yet passed. Per the user's verification policy, relayed by integration on 2026-11-04, this stream stops building and testing; integration runs the compile and rendered run 4 once in its combined batch and returns failures here.

**Commits:**
- `636d832` adds the subsystem, the materials, the fixture and the tests.
- `50ec37c` adds the material warm-up and hardens the test.
- `40a7a83` makes the warm-up wait for a live view and makes the test clicks deterministic.
- `22b1d98` moves the fixture to practice mode on ports 7901–7904 and adds the handoffs.

No shared file, build file, config, other stream's file or central document is changed.

**Verified evidence (all Development Editor on the M3 Mac with UE 5.8.4 / Xcode 26.1.1; loopback; no artificial latency):**
- Engine-free: 68/68 reducer checks (`Tests/Effects/shot_effects_state_test.cpp`, Apple clang 17, `-Wall -Wextra -Werror -pedantic`). The 13/13 runner unit tests pass at `22b1d98`.
- Materials: `M_VSShotTracer` and `M_VSImpactMark` were created, compiled and saved by the Unreal Python commandlet, exit 0, with `-AllowCommandletRendering`. Report: `Saved/Tests/Effects/materials.json`. Both are unlit (tracer additive, mark translucent), use only custom-primitive-data parameters and no textures. The engine Cylinder and Plane have centered pivots (extent 50 cm). The only warnings were the expected not-found messages before creation and an API deprecation, since fixed with a fallback.
- Builds: full unity builds succeeded at `636d832` (14.2 s) and `50ec37c` (18.3 s). `40a7a83` and `22b1d98` have **not** been compiled.
- Rendered runs (`Saved/Tests/ShotEffects/<run>/result.json`). One null-RHI dedicated server and two rendered 1000×700 `-game` clients; runs 1–3 still used ports 7871 and the old test game mode.
  - `20261103T212632Z_c633b6` (`636d832`): failed with `players_not_ready`. The client windows received real desktop keyboard/mouse input: 30 unscripted shots and a reload. Fixed: the fixture now ignores OS input on its clients.
  - `20261103T214248Z_5069a2` (`636d832` plus uncommitted fixes): 63/71. All per-shot checks, the pool cap (32 active of 32, oldest recycled), fading, no replay on respawn and rejoin, and no server subsystem/actors/components passed. Failures:
    - A ~2.3 s first-draw stall shifted the client's engine clock by ~1.6 s, so later clicks went out late.
    - Observer impacts were 8–10 cm from the server point. This matches the 8-bit replicated yaw (precision handoff above).
  - `20261103T222819Z_b8690f` (`50ec37c`): 56/73. The new extrapolated clock jittered ~0.1 s, so one scripted click was skipped and one doubled. The first-effect stall (~1.6 s) still happened: the warm-up had run during loading frames.
- Screenshots inspected from runs 2–3:
  - The tracer reads as a thin, warm-white line in first person and for the observer.
  - With R01 visuals it starts exactly at the muzzle in both views.
  - Impact marks are small pale rings with dark cores that stay readable in dim light.
  - The incoming tracer is visible to the struck observer, with no mark in their camera.
  - No effects exist on the dedicated server.

**Fixes not yet compiled (`40a7a83`, `22b1d98`):**
- The warm-up now waits for a live camera view target and draws 60 cm ahead for 8 frames.
- Clicks and reload are edge-triggered and released on the next tick.
- The client clock uses the minimum receipt offset over a 3 s window of replicated server-time samples, read by reflection, and is monotonic.
- A priming "start" screenshot is taken before any effect, and the stall is measured across frames.
- The fixture runs on the normal practice mode, started by a module hook. The tracer is expected from any visible `Muzzle` mesh.

**Next action:** integration compiles `22b1d98` and runs `test_shot_effects.py` plus the movement, combat and match suites in its batch, then reports any failures to this stream. A failing `first_effect_no_stall` would mean the warm-up does not remove the first-shot pipeline stall; that is a real gameplay defect to fix here, not a test-only issue.

## 2026-11-04 — GPT-08: tracers and impacts follow the recoil/spread direction (verification pending)

**Status:** implementation ready; compile and runtime verification pending (integration). Built on GPT-02 (PR #35, base `a64dfa5`). #35 is merged; the branch now includes `origin/main`, so this is a normal PR to main.

**Change (Effects only; Combat, Presentation, runners and Content untouched):**
- The cosmetic trace, and so the tracer end and impact mark, now uses a per-effect direction, recorded as `FVSShotEffectRecord::DirectionSource`. Base aim is still the default.
  - **Local shooter:** the subsystem notes `GetLastPredictedShot()` once per frame into a small per-weapon queue (`VSShotEffects::PredictedShots`). Each accepted-shot increment consumes entries oldest first, so a tracer pairs with the prediction for its own shot, even when several shots are in flight. If two shots are sent in one frame, the skipped id becomes an unknown entry, which keeps the order. A coalesced batch draws its latest shot. Entries expire after 1 s. A shot the server rejected therefore shifts the pairing by at most one burst. The queue holds at most 32 entries.
  - **Listen host, other pawns:** uses `GetLastServerShot()` when its shot id is new since the last effect for that weapon.
  - **Remote clients observing others, and any shot with no known direction:** base aim, as before. Runner/`DevelopmentRequest` shots never set a prediction, so they also use base aim.
- `ChooseDirection` takes the prediction first for the local shooter, then a new server shot, then base aim. The same accepted-shot counter still produces exactly one effect per observation. Once-only, coalescing, pool cap and no-replay are unchanged.
- New stats: `PredictedDirections`, `ServerDirections`, `BaseAimDirections`.

**Checks actually run:**
- `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Effects/shot_effects_state_test.cpp`: 95/95, up from 68. New cases: direction choice; in-order pairing at high latency; coalesced batches; no prediction; same-frame gap; expiry of a rejected shot; id jump; capacity; wrap-around.
- `git diff --check` clean.

**Deferred to integration:**
- Combined build on current main (verification pending).
- `Scripts/test_shot_effects.py`: baseline 73/73. Its shots are first-shot-accurate, so the predicted direction equals aim at send. No expectation change is expected.
- Sustained-fire visual check: the local tracers should fan out with the spread pattern. Observers on a dedicated server still see base-aim tracers.

**Remaining limit / follow-up for the lead:** a remote observer cannot show spread without a replicated shot direction or impact. None exists today, and none was added, per the brief. Showing spread there would need a Combat-owned replicated field, for example the last shot's seed index or its impact point.

**Next action:** gameplay lead reviews; integration builds and runs the shot-effects suite.
