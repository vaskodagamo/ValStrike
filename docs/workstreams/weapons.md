# Weapons workstream: visible rifle and rifle/pistol switching

Append-only. Dates and times are Europe/Berlin. Branch `claude/weapons-switching-e7e841`, base `claude/playtest-base` = `93b87c8` (main + PR #16 unity-build fix). Machine: Apple M3, 16 GB, macOS 15.6.1, Xcode 26.1.1, Unreal 5.8.4 (CL 58902052) at `/Users/lukasmac/Desktop/EPIC GAMES/UE_5.8`. Every Unreal build/run went through `Scripts/with_ue_slot.py`.

## 2026-11-03 — Scope, ownership and design

**Requested scope.** (1) Attach the existing R01 rifle presentation to the player so matches show a gun, via the "Requested coordinator seam" in `rifle-presentation.md`; separate commit. (2) Rifle ↔ pistol switching: fixed loadout, keys 1/2/Q, server-authoritative equip with validation, equip time blocking fire, per-weapon ammo, semi-automatic lower-damage pistol, P01 art in first person and for observers, HUD name/ammo, both weapons restored on round reset.

**Scope expansion recorded.** A second weapon was deferred as NEXT-03 ("Second weapon, economy and tactical utility", deferred until playtest feedback). The user explicitly requested rifle/pistol switching now. Only the fixed two-weapon loadout is implemented; economy, buying, drops, utility, recoil/spread differences, audio and arms remain out of scope. The integration session owns BACKLOG/WORK_LOG updates for this.

**Ownership respected.** Edited only: `VSCharacter.h/.cpp` (not the `LookYaw`/`LookPitch` bodies or `MouseSensitivity`), `Combat/**`, `Presentation/**`, `VSHUD.cpp`, `VSHUDData.h` (additive fields), `Scripts/test_rifle_presentation.py`, new `Scripts/test_weapons.py`, `Tests/Presentation/`, new `Tests/Weapons/` and this log. No `Match/`, `UI/`, `VSPlayerController`, `ValStrike.Build.cs`, config, map or `.uasset`/`.umap` change. No new dependency or asset. P01/R01 art and the existing flash material are referenced read-only.

**Design.**
- `Combat/VSWeaponComponent` holds the previous rifle logic unchanged in behavior (sequence IDs, cadence tolerance, origin/direction/NaN validation, live match gate, current-position trace). `UVSRifleComponent` (same tuning: automatic, 30/90, 0.12 s, 2 s reload, 30/100) and the new `UVSPistolComponent` only set defaults. Each weapon has its own replicated `FVSRifleState`, shot sequence and cadence. Stable APIs kept: `GetRifleComponent()`, `UVSRifleComponent::GetState()` incl. `ShotsFired`, all existing `GetHUDData()` fields.
- Pistol proposal (all `EditDefaultsOnly`): semi-automatic (one request per press; server still enforces cadence), magazine 12, reserve 36, 0.2 s minimum interval, 1.6 s reload, body 20 / head 60, range 80 m.
- `Combat/VSWeaponLoadoutComponent` replicates `{Equipped, Previous, bEquipping, EquipCount}`. Local input sends one reliable `ServerRequestEquip(uint8)`; the server validates possession/ownership, the raw byte, alive, the live combat gate (`AVSMatchGameMode::AllowsCombat` + `CanMatchCombat()`), duplicate (already equipped) and a 0.1 s switch rate limit (`MinSwitchInterval`). Q ("last") is resolved from server state. An accepted switch holsters the outgoing weapon (stops fire, cancels an authoritative reload without ammo transfer) and blocks fire and reload of the new weapon until `EquipDuration` (0.5 s) has elapsed on the server clock; weapons reject such requests as `holstered`/`equip`. The decision order lives in engine-free `Combat/VSWeaponLoadoutRules.h`.
- Round resets: `Match/` already destroys and respawns pawns (`ResetPawns`), and a fresh character starts full with the rifle ready, so no `Match/` hook is needed.
- `Presentation/VSWeaponPresentationComponent` generalizes the R01 component (same reducer, visibility, NoCollision/no-navigation, no-replay and death rules). `UVSRiflePresentationComponent` keeps its class/name/assets/poses; `UVSPistolPresentationComponent` uses P01 Body/Slide/Magazine on their authored sockets, reuses the original flash material and kicks the slide per accepted shot. A view is available only while its slot is equipped, so a stowed weapon is hidden and re-equipping never replays shots. During equip the view rises from a lowered pose (presentation only).
- HUD: ammo panel title shows the equipped weapon name (muted with "EQUIPPING" during equip time); the help line lists `R RELOAD  1 RIFLE  2 PISTOL  Q LAST WEAPON`.

## 2026-11-03 — Deliverable 1: rifle visible in real play (commit `1c16262`)

**Change.** `AVSCharacter` constructs `UVSRiflePresentationComponent` as default subobject `RiflePresentation` (UPROPERTY) and, after `Super::BeginPlay()`, calls `Initialize(this, FirstPersonCamera, GetMesh())` on every non-dedicated instance, logging an error on failure (no substitute visual). The presentation fixture now asserts this production wiring: the dedicated server's default subobject stays uninitialized with no visual components, and clients use the character's own component instead of attaching one. `Scripts/test_rifle_presentation.py --map site01` runs the same contract on Site01 under its authored lighting (no fixture light override; the "dim" capture is then simply a second authored-light capture). It also accepts this stream's ports 7891–7894.

**Fixture robustness (test code only).** On this 16 GB M3 the unchanged fixture failed repeatedly for reasons unrelated to the visuals, and each cause was confirmed in logs:
- UE 5.8's `GetServerWorldTimeSeconds()` on clients averages up to 250 offset samples, so connection-time hitches left a client's scheduled inputs about 1 s late relative to the server for the whole run (server checks at 4.2/7.7 s saw stale state). New `Combat/VSTestServerClock.h` uses the best replicated-time sample in a 3 s window; it logs `VSCLOCK` when its offset jumps. Example from the final graybox run: after a client hitch the true offset grew 10.50 → 11.64 s while the engine estimate stayed 10.44 s.
- A 0.05 s key window could span a long frame, so the automatic timer added a second shot. Single shots are now one-frame taps: the release is processed in the next frame's input pass, before the timer can tick.
- Rendered test windows received real OS input (unscripted observer shots that damaged the owner, the observer walking and turning, an owner burst of 13 shots before T0). Scripted clients now call `UGameViewportClient::SetIgnoreInput(true)` and `SetIgnoreLookInput(true)`; scripted keys still go straight to `PlayerController::InputKey`.

**Verification** (Editor build of a clean detached checkout of the final D1 commit; full unity blob, UE 5.8.4 CL 58902052, Apple clang 17.0.0, Mac SDK 26.1; one null-RHI Editor server plus two rendered 1000×700 Editor clients, loopback, no audio, no artificial latency/loss):

| Run | Result | Evidence |
| --- | --- | --- |
| Rifle presentation, graybox | **39/39**, 11 PNGs, clean checkout `1c16262` | `Saved/Tests/RiflePresentation/20261103T212823Z_graybox_079620/result.json` |
| Rifle presentation, Site01 (authored lighting) | **39/39**, 11 PNGs, clean checkout `1c16262` | `Saved/Tests/RiflePresentation/20261103T212945Z_site01_8f7c68/result.json` |

These were recorded in the temporary verification checkout (scratchpad `d1verify`) because this worktree was already on deliverable 2. Earlier failed attempts are kept as evidence of the fixture defects above: graybox `20261103T210232Z…` 36/39, `…210851Z…` 31/39, `…211601Z…` 37/39 (aim changed by OS mouse), `…212128Z…` 29/39 (13 shots fired by a real click before T0); Site01 `…210541Z…` (cold first launch with a 17 s stall), `…211018Z…` 26/39. Site01 `…212421Z…` passed 39/39 at the intermediate revision `db9f6cb`, before the viewport-input isolation was added; its screenshots still show unscripted observer input.

**Screenshots inspected.** On Site01 the owner's first-person R01 is large, sharp and readable against the beige walls and blue sky. The other player's world R01 is visible on the placeholder body in the owner's view. The observer sees the owner's world rifle and the amber muzzle flash. The reload pose lowers and tilts the rifle with the magazine withdrawn, and death removes the weapon. **Site01 lighting:** under the authored daylight the dark R01 surfaces are clearly separated from walls, floor and sky. The dim-fixture readability problem from `rifle-presentation.md` did not reproduce at these positions; dark teal kit pieces behind the weapon give the lowest contrast but stay readable. Shadowed corners and other angles were not surveyed. **Map issue (not this stream's):** every Site01 client frame shows UE's on-screen warning "Multiple directional lights are competing to be the single one used for forward shading…". The map has more than one directional light without `ForwardShadingPriority`; the map owner should resolve it.

## 2026-11-03 — Deliverable 2: rifle ↔ pistol switching (commit `a1d682d`)

**Implemented** as described in the design entry above. New engine-free `Combat/VSWeaponLoadoutRules.h` decides the server verdict in a fixed order: malformed byte, dead owner, closed combat gate, already equipped, rate limit. It also holds the equip-readiness rule. Development harness `Combat/VSWeaponSmoke.cpp` (opt-in `-VSWeaponSmoke=`, excluded from Shipping/Test) and runner `Scripts/test_weapons.py` (ports 7891–7894) have three scenarios:
- `practice`: graybox, null-RHI clients, dense validation timeline.
- `visual`: graybox, rendered clients, spaced captures.
- `match`: Site01 under `VSMatchGameMode -VSMatchMinimum=1`, rendered clients.

`visual` and `match` use deadline-based checks (`Eventually`) and seconds of slack. Rendered clients stalled up to ~1.7 s around screenshot captures, which broke an earlier dense rendered schedule. That is why the dense validation timeline runs only with null-RHI clients.

**Engine-free checks:** `Tests/Weapons/weapon_loadout_rules_test.cpp` **20/20** (Apple clang 17, `-Wall -Wextra -Werror -pedantic`; also with ASan/UBSan). `Tests/Weapons` runner contract **9/9**, `Tests/Presentation` **12/12**, `Tests/Combat`, `Tests/test_ue*.py` OK. Rifle presentation reducer **47/47**. `git diff --check` clean.

**Network/runtime evidence** (Editor build of `a1d682d`, clean checkout, full unity blob; one null-RHI Editor server and two Editor clients, loopback, no audio, no artificial latency/loss):

| Scenario | Result | Evidence |
| --- | --- | --- |
| practice (null-RHI clients) | **74/74** | `Saved/Tests/Weapons/20261103T214030Z_practice_b08374/result.json` |
| visual (rendered 1000×700) | **13/13**, 12 PNGs | `Saved/Tests/Weapons/20261103T214439Z_visual_f001c4/result.json` |
| match on Site01 (rendered) | **29/29**, 8 PNGs | `Saved/Tests/Weapons/20261103T214603Z_match_797e2e/result.json` |

Covered by those checks, among others:
- Equip validation: dead owner (`reject_dead`); closed combat gate in practice (`reject_phase`, gate closed via `SetMatchGates`) and match preparation (`preparation_blocks_equip`); duplicate; same-frame spam; malformed byte 7. All were rejected on the server with no state change, and the client sends nothing for gated/dead/duplicate presses.
- Equip time: raw pistol request during equip rejected `equip`, raw rifle request after switching away rejected `holstered`, client fire held during the rifle equip sent nothing.
- Mid-reload switch cancels the pistol reload without transfer (9/36 kept).
- Separate ammo per weapon (rifle 29/90 and pistol 9/36 at once).
- Semi-automatic pistol: a 0.5 s hold gives one shot. Damage: pistol body 20 (100→80→40), rifle body 30 (→10), rifle head 100 eliminates in match.
- Q returns to the last weapon without replaying earlier shots.
- HUD name/ammo/EQUIPPING.
- Correct first-person/world models for owner and observer; stowed weapon hidden; NoCollision and no navigation on every weapon part; dedicated server has no weapon visuals.
- Respawned pawn full; and a real `Match/` round reset (fresh pawns from `ResetPawns`) restoring rifle 30/90 + pistol 12/36 with the rifle ready, on server and both clients.

Failed attempts kept as evidence:
- `practice_rendered_1d5b50` (old dense rendered schedule): a client stall after a capture merged the `2` and `LMB` taps.
- `match_rendered_e977e0` (old match schedule): same stall cause, pistol leg failed. Preparation gate, elimination and round reset passed even there.
- `practice_244d8a`: 74/74 at the previous commit `797188d`, marked dirty only by the untracked log.

**Screenshots inspected.**
- Visual run: first-person P01 is framed close to the art-direction composition, lower right, slightly overlapping the ammo panel. The equip start shows the weapon lowered out of view with "PISTOL EQUIPPING". The pistol flash and kick show in first person, and the observer sees the driver's world pistol and its flash. The reload pose lowers the pistol and drops the magazine. Switching back shows the rifle.
- Match run: the same holds on Site01 next to the live match HUD. The observer on Site01 sees the driver holding the pistol. Round 2 preparation shows a fresh pawn with the rifle and full ammo. The Site01 directional-light warning is still visible in every frame.

## 2026-11-03 — Regression suites at the final commits

All at clean checkouts, Editor build of the stated commit, loopback, no audio, no artificial latency/loss. Ports: weapons runners 7891–7894; other runners at their own validated ports (match 7831).

| Suite | Commit | Result | Evidence |
| --- | --- | --- | --- |
| Movement (server + 2 null-RHI clients) | `a1d682d` | **45/45** | `Saved/Tests/Movement/20261103T214959Z_e04fb9/result.json` |
| Combat (server + 2 null-RHI clients) | `a1d682d` | **55/55** | `Saved/Tests/Combat/20261103T215112Z_78d358/result.json` |
| Combat look (standalone rendered) | `a1d682d` | **8/8** | `Saved/Tests/CombatLook/20261103T215237Z_bfac59/result.json` |
| Match (server + clients) | `a1d682d` | **43/43** | `Saved/Tests/Match/20261103T215501Z_80e667/result.json` |
| Rifle presentation, graybox (refactored component) | `a1d682d` | **39/39**, 11 PNGs | `Saved/Tests/RiflePresentation/20261103T214806Z_graybox_16002d/result.json` |
| UI session (`Tests/UI/test_session.py`, null RHI) | `a1d682d` | 24/32 (twice) | `Saved/Tests/UI/20261103T215325Z_bc0540`, `…215827Z_60a82c` |
| UI session, **untouched base** | `93b87c8` | 24/32, the same 8 failures | scratch checkout `Saved/Tests/UI/20261103T220024Z_ba7a2d` |

**UI suite: pre-existing failure, not caused by this branch.** The same eight checks fail on the base without any weapons change. The first is `address_text_entry_updates_session`: Slate character events after Tab×2 do not reach the address field. The rest are consequences: no connection, deadline, incomplete HUD snapshots. All menu/input/movement checks before it pass. Text entry uses `FSlateApplication::ProcessKeyCharEvent`, not Enhanced Input, so the new 1/2/Q bindings are not involved. The Settings session owns `UI/` and has a passing UI run on its own branch (`b18f038`). No UI file was changed here.

## Limitations and next actions

- Development Editor processes on one Mac only. No packaged client, packaged dedicated server, LAN/second machine, Windows/Linux, latency/loss or performance acceptance. No human playtest yet.
- The pistol has no tracer, recoil, spread or audio. Tracers belong to the Shot effects session (accessors in the hand-over note `weapons-switching-handoff.md`).
- Switching is not predicted. The request and the visual switch wait for replication, so on a real network the equip feels RTT/2 later, and a click within one RTT of pressing 2 can still send a request for the old weapon, which the server rejects as `holstered`.
- Server-accepted shot feedback includes replication delay, unchanged from the GUN-03 visual foundation.
- The first-person pistol pose matches the art direction's composition and overlaps the ammo panel slightly. The world pistol and rifle float in front of the placeholder body (no arms rig). Both need tuning with real arms.
- Rendered clients on this 16 GB M3 stall up to ~1.7 s around screenshot captures and first draws. Time-scripted rendered fixtures must use slack or state-triggered steps. The test-only `VSTestServerClock` and one-frame taps make the presentation fixture robust here.
- Real OS input reaching test windows (user clicks/moves) contaminated several early rendered runs. Scripted clients now ignore viewport input. Other sessions' rendered runners (UI, combat look) do not.
- Pistol values are proposals for playtest tuning.
- NEXT-03 remains Deferred for economy/utility. Integration should record this partial scope (fixed rifle + pistol loadout).
- Add `Tests/Weapons` (runner contract + `weapon_loadout_rules_test.cpp`) to the engine-free CI workflow. That is integration-owned.

**Next action:** integration review of the PR. Shot effects adds pistol tracers through `GetAcceptedShotCount(EVSWeaponSlot::Pistol)` / `GetMuzzleTransform`. Then a human 2v2 Site01 session to tune pistol values, equip time and first-person poses.
