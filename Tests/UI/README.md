# HUD/session checks

These checks use the Mac **development editor** executable, not a packaged client or dedicated server. Run from this checkout with the pinned Unreal 5.8.4 / Xcode 26.1.1 toolchain.

```sh
python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
python3 Tests/UI/test_session.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --rendered
```

The runner starts its own loopback server on UDP 7813 and a client initially in the local arena. It holds a silent UDP socket on 7814 for the expected connection timeout. Use `--port` / `--failure-port` if occupied. Only the runner's processes are closed. Omit `--rendered` for null rendering; rendered mode captures native 960×540 screenshots. All outputs go to ignored `Saved/Tests/UI/<run-id>/`, including exact process commands, engine version, source hashes, individual checks and `result.json`. Exit zero requires complete passing evidence, including actual client termination after Quit.

`-VSUISmoke` enables the C++ harness only in development builds; normal launches do not run it, and Shipping/Test exclude its implementation. The harness exercises controller input, actual Slate focus/navigation, validated travel, real client connection, input isolation during a menu, disconnect, cancellation and timeout recovery. The baseline has unavailable combat data; the test does not inject health/ammo or simulate a match. The engine's configured connection timeout is used unchanged.

The native address automation suite also checks accepted values against Unreal's actual URL parser:

```sh
/Users/webproduktion3/UE_5.8/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor "$PWD/ValStrike.uproject" -nullrhi -nosound -unattended '-ExecCmds=Automation RunTests ValStrike.UI.ServerAddress' '-TestExit=Automation Test Queue Empty' "-abslog=$PWD/Saved/Tests/UI/address-automation.log"
```

Require `Result={Success}` for `ValStrike.UI.ServerAddress`. The existing engine startup reports two unrelated `Condition failed` automation messages before this suite; they are not a suite pass/fail result.

Controls: Escape opens/resumes; Tab/Shift+Tab moves focus; Enter submits the address or activates a button; mouse buttons use standard Slate controls. Escape cancels an in-progress connection. Cancelling returns to a fresh local arena because Unreal may already have disconnected the previous server. A listen host's disconnect button says **End hosted session**. Opening a menu never pauses the world.

Integration must additionally exercise held fire release, replicated health/ammo/reload/death, respawn while a menu is open, a real second player's activity during the menu, server shutdown, and manual mouse use. IPv6 is explicitly unsupported in this foundation. Settings, audio, round/score UI, packaged builds, cross-machine play and latency/loss remain separate gates.
