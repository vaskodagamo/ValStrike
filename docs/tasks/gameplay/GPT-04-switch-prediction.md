# Task GPT-04: client-predicted weapon switching with server reconciliation

- Status: In progress
- Backlog ID: GUN-03 follow-up. The weapons handoff limitation reads "Switching is not predicted … a click within one RTT of pressing 2 can still send a request for the old weapon".
- Owner/session and provider: Claude session "GPT-04 switch prediction"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-04-switch-prediction` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-04-switch-prediction`
- Owns:
  - `Source/ValStrike/Combat/VSWeaponLoadoutComponent.h/.cpp`
  - `Source/ValStrike/Combat/VSWeaponLoadoutRules.h`
  - `Tests/Weapons/weapon_loadout_rules_test.cpp`
  - `Source/ValStrike/Presentation/VSWeaponPresentationComponent.h/.cpp`, only to present the predicted slot
  - `docs/workstreams/switch-prediction.md`
- Read-only dependencies / other owners:
  - `Combat/VSWeaponComponent.*`, `VSRifleComponent.*`, `VSPistolComponent.*`, `Tests/Weapons/test_weapons_runner.py`: all GPT-02
  - `VSHUD.*`, `VSPlayerController.*`: GPT-01
  - `VSCharacter.*`, `Damage/**`, `Effects/**`, `Match/**`, `Config/`
  - all Content, art and maps
  - If fire gating needs a weapon-component change, describe the exact hook in the handoff for the lead to schedule after GPT-02. Do not edit it.
- Result:
  - The owning client switches its local equipped slot and starts the equip timer immediately on 1/2/Q.
  - It sends a sequenced request; the server stays authoritative.
  - On replication the client reconciles: it keeps the prediction if the server agrees with the latest request sequence; otherwise it snaps to the server state with no flicker for acknowledged requests.
  - Rapid 1/2/1 presses settle on the last request.
  - Remote observers still see only server state.
  - Fire attempts during the predicted equip use the predicted slot locally. The server still rejects fire for a weapon that is not equipped server-side.
  - The pure reconciliation rule lives in `VSWeaponLoadoutRules.h` and is covered by the existing engine-free test.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope: weapon-component fire code, animation/assets, new weapons, economy grants, recoil/spread.
- Validation tier: V2 (replicated state, server authority)
- Contributor checks: extend and compile `Tests/Weapons/weapon_loadout_rules_test.cpp` (clang `-Wall -Wextra -Werror -pedantic`) for sequence and ack reconciliation, rejection snap-back and rapid presses; `git diff --check`. No Unreal build or runtime.
- Integration-owned checks: combined Editor build, then `Scripts/test_weapons.py` (practice and match) and `Scripts/test_combat.py`. With a GPT-03 `--net-profile` later, check perceived equip latency.
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
