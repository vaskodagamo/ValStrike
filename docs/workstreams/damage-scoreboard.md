# Damage zones, kill stats and scoreboard workstream

Append-only log for the damage-scoreboard stream. Dates are Europe/Berlin.
Branch `claude/damage-scoreboard-b38eeb`, based on `claude/playtest-base` (`93b87c8`).

## 2026-11-03 Phase 1: hidden hit zones, damage model, stats and Tab scoreboard

### Request and scope

The user asked for a hidden "skeleton" of hit zones on every player: a headshot kills instantly, feet take less damage, arms and hands take their own damage, like a real tactical shooter, with ValStrike's own values. Phase 1 lives entirely in new folders because the Weapons stream currently owns `VSCharacter`, `Combat/`, `Presentation/` and `VSHUD`. Phase 1 does **not** change live gameplay: the rifle still uses the legacy body/head regions until Phase 2.

Owned paths: `Source/ValStrike/Damage/**`, `Source/ValStrike/Scoreboard/**`, `Tests/Damage/`, `Tests/Scoreboard/`, `Scripts/test_damage.py`, this log. No other file was edited.

### Design

**Damage model** (`Damage/VSDamageModel.*`, plain C++17, no Unreal). `Compute(table, {weapon, zone, distance})` returns whole hit points. It is a pure function: it reads no health, time or client input, and it sanitizes the profile it reads.

- Damage = base damage per weapon class × zone multiplier × distance falloff scale.
- The result is rounded to the nearest whole point. Any positive hit removes at least 1 point, and a hit is capped at 1000.
- A lethal head rule per weapon returns `LethalDamage` (10000). The existing server health clamp then ends the life. The rule ignores falloff and never fires for a weapon with 0 base damage.
- Clamps: base damage 0..500, multipliers 0..10, falloff scale 0..1, and distances 0..10 km. Non-finite values fall back to safe defaults. An inverted falloff range collapses to a step at its start distance. Unknown zones and weapons return 0 with a status.

**Proposed values, one table** (`VSDamageModelBuildDefault`). These are ValStrike proposals for playtest tuning, not taken from another game. The rifle keeps today's 30 body damage, so its upper-torso time-to-kill is unchanged. The pistol base matches the Weapons stream's pistol body damage (20). That stream's pistol head damage is 60, but the brief asks for a lethal head on both weapons, so Phase 2 makes pistol headshots lethal; integration should confirm that choice.

| Zone | Multiplier | Rifle (base 30) | Pistol (base 20) | Rifle hits to kill 100 HP |
| --- | --- | --- | --- | --- |
| Head | lethal (4.0 if the rule is off) | kill | kill | 1 |
| Upper torso (chest, shoulders) | 1.00 | 30 | 20 | 4 |
| Lower torso (stomach, pelvis) | 0.90 | 27 | 18 | 4 |
| Upper arm | 0.80 | 24 | 16 | 5 |
| Forearm / hand | 0.70 | 21 | 14 | 5 |
| Thigh | 0.80 | 24 | 16 | 5 |
| Lower leg | 0.70 | 21 | 14 | 5 |
| Foot | 0.55 | 17 | 11 | 6 |

Distance falloff is defined but **off** for both weapons. Rifle: full damage to 25 m, then linear down to 0.85× at 50 m and beyond. Pistol: full to 15 m, then down to 0.75× at 35 m.

**Hit-zone geometry** (`Damage/VSHitZoneLayout.*`, plain C++). It produces 13 capsule-relative volumes: one head sphere, the upper-torso and lower-torso cores, and left/right upper arms, forearms, thighs, lower legs and feet.

- Their union is exactly today's prototype silhouette. That is the 50 × 40 body box from the floor to 32 cm below the capsule top, matching `BodyHitRegion` and the cube visual, plus the 16 cm head sphere at the top, matching `HeadHitRegion` and the sphere visual. A shot that hits the visible prototype therefore hits a zone, and vice versa.
- Body bands are fractions of body height: feet 10, shins 46, thighs 84, stomach 112 and chest 144 cm at standing height (144 cm body). Crouching to a 56 cm half-height scales the bands to the 80 cm crouched body, as the prototype visuals do, while the head stays a 16 cm sphere at the top.
- Arms are the outer 7 cm strips beside the torso core at chest height (upper arms) and stomach height (forearms/hands). Legs split the box left/right. The boxes keep the prototype's full 50 cm depth so there are no gaps; a real skeletal character will replace these proportions.
- Each volume names a proposed skeleton bone (`head`, `spine_03`, `spine_01`, `upperarm_l/r`, `lowerarm_l/r`, `thigh_l/r`, `calf_l/r`, `foot_l/r`). Binding a shape to a bone later only changes its attach parent and offset, not the zone lookup.

**`UVSHitZoneSet`** (`Damage/VSHitZoneSet.*`, `USceneComponent`). It creates the 13 shapes (`UBoxComponent` / `USphereComponent`) under the owner's capsule.

- Collision: QueryOnly, with every channel ignored except the rifle's trace channel (`ECC_Visibility`). There is no physics, no overlap events and no navigation, and the shapes are hidden and not replicated.
- Trace modes:
  - **Shadow** (Phase 1 default): *Overlap* on Visibility. The rifle's `LineTraceSingleByChannel` ignores overlaps, so live gameplay is unchanged, while multi-hit traces report the zones for verification.
  - **Authoritative** (Phase 2): *Block*.
- The shapes follow the current capsule half-height. `RefreshLayout()` is public for an explicit call on crouch, and the tick also catches any change. They are disabled while the owner's `UVSHealthComponent` reports death.
- `ResolveZone(UPrimitiveComponent*)` maps a hit component to its zone.
- Debug view (development builds only): `vs.Damage.DrawHitZones 1` draws every *other* player's zones as depth-tested, colour-coded lines 0.5 cm outside each shape, so they do not z-fight with the prototype meshes.

**`UVSDamageSubsystem`** (world subsystem, Game and PIE worlds). It attaches a zone set (Shadow) to every `ACharacter` on spawn on every machine, skipping characters that already have one. It also provides the hook (below).

**Stats** (`Scoreboard/`):

- `VSStatsLedger` is plain C++. It counts kills, deaths, assists, headshot kills and damage dealt to enemies.
  - At most one death and one kill credit per life.
  - A teammate kill, a self kill or a kill with no attacker counts the death without credit. Team 0 (practice) is nobody's teammate.
  - An assist goes to an enemy other than the killer who removed at least 40 health from that life (a proposal).
  - A new life clears the damage contributions. A new match zeroes the counters but keeps the roster. Scoreboard order is a total order.
- `UVSMatchStatsSubsystem` runs on the server only:
  - It tracks players from `FGameModeEvents` post-login/logout.
  - Deaths come from each `AVSCharacter`'s public `UVSHealthComponent::OnLifeStateChanged`, so *every* death counts, attributed or not.
  - Kills, headshot kills, assists and damage come from the hook.
  - A rematch (leaving `MatchComplete`) or a match cleared by the rules (round count drops) resets the counters.
  - It reads teams from `AVSMatchPlayerState::TeamId` and the phase/round count from `AVSMatchGameState::Match`, both read-only. It spawns and fills one replicated, always-relevant `AVSScoreboardState`.
- The scoreboard is native Slate (`SVSScoreboard`). `UVSScoreboardSubsystem` adds it per local player at viewport Z-order 40, below the session menu at 100.
  - It is shown while `PlayerController::IsInputKeyDown(Tab)` is true and hidden while `AVSPlayerController::IsSessionMenuOpen()`. It binds no keys and is never focusable (`HitTestInvisible`). The game viewport stops Tab navigation, so Tab never moves Slate focus.
  - Columns: both teams with the attacking/defending role and round score from the replicated match state, then player, K, D, A, HS, DMG, ping and alive/dead/spectating, with the local row highlighted.
  - It uses the session menu's DPI rule, so it is readable at 960 × 540.

### Hook for Phase 2 and the Economy stream

- Header: `Source/ValStrike/Damage/VSDamageSubsystem.h` (`#include "Damage/VSDamageSubsystem.h"` from module code).
- Get the service with `UVSDamageSubsystem::Get(WorldContext)`.
- **Apply a weapon hit (server):** `float ApplyWeaponHit(const FVSWeaponHit&)`. `FVSWeaponHit` holds the shooter pawn, the victim pawn, the hit component (`FHitResult::GetComponent()`), a `ValStrike::Damage::WeaponClass` (Rifle or Pistol) and the distance in cm.
  - It resolves the zone from the victim's zone set and rejects components that are not the victim's zones.
  - It computes damage with the table and applies it through the victim's `UVSHealthComponent::ApplyPointDamage`, which keeps the match damage gate.
  - It returns the health removed and broadcasts the events.
- **Events (server only):**
  - `OnDamage()` fires for every attributed hit with applied damage.
  - `OnKill()` fires only for hits that took the victim from alive to dead, after `OnDamage`.
  - Both carry an `FVSDamageEvent`: instigator controller and pawn, victim controller and pawn, zone, weapon, distance, requested and applied damage, `bLethalRule`, `bKilled`, and `IsHeadshot()`. The pointers are valid during the broadcast only.
- **Other damage sources:** `PublishDamage(const FVSDamageEvent&)` reports damage applied elsewhere, so stats and economy still see it.
- **Economy:** bind `UVSDamageSubsystem::Get(World)->OnKill().AddUObject(...)` on the server for kill rewards. In Phase 1 nothing in live gameplay broadcasts yet, because the rifle does not call the hook. Real kills arrive after Phase 2.

### Phase 2 integration plan (after the Weapons PR merges)

1. **`VSCharacter`:**
   - Remove `BodyHitRegion`/`HeadHitRegion`. Add `UPROPERTY(VisibleAnywhere) TObjectPtr<UVSHitZoneSet> HitZones`, created in the constructor, attached to the capsule and set to `EVSHitZoneTraceMode::Authoritative` there. The shapes are created at `BeginPlay` with that mode. The subsystem then skips auto-attach for this class.
   - Call `HitZones->RefreshLayout()` from `UpdatePrototypeVisuals()`, which runs on crouch and uncrouch, so a shot in the same frame as a crouch already sees the new layout.
   - Drop the region lines from `HandleHealthChanged()`; the zone set handles death itself.
   - Replace `IsHeadHit()` with `HitZones->ResolveZone(C) == HitZone::Head`, or remove it.
2. **Weapon fire path:** in the Weapons stream's branch (`a1d682d`), the single damage call is in `UVSWeaponComponent::ServerRequestFire` (`Combat/VSWeaponComponent.cpp`), shared by the rifle and pistol subclasses.
   - Replace `ApplyPointDamage(Target->IsHeadHit(...) ? HeadDamage : BodyDamage)` with `UVSDamageSubsystem::Get(this)->ApplyWeaponHit({Pawn, Target, Hit.GetComponent(), DamageClass, static_cast<float>(Hit.Distance)})`.
   - `DamageClass` is a `ValStrike::Damage::WeaponClass` set by each subclass: Rifle or Pistol.
   - Retire the per-weapon `BodyDamage`/`HeadDamage` properties, since the table is the single source.
3. **Combat smoke expectations:** body shots now depend on the zone. A shot at the capsule center lands in the lower torso (27, not 30). Aim body probes at the chest band (capsule-relative z ≈ +40 standing) or assert the zone-specific value.
4. **Stats and scoreboard:** no change needed. Real kills reach the ledger and the scoreboard through `OnDamage`.
5. **Verify:**
   - Rerun the combat, match and movement suites, and `Tests/Damage/run_runtime.py`.
   - Its `rifle_path_unchanged` check is Phase 1 specific and should become "rifle hits resolve to zones" with real fire RPCs.
   - Delete or adapt the Shadow-mode checks.

### Verification

**Machine and toolchain:** Apple M3, 16 GB, macOS 15.6.1, Xcode 26.1.1, Unreal 5.8.4 (`++UE5+Release-5.8`). Every Unreal build and run went through `Scripts/with_ue_slot.py`. All runtime evidence is development-editor loopback evidence at clean commit `61c5eb9`, Editor module SHA256 `7f2e1911c21972573f0eaffdb3b6953f8caca4dc9b5e86344f14185f4cc01e1f`.

**Build:** the first build of this checkout compiled the full unity blob (`Module.ValStrike.cpp`, every module file in one translation unit) from committed sources, in 36.7 s with 0 warnings. Later builds were incremental.

**Standalone suites:** `python3 Scripts/test_damage.py`, with and without `--sanitize` (ASan/UBSan), using Apple clang 17 with `-std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Werror`.

- Damage model and layout: 14/14 cases, 1091 checks.
- Stats ledger: 11/11 cases, 112 checks.
- The sources also compile cleanly with `-Wshadow-all -Wconversion`.
- A deliberate foot-multiplier mutation (0.55 → 0.60) failed 3 checks, which shows the table is actually tested.
- Evidence: `Saved/Tests/Damage/20261103T221244Z_a569c1/result.json` (normal) and `Saved/Tests/Damage/20261103T221246Z_2f6a56/result.json` (sanitized).

**Runner contract:** `python3 -m unittest discover -s Tests/Damage -p 'test_*.py'` ran 12 tests, all OK, without launching Unreal.

**Runtime smoke** (`python3 Scripts/with_ue_slot.py -- python3 Tests/Damage/run_runtime.py --engine …`):

- **Setup:** Site01 in `VSMatchGameMode`, with a dedicated `-server` (null RHI) on port 7921. Two `-game` clients (driver and observer) render at 960 × 540 over loopback, with no audio and no artificial latency or loss.
- **Result:** **44/44 checks passed** in 54 s. Evidence: `Saved/Tests/DamageRuntime/20261103T220224Z_8ffb44/result.json`, server/client logs and `Screenshots/`.
- **Server checks:**
  - Every pawn has exactly one zone set: 13 shapes in Shadow mode on Visibility, QueryOnly, every other channel ignored, no physics, overlap events or navigation, hidden and not replicated.
  - Standing layout: half-height 88, head z 72 r 16, chest top 56, feet at −88.
  - A real client crouch (Left Ctrl through the driver's input) gave the crouched layout: half-height 56, head z 40, chest top 24.
  - After standing up, the layout returned to 88.
  - 19 server traces (13 front, 4 side-arm, from above the head, from behind the chest) resolved to the expected zones, covering all 8 zone types. This held crouched, in Shadow mode with a multi-hit trace, and in Authoritative mode with a single-hit trace.
  - The live rifle's single-hit trace still hits `HeadHitRegion`/`BodyHitRegion`, not the zones.
  - Through the hook, at 300 cm:
    - Thigh hit: 24 damage, health 76.
    - Head hit: lethal; 100 applied and the victim died.
    - One `OnKill` was broadcast, with the correct killer.
  - These all applied 0 without events: a dead victim, another pawn's shape, an invalid weapon class, the victim's legacy `BodyHitRegion`, a null component, and any hit during preparation (the match damage gate).
  - The dead victim's zones switched to NoCollision.
  - Stats after round 1: the killer had K1 D0 HS1 DMG100 and the victim K0 D1 DMG24. The round was won by elimination.
  - Round-2 fresh pawns had rebuilt zones and both rows showed alive within the same frame (published on possession).
  - In round 2, a raw `ApplyPointDamage` death with no shooter counted D+1 without any kill credit. The score was 1:1.
- **Client checks** (both clients):
  - The autonomous and simulated pawns have zones. The simulated capsule is 87.99 because of CharacterMovement's 0.01 cm proxy shrink.
  - The stats actor replicates (not authority on the client) with 2 rows.
  - Crouched zones were seen on the driver's own pawn and on the observer's simulated driver.
  - The replicated kill was seen.
  - The board is hidden by default. Tab shows it as HitTestInvisible and non-focusable, with keyboard focus unchanged (`SViewport`); releasing Tab hides it.
  - With the menu open, Tab plus a forced show leaves the board hidden and menu focus on its `SButton`.
  - The final replicated stats matched, and the board's round score matched `AVSMatchGameState` at 1:1.
  - The observer's debug draw drew the other player's zones and not its own.
- **Screenshots inspected:**
  - `observer_zones_standing.png` and `observer_zones_crouched.png` show colour-coded zones on the prototype.
  - `driver_scoreboard.png` and `observer_scoreboard.png` show both teams with K/D/A/HS/DMG, ping and alive/dead. They are readable at 960 × 540, with the local row highlighted and the round HUD strip still visible.
  - `*_scoreboard_final.png` shows the 1:1 state.

**Existing suites with these components present** (same commit and module; movement on port 7922, combat on 7923; the match runner only accepts 7831–7834):

| Suite | Result | Evidence |
| --- | --- | --- |
| Movement | 45/45 | `Saved/Tests/Movement/20261103T220749Z_3024db/result.json` |
| Combat | 55/55 | `Saved/Tests/Combat/20261103T220902Z_37b827/result.json` |
| Match | 43/43 | `Saved/Tests/Match/20261103T221026Z_df2122/result.json` |

**Failed attempt retained:** the first runtime run, `Saved/Tests/DamageRuntime/20261103T215722Z_db9991` at `b3635df`, failed: its reports recorded 41 checks, 38 passed, and three required check results were missing (both clients' fresh-pawn check and the observer's crouch check) (only an uncommitted doc edit made the checkout dirty). Fixed in `d2a1479`:

- `client_zones_present` failed because the 0.01 cm comparison was stricter than the engine's simulated-proxy capsule shrink.
- `client_fresh_pawn_zones` never ran, because the check compared weak pointers to destroyed round-one pawns.
- The observer's crouch check was skipped, because it timed stages with the estimated server clock.
- `stats_persist_across_rounds` saw a stale "dead" row, because the stats only re-published at the 4 Hz sync.

The fixes:

- The test tolerance is now 0.05 cm.
- Round-one pawns are compared by name.
- Clients time stages with a local clock.
- **Product fix:** the stats now publish from each controller's `OnNewPawn`, so a respawn shows as alive immediately.
- The scoreboard now names the round that just ended during Resolved ("ROUND 1 / 6", not "ROUND 2 / 6") and sits below the round HUD strip.

**Re-verified on current main:** main gained the Settings, Economy and Weapons PRs (#18–#20) after the base commit. I merged `origin/main` into this branch at `dcd29f4`; there were no conflicts and no overlap with owned paths. The merged head compiled clean in two unity blobs (`Module.ValStrike.1.cpp`/`.2.cpp`, 22 s, 0 warnings). Module SHA256 `7c46b80fab529f1c…`. All results are on the clean checkout:

| Suite | Result | Evidence |
| --- | --- | --- |
| Damage runtime smoke | 44/44 (99 s, with main's 30 s buy phase) | `Saved/Tests/DamageRuntime/20261103T221820Z_66c156/result.json` |
| Movement | 45/45 | `Saved/Tests/Movement/20261103T221959Z_5c9c84/result.json` |
| Combat | 55/55 | `Saved/Tests/Combat/20261103T222113Z_6ff900/result.json` |
| Match | 43/43 | `Saved/Tests/Match/20261103T222237Z_68f248/result.json` |

The merged-head screenshots show the zones drawn alongside the new visible rifle, and the scoreboard clear of the new credits panel. Main's `UVSEconomySubsystem::ReportKill(AController* Killer, AController* Victim)` is the Economy side of the kill hook. Its bridge is one `OnKill` bind in Economy's `Initialize` (`ReportKill(Event.Instigator, Event.VictimController)`), left to the Economy owner or integration because this stream does not edit `Economy/`.

### Limitations

- **Live gameplay is unchanged by design.** Real rifle kills count deaths only, with no kill credit and no zone damage, until Phase 2 routes fire through `ApplyWeaponHit`.
- **Hook tests drive the hook directly.** The runtime test calls `ApplyWeaponHit` from the server harness with real zone components, not through a client fire RPC.
- **Zones are prototype boxes.** They match the box-and-sphere prototype, not an anatomical model. They are not yet bound to bones, because no skeletal character exists.
- **No lag compensation.** Hit registration uses current server positions; NEXT-02 is unchanged.
- **Assists are untested at runtime.** They are implemented and unit-tested, but the 1v1 runtime test cannot produce one. The 40-damage threshold is a proposal.
- **Stats lifetime:** stats reset on a rematch or when the rules clear the match. Players who disconnect lose their row.
- **Not covered:** a late-join spectator row, 2v2, packaged builds, LAN, a real human Tab key press, and a physical-mouse pass.
- **Debug view:** in development builds anyone can enable `vs.Damage.DrawHitZones`. It is never compiled into Shipping or Test builds.

### Next action

Integration reviews the PR. After the Weapons PR merges, run Phase 2 as planned above. Optionally add `Scripts/test_damage.py` (normal and `--sanitize`) and the `Tests/Damage` unittest to the engine-free CI workflow, which the server session owns.
