# Task KNIFE-01 gameplay: knife as slot 3, server-authoritative slash/stab

- Status: **Phase A active** (2026-11-04, producer release). Phase B waits for the INT-ART-02 phase-A freeze and an explicit release. Gaps 1–4 below are resolved as proposed by the producer contract.
- Contract: the producer's `docs/tasks/KNIFE-01-full-feature.md` (Direktor worktree `/Users/webproduktion3/.codex/worktrees/10a1/ValStrike`). User scope expansion of 2026-11-04. This brief is the engineering boundary only; the contract governs values and acceptance.
- Owner: the **existing GPT-04 session** ("GPT-04 switch prediction", `local_e7f5a62b-3f49-42e2-854a-1dcddd518154`). It wrote the predicted switching, `VSWeaponLoadoutRules.h` and the Last-request semantics. It is idle, and its worktree is clean.
- Worktree: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-04-switch-prediction` (clean, on the merged `claude/gpt-04-switch-prediction`). At release: confirm it is clean, then create a new branch `claude/knife-01-gameplay` from the then-current `origin/main`.
- Lead: Claude gameplay lead. File needs outside this list go through the lead to the producer. No new contributor.
- Validation tier: V2 (melee authority and lifecycle). No contributor Unreal build. One implementation PR to main plus a focused handoff; then stop.

## Protocol rule (verified trap)

Today `Slot { Rifle=0, Pistol=1 }` and the request bytes `RequestRifle=0`, `RequestPistol=1`, `RequestLast=2` (`Combat/VSWeaponLoadoutRules.h:10-15`). `ServerRules` and `Prediction::Press` both do `static_cast<Slot>(Raw)` (`:44`, `:113`). Required:
- Add `Slot::Knife = 2` and a **distinct** `RequestKnife = 3`. Rifle 0, Pistol 1 and Last 2 keep their meaning byte for byte.
- Add explicit `EncodeRequest(Slot)` / `DecodeRequest(uint8) -> {Slot|Last|Invalid}` and use them everywhere: owner prediction, the queued and sent target, server resolution, `ServerRequestEquip`, development smoke requests, and Q history. No `static_cast` between the slot and request domains anywhere.
- Raw bytes above 3 are invalid. Q resolves the previous of all three slots, server-side from server history and predicted from the predicted view, exactly as today.
- Default after spawn or round reset: rifle equipped, previous pistol, and the knife always carried (no purchase, no ammo).

## Phase A: disjoint files (may start as soon as the producer admits the task)

| File | Change |
| --- | --- |
| new `Source/ValStrike/Combat/VSKnifeRules.h` | Pure rules: slash and stab tuning (40 dmg / 125 cm / impact 0.15 s / next 0.55 s; 65 dmg / 150 cm / impact 0.30 s / next 0.90 s; equip 0.40 s). Request validation verdicts (owner, alive, phase, equipped knife, equip-ready, cooldown, sequence dedupe and bounded pending), impact scheduling and cancellation, a candidate filter (range, arc, obstruction result in; one hit per target per swing), and the action ID window |
| new `Source/ValStrike/Combat/VSKnifeComponent.h/.cpp` | Server RPC `ServerRequestKnifeAttack(uint8 Kind, uint32 Sequence)`. Server timers at impact time compute candidates from authoritative pawn and view state, with a server trace for obstruction. Damage goes through the existing authoritative damage path with the `Knife` class. Cancellation on switch, death, `EndPlay`, round reset or closed phase. Replicated cosmetic action events (kind, sequence, result: miss, wall, player) that never apply damage. Rejection counters behave like `UVSWeaponComponent::Reject` |
| `Combat/VSWeaponLoadoutRules.h`, `Combat/VSWeaponLoadoutComponent.h/.cpp` | The knife slot, the request helpers above, readiness for the knife, and `GetEquippedWeapon()` returning **null** for the knife, plus a new `GetEquippedSlot()==Knife` path. Callers must not fall back to rifle behavior |
| `Damage/VSDamageModel.h/.cpp`, `Damage/VSDamageEvents.h`, `Damage/VSDamageSubsystem.*` (only if the knife hit needs its own entry point) | `WeaponClass::Knife`. Flat damage by attack kind: no headshot multiplier, no falloff, no penetration. Existing team and match gates stay. `WeaponName` gets "knife" |
| `Scoreboard/VSKillFeedModel.h`, `Scoreboard/VSMatchStatsSubsystem.cpp` | Kill-feed `Weapon::Knife`, label "KNIFE", mapped from the damage class. Ledger attribution stays unchanged |
| `Feedback/VSHitFeedbackState.h` | Map the knife class, if it switches on weapon |
| `Audio/VSGameplayAudioState.h`, `Audio/VSGameplayAudioSubsystem.cpp` | Swing (miss), wall-contact and player-contact cues, procedural placeholders like GPT-05's, driven by the cosmetic events. Firearm-only branches don't fire with the knife |
| `VSMovementRules.h`, `VSCharacterMovementComponent.cpp` | `WeaponKind::Knife` → multiplier 1.0 (450 cm/s). Rifle and pistol unchanged |
| `Economy/**` | Audit only. Knife is not purchasable and the kill reward is the existing flat 200. Change code only if a firearm-only assumption breaks with the knife, and say so in the PR |
| Tests: new `Tests/Weapons/knife_rules_test.cpp`; extend `Tests/Weapons/weapon_loadout_rules_test.cpp` (slot/request round trip, Last=2 unchanged, Knife=3, invalid >3, Q across three slots, prediction reconcile); `Tests/Movement/movement_rules_test.cpp` (knife 1.0); `Tests/Scoreboard/kill_feed_model_test.cpp` (label) | clang and g++ with `-Wall -Wextra -Werror -pedantic -Wshadow` |

## Phase B: shared files, after the INT-ART-02 phase-A freeze and a producer release

| File | Change |
| --- | --- |
| `VSCharacter.h/.cpp`, **input, component and slot hunks only** | Create `UVSKnifeComponent`; key **3** → `EncodeRequest(Knife)`; LMB → slash and RMB → stab while the knife is equipped (RMB is unbound today); `GetMovementWeaponKind()` Knife. Do not touch the #93 or INT-ART-02 presentation hunks |
| `VSHUD.cpp` | With the knife: a "KNIFE" label, no ammo, reload or magazine UI, static reticle. Don't touch the kill-feed, hit-marker or damage-indicator logic beyond the label |
| `VSPlayerController.h/.cpp` | Map hit feedback for the knife class, if needed |
| new `Source/ValStrike/Presentation/VSKnifePresentationEvents.h` | A gameplay-owned cosmetic event API (equip, slash, stab, hit result, cancel) for native to bind. Existing presentation components remain **native-owned** and untouched. Frozen and handed to INT-ART-03 |
| `Combat/VSWeaponSmoke.cpp`, `Scripts/test_weapons.py`, `Tests/Weapons/test_weapons_runner.py` | Extend the existing weapons smoke (no new harness): equip 3, Q across slots, slash and stab hit, miss, out of range, wall rejection, one hit per target per swing, cancellation on switch, death, reset and phase, a delayed impact after a lifecycle transition, and existing firearm checks unchanged. New checks only; existing counts unchanged |

## Proposed contract gaps (raised once, for the producer)

1. **Arc.** The contract gives the reach, not the arc. Proposal, tunable in `VSKnifeRules.h`: slash takes candidates within a 35° half-angle cone of the server view at impact; stab within 10°. Line of sight is from the server eye to the target's capsule center or nearest hit zone; a world block rejects. This needs agreement with the ART-26/27 impact framing.
2. **Kill reward.** Use the existing flat 200 (no CS-style knife bonus). Confirm, or add as a later economy task.
3. **Zones.** Flat damage regardless of hit zone. "Headshot" in the kill feed is never set for the knife.
4. **Attack while airborne or crouched.** Allowed, with the same rules. No movement-state restriction.

## Integration (not this task)

INT-ART-03 owns the native binding and full acceptance: an actual server plus rendered clients, first-person and remote visuals, and firearm regression.
