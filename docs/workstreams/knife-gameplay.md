# Knife gameplay (KNIFE-01 phase A)

- **Task:** [KNIFE-01 gameplay](../tasks/gameplay/KNIFE-01-gameplay.md) (on `claude/gameplay-board-knife01`). The producer contract `docs/tasks/KNIFE-01-full-feature.md` governs the values.
- **Owner:** existing GPT-04 session. Branch `claude/knife-01-gameplay` from `origin/main` `5407272`.
- **Validation tier:** V2.
- **Implementation commit:** `2872af5`. The PR head adds only this handoff on top of it.
- **Status:** implementation ready; **verification pending: INT-ART-03**.

## Behavior

### Slots and protocol

- The knife is `Slot::Knife = 2` / `EVSWeaponSlot::Knife`. Its request byte is `RequestKnife = 3` / `EVSEquipRequest::Knife`.
- Rifle 0, Pistol 1 and Last 2 keep their bytes.
- `EncodeRequest`, `DecodeRequest` and `ResolveRequest` (`VSWeaponLoadoutRules.h`) are the only bridge between the two domains. They are used by:
  - server resolution (`Evaluate`);
  - owner prediction (`Predictor::Press`);
  - the queued and sent targets (`SendPredictedRequest`);
  - the listen-host path.
- Bytes above 3 are invalid.
- Q resolves the previous of all three slots: from server history on the server, and from the predicted view on the owner.
- Spawn and round reset: rifle equipped, pistol as "last", knife carried.

### Loadout

- Equip time is per slot: firearms `EquipDuration`, knife `VSKnifeRules::EquipSeconds` (0.40 s). It applies on the server, in the owner's prediction and in `GetEquipDuration()` for presentation.
- With the knife equipped:
  - `GetEquippedWeapon()` and `GetWeapon(Knife)` return null.
  - `GetReadiness(firearm)` returns `Holstered`, so firearms never fall back to rifle behavior.
  - `IsKnifeEquipped()` and `GetKnifeReadiness()` are the knife gate.
- Equipping the knife requires a `UVSKnifeComponent` on the pawn. Until phase B creates one, a knife request is rejected as `invalid`.
- Switching away from the knife, and `DevelopmentReset`, cancel pending knife impacts.

### Knife (`Combat/VSKnifeRules.h`, `Combat/VSKnifeComponent.*`)

**Tuning**

| Attack | Damage | Reach | Cone (half-angle) | Impact | Next attack | Max targets |
| --- | --- | --- | --- | --- | --- | --- |
| Slash | 40 | 125 cm | 35° | 0.15 s | 0.55 s | 2 |
| Stab | 65 | 150 cm | 10° | 0.30 s | 0.90 s | 1 |

Times count from the accepted start. Max targets is a gameplay-side choice: one hit per target per swing, nearest first.

**Request**

- `ServerRequestKnifeAttack(uint8 Kind, uint32 Sequence)` is reliable.
- The server validates in this order: owner, sequence, kind, alive, combat gate, knife equipped, equip ready, cooldown (20 ms arrival tolerance, no credit), at most two pending.
  - The sequence check rejects zero, duplicates, replays and jumps of more than 1024.
  - A rejected request still consumes its sequence after the sequence check.
- Rejection counters: `owner`, `sequence`, `invalid`, `dead`, `phase`, `holstered`, `equip`, `cooldown`, `busy`, `cancelled`.

**Impact**

- At the impact time the server takes `Actions.Take` with the epoch check. Then it re-checks owner, alive, `AllowsCombat`, `CanMatchCombat` and that the knife is still equipped; if any fails, it cancels everything.
- It reads the authoritative eye and view and pre-filters live `AVSCharacter`s by reach and cone.
- For each remaining candidate, it traces `ECC_Visibility` from the eye toward the capsule-axis point at eye height. Hit zones block that channel; world geometry or another pawn first means obstructed.
- A forward trace over the reach detects wall contact.
- `VSKnifeRules::Select` picks the victims. Each one goes through `UVSDamageSubsystem::ApplyWeaponHit` with `WeaponClass::Knife` and the attack damage, which keeps team and match gates, attribution, stats and the 200 kill reward.
- The client sends only the kind and the sequence.

**Cancellation**

These all advance the action epoch and clear the timers, so a delayed impact can never land:
- switching away from the knife;
- death (`OnLifeStateChanged`);
- `EndPlay` (destroyed pawn or round reset);
- `DevelopmentReset`;
- a closed phase seen at impact time.

**Cosmetics**

- Replicated `FVSKnifeState`: `AttackCount`/`LastAttack`, `ImpactCount`/`LastImpact`/`LastImpactAttack` (miss, wall, player), and `CancelCount`.
- Native `OnKnifeEvent()` events:
  - `LocalStart`: owner, on send.
  - `Start`: remote pawns.
  - `Impact`.
  - `Cancel`.
- First sight adopts the counters without replay. None of this can apply damage.

### Damage, feed and other systems

- **Damage:** `WeaponClass::Knife` (`"Knife"`). Its profile is flat 1.0 on every zone, with no lethal head and no falloff. `HitRequest`/`FVSWeaponHit::AttackDamage` overrides the base damage (same clamp, finite only).
- **No knife headshot:** `IsHeadshotHit` and `FVSDamageEvent::IsHeadshot()` are never true for the knife, so the hit markers, kill-feed headshot flag, stats and the "headshot" death text never show one.
- **Kill feed:** `Weapon::Knife` is appended after `Unknown`, so replicated bytes are stable. `WeaponFromDamageClass` maps to the label `KNIFE`, which the existing HUD shows through `WeaponLabel`. The death text reads "Knife" through `WeaponName`.
- **Hit feedback:** `VSHitFeedbackState.h` has no weapon branch, so it is unchanged.
- **Movement:** `WeaponKind::Knife` gives multiplier 1.0 (450 cm/s). The movement component maps the knife slot itself until `VSCharacter::GetMovementWeaponKind` does. Rifle and pistol are unchanged.
- **Audio:** new cues `KnifeSwing` (miss), `KnifeWall` and `KnifeFlesh`. Each has a config art slot and a procedural placeholder, and plays once per server impact from the replicated counter. The knife has no shot counter, so firearm cues can't fire for it.
- **Economy:** audited, no change. The kill reward is weapon-agnostic (`OnKill`, 200), and the knife is not in the catalog.

## Phase B must wire (after the INT-ART-02 freeze and a release)

- **`VSCharacter.*`:**
  - Create `UVSKnifeComponent`.
  - Bind key **3** to `Loadout->RequestEquip(EVSEquipRequest::Knife)`. Never send raw slot 2.
  - While `Loadout->IsKnifeEquipped()`, route LMB to `Knife->RequestAttack(EVSKnifeAttack::Slash)` and RMB to `EVSKnifeAttack::Stab`; keep LMB to the firearm otherwise.
  - Add `case EVSWeaponSlot::Knife: return VSMovementRules::WeaponKind::Knife;` to `GetMovementWeaponKind`.
  - `GetWeaponPresentation(Knife)` stays null unless a knife view is added.
  - `StopCombatInput` needs no knife change, because there is no held fire.
- **`VSHUD.cpp`:** with the knife equipped, `GetHUDData()` already reports `bHasWeapon = false`, so there is no ammo or firearm label. Add the "KNIFE" label and a static reticle.
- **Presentation:** bind `OnKnifeEvent()`. `LocalStart` drives the owner's swing and `Start` drives remote swings. Use `Impact` for contact effects, and use `Cancel` plus a slot change to stop animations. Equip blend time comes from `GetEquipDuration()`.
- **`VSWeaponSmoke.cpp`:** add knife checks using `DevelopmentRequestEquip(VSWeaponLoadoutRules::EncodeRequest(Slot::Knife))` and `UVSKnifeComponent::DevelopmentRequestAttack(kind, sequence)`. Its existing `static_cast<uint8>(EVSEquipRequest::…)` stays in the request domain and is correct.

## Completion handoff

- **Commit/PR:** implementation `2872af5`. The PR to `main` is titled "KNIFE-01 phase A".
- **Changed:**
  - `Combat/VSKnifeRules.h` (new), `Combat/VSKnifeComponent.h/.cpp` (new)
  - `Combat/VSWeaponLoadoutRules.h`, `Combat/VSWeaponLoadoutComponent.h/.cpp`
  - `Damage/VSDamageModel.h/.cpp`, `Damage/VSDamageEvents.h`, `Damage/VSDamageSubsystem.cpp` (one line passes `AttackDamage`)
  - `Scoreboard/VSKillFeedModel.h`, `Scoreboard/VSMatchStatsSubsystem.cpp`
  - `Audio/VSGameplayAudioState.h`, `Audio/VSGameplayAudioSubsystem.h/.cpp`
  - `VSMovementRules.h`, `VSCharacterMovementComponent.cpp`
  - Tests: `Tests/Weapons/knife_rules_test.cpp` (new, 60 checks); `weapon_loadout_rules_test.cpp` (78, up from 54); `Tests/Movement/movement_rules_test.cpp`; `Tests/Scoreboard/VSKillFeedModelTests.cpp`
  - **Outside the brief's list:** `Tests/Damage/VSDamageTests.cpp`. Its anatomy-ordering case now loops over the two firearms only, because the intentionally flat knife row would otherwise fail it.
- **Checks actually run** (Apple clang 17.0.0, `-std=c++17 -Wall -Wextra -Werror -pedantic -Wshadow`):

  | Test | Result |
  | --- | --- |
  | knife rules, with `VSDamageModel.cpp` | 60/60 |
  | weapon loadout rules | 78/78 |
  | movement rules | 64/64 |
  | kill feed, with `VSStatsLedger.cpp` | 64 checks, 0 failures |
  | gameplay audio state (all cues generate valid placeholders) | 230 checks, 0 failures |
  | hit feedback state | 58/58 |
  | `Scripts/test_damage.py`: damage model | 14/14 cases |
  | `Scripts/test_damage.py`: stats ledger | 11/11 cases |
  | `git diff --check` | clean |

  - No Unreal build was run. The engine build uses `-Wall -Werror -Wno-switch -Wshadow` (taken from an existing ValStrike response file), so the untouched rifle/pistol switches in `VSCharacter.cpp` don't fail on the new enumerator.
- **Deferred (INT-ART-03): verification pending.**
  - Combined Editor build: UHT for the new `UVSKnifeComponent`, the `FVSKnifeState` struct, the new `EVSWeaponSlot`/`EVSEquipRequest` values and the audio art slots.
  - Then `Scripts/test_weapons.py`, `Scripts/test_combat.py` and `Scripts/test_damage.py --runtime` for firearm regression.
  - After phase B, real server and client runs covering:
    - equip 3 and Q across three slots;
    - slash and stab hit, miss, out of range and wall;
    - one hit per target per swing;
    - cancellation on switch, death, reset and phase, including a delayed impact after the transition;
    - knife kill feed and death text, with no knife headshot;
    - audio cues;
    - 450 cm/s run speed;
    - remote cosmetics.
- **Remaining risk:**
  - The engine-side C++ is uncompiled.
  - The candidate probe aims at the capsule axis at eye height. A target whose zones leave a gap on that line is a miss (`Probe::Missed`), never a free hit. Tune in runtime if needed.
  - The owner's `LocalStart` is cosmetic and can precede a server rejection.
- **Next owner:** gameplay lead review, then the phase B release and INT-ART-03.

## Phase B (2026-11-04)

- **Branch:** `claude/knife-01-phase-b`, from `claude/knife-01-gameplay` (`1fa433e`, PR #105) with `origin/main` `a22b9bf` merged in (includes INT-ART-02 #103).
- **Implementation commit:** `d53eaa4`. The PR head adds this handoff section on top of it.
- **Merge order:** the phase B PR builds on #105, which should merge first.
- **Status:** implementation ready; **verification pending: INT-ART-03**.

### Changes

- **`VSCharacter.h/.cpp`:** input, component and slot hunks only. The diff is additions only: 45 lines added, none removed, so the #93/#103 presentation hunks (character presentation, P01 `hand_r` attachment, pistol hold) are byte for byte unchanged.
  - `UVSKnifeComponent` is created as `Knife` (`GetKnifeComponent()`).
  - Key **3** maps to `EquipKnife`, which calls `RequestEquip(EVSEquipRequest::Knife)`, request byte 3.
  - While `IsKnifeEquipped()`, LMB slashes and returns before any firearm path. RMB is a new `Secondary` action that stabs. `GetEquippedWeapon()` is null with the knife, so reload and fire never reach a firearm.
  - `GetMovementWeaponKind()` maps Knife to `WeaponKind::Knife`.
  - `OnRep_MatchGates` with combat closed (phase or death) also calls `Knife->CancelActions()`, which acts on the server only.
  - `GetHUDData()` with the knife: `WeaponName = "KNIFE"`, `bEquipping`, and `bHasWeapon` stays false, so there is no ammo.
- **`VSHUD.cpp`:**
  - With the knife, a label panel shows "KNIFE" (or "KNIFE  EQUIPPING") and "LMB SLASH  RMB STAB", with no ammo, magazine or reload.
  - The help line reads "1 RIFLE  2 PISTOL  3 KNIFE  Q LAST WEAPON".
  - The reticle stays at its static gap: no firearm is equipped, so `ReticleGap` returns `StaticGap`.
  - The firearm panel, kill feed, hit markers and damage indicators are untouched.
- **`VSPlayerController.*`:** no change needed. The death text already adds "Knife" through `WeaponName`. The server never sets the headshot flag for the knife (`FVSDamageEvent::IsHeadshot`), and the hit-marker path has no weapon branch.
- **`Combat/VSKnifeComponent.*`:** development hook only. `DevelopmentRequestAttack(kind, 0)` takes the next local sequence, and `DevelopmentLastSequence()` exists for the replay check.
- **Economy:** audited again, no change.

### Event API for native (`Presentation/VSKnifePresentationEvents.h`, frozen)

A native presentation component owns one `FVSKnifePresentationEvents`. It calls `Bind(AVSCharacter*)`, then `Tick()` once per frame, and handles `OnCue()`, which delivers an `FVSKnifeCueEvent`. Every cue is cosmetic.

| Cue | When | Fields |
| --- | --- | --- |
| `Equip` | The knife slot became equipped in this machine's view: the owner's prediction, server state elsewhere. | `Duration` = equip time (0.40 s); `bLocal` on the owner |
| `Holster` | The knife slot was left (switch or reset). Stop knife animations. | none |
| `Slash` / `Stab` | An attack started. On the owner (`bLocal`) this is its own press, which the server may still reject; on remote views the server accepted it. | `Duration` = seconds to the impact marker (0.15 / 0.30); `Recovery` = seconds to the next attack (0.55 / 0.90) |
| `Impact` | The server resolved an impact. | `Result`: `Miss`, `Wall` or `Player` |
| `Cancel` | Pending impacts were cancelled (switch, death, reset or closed phase). Stop the attack before its impact. | none |

- Late joins and relevancy returns get `Equip` on the next `Tick` and never replay old attacks.
- The weapons smoke exercises this API on both clients (`knife_presentation_cues`).

### Weapons smoke (`practice`), new checks only

The knife segment runs from 13.0 to 24.4 s, after the unchanged firearm timeline. The firearm `Finish()` now happens at the end of the knife segment.

- **Common, on all three roles (10):**
  - `knife_equipped`
  - `knife_slash_hit_once`
  - `knife_out_of_range`
  - `knife_wall`
  - `knife_cancel_switch`
  - `knife_equip_last`
  - `knife_cancel_phase`
  - `knife_cancel_reset`
  - `knife_stab_hit`
  - `knife_cancel_death`
- **Server only (+5):**
  - `knife_reject_equip`
  - `knife_reject_cooldown`
  - `knife_reject_invalid`
  - `knife_reject_replay`
  - `knife_reject_holstered`
- **Client only (+2):**
  - `knife_hides_firearms`
  - `knife_presentation_cues`

**Expected practice totals:**

| Role | Before | New | Total |
| --- | --- | --- | --- |
| Server | 24 | 15 | 39 |
| Driver | 25 | 12 | 37 |
| Observer | 25 | 12 | 37 |

Visual and match scenarios are unchanged.

**Scenario (server-placed):**

1. The target stands 100 cm in front of the driver. Key 3 equips the knife; a raw attack during the equip is rejected.
2. A slash takes the target 100 to 60, with no firearm shot. A raw slash 0.1 s later is rejected as cooldown; a raw kind 7 attack and a replayed sequence are also rejected.
3. The target moves to 300 cm: the slash misses.
4. The target returns behind a server-spawned slab: wall contact, no damage.
5. Each of these happens on the server tick that sees a pending stab, and the stab never lands:
   - a switch to the rifle;
   - a closed combat gate;
   - `DevelopmentReset`.
6. A raw attack with the rifle equipped is rejected as holstered. Q from the rifle returns to the knife.
7. A stab kills the target.
8. A final stab is cancelled by the driver's own death.

- **Runner:** `Scripts/test_weapons.py` gains the `KNIFE_*` tuples. `Tests/Weapons/test_weapons_runner.py` asserts the firearm tuple sizes are unchanged (16, 25, 24), adds the knife names and keeps them out of the visual and match scenarios.

### Checks actually run

- `python3 -m unittest Tests/Weapons/test_weapons_runner.py`: 10 tests OK, including "every required name exists in the native harness".
- `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -Wshadow` on the knife rules test (with `VSDamageModel.cpp`, 60/60) and the weapon loadout rules test (78/78). This was a post-merge sanity check; phase B changed no rules headers.
- `git diff --check`: clean.
- No Unreal build and no runtime.

### Deferred to INT-ART-03 (verification pending)

- Combined Editor build: UHT for the new `Knife` subobject and actions. The new header compiles only through `VSWeaponSmoke.cpp`.
- `Scripts/test_weapons.py --scenario practice`, expecting server 39, driver 37 and observer 37 checks. Also `visual` and `match` (unchanged) and `Scripts/test_combat.py`.
- Then native binding of `FVSKnifePresentationEvents` and full rendered acceptance.

### Risks

- The engine C++ is uncompiled.
- The smoke's lifecycle steps depend on the server seeing each stab accepted before its 0.15 s/0.30 s impact. They trigger on that state, not on time, but a frame longer than the impact delay would turn a cancel check into a hit.
- The slab is server-only collision; clients never see it.

## KNIFE-01-M1 correction (2026-11-04)

#108 merged at `fcf6a9c`, not at its reviewed head `22df932`. The extra main merge reintroduced two duplicates in the development hook:

- **`VSKnifeComponent.h`:** a second `DevelopmentRequestAttack(uint8, uint32)` declaration and its old comment. This redeclaration does not compile.
- **`VSKnifeComponent.cpp`:** a second `ServerRequestKnifeAttack(RawKind, Sequence)` call, which made every development attack send two RPCs.

**Fix:** branch `claude/knife-01-m1-fix` from `caf7551` removes exactly those lines. Behavior is unchanged from the reviewed hook: sequence 0 takes the next local sequence, any other value is sent as is, and there is one RPC.

**Checks run:**
- `git diff 22df932 -- Source/ValStrike/Combat/VSKnifeComponent.h Source/ValStrike/Combat/VSKnifeComponent.cpp` is empty: both files match the reviewed version, and main had not changed them otherwise.
- `git diff --check`: clean.

No new test and no Unreal build. **Verification pending: INT-ART-03.**
