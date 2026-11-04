# GPF-02: session connect failure (INT-06-D2)

- Status: Implementation ready, verification pending (V1). No Unreal build or run by this worker.
- Branch: `claude/gpf-02-session-connect-failure` from `origin/main` (30997f4).
- Evidence read, read-only: `team-integration/Saved/Tests/UI/20261104T140749Z_181840/` (fail) and `…20261104T141019Z_657bbf/` (pass).

## Root cause

The 8.4 s close was a `CancelPending` from game code (`AVSPlayerController::CancelConnection`). It was not an engine failure path. The fail log's 8.4 s close has exactly the signature of the harness's own cancel 9 s earlier, and none of the signature of a failure:

| | Cancel (stage 131), fail log 1579–1584 | Unreachable attempt (stage 14), fail log 1628–1633 | Timeout, pass log 1628–1641 |
|---|---|---|---|
| Preceding warning | none | none | `Connection TIMED OUT` (1628) |
| `UEngine::BroadcastNetworkFailure` | none | **none** | yes (1629) |
| `UNetConnection::Close` close reason | no ` - Result=` line | **no ` - Result=` line** | ` - Result=ConnectionTimeout` (1634) |
| Pending driver destroyed | same frame as Close [621] | same frame as Close [163] | next frame |
| `Browse …?closed` | next frame [622] | next frame [164] | next frame |
| `TravelFailure:` log | none | none | none |

In UE 5.8.4, `UEngine::CancelPending(FWorldContext&)` (`UnrealEngine.cpp:15864`) is what calls `ServerConnection->Close()` with no reason and then destroys the pending driver at once. `?closed` in the next frame is `HandleDisconnect(World)` → `SetClientTravel` (`UnrealEngine.cpp:15527`). That is exactly what `CancelConnection` does. The engine's other `CancelPending` callers each log something first. The `TickWorldTravel` connection-error path broadcasts (`:16017`). `HandleTravelFailure` logs `TravelFailure:` (`:15217`). The `CANCEL` console command doesn't travel. The ICMP explanation is ruled out too: `test_session.py` holds a bound, silent UDP socket on 7814, so no port-unreachable packet is generated, and `UIpNetDriver::TickDispatch` ignores `SE_UDP_ERR_PORT_UNREACH` anyway (`IpNetDriver.cpp:1102`).

In this sequence the smoke harness calls `CancelConnection` only at stage 131. The only other callers are the menu's **Escape** handler and its **Cancel connection** button, which has focus while connecting. So the stage-14 attempt was cancelled through the menu by input the harness did not send. Outside input is confirmed afterwards. At fail log 1658–1659 the menu closes (`MouseCaptureMode … -> CapturePermanently` with no matching revert), even though no harness stage closes it there. That is the second Escape after a cancel. At 1660 `HandleMouseConnected: physical mouse connection` follows. The passing log has neither. A cancel deliberately sets `bConnecting=false`, `bError=false`, "Connection cancelled…" (`cancel_feedback_survives_travel` requires this), so stage 14 waited until the 80 s deadline.

This is an inference from the log signature, because the old build did not log the cancel source. The new logging below makes it provable.

## Fix (game, plus a precise harness diagnostic)

- `UI/VSSessionState.*`: a core-ticker watchdog, `WatchConnectAttempt`. While `bConnecting` is true, if the game instance's world context has no `PendingNetGame` and no queued `TravelURL`, and its world has no server connection, for two consecutive ticks, the attempt ended without a broadcast. It calls `ShowFailure` with "The connection attempt ended before it completed. Check the server address and port, then connect again." and logs a `LogValStrike` warning. This covers any silent engine teardown of a pending net game, alongside the existing `NetworkFailure` and `TravelFailure` handlers. Success is unaffected: the new client world owns the server connection until the new controller clears `bConnecting`. A player cancel is unaffected, because it clears `bConnecting` first.
- `UI/SVSSessionMenu.*`: Escape and the Cancel button go through `CancelConnection(Source)`, which logs `Session menu: connection cancelled by Escape key|Cancel button.` The harness's direct call doesn't log, so the source is unambiguous.
- `UI/VSUISmoke.cpp` stage 14: if the unreachable attempt ends as a non-error (only a cancel does that), log `VSUI unreachable attempt ended without an error: <status>` and fail at once with `unreachable_attempt_reports_error`. The run still fails, and fails sooner. No check or deadline was loosened, and the passing check set is unchanged.
- `Tests/UI/README.md`: one paragraph describing the three outcomes and the new diagnostic.

User cancel intentionally stays a non-error. Every other way an attempt can end without connecting now leaves `bConnecting=false`, `bError=true` and a status.

## Checks

- Run: `git diff --check` (clean). Engine APIs checked against UE 5.8.4 source: `FTSTicker`, `UGameInstance::GetWorldContext`, `FWorldContext::{PendingNetGame,TravelURL}`, `UNetDriver::ServerConnection`, `SetClientTravel` sets `TravelURL` synchronously (`UnrealEngine.cpp:15614`).
- Deferred to integration: Editor build; `Tests/UI/test_session.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --rendered` ×3, 72/72 each; `Tests/UI/test_join_key.py` if present.

## What integration should look for

- Keep the rendered driver window free of keyboard and mouse input during the run. Nobody at the Mac, no Screen Sharing or remote session attached.
- If stage 14 fails again: `Session menu: connection cancelled by …` in `driver.log` means outside input, not a game defect. `Connection attempt to … ended without a network or travel failure` means the watchdog caught a real silent engine path. The run should then pass stage 14/15 with the "ended before it completed" status. Report that path's preceding log lines.

## Risks

- The watchdog's "in flight" test relies on engine defaults that ValStrike doesn't override. `Browse` shuts down the old world's net driver before a new pending connection (`ShouldShutdownWorldNetDriver`, `UnrealEngine.cpp:15803`), so a connect made while already connected is covered too. If someone later binds `OnOverrideBrowseURL` or `OnOverridePendingNetGameUpdate`, re-check this.
- Runtime behaviour is unverified until integration runs the suite.
