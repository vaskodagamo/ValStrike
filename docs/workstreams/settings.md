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

## 2026-11-03 — Settings verification after PR #19 opened (appended 2026-11-04)

PR #19 was merged into main at 2026-11-03 23:01 (merge `f1e3779`) while these runs were in progress.

- Native `ValStrike.UI` on the final source (`fff06a8`, same source as `b18f038`): `Settings`, `ServerAddress` and `RoundPresentation` all `Result={Success}` (`Saved/Tests/UI/settings-automation-final.log`).
- Mouse-look suite rerun: **PASS 8/8** at `fff06a8` (`Saved/Tests/CombatLook/20261103T221235Z_366980`). `lower_pitch_limit` measured exactly −89.900°. The earlier 7/8 run had measured yaw −9.5° in a MouseY-only phase, consistent with physical pointer input.
- Exclusive Fullscreen (`--exclusive-fullscreen`) was **not run**. The attempt was queued behind the shared slot and cancelled when the join-key task started, so Fullscreen remains unverified on macOS.

## 2026-11-04 — Optional private-server join key in the session menu

**Branch/base:** `claude/join-key-menu` from `origin/main` `b35fc69` (includes PR #19 Settings and PR #21's server join key). Same worktree. Requested by the coordinating session; the server side belongs to the Server stream.

**Scoped completion condition:** an optional key field beside the address that is readable at 960×540 and follows the menu's focus/keyboard rules; client validation with the server's rule; the key passed to `ConnectToServer` separately and appended only after address validation; never logged or shown. Verified by engine-free rule tests and by a rendered check against `ue.py server --join-key lan-Key_0001`: refused without the key, joined with it.

**Changed paths:**

- `Source/ValStrike/UI/VSJoinKey.h/.cpp` (new, standard library only): trim, empty = no key, rule through the server's `ValStrike::Server::IsValidJoinKey` (reused, not copied), `ComposeDestination` that refuses any URL syntax in the host:port.
- `Source/ValStrike/UI/VSServerAddress.h/.cpp`: `Validate` unchanged. New `ValidateJoinKey` (its error text never echoes input) and `JoinDestination`.
- `Source/ValStrike/VSPlayerController.h/.cpp`: `ConnectToServer(Address, JoinKey = "")`. The address is validated first; a failed address containing "JoinKey" gets a hint to use the key field. The key is validated and composed only after that. The status names the address only. Also ticks the new dev harness.
- `Source/ValStrike/UI/SVSSessionMenu.h/.cpp`: masked `IsPassword` key field (168 px) beside the address, label "JOIN KEY · OPTIONAL", Tab order address → key → Connect. Cancel and Resume focus targets are now named members rather than navigation indices. Address and key fields keep focus after a rejected Enter (`ClearKeyboardFocusOnCommit(false)`); previously a rejected Enter dropped keyboard focus out of the menu.
- `Source/ValStrike/UI/VSSessionState.h/.cpp`: `JoinKey` kept in memory only, so the field survives travel. The server's "Join key missing or wrong" refusal shows key guidance instead of "Could not reach the server".
- `Source/ValStrike/UI/VSUISmoke.cpp`: one more Shift+Tab to reach the address (key field inserted).
- Tests: `Tests/UI/VSJoinKeyTests.cpp` + `Tests/UI/test_join_key_rule.py` (engine-free), `Source/ValStrike/UI/VSJoinKeyAutomationTests.cpp` (`ValStrike.UI.JoinKey`), `Source/ValStrike/UI/VSJoinSmoke.h/.cpp` + `Tests/UI/test_join_key.py` (rendered), `Tests/UI/README.md`.
- Server/, Build.cs, config, central docs, Combat/, VSCharacter.*, Damage/ and Economy/ are unchanged.

**Evidence so far:**

- Engine-free: `python3 -m unittest discover -s Tests/UI -p 'test_join_key_rule.py' -v` **2/2 OK**, also with `VS_JOIN_KEY_SANITIZE=1` (ASan/UBSan). This covers C++ assertions for empty/whitespace, 7/8/64/65 lengths, 13 bad-character cases including URL syntax, non-ASCII and embedded NUL, trimming, and composition refusing a key or any option in the address text. It also cross-checks the C++ rule against `ue.py`'s `join_key()` on 14 shared examples.
- Editor build (Mac Development) **succeeded, no warnings** at `84708f3`.
- Pending (queued behind the shared slot): native `ValStrike.UI` including `JoinKey`, the rendered `Tests/UI/test_join_key.py` run, and the UI session suite regression. Results will be appended here.

**Known limitation (needs a central decision):** Unreal itself logs the travel URL with its options on the client (`LogNet: Browse:`, `LogLoad: LoadMap:`). The server logs the login options (`LogNet: Login request:`) and its command line (`-VSServerJoinKey=`). So keyed joins put the key into local log files even though ValStrike code never logs it. Keeping it off the client's lines needs a `ULocalPlayer` subclass that supplies `GetGameLoginOptions()`, registered through `LocalPlayerClassName` in `DefaultEngine.ini`. The server side is the Server stream's call. The rendered runner counts these engine-written occurrences per file and line kind, then redacts them, so test artifacts do not keep the key.

## 2026-11-04 — GPT-06: persistent master volume

**Branch/base:** `claude/gpt-06-volume-setting` from `90849f5`. Task brief `docs/tasks/gameplay/GPT-06-volume-setting.md` (lead: Claude gameplay lead). Validation tier V1.

**Observable change:** the Settings page has a **MASTER VOLUME** row under mouse sensitivity, built like it: a slider (Left/Right step 5 %) and a numeric field, range label "0 – 100 % · default 100". The value is stored as `[ValStrike.UserSettings] MasterVolume=<percent>` next to `MouseSensitivity` in this machine's `GameUserSettings.ini` and saved through `UGameUserSettings::SaveSettings` when the page closes. Missing or malformed stored text falls back to 100; numbers clamp to 0–100 and snap to 5 %.

**Engine API choice (UE 5.8.4 source):** `FAudioDevice::SetTransientPrimaryVolume` on the local world's audio device.

- `AudioDevice.cpp` `FAudioDevice::Update`: `PrimaryVolume = GetTransientPrimaryVolume()`, then multiplied by `FApp::GetVolumeMultiplier()` (the unfocused-window multiplier stays intact).
- `AudioMixerSource.cpp`: every non-preview source multiplies by `AudioDevice->GetPrimaryVolume()`. So it covers all gameplay audio without a shared `USoundClass`/`USoundMix`; the GPT-05 `Audio/` module needs no dependency on settings.
- The setter marshals itself to the audio thread when called from the game thread.
- `UnrealEngine.cpp` `UEngine::LoadMap` calls `SetTransientPrimaryVolume(1.0f)` during teardown, so the value is reapplied when each local player's menu is built for the new world (`SVSSessionMenu::MakeSettingsPage`). Seamless travel does not reset it. A dedicated server never builds the menu, and `ApplyMasterVolume` also refuses `NM_DedicatedServer`.
- A code-created master sound class was rejected: it only affects sounds assigned to it (or a configured default class), which would couple settings to GPT-05's assets and `Config/`.

**Changed paths:**

- `Source/ValStrike/UI/VSUserSettings.h/.cpp`: volume constants, clamp/parse/format/gain, read/write/load/store, `ApplyMasterVolume`. The decimal grammar shared with sensitivity moved to a local `ParseDecimal`; sensitivity behaviour is unchanged.
- `Source/ValStrike/UI/SVSSessionMenu.h/.cpp` (settings page only): the row, Tab order slider → field for sensitivity, then volume, then window mode → resolution → Apply → Back. Volume is applied on change and saved on page close. Vertical spacing was tightened (12→8 under help/slider rows, 10→8 under the mode selector and above the key hint) to keep the page inside 540 px.
- `Source/ValStrike/UI/VSUserSettingsTests.cpp`: volume clamp, parse accept/reject, format round trip for every step, gain, stored fallback, write next to sensitivity, null-world apply.
- `Source/ValStrike/UI/VSSettingsSmoke.cpp`, `Tests/UI/test_settings.py`: the first run checks the unset default, Tab to the volume slider, Left → 95 % applied and stored, Tab through the field to the window mode; the runner checks `MasterVolume=95` on disk; restart checks the loaded and displayed 95.
- `Tests/UI/README.md` settings section.
- Untouched: `VSPlayerController.*`, `Audio/`, `Config/`, `Build.cs` (`AudioDevice.h` is reachable through Engine's public include paths).

**Checks actually run:**

- `python3 -m unittest discover -s Tests/UI -p 'test_settings_runner.py' -v`: **6/6 OK** (includes uniqueness of the new required check names).
- Cross-check that every required runner check name is emitted by the smoke source (look checks via `CheckLook`).
- `git diff --check`: clean.

**Deferred to integration (verification pending):** C++ compile (Editor, Mac Development, unity), native `ValStrike.UI.Settings`, rendered `Tests/UI/test_settings.py` (review the `settings` screenshot at 960×540 for fit: the extra row adds about 70 px and spacing was reduced by about 18 px; the estimate is not measured), `Tests/UI/test_session.py` for the session-page Tab order. Listening check: with GPT-05 audio present, 0 % is silent, 50 % audibly quieter, and the value survives a map change and a restart.

**Risks:**

- PIE with "Enable game sound" off: the editor mutes the PIE device with this same setter, and building the menu sets it back to the saved volume. Editor-only.
- A `PlayerCameraManager` fade with `bFadeAudio` also drives this setter and would override the volume while fading. ValStrike does not use audio fades today.
- Changing volume and then leaving the world without closing the Settings page (e.g. a disconnect while the page is open) keeps the value in memory but does not write it to disk until the next settings save. The controller's own save only tracks sensitivity, and that file is GPT-01's.

**Next action/owner:** Claude gameplay lead reviews, INT-01 schedules the checks above in the next batch, and the user merges.
