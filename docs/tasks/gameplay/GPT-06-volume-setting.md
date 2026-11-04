# Task GPT-06: persistent master volume in the settings page

- Status: In progress
- Backlog ID: PLAY-03 settings part. The MVP settings row covers "Mouse sensitivity, volume, window/resolution options, exit to menu".
- Owner/session and provider: Claude session "GPT-06 volume setting"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-06-volume-setting` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-06-volume-setting`
- Owns:
  - `Source/ValStrike/UI/VSUserSettings.h/.cpp` and `UI/VSUserSettingsTests.cpp`
  - `Source/ValStrike/UI/SVSSessionMenu.h/.cpp`, the settings page section only
  - `Source/ValStrike/UI/VSSettingsSmoke.h/.cpp`, `Tests/UI/test_settings.py`, `Tests/UI/test_settings_runner.py`: only as needed to keep the existing settings checks consistent with the new row
  - `Tests/UI/README.md` (settings section)
  - `docs/workstreams/settings.md`: append a new dated section
- Read-only dependencies / other owners:
  - `UI/VSJoinKey*`, `VSServerAddress*`, `VSSessionState*`, `VSUISmoke*`, `VSJoinSmoke*`, `UI/Round/**`
  - `VSHUD.*`, `VSPlayerController.*` (GPT-01), `Combat/**`, `Audio/**` (GPT-05)
  - `Config/`, `Build.cs`, all Content, art and maps
- Result:
  - A master volume setting (0–100 %, step 5, default 100) stored next to mouse sensitivity in the same client-local config section and format, with the same clamp/parse/format helpers style.
  - Applied on load and on change, client-only, through the engine's audio device: transient primary volume, or a code-created master `USoundMix`/`USoundClass` adjuster; pick one and justify it from the UE 5.8 source.
  - The settings page gets one keyboard-navigable row matching the sensitivity row, readable at 960×540, with Tab order preserved.
  - Invalid stored values fall back to the default.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope: per-category volumes, audio content (GPT-05), window/resolution options, keybinding.
- Validation tier: V1 (UI behavior and persistence)
- Contributor checks: the existing engine-free/native settings tests extended only for volume clamp/parse/format/default fallback, plus `python3 -m unittest discover -s Tests/UI -p 'test_settings_runner.py' -v` if it is engine-free; `git diff --check`. No Unreal build or runtime.
- Integration-owned checks: combined Editor build, native `ValStrike.UI.Settings` automation, `Tests/UI/test_settings.py` rendered, plus `Tests/UI/test_session.py` to confirm the Tab order is unbroken.
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
