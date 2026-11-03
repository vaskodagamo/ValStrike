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
