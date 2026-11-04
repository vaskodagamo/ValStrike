# KNIFE-01-C1: first remote swing and observer view

- **Brief:** producer `docs/tasks/gameplay/KNIFE-01-C1-remote-cues.md`.
- **Owner:** existing GPT-04 session. Branch `claude/knife-01-c1-remote-cues` from `origin/main` `561ea4d`.
- **Status:** implementation ready. Validation tier V1. **Verification pending in the native owner's combined build.**

## Diagnosis: the update sequence that lost the first Start

Evidence: INT-ART-03 `cue-timeline.json` on `1b572d0`. The observer saw Slash 2/3, Stab 5/5, Impact 4/4 and Cancel 4/4, and the first attack arrived as Impact only.

1. **The pawn spawns on the observer.** `FVSKnifeState` still equals its defaults: every counter is 0. Unreal sends only properties that differ from the defaults, so the initial bunch carries no knife state and **no `OnRep_State` fires**. Equipping the knife changes only the loadout, so nothing knife-related replicates either.
2. **The first slash is accepted.** `AttackCount` goes 0→1. The impact happens 0.15 s later, so this change replicates alone. It is the **first** `OnRep_State` this client receives.
3. **The old guard adopted it.** `if (!bSeenState) { SeenState = State; return; }` treated that notification as the late-join/relevancy baseline and adopted `AttackCount = 1`. **The live Start of attack 1 was dropped.**
4. **The impact update was diffed.** `ImpactCount` 0→1 now compares against a baseline that already exists, so Impact played. That is the "Impact only" first attack.
5. **Everything after is live.** All later starts, including all five stabs, compare against an existing baseline. That gives 2/3 slashes and 5/5 stabs in both runs.

Coalescing is not the cause: the first slash and its impact arrive in separate updates. The owner was unaffected because its swings come from `LocalStart`, and its first Impact played for the same reason as the observer's.

## Fix (cosmetic replication only)

**Baseline when the pawn arrives, not at the first notification:**
- `UVSKnifeComponent::BeginPlay` adopts the current counters on clients. For a fresh pawn these are the defaults.
- An `OnRep_State` before `HasActorBegunPlay()` is the initial replication of a pawn that arrives with history (late join or relevancy return). It is adopted and never played.
- Every notification after BeginPlay is live, so the first slash after a normal spawn and equip now plays.

**Ordered cues:**
- New pure `Combat/VSKnifeCueRules.h::Watch` turns each live update into cues ordered by attack index, with Start before that attack's own Impact or Cancel.
- A Start that the same update already shows as cancelled never presents.
- Several starts coalesced into one update still play the latest one once, as before.

**Two replicated cosmetic indices** were added to `FVSKnifeState` to order an update that mixes attacks:
- `ImpactAttackIndex`: the attack index the latest impact resolved, recorded per pending slot.
- `CancelAttackIndex`: `AttackCount` at the latest cancellation.

**Unchanged:**
- owner dedupe (the owner never receives a remote Start);
- the `Impact` and `Cancel` events and their fields;
- the `VSKnifePresentationEvents.h` API;
- damage, range, cooldown, the RPC protocol and server validation.

Cues stay cosmetic and cannot apply damage.

**Runtime assumption to confirm:** for a pawn spawned by replication, Unreal calls the initial `OnRep` before the actor's BeginPlay. If a component's initial state ever arrived after BeginPlay, a late joiner could see at most one historical cue.

## Practice smoke (`Combat/VSWeaponSmoke.cpp`, practice only)

- **Observer view:** from `Time >= 13.0`, the observer aims from its view location at the driver's upper body (actor location +40 cm). The firearm timeline before 13 s and the visual and match scenarios are unchanged.
- **Tightened assertion:** `knife_presentation_cues` now requires every live start on both clients, Slash ≥ 3 and Stab ≥ 5, instead of ≥ 1 each. The old code fails it on the observer (2 slashes).
- Check names and totals are unchanged: practice is still server 39, driver 37, observer 37. Scenario timing is unchanged.

## Checks

- **Run:**
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -Wshadow -ISource/ValStrike Tests/Weapons/knife_rules_test.cpp Source/ValStrike/Damage/VSDamageModel.cpp`: 70/70. The new `VSKnifeCueRules` case covers the first live start after spawn, late-join history, ordering in mixed updates, a start cancelled in the same update, and an unbaselined watch.
  - `git diff --check`: clean.
- **Deferred to the native owner** (one combined build): `Scripts/test_weapons.py --scenario practice`, expecting 39/37/37 with `knife_presentation_cues` passing, plus rendered remote acceptance. Expected cues: 3 of 3 live slash starts and 5 of 5 stab starts, with the observer camera showing the driver.
