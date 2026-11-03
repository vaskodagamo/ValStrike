# Settings workstream

## 2026-11-03 — Settings screen: mouse sensitivity and window mode

**Branch/base:** `claude/settings-menu-764d4f` from `claude/playtest-base` (`93b87c8` = main + PR #16). Worktree `.claude/worktrees/settings-menu-764d4f`. No merges into main or other branches. Contributes the "settings persist" part of PLAY-03; sound and the rest of PLAY-03 stay open.

**Scoped completion condition:** a Settings page reachable from the Escape menu with mouse sensitivity (slider plus numeric value, clamped, applied immediately, persisted, unset = 0.12) and window mode / resolution through `UGameUserSettings` with Apply; existing Escape/menu behavior intact; unit tests, a rendered settings test including a restart, and the existing UI session and mouse-look suites passing.

**Changed paths (all within this stream's ownership):**

- `Source/ValStrike/UI/VSUserSettings.h/.cpp` (new): clamp (0.02–1.00, 0.001 precision, non-finite → default), parse (plain decimal, optional sign, comma accepted), fixed three-decimal format, read/write of `[ValStrike.UserSettings] MouseSensitivity` on an `FConfigFile`, load/store/save against this machine's GameUserSettings, window-mode cycle and labels, current window state, standard window sizes that fit the work area, resolution option lists and `ApplyWindowSettings`.
- `Source/ValStrike/UI/SVSSessionMenu.h/.cpp`: the existing page becomes the first page of a widget switcher, with a **Settings** button beside Resume. New Settings page: slider plus numeric field, window mode and resolution selectors (value button is the Tab stop; Left/Right or the ‹ › arrows change it), status line, **Apply display** and **Back · Esc**. Esc on the Settings page goes back; Esc on the session page still resumes. Tab/Shift+Tab cycle the active page and skip disabled controls. The slider gets a focus colour because Slate's slider draws no focus outline.
- `Source/ValStrike/VSPlayerController.h/.cpp`: `GetMouseSensitivity` (lazy load from GameUserSettings), `SetMouseSensitivity` (clamp, in-memory store, dirty flag), `SaveUserSettings` (on leaving Settings, closing the menu, Quit and EndPlay), `GetSessionMenu` for the harness, and the dev-only `VSSettingsSmoke::Tick`. Existing `StopCombatInput()` / `CancelObjectiveInput()` calls, input ignore balancing and focus handling are unchanged.
- `Source/ValStrike/VSCharacter.cpp`: only the bodies of `LookYaw` / `LookPitch` (multiply by the local `AVSPlayerController`'s sensitivity, falling back to the member) and one `#include "VSPlayerController.h"` line, which the new bodies need. `VSCharacter.h`: a two-line comment on the `MouseSensitivity` member; its value stays 0.12.
- `Source/ValStrike/UI/VSUserSettingsTests.cpp` (new): `ValStrike.UI.Settings` automation test.
- `Source/ValStrike/UI/VSSettingsSmoke.h/.cpp` (new): opt-in `-VSSettingsSmoke` rendered harness, Development only (excluded from Shipping/Test).
- `Tests/UI/test_settings.py`, `Tests/UI/test_settings_runner.py` (new), `Tests/UI/README.md`: runner, its engine-free parsing tests, and documentation.
- `Config/DefaultEngine.ini`, `ValStrike.Build.cs`, assets, maps and central docs are unchanged.

**Design notes:**

- Sensitivity is a section in GameUserSettings.ini rather than a `UGameUserSettings` subclass (which would need `DefaultEngine.ini`) or a `USaveGame`. It is saved through `UGameUserSettings::SaveSettings()`, not a bare `GConfig->Flush`, because `UGameUserSettings::ValidateSettings` deletes a GameUserSettings file whose engine version key is missing at startup; a fresh install that only changed sensitivity would otherwise lose it after a crash before `PreExit`.
- The value is client-local: a plain controller member, never a replicated property, never sent in an RPC.
- Window changes use `SetFullscreenMode`, `SetScreenResolution`, `ConfirmVideoMode` (the explicit Apply is the confirmation; there is no timed revert dialog), `ApplyResolutionSettings(false)` and `SaveSettings`. `ApplySettings` was not used because it also re-applies scalability, VSync and frame-rate limits that this page does not edit.
- Windowed sizes are standard 16:9/16:10 sizes from 1280×720 that fit the work area, plus the current window size. The engine's `GetConvenientWindowedResolutions` produced 1283×721, 1425×1068 etc. on this 1710-wide desktop, which players would not recognise.
