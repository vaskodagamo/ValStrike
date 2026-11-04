# Switch prediction (GPT-04)

Task: [GPT-04](../tasks/gameplay/GPT-04-switch-prediction.md) (on `claude/gameplay-team-kickoff`), GUN-03 follow-up. Owner: Claude session "GPT-04 switch prediction". Branch `claude/gpt-04-switch-prediction` from `90849f5`. Validation tier V2.

## Behavior

- **Owning remote client.** Pressing 1/2/Q switches the local equipped slot and starts the equip timer at once (`VSWeaponLoadoutRules::Predictor`). "Last" resolves from the predicted view, and the request on the wire is always an explicit slot with a sequence number (`ServerRequestEquip(uint8 Request, uint32 Sequence)`). Presses closer together than `MinSwitchInterval + 0.05 s` are coalesced. Only the newest target is sent when the window opens. Pressing back to the in-flight target cancels the queued request, so the server never gets a duplicate.
- **Server.** Validation is unchanged (`Evaluate`). Every processed sequenced request is acked in `FVSLoadoutState::AckSequence`, accepted or rejected, followed by `ForceNetUpdate`. Sequence 0 is unsequenced: listen-server host and raw development requests. `DevelopmentReset` keeps the ack.
- **Reconciliation** (`OnRep_State`, owning client only):
  - **Keep:** a request is queued or not yet acked.
  - **Confirm:** the server acked the latest request and agrees. The client keeps its own equip timer, so there is no flicker and no restart.
  - **Snap:** the server acked the latest request and disagrees (rejection), or a confirmed prediction sees an `EquipCount` change it did not request. The client adopts server state.
  - **Settle:** the switch is confirmed and both sides have finished the equip. The client goes back to mirroring the server.
- **Local accessors.** `GetEquippedSlot`, `IsEquipping`, `GetEquippedWeapon` and `GetReadiness` now return the predicted view on the owning client. HUD, local fire gating and the weapon presentation follow the press through these accessors without edits to GPT-01/GPT-02 files. `GetState()` remains the replicated server state, which the smoke tests read.
- **Observers and server** never activate the prediction. They see only server state. Server fire/reload readiness still uses the server clock and server slot.

## Completion handoff

- **Commit/PR:** see the PR for branch `claude/gpt-04-switch-prediction` (the commit carries this file).
- **Changed:**
  - `Combat/VSWeaponLoadoutRules.h`: `IsSequenceNewer`, `ServerView`, `Outcome`, `Predictor`.
  - `Combat/VSWeaponLoadoutComponent.h/.cpp`: sequenced RPC, ack, prediction, reconciliation, send queue timer.
  - `Presentation/VSWeaponPresentationComponent.h/.cpp`: comments only. It already reads the loadout view.
  - `Tests/Weapons/weapon_loadout_rules_test.cpp`: 54 checks, up from 20. They cover sequence order and wrap, press and queue spacing, keep/confirm/settle/snap, server-driven change and dropped queue. A 60 ms one-way network simulation against `Evaluate` covers accepted switches without flicker, rapid 2/1/2/1 settling on the last press with 2 server switches, cancel-to-in-flight, and rejection snapping exactly once.
- **Checks actually run and result:**
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/weapon_loadout_rules_test.cpp`: 54/54 passed (Apple clang 17.0.0).
  - The same test with `-fsanitize=address,undefined`: 54/54 passed.
  - `git diff --check`: clean.
- **Deferred checks (integration owner INT-01):** compile and runtime are **verification pending**.
  - Combined Editor build: UHT for the changed RPC signature and the new `FVSLoadoutState` field. The C++ component was not compiled here.
  - `Scripts/test_weapons.py`, practice and match. They use the raw development RPC (sequence 0) and `GetState()`, so their expectations should be unchanged.
  - `Scripts/test_combat.py`.
  - With a GPT-03 `--net-profile`, run these server/client scenarios on a real client:
    - Press 2: pistol shows and lowers/raises on the press, the HUD shows equipping at once, and there is no flicker when the ack arrives.
    - Press 2 then fire during the equip: no shot is sent before the predicted ready time, and no `holstered` rejections.
    - Rapid 1/2/1 ends on the last press on both client and server.
    - Press 2 during preparation or when dead: no prediction (the local gate). For a server-only rejection, the view snaps back once.
    - Observers see only the server switch.
- **Remaining defect/dependency:**
  - **Equip-boundary jitter.** The predicted ready time is about RTT/2 earlier than the server's. A shot sent at the predicted ready time reaches the server at about its own ready time, so jitter can occasionally reject the first shot as `equip`. Held automatic fire then retries on the next interval. If integration sees this, the lead can choose one of two fixes:
    - (a) A server equip arrival tolerance in `UVSWeaponLoadoutComponent::GetReadiness` (owned here), like the fire-cadence 20 ms tolerance.
    - (b) A GPT-02 hook in `UVSWeaponComponent::SendShot`/`IsReadyLocally` that adds a small local margin.
  - No GPT-02 or GPT-01 hook is required for this task.
  - Under very heavy jitter, coalesced requests can still arrive inside the server rate limit. That produces a `spam` rejection and one snap-back to server state, which is never a desync.
  - `weapon_loadout_rules_test.cpp` is not in the GitHub engine-free CI matrix. Adding it there is a CI-owner decision.
- **Next action/owner:** gameplay lead reviews, then INT-01 runs the combined build and the scenarios above. The user merges.
