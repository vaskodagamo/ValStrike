# Network emulation workstream

## 2026-11-04 — GPT-03: repeatable latency/loss profiles for development server/client runs

**Branch/base:** `claude/gpt-03-net-emulation` from `90849f5`. Brief: `docs/tasks/gameplay/GPT-03-net-emulation.md` (on `claude/gameplay-team-kickoff`). Enabler for MOVE-03 and GUN-04. Validation tier V1 (tooling).

- **Commit/PR:** `aaa0e2c` on `claude/gpt-03-net-emulation`; PR https://github.com/vaskodagamo/ValStrike/pull/36.
- **Changed:**
  - `Scripts/ue.py`: `server` and `client` gain an optional "network emulation" group: `--net-profile {lan,good,typical,poor}`, `--pkt-lag 0-1000`, `--pkt-lag-variance 0-500`, `--pkt-loss 0-100`. Any of them appends `-PktLag=N -PktLagVariance=N -PktLoss=N` after the existing argv; explicit values override the profile. Variance above lag is rejected (`SetupError`); negatives, fractions and loss over 100 are rejected by the parser. No flag leaves argv byte-for-byte unchanged. Profiles are one-way per process (`NET_PROFILES`): lan 0/0/0, good 20/5/0, typical 40/10/1, poor 90/30/3.
  - `Tests/test_ue.py`: two cases — opt-in composition for both actions (default unchanged, profile suffix, override, explicit-only, variance-without-lag error) and out-of-range rejection.
  - `docs/playtest/net-emulation.md` (new): profile table, engine semantics with source citations, the MOVE-03 measurement procedure (RTT, loss, corrections, stuck, divergence) and limits.
- **Engine verification (UE 5.8.4 pinned source, `Engine/Source/Runtime/Engine/`):** fields `Classes/Engine/NetDriver.h:471,519,528`; `DO_ENABLE_NET_TEST = !(UE_BUILD_SHIPPING)` at `NetDriver.h:450` (compiled out of Shipping); command line overrides ini in `Private/NetDriver.cpp:786-788`; `-PktLag=` style parsing and `LogNet: PktLag set to N` logs in `Private/Net/NetEmulationHelper.cpp:534-626`; loss clamp `:472`; lag ± variance only when `PktLag` non-zero at `Private/NetConnection.cpp:2666-2673`, in-order release `:5184-5188`; loss at `:2709-2714`.
- **Checks actually run and result:**
  - `python3 -m unittest discover -s Tests -p test_ue.py -v`: 33 tests, OK.
  - `UE_ROOT=… python3 Scripts/ue.py --dry-run server --net-profile poor` and `client --net-profile poor`: argv ends with `-PktLag=90 -PktLagVariance=30 -PktLoss=3`; plain `server`/`client` dry runs have no `-Pkt*` switch. `client --pkt-lag 50 --pkt-lag-variance 60` exits 1 with the variance error; `server --pkt-loss 101` exits 2.
  - `git diff --check`: clean.
  - No Unreal launch (engine slot belongs to INT-01).
- **Deferred checks and integration owner:** INT-01 / integration, in a later MOVE-03 batch: one real server + client run with `--net-profile typical`, confirming each process log shows `LogNet: PktLag set to 40`, `PktLagVariance set to 10`, `PktLoss set to 1`, and `stat net` ping rises by roughly 80 ms over the `lan` baseline. Then the MOVE-03 procedure in `docs/playtest/net-emulation.md`.
- **Remaining defect/dependency:** none known. Emulation is outgoing-only per process (both sides need the profile); no bandwidth or burst-loss model. Runners in `Scripts/test_*.py` do not use the flags yet (out of scope).
- **Proposed `docs/SETUP.md` lines (coordinator applies):**

  > Impaired-network runs (Development editor only; packet simulation is compiled out of Shipping): add the same `--net-profile {lan,good,typical,poor}` to `python3 Scripts/ue.py server` and each `client`, or set `--pkt-lag MS --pkt-lag-variance MS --pkt-loss PCT` explicitly. Check the process log for `LogNet: PktLag set to …`. Profiles and the MOVE-03 procedure: `docs/playtest/net-emulation.md`.

- **Next action/owner:** gameplay lead reviews the PR; user merges; integration runs the deferred `typical` check with the MOVE-03 batch.
