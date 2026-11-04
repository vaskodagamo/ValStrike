# Task GPT-15: weapon weight, more natural gravity and CS2-style crouch-jump

- Status: In progress
- Requested by the user on 2026-11-04: "the weapons have weight, we move slower with heavy weapons"; "change gravity so it feels more natural"; "if we jump with ctrl and space so we jump higher a little bit and if we run and jump then also like that, same as in cs2".
- Backlog: MOVE-02 (movement feel). It must not regress MOVE-01/02 evidence.
- Owner/session and provider: Claude session "GPT-15 movement feel"
- Branch and base: `claude/gpt-15-movement-feel` from `origin/main` `b342ca9`
- Worktree: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-15-movement-feel`
- Owns:
  - `Source/ValStrike/VSCharacter.h/.cpp`: movement setup, max-speed selection, jump and crouch handling. No other character areas.
  - a custom `UCharacterMovementComponent` subclass if needed (new `Source/ValStrike/VSCharacterMovementComponent.h/.cpp`)
  - a new pure `Source/ValStrike/VSMovementRules.h`
  - `Source/ValStrike/Tests/VSMovementSmoke.*`: only to keep its scenario working with the new tuning or to add new sample stages
  - a new `Tests/Movement/movement_rules_test.cpp`
  - a new `docs/workstreams/movement-feel.md`
- **Not owned:** `Scripts/test_movement.py`, which GPT-12 (PR pending) is editing. If its expectations (speeds, `50 < jump height < 120`) must change, list the exact new bounds and why in the handoff and the PR. The lead assigns the runner edit after GPT-12 merges. Also read-only: `Combat/**` (read the equipped weapon from the loadout; do not add fields there), `Scripts/*` and every other runner, Content, art and maps (Site01 is read-only: report reachable heights instead of changing the map).
- Current values: `MaxWalkSpeed 450`, `MaxWalkSpeedCrouched 180`, `MaxAcceleration 2048`, `BrakingDecelerationWalking 2048`, `JumpZVelocity 420`, `AirControl 0.15`, default gravity (980 cm/s², `GravityScale 1`). That gives an apex of about 90 cm and about 0.86 s airtime, which is floaty.
- Result:
  1. **Weapon weight.** Max ground speed scales by the equipped weapon, using CS2 ratios relative to unarmed or knife 250: pistol about 240 (0.96), rifle about 215 (0.86). Crouched and walk (slow-walk) speeds use the same multiplier. Server and owning client must compute the same max speed from the same equipped/predicted loadout state, so steady movement produces no corrections. A one-frame mismatch during a predicted switch is acceptable; document it. Put the multiplier table in `VSMovementRules.h`, keyed by weapon kind.
  2. **Gravity and jump arc.** Snappier and less floaty, matching CS2's arc scaled to this character's height. CS2: gravity 800 u/s², jump apex about 57 u for a 72 u player (about 0.79 of standing height), about 0.75 s airtime. Pick `GravityScale` and `JumpZVelocity` so the apex-to-height ratio and the airtime land near those numbers. Show the arithmetic in the handoff.
  3. **Crouch-jump.** Pressing crouch while jumping (in the air, or Ctrl together with Space) tucks the capsule, so the feet clear about 9–10 u more, scaled; CS2 crouch-jump reaches about 66 u ledges versus about 57 u. It works the same standing or running. No extra vertical velocity: the gain must come from the tuck, as in CS2. If UE's crouch in air does not raise the feet, implement the tuck explicitly, server-authoritative and predicted.
  4. **Running jump.** As in CS2, a running jump carries ground speed into the air with low air control, so it travels farther but not higher. It also takes the weapon-weight speed, and landing doesn't add speed (no bunny-hop gain). Note in the handoff that CS2 adds no height on a running jump; if the user wants extra height there, that is a follow-up.
  5. Report the reachable heights: standing jump, crouch-jump, and the tallest Site01 cover or crate heights you can find read-only. Say which surfaces become reachable or unreachable.
  6. A pure `VSMovementRules.h` (speed multiplier, apex/airtime from gravity and jump velocity, crouch-jump clearance) covered by an engine-free test.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope: stamina, fall damage, strafe-jumping or air-strafe mechanics, ladders, new input bindings beyond the existing Space/Ctrl, and any map or asset edit.
- Validation tier: V2 (server-authoritative, predicted movement).
- Contributor checks: the engine-free test (clang and g++ `-Wall -Wextra -Werror -pedantic`), `git diff --check`. No Unreal build or runtime.
- Integration-owned checks: Editor build; `Scripts/test_movement.py` with the bounds the handoff proposes (which needs the runner edit after GPT-12); `Scripts/test_combat.py`, `Scripts/test_weapons.py --scenario practice`, `Scripts/test_damage.py` (moving-spread and hit tests may depend on speed); no correction spam in the logs. A human playtest decides the feel.
- Engine slot: none. Publication: push and open a PR; the user merges.
