# Hit feedback workstream

## 2026-11-04 — GPT-01 server-confirmed hit, damage-taken and death feedback

**Task:** GPT-01 (GUN-03, confirmed hits and death feedback part). Owner: Claude session "GPT-01 hit feedback"; branch `claude/gpt-01-hit-feedback` from `90849f5`. State: Implementation ready, verification pending.

### Completion handoff

- **Commit/PR:** see the PR for `claude/gpt-01-hit-feedback` to `main` (the commit that adds this file).
- **Changed:**
  - New `Source/ValStrike/Feedback/VSHitFeedbackState.h`: engine-free reducer. Sliding 64-entry sequence window (dedup, late reliable kill still shown once), marker priority Kill > Headshot > Hit, hold-then-fade lifetimes (0.25 / 0.35 / 0.6 s), up to 4 damage indicators for 1.6 s (same direction within 20° refreshes instead of stacking), death state held until `BeginLife()`.
  - New `Feedback/VSHitFeedbackSubsystem.h/.cpp`: server-only world subsystem bound to `UVSDamageSubsystem::OnDamage` (kills included). For each event it calls the shooter's `AVSPlayerController` (skipped for self-damage) and the victim's. AI or missing controllers get nothing; no multicast.
  - New `Feedback/VSHitFeedbackView.h`: HUD snapshot (marker kind/alpha, indicators relative to view yaw, "Eliminated by …" text).
  - `VSPlayerController.h/.cpp`: per-receiver server sequence; client RPCs `ClientHitConfirmed` (unreliable), `ClientKillConfirmed` (reliable), `ClientDamageTaken` (unreliable), `ClientEliminated` (reliable). Payload: sequence, receiver pawn, flags (headshot/killed/has-source), applied damage; the victim also gets the instigator location (`FVector_NetQuantize`), and on a kill the killer `APlayerState` plus weapon class. The client drops an event whose pawn is not its current pawn (respawn or unresolved). A pawn change clears all feedback through `PlayerTick`. Names go through `VSScoreboardModel::DisplayName`.
  - `VSHUD.h/.cpp`: diagonal hit marker around the reticle (white hit, gold headshot, longer and heavier red kill), red directional arcs on a ring around the reticle (alive only), and a second death-banner line "Eliminated by <name> · Rifle|Pistol[ · headshot]" until respawn. Canvas only, no assets. Sized from the existing 0.75 minimum scale for 960×540.
  - New `Tests/Feedback/hit_feedback_state_test.cpp`.
- **Checks actually run and result:**
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Feedback/hit_feedback_state_test.cpp -o Saved/Tests/Feedback/hit_feedback_state_test && Saved/Tests/Feedback/hit_feedback_state_test`: 58/58 checks passed (Apple clang, macOS).
  - `git diff --check`: clean.
  - No Unreal compile or runtime run (the engine slot belongs to INT-01). The UE-side code (`Feedback/*Subsystem*`, controller RPCs, HUD) is **not compiled**.
- **Deferred checks and integration owner (INT-01):**
  - Combined Editor build (first compile of the new RPCs/UHT and the HUD code).
  - `Tests/Damage/run_runtime.py`, `Scripts/test_combat.py`, `Scripts/test_match.py`: regression only. No runner asserts the new feedback.
  - Rendered server + 2 clients (+ an observer for the third case) at 960×540 to observe:
    1. **Shooter only:** A body-shots B. A sees a short white diagonal marker around the reticle. B and the observer see no marker.
    2. **Headshot variant:** A headshots B without killing (pistol at range, if the tuning allows). A sees a gold marker.
    3. **Kill variant:** A kills B. A sees the longer red marker. A trailing hit in the same burst does not downgrade it during its 0.6 s.
    4. **Victim only:** B, hit from the side or behind, sees a red arc pointing towards A that follows B's view as B turns and fades in about 1.6 s. A and the observer see no arc. Repeated hits from one direction refresh one arc rather than stacking.
    5. **Death text:** On death B sees "ELIMINATED" plus "Eliminated by <A's name> · Rifle[ · headshot]". It stays until B's next pawn and is gone after round reset/respawn.
    6. **No replay:** After a respawn, nothing from the previous life reappears. A late join or rejoin shows no markers for events from before the join.
    7. **Listen server host** (if used) as shooter and victim gets the same feedback, through local RPC execution.
    8. Self or no-instigator damage (if any path produces it): the victim gets no hit marker and no arc without a distinct source; the death line reads "Eliminated" without a name.
- **Remaining defect/dependency / risks:**
  - The UE code is uncompiled. Possible compile nits are small (UHT RPC signatures, `FVector2f`/`UE_INV_SQRT_2` usage).
  - A hit whose receiver pawn GUID has not yet resolved on the client (right after spawn) is dropped by design.
  - The engine-free test is not wired into `.github/workflows` CI, which is outside this task's owned paths. The lead/coordinator may add it next to `shot_effects_state_test`.
  - Applied damage only scales marker length slightly. There are no damage numbers (out of scope).
  - The existing "ELIMINATED" banner still shows for deaths without an attributed kill event (e.g. a development reset), with no killer line.
- **Next action/owner:** Gameplay lead reviews the PR. INT-01 builds and runs the scenarios above in a batch. The user merges.
