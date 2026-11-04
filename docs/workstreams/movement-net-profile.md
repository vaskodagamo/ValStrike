# Movement net-profile workstream

## 2026-11-04 — GPT-12: `test_movement.py` accepts `--net-profile` and records correction evidence

**Branch/base:** `claude/gpt-12-movement-net-profile` from `b342ca9`. Brief: `docs/tasks/gameplay/GPT-12-movement-net-profile.md` (on `claude/gameplay-board-int06`). MOVE-03 enabler. Validation tier V1. Status: implementation ready, verification pending.

- **Commit/PR:** see the PR for `claude/gpt-12-movement-net-profile`.
- **Changed:**
  - `Scripts/test_movement.py`:
    - Accepts `--net-profile {lan,good,typical,poor}` and `--pkt-lag/--pkt-lag-variance/--pkt-loss` through `ue.add_net_emulation`. The switches come from `ue.net_emulation_switches`, so `ue.py` stays the single source of composition and bounds. The same switches go to the server, driver and observer. Variance above lag is rejected before launch.
    - With emulation, `-ExecCmds` also sets `p.NetShowCorrections 1`, an engine Development-only cvar. With no flag the argv is unchanged.
    - A run is **invalid** (exit 1, `Invalid network emulation run (requested …): <role>: …`) unless each role's abslog has `LogNet: PktLag/PktLagVariance/PktLoss set to N` lines, all equal to the requested values.
    - The `net` block in `result.json` holds the profile and overrides, the requested and observed values, `rtt_ms` and per-role `corrections` (`{count, max_error_cm, by_character}` or `"unavailable"`). `conditions` now names the emulated lag and loss.
    - `evaluate()` and every tolerance are unchanged.
    - Launch argv and the log parsing are now small functions (`build_parser`, `net_emulation`, `launch_command`, `emulation_problem`, `corrections`, `net_summary`) so they can be tested without an engine. `stop`, `records` and `evaluate` keep their signatures for the UI runners that import them.
  - `Tests/Movement/test_movement_runner.py` (new, no engine): 13 tests. They cover argument parsing and rejection, that the composed values match `ue.py` for every profile and override, the per-role argv (the default argv is byte-identical), and invalid-run detection: missing log, missing line, wrong value, conflicting repeated values, lan zeros, and lookalikes such as `PktLagMin` or the wrong category. They also cover correction counting with and without the cvar confirmation and the `net` summary shape.
  - `docs/playtest/net-emulation.md`: new "Runner adoption" section. The "no runner uses these flags" limit was corrected to name this runner.
- **Engine facts used (UE 5.8.4 source at `/Users/webproduktion3/UE_5.8/UE_5.8/Engine/Source/Runtime/`; read only, nothing built):**
  - `Engine/Private/Net/NetEmulationHelper.cpp:550,570,580` log `PktLoss/PktLag/PktLagVariance set to %d` under `LogNet`.
  - `Engine/Private/Components/CharacterMovementComponent.cpp:420-426`: `p.NetShowCorrections` is `ECVF_Cheat` and compiled out of Shipping. With it on, `:10452` logs the server warning `*** Server: Error for %ls at Time=%.3f is %3.3f …` and `:11423` logs the client warning `*** Client: Error for …`.
  - `Core/Private/HAL/ConsoleManager.cpp:3230`: setting a cvar from the console echoes `name = "value"` to the log. The runner uses that echo to confirm correction logging was on, so a zero count is a real zero.
- **RTT:** `"unavailable"` for both clients. The movement smoke (`Source/ValStrike/Tests/VSMovementSmoke.cpp`) logs no ping, and adding it would need a source change, which is out of scope. Use `stat net` per the MOVE-03 procedure.
- **Corrections:** these come from engine log lines only, with no source change. The server count is the server's correction decisions; the driver count is corrections the client applied. The observer has none (it is a simulated proxy) unless its own idle pawn is corrected.
- **Checks that may be latency-sensitive (not loosened; integration should watch them under `typical`/`poor`):**
  - Sample `t` is `GetServerWorldTimeSeconds() - StartTime` on each process. The driver and observer see the server's state about one one-way lag late, and the observer's view of the driver is about two lags late. Every fixed time window can shift by up to about 180 ms (`poor`, ±60 ms jitter) on the client roles. The narrowest windows are the most exposed:
    - `return to standing` (7.5-7.9 s, at least 3 samples)
    - `crouch state and capsule` / `crouch move speed` (6.0-6.8 s)
    - `jump and land` (jump 8.0-9.5 s, landing 10.0-10.8 s)
    - `boundary blocks movement` (16.5-17.5 s)
    - `cover blocks movement` (30.0-31.0 s)
  - Loss can drop replicated updates and leave fewer samples than a window's minimum count, which affects all `len(...) >= N` sample-count conditions on the observer.
  - `driver: camera follows crouch` (eye height within 1 cm) relies on client-predicted crouch staying in phase with replicated state. A correction during crouch could break it.
- **Checks actually run:**
  - `python3 -m unittest Tests/Movement/test_movement_runner.py`: 13 tests, OK.
  - `python3 -m unittest Tests/test_ue.py`: OK (unchanged `ue.py`).
  - `python3 -m py_compile Scripts/test_movement.py Tests/Movement/test_movement_runner.py`: OK.
  - The UI runners' `from test_movement import stop`, `records` and `evaluate` still import. `test_movement.py --help` lists the network-emulation group.
  - `git diff --check`: clean.
  - No Unreal build or launch.
- **Deferred (integration-owned, later INT batch):**
  - Run `python3 Scripts/test_movement.py` with no flag and confirm the 45/45 baseline is unchanged.
  - Then `--net-profile lan`, `good`, `typical` and `poor`. Record each `result.json` `net.corrections` and the pass or fail of the checks listed above.
  - Confirm on the first emulated run that the abslogs contain the `LogNet: Pkt* set to` lines and `p.NetShowCorrections = "1"`. If the echo is absent, corrections stay `"unavailable"` and the run is still judged on the movement checks.
- **Risks:**
  - Enabling `p.NetShowCorrections` also queues debug capsule draws, which do nothing under `-nullrhi`. It changes the emulated-run argv relative to the no-flag baseline, so compare `--net-profile lan` with the other profiles.
  - The `--net-profile` help text, inherited from `ue.py`, says "for this process". In this runner it applies to all three processes.
  - A `poor` run may need a larger `--timeout`.
- **Next action/owner:** the gameplay lead reviews the PR, the user merges it, and integration runs the deferred profile matrix.
