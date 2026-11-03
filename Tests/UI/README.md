# Integrated combat HUD and session checks

These checks use the Mac **development editor** executable, not a packaged client or dedicated server. Run from this checkout with the pinned Unreal 5.8.4 / Xcode 26.1.1 toolchain.

```sh
python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
python3 Tests/UI/test_session.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --rendered
```

The runner starts its own loopback server on UDP 7813 and a client initially in the local arena. It holds a silent UDP socket on 7814 for the expected connection timeout. Use `--port` / `--failure-port` if occupied. Only the runner's processes are closed. Omit `--rendered` for null rendering; rendered mode captures native 960×540 screenshots. All outputs go to ignored `Saved/Tests/UI/<run-id>/`, including exact process commands, engine version, source hashes, individual checks and `result.json`. Exit zero requires complete passing evidence, including actual client termination after Quit.

`-VSUISmoke` enables the C++ harness only in development builds; normal launches do not run it, and Shipping/Test exclude its implementation. This test expects the integrated combat implementation: 100/100 health, an alive player, a 30-round rifle and 90 reserve rounds. It reads the actual `GetHUDData()` snapshot; it does not inject combat state or simulate a match. The old unavailable-combat baseline is no longer the acceptance target.

The harness preserves controller input, actual Slate focus/navigation, validated travel, real client connection, menu input isolation, disconnect, cancellation and timeout recovery checks. Once connected, it verifies the owning client's live health/weapon defaults, presses and holds Left Mouse through Enhanced Input, and observes replicated ammo decrease. Escape must clear held fire. After a 0.35-second settling interval following the menu checks, ammo/reserve must remain unchanged throughout a further 0.75 seconds; the magazine must remain nonempty so running out of ammunition cannot produce a false pass. After resuming, an R-key press must expose the reload state and complete with the correct magazine/reserve transfer. No weapon component setters or test reset controls are used.

Every combat checkpoint records a `VSUI_HUD` snapshot in the driver log and `result.json`. Missing, malformed or duplicate snapshots, missing required checks, and any reported failure make the runner fail. Rendered mode adds native `connected-hud`, `connected-reloading` and `connected-reloaded` screenshots to the existing session screenshots. The C++ sequence retains its 80-second deadline; the runner's default 110-second limit includes startup, and native quit has its separate 30-second bound. The engine's configured connection timeout is unchanged.

The native address automation suite also checks accepted values against Unreal's actual URL parser:

```sh
/Users/webproduktion3/UE_5.8/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor "$PWD/ValStrike.uproject" -nullrhi -nosound -unattended '-ExecCmds=Automation RunTests ValStrike.UI.ServerAddress' '-TestExit=Automation Test Queue Empty' "-abslog=$PWD/Saved/Tests/UI/address-automation.log"
```

Require `Result={Success}` for `ValStrike.UI.ServerAddress`. The existing engine startup reports two unrelated `Condition failed` automation messages before this suite; they are not a suite pass/fail result.

Controls: Escape opens/resumes; Tab/Shift+Tab moves focus; Enter submits the address or activates a button; mouse buttons use standard Slate controls. Escape cancels an in-progress connection. Cancelling returns to a fresh local arena because Unreal may already have disconnected the previous server. A listen host's disconnect button says **End hosted session**. Opening a menu never pauses the world.

Remaining integration gates include damage/death HUD transitions, respawn while a menu is open, a real second player's activity during the menu, server shutdown, and manual mouse use. This focused test proves connected HUD responses and the menu/combat input boundary; it is not a separate server-side combat-authority test. IPv6 is explicitly unsupported in this foundation. Settings, audio, round/score UI, packaged builds, cross-machine play and latency/loss remain separate gates.

## Settings screen checks

Escape → **Settings** (Tab to it or click it) opens a second page of the same native Slate menu:

- **Mouse sensitivity**: slider (Left/Right step 0.01) plus a numeric field (Enter commits; a comma decimal is accepted). Range 0.02–1.00, default 0.12, stored with 0.001 precision. Out-of-range numbers clamp; non-numbers are rejected with an error and the previous value stays. A change applies at once to the local player's look (`AVSCharacter::LookYaw/LookPitch` read it from `AVSPlayerController`). It is client-local and never replicated.
- **Window mode / resolution**: Fullscreen, Windowed Fullscreen or Windowed, plus a resolution (engine's convenient window sizes for Windowed, the display's modes for Fullscreen, the desktop size for Windowed Fullscreen). Left/Right or the ‹ › arrows change the pending choice; nothing changes until **Apply display**, which uses `UGameUserSettings` (`SetFullscreenMode`, `SetScreenResolution`, `ConfirmVideoMode`, `ApplyResolutionSettings(false)`, `SaveSettings`).
- **Persistence**: sensitivity lives in `[ValStrike.UserSettings] MouseSensitivity=` of this machine's `GameUserSettings.ini`, saved through `UGameUserSettings::SaveSettings` when the player leaves the page, closes the menu, quits or travels. Saving through the engine object also stores its settings version; Unreal deletes a version-less GameUserSettings file at startup. `Config/DefaultEngine.ini` is unchanged. Missing or malformed stored values fall back to 0.12.
- Keyboard: Tab/Shift+Tab move focus (a fixed Windowed Fullscreen resolution is skipped), Left/Right change the focused slider/selector, Enter activates, Esc returns to the session page and a second Esc resumes.

Unit tests (`ValStrike.UI.Settings`) cover clamp, parse, format round trip, file persistence through a temporary `FConfigFile`, default fallback, the character fallback default, the window-mode cycle and resolution list handling. They never touch the running process's GameUserSettings:

```sh
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/ValStrike.uproject" -nullrhi -nosound -unattended '-ExecCmds=Automation RunTests ValStrike.UI.Settings' '-TestExit=Automation Test Queue Empty' "-abslog=$PWD/Saved/Tests/UI/settings-automation.log"
```

The rendered runner starts two sequential game processes with an isolated `-UserDir` under `Saved/Tests/Settings/<run-id>/user`, so the checkout's own `Saved/Config` is never read or changed:

```sh
python3 Scripts/with_ue_slot.py -- python3 Tests/UI/test_settings.py --engine "$UE_ROOT"
```

1. **first** (`-windowed -ResX=960 -ResY=540`): checks the unset default (0.12, injected look of 1000 counts turns 8.4°), opens the menu with Escape, reaches Settings by Tab and opens it with a Slate mouse click, steps the slider with the arrow keys, types `5` (clamps to 1.000), `abc` (rejected) and `0.36`, applies Windowed Fullscreen by keyboard, then Windowed 1280×720 via the mouse arrow and keyboard, returns with Esc twice and checks the same injected look now turns 25.2° (ratio 3.0).
2. The runner checks the saved `GameUserSettings.ini` on disk.
3. **restart** (no window arguments): the saved Windowed 1280×720 window and 0.36 sensitivity must be restored, the look must turn 25.2°, and the Settings page must show both.

`--exclusive-fullscreen` also applies exclusive Fullscreen. On macOS Unreal captures the display (`CGDisplayCapture`) and may switch its display mode, so it is opt-in. Mouse clicks are synthesized through `FSlateApplication` at the widget's painted centre (real Slate hit-testing; the OS cursor is not moved). Screenshots and `result.json` are written under `Saved/Tests/Settings/<run-id>/`.
