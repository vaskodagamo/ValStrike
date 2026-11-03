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
   - Remove `BodyHitRegion`/`HeadHitRegion`. Add `UPROPERTY(VisibleAnywhere) TObjectPtr<UVSHitZoneSet> HitZones` created in the constructor and attached to the capsule, then call `HitZones->SetTraceMode(EVSHitZoneTraceMode::Authoritative)` in `BeginPlay`. The subsystem then skips auto-attach for this class.
   - Call `HitZones->RefreshLayout()` from `UpdatePrototypeVisuals()`, which runs on crouch and uncrouch, so a shot in the same frame as a crouch already sees the new layout.
   - Drop the region lines from `HandleHealthChanged()`; the zone set handles death itself.
   - Replace `IsHeadHit()` with `HitZones->ResolveZone(C) == HitZone::Head`, or remove it.
2. **Rifle and pistol `ServerRequestFire`:** replace `ApplyPointDamage(IsHeadHit ? HeadDamage : BodyDamage)` with `UVSDamageSubsystem::Get(this)->ApplyWeaponHit({Pawn, Target, Hit.GetComponent(), WeaponClass::Rifle /*or Pistol*/, Hit.Distance})`. Retire the `BodyDamage`/`HeadDamage` properties, since the table is the single source.
3. **Combat smoke expectations:** body shots now depend on the zone. A shot at the capsule center lands in the lower torso (27, not 30). Aim body probes at the chest band (capsule-relative z ≈ +40 standing) or assert the zone-specific value.
4. **Stats and scoreboard:** no change needed. Real kills reach the ledger and the scoreboard through `OnDamage`.
5. **Verify:**
   - Rerun the combat, match and movement suites, and `Tests/Damage/run_runtime.py`.
   - Its `rifle_path_unchanged` check is Phase 1 specific and should become "rifle hits resolve to zones" with real fire RPCs.
   - Delete or adapt the Shadow-mode checks.
