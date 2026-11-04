# Task GPT-03: repeatable latency/loss profiles for development server/client runs

- Status: In progress
- Backlog ID: MOVE-03 enabler (also needed by GUN-04)
- Owner/session and provider: Claude session "GPT-03 network emulation profiles"
- Lead: Claude gameplay lead (`team-gameplay`)
- Branch and exact base SHA: `claude/gpt-03-net-emulation` from `90849f5`
- Absolute worktree path: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/gpt-03-net-emulation`
- Owns:
  - `Scripts/ue.py`: additive optional flags on the `server`/`client` parsers and their argv composition only
  - `Tests/test_ue.py`: new cases only
  - new `docs/playtest/net-emulation.md`
  - `docs/workstreams/net-emulation.md`
- Read-only dependencies / other owners:
  - every `Scripts/test_*.py` runner and `Scripts/with_ue_slot.py`
  - all `Source/`, `Config/`, `Content/`
  - central docs, including `docs/SETUP.md` (propose the SETUP lines in the handoff)
- Result:
  - `ue.py server|client --net-profile {lan,good,typical,poor}` and/or explicit `--pkt-lag MS --pkt-lag-variance MS --pkt-loss PCT`.
  - Composed into the engine's documented packet-simulation options (UE 5.8 `PktLag`/`PktLagVariance`/`PktLoss` launch settings or `NetEmulation.*` cvars). Verify the exact names and semantics against the pinned engine source under `/Users/webproduktion3/UE_5.8/UE_5.8/Engine/Source` (`PacketSimulationSettings`); cite file:line in the handoff.
  - Bounded validation: reject negative values and loss over 100.
  - The default (no flag) leaves argv unchanged.
  - The Development/Editor-only limitation is stated: packet simulation is compiled out of Shipping.
  - `docs/playtest/net-emulation.md` gives the profile table and the MOVE-03 measurement procedure (what to record: RTT, corrections, stuck/divergence) for integration to run later.
  - Stop when committed, the PR is open and the handoff is written.
- Out of scope:
  - changing existing runners to use profiles
  - OS-level traffic shaping (dummynet/pfctl)
  - running any measurement
  - lag compensation
- Validation tier: V1 (tooling)
- Contributor checks: `python3 -m unittest discover -s Tests -p test_ue.py -v`, `ue.py --dry-run server|client --net-profile poor` argv inspection, `git diff --check`. No Unreal launches.
- New test, justified: argv composition and bounds are the regression surface; extend the existing `Tests/test_ue.py`.
- Integration-owned checks: one actual server+client run with `--net-profile typical`, confirming the engine logs the simulated settings (in a later MOVE-03 batch).
- Engine slot: none.
- Publication: owner pushes the branch and opens a PR; the user merges.

## Completion handoff

- Commit/PR:
- Changed:
- Checks actually run and result:
- Deferred checks and integration owner:
- Remaining defect/dependency:
- Next action/owner:
