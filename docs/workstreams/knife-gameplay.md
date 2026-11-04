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
