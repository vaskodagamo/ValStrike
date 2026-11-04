# Task GPT-01: server-confirmed hit, damage-taken and death feedback on the HUD

- Status: In progress
- Backlog ID: GUN-03 (confirmed hits and death feedback part)
- Owner/session and provider: Claude session "GPT-01 hit feedback"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-01-hit-feedback` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-01-hit-feedback`
- Owns:
  - new `Source/ValStrike/Feedback/**`
  - `Source/ValStrike/VSHUD.h/.cpp` and `Source/ValStrike/VSHUDData.h`
  - `Source/ValStrike/VSPlayerController.h/.cpp`, limited to hosting the feedback component or client RPC
  - new `Tests/Feedback/**`
  - `docs/workstreams/hit-feedback.md`
- Read-only dependencies / other owners:
  - `Damage/**`: subscribe to `UVSDamageSubsystem`'s `FVSDamageEvent`; do not change it
  - `Combat/**`, which GPT-02 owns
  - `Effects/**`, `Match/**`, `Scoreboard/**`, `UI/**`, `VSCharacter.*`, `Build.cs`, `Config/`
  - all Content, art and maps
  - If a change outside the owned paths seems required, stop and ask the lead.
- Result:
  - The server routes each attributed `FVSDamageEvent` to the shooter's and the victim's owning clients only, through one unreliable client RPC per side (or one reliable RPC for a kill). The payload is minimal: zone/headshot, applied damage, killed, and the instigator direction for the victim.
  - The shooter's HUD draws a short hit marker near the reticle, a distinct headshot variant and a distinct kill variant.
  - The victim's HUD draws a directional damage indicator. On death it shows "Eliminated by <name> · <weapon>[ · headshot]" until respawn.
  - Feedback is driven only by server-confirmed events, never by local predictions.
  - Duplicate events, a respawn and a rejoin produce no replayed or stacked markers.
  - Drawn with the existing `AHUD` canvas style: 960×540 readable, no assets.
  - Stop when the implementation and engine-free test are committed, a PR is open and the handoff is written.
- Out of scope: audio, kill feed for third parties, scoreboard changes, crosshair spread (GPT-02 exposes data later), damage-number popups, Blueprint/assets.
- Validation tier: V2 (new RPC, server-only routing)
- Contributor checks:
  - An engine-free C++ test of the pure feedback-state reducer: expiry, kill/headshot priority, dedup by event sequence, reset on respawn. Follow the pattern of `Tests/Effects/shot_effects_state_test.cpp` (clang, `-Wall -Wextra -Werror -pedantic`).
  - `git diff --check`.
  - **No Unreal build or runtime runs:** the engine slot belongs to INT-01.
- New test, justified: the dedup/replay rule is the GUN-03 completion condition ("without duplicate effects"). Keep it to that reducer.
- Integration-owned checks: combined Editor build, and `Tests/Damage/run_runtime.py`, `Scripts/test_combat.py`, `Scripts/test_match.py`. List the scenarios QA must observe in the handoff (shooter-only marker, victim-only indicator, observer sees nothing).
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR to `main`; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
