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

Every connect attempt ends in exactly one of: connected, a player cancel (non-error status; the driver log records `Session menu: connection cancelled by Escape key` or `… by Cancel button`), or an error status. If the engine ends a pending connection without a network or travel failure, `UVSSessionState` reports "The connection attempt ended before it completed…" and logs a `LogValStrike` warning. Stage 14 fails at once with `unreachable_attempt_reports_error` if the unreachable attempt ends as a non-error. The harness sends no input then, so this means outside input reached the rendered driver window.

Remaining integration gates include damage/death HUD transitions, respawn while a menu is open, a real second player's activity during the menu, server shutdown, and manual mouse use. This focused test proves connected HUD responses and the menu/combat input boundary; it is not a separate server-side combat-authority test. IPv6 is explicitly unsupported in this foundation. Settings, audio, round/score UI, packaged builds, cross-machine play and latency/loss remain separate gates.

## Settings screen checks

Escape → **Settings** (Tab to it or click it) opens a second page of the same native Slate menu:

- **Mouse sensitivity**: slider (Left/Right step 0.01) plus a numeric field (Enter commits; a comma decimal is accepted). Range 0.02–1.00, default 0.12, stored with 0.001 precision. Out-of-range numbers clamp; non-numbers are rejected with an error and the previous value stays. A change applies at once to the local player's look (`AVSCharacter::LookYaw/LookPitch` read it from `AVSPlayerController`). It is client-local and never replicated.
- **Master volume**: slider (Left/Right step 5 %) plus a numeric field (Enter commits; a trailing `%` is accepted). Range 0–100 %, default 100, stored in whole percent and snapped to 5 %. Out-of-range numbers clamp; non-numbers are rejected with an error. A change applies at once through the local world's audio device (`FAudioDevice::SetTransientPrimaryVolume`, linear gain), which scales every sound that device plays, so gameplay audio needs no shared sound class. `UEngine::LoadMap` resets that device volume to 1, so the menu reapplies the saved value whenever a local player's menu is built for a new world. Client-only; a dedicated server is never touched.
- **Window mode / resolution**: Fullscreen, Windowed Fullscreen or Windowed, plus a resolution (standard 16:9/16:10 sizes from 1280×720 that fit the work area, plus the current window size, for Windowed; the display's modes for Fullscreen; the desktop size for Windowed Fullscreen). Left/Right or the ‹ › arrows change the pending choice (clicking the value also advances it); nothing changes until **Apply display**, which uses `UGameUserSettings` (`SetFullscreenMode`, `SetScreenResolution`, `ConfirmVideoMode`, `ApplyResolutionSettings(false)`, `SaveSettings`).
- **Persistence**: sensitivity and volume live in `[ValStrike.UserSettings] MouseSensitivity=` / `MasterVolume=` of this machine's `GameUserSettings.ini`, saved through `UGameUserSettings::SaveSettings` when the player leaves the page, closes the menu, quits or travels. Saving through the engine object also stores its settings version; Unreal deletes a version-less GameUserSettings file at startup. `Config/DefaultEngine.ini` is unchanged. Missing or malformed stored values fall back to 0.12 and 100 %. Volume is saved when the player leaves the Settings page.
- Keyboard: Tab/Shift+Tab move focus (a fixed Windowed Fullscreen resolution is skipped), Left/Right change the focused slider/selector, Enter activates, Esc returns to the session page and a second Esc resumes.

Unit tests (`ValStrike.UI.Settings`) cover sensitivity and volume clamp, parse, format round trip, file persistence through a temporary `FConfigFile`, default fallback, the character fallback default, the window-mode cycle and resolution list handling. They never touch the running process's GameUserSettings:

```sh
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" "$PWD/ValStrike.uproject" -nullrhi -nosound -unattended '-ExecCmds=Automation RunTests ValStrike.UI.Settings' '-TestExit=Automation Test Queue Empty' "-abslog=$PWD/Saved/Tests/UI/settings-automation.log"
```

The rendered runner starts two sequential game processes with an isolated `-UserDir` under `Saved/Tests/Settings/<run-id>/user`, so the checkout's own `Saved/Config` is never read or changed:

```sh
python3 Scripts/with_ue_slot.py -- python3 Tests/UI/test_settings.py --engine "$UE_ROOT"
```

1. **first** (`-windowed -ResX=960 -ResY=540`): checks the unset default (0.12; an injected look of 1000 MouseX/MouseY counts turns 8.4°), opens the menu with Escape and Settings with Tab+Enter, checks that the page fits the 960×540 window without scrolling, steps the slider with the arrow keys, types `5` (clamps to 1.000), `abc` (rejected with an error) and `0.36`, tabs to the volume slider and steps it to 95 % with Left, tabs through the volume field to the window mode, applies Windowed Fullscreen by keyboard, switches back to Windowed with a mouse click on the window-mode arrow, picks 1280×720 with the arrow keys and applies it, then uses the mouse for Back and Settings, returns with Esc twice and checks that the same injected look now turns 25.2° (ratio 3.0).
2. The runner checks the saved `GameUserSettings.ini` on disk (sensitivity `0.360`, volume `95`, Windowed 1280×720, engine settings version).
3. **restart** (no window arguments): the saved Windowed 1280×720 window, 0.36 sensitivity and 95 % volume must be restored, the look must turn 25.2°, and the Settings page must show them.

Look checks measure the rotation change across the injection over three frames. Physical mouse or trackpad input that reaches the game window during a run can still disturb them (one such run measured 14.6°/4.4° with camera and control disagreeing); leave the pointer alone while a rendered runner is active.

Mouse clicks are synthesized through `FSlateApplication` at the widget's desktop-space centre, so they use real Slate hit-testing without moving the OS cursor. On macOS, Unreal's `FMacWindow::IsPointInWindow` rejects every point while the window is on an inactive Space, which is the case for a window launched behind another full-screen app. The runner therefore does its windowed mouse checks after the Windowed Fullscreen round trip, when macOS has activated the game, and logs `hit_testable` for every click. Slate also grants mouse capture only while the application is active (`FSlateApplication::ProcessReply`), and a default `SButton` clicks on release only while it holds capture. Synthesized events do not activate the app the way a real click does, so a game left behind another app (common with two displays) had its buttons pressed but never clicked. `Click()` therefore allows capture while inactive (`SetHandleDeviceInputWhenApplicationNotActive`) for its own press and release only, then restores the setting. Each click also logs `VSSET_CLICK_TRACE` with `app_active`, captor, focus, hit-test target, hover, captor after press, and the pending window mode before and after.

`--exclusive-fullscreen` also applies exclusive Fullscreen. On macOS Unreal captures the display (`CGDisplayCapture`) and may switch its display mode, so it is opt-in. Screenshots, logs, the saved ini and `result.json` are written under `Saved/Tests/Settings/<run-id>/`.

## Private-server join key

The session page has an optional, masked **Join key** field beside the server address (Tab order: address → join key → Connect). Empty means no key. Otherwise it must match the server's rule, 8–64 letters, digits, `-` or `_`; surrounding spaces are trimmed. The client reuses the server's own `ValStrike::Server::IsValidJoinKey` through the engine-free `Source/ValStrike/UI/VSJoinKey.*`, so the two cannot drift. An invalid key shows a status-line error and does not travel. `AVSPlayerController::ConnectToServer(Address, JoinKey)` validates the address unchanged and appends `?JoinKey=<key>` only to the validated `host:port`. A key typed into the address field is refused with a hint to use the key field. When a server started with `-VSServerJoinKey` refuses a join, the status line reads "Join key missing or wrong. Enter the key the host gave you, then connect again." The key is kept in game-instance memory only, so the field survives travel. It is never saved, never shown unmasked and never logged by ValStrike code. Address and key fields keep keyboard focus after a rejected Enter.

Unreal itself writes the travel URL, with its options, to the client log (`Browse:`, `LoadMap:`), and the server writes the login options and its command line. So a joined client's and a keyed server's local logs contain the key. Keeping it out of the client's lines would need a `ULocalPlayer::GetGameLoginOptions()` override, which means setting `LocalPlayerClassName` in `DefaultEngine.ini`; that is a central change outside this stream.

Engine-free rule tests (system C++ compiler; set `VS_JOIN_KEY_SANITIZE=1` for ASan/UBSan):

```sh
python3 -m unittest discover -s Tests/UI -p 'test_join_key_rule.py' -v
```

Native automation `ValStrike.UI.JoinKey` covers the FString wrappers, key-in-address refusal and the composed URL through Unreal's `FURL` parser and the server's `ParseOption`. Rendered check against a real keyed server (loopback port 7883 by default):

```sh
python3 Scripts/with_ue_slot.py -- python3 Tests/UI/test_join_key.py --engine "$UE_ROOT"
```

The runner starts `ue.py server --join-key lan-Key_0001` and a 960×540 client whose `-VSJoinSmoke` harness gets the key from the `VSJOINSMOKE_KEY` environment variable, never its command line. The harness drives the real menu: an invalid key and a key in the address are refused before travel, a keyless join is refused by the server, and the key typed into the masked field joins. Afterwards every text artifact is scanned. A key in a ValStrike log line or in `result.json` fails the run; Unreal's own occurrences are counted per file and line kind, then redacted. Evidence goes to `Saved/Tests/JoinKey/<run-id>/`.
