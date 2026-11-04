# Task GPF-02: fix INT-06-D2, where a connect to an unreachable server can close without an error state

- Status: In progress
- Defect: INT-06-D2 in `docs/workstreams/integration-06.md` (PR #77). `Tests/UI/test_session.py --rendered` run 1 failed 65/72. The connect to the unreachable `127.0.0.1:7814` closed after 8.4 s **without** `BroadcastNetworkFailure`, so `bError` was never set. `VSUISmoke` stage 14 (`Source/ValStrike/UI/VSUISmoke.cpp:349`) waited until the 80 s deadline, and 3 more checks cascaded. Passing runs instead hit the 20 s `ConnectionTimeout`, which broadcasts. No UI, `Tests/UI` or `ue.py` change since INT-05, so this is pre-existing and intermittent.
- Evidence (read-only, QA checkout): `.parallel-worktrees/team-integration/Saved/Tests/UI/20261104T140749Z_181840/` (fail) versus `…20261104T141019Z_657bbf/` (pass).
- Owner/session: Claude session "GPF-02 session connect failure"
- Branch and base: `claude/gpf-02-session-connect-failure` from current `origin/main`, worktree `.parallel-worktrees/gpf-02-session-connect-failure`
- Owns: the connect and failure handling in `Source/ValStrike/UI/VSSessionState.*` and in `SVSSessionMenu.*` (not its settings section), `Source/ValStrike/UI/VSUISmoke.*`, the session section of `Tests/UI/README.md`, and a new `docs/workstreams/session-connect-failure.md`. The settings code, `VSSettingsSmoke.*` and `Scripts/ue.py` are read-only.
- Result:
  - Find from the evidence logs how the pending connection closed without a network-failure broadcast. Candidates: a `NetDriver` closing on an ICMP port-unreachable, a travel failure, a `PendingNetGame` error, or `HandleDisconnect`.
  - Fix the **game**: every way a connect attempt ends without connecting must leave `bConnecting=false` and `bError=true` with a status. Hook the engine's `TravelFailure` as well as `NetworkFailure`, or detect that the pending net game went away. Players would otherwise see a menu stuck at "Connecting…".
  - Don't loosen the stage-14 check or its deadline.
  - If the root cause can only be shown at runtime, say exactly what integration should look for.
- Validation tier: V1. Contributor checks: the existing engine-free `VSServerAddress`/session tests if applicable; `git diff --check`. No Unreal build.
- Integration-owned checks: Editor build; `Tests/UI/test_session.py --rendered` ×3 (72/72 each); `Tests/UI/test_join_key.py` if it exists.
- Publication: push and open a PR; the user merges.
