# Mac source tooling rehearsal — 2026-11-03

SET-05 tooling evidence, Europe/Berlin. This rehearsal uses local tooling commit `cb891e62558ccfc51075bc239863a7b238401692` on common foundation `a520ff20fba6bb9a94d0fc6ae2c59acff3d94606`. It is not a published integrated match revision. Later documentation-only commits do not change the tested launch code.

## Verified

| Check | Evidence |
| --- | --- |
| Python checks | 28 helper + 24 preflight + 12 session tests pass; syntax and whitespace checks pass |
| Exact tools | Apple silicon arm64, UE 5.8.4 changelist 58902052, Xcode 26.1.1 build 17B100, macOS SDK 26.1, executing Metal compiler, Git LFS 3.8.1 |
| Native loading | Assigned implementation working tree before the tooling commit: compiled VSGameMode, Site01 world and 18 hard dependency packages loaded successfully in an actual commandlet |
| Independent clone | `git clone --no-local --no-checkout --single-branch --branch codex/mac-playtest` from the local committed source; detached exact commit; no Git alternates or preexisting LFS object cache |
| Negative LFS check | Before retrieval, preflight rejected the unresolved Site01 map pointer |
| Real LFS retrieval | Changed clone origin to `https://github.com/vaskodagamo/ValStrike.git`; `git lfs pull origin` + `git lfs fsck` exited 0. 70 LFS files retrieved; all 31 native Content packages match HEAD pointer size/SHA256 |
| Independent build | No Binaries/Intermediate/Saved/DerivedDataCache existed before preparation; no generated outputs copied. Editor build ran 21 actions, succeeded in 30.70s, and recorded its own current build receipt |
| Server/two clients | Clean detached checkout; Site01 / practice / UDP 7861, two separate headless clients. Actual expected map travel and server mode/listener/join evidence passed; 25-second observation after connection, 42.455s whole runner |
| Default graybox regression | Map/mode options omitted: graybox/practice selected, two clients joined on UDP 7863, 5s observation; 21.043s whole runner, all three exited 0 and owned groups absent |
| Missing match guard | Actual compiled-class probe rejected unavailable VSMatchGameMode; helper exited 1 before its server RUN line, and no server log was created. No fallback server |
| Cleanup | Server and both clients exited 0 after intentional owned cleanup; all three owned process groups confirmed absent |

The source checkout came from an unpublished local commit so it could contain the new tooling without publishing another stream. The binary retrieval used the actual GitHub LFS endpoint and the clone's private `.git/lfs/objects`, not the source checkout's media cache. This exercises source transfer, binary retrieval and independent project compilation; it does not test another Mac's installation or credentials.

Heavy work used the assigned worktree's `Scripts/with_ue_slot.py`, including commands targeting the separate clone. The clone's independent slot was deliberately not used on this shared machine. Busy exit 75 attempts launched nothing and were retried after other useful work. No network/firewall settings were changed.

## Reproduce the launch checks

The [LAN checklist](MAC_LAN.md) covers independent Macs. For the shared development machine's exact rehearsal, run from the assigned checkout. Explicitly select its slot wrapper and the independent clone's runner:

```sh
python3 -m unittest discover -s Tests -p test_ue.py -q
python3 -m unittest discover -s Tests/Playtest -p 'test_*.py' -q

VS_SLOT_ROOT='/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/mac-playtest'
VS_PLAYTEST_ROOT="$VS_SLOT_ROOT/Saved/Playtest/clean-checkout"
VS_ENGINE_ROOT='/Users/webproduktion3/UE_5.8/UE_5.8'

python3 "$VS_SLOT_ROOT/Scripts/with_ue_slot.py" -- \
  python3 "$VS_PLAYTEST_ROOT/Tests/Playtest/run_session.py" --engine "$VS_ENGINE_ROOT" \
  host --map site01 --mode practice --port 7861 --clients 2 \
  --duration-seconds 25 --headless-clients
```

The independent clone must already have its own successful helper build receipt. Its own slot wrapper is appropriate only on a machine without another ValStrike repository coordinating Unreal jobs; otherwise use the existing shared wrapper as above.

## Evidence locations and remaining gates

Full machine logs are retained under the assigned checkout's ignored `Saved/Playtest/`. The independent clone is `Saved/Playtest/clean-checkout/`. Native preflight evidence is `Saved/Playtest/native/1d630960fa0146b08fee158cb207cf20/`; the clean-clone connection report and three process logs are `Saved/Playtest/clean-checkout/Saved/Playtest/20261103T171016Z_b163bcfe/`. The [compact committed evidence record](evidence/2026-11-03-source-rehearsal.json) retains identities, outcomes and log hashes.

Match runtime is absent from the tested foundation. Successful match launch, objective/round/HUD integration, visible weapon presentation, human input/gameplay checks, four-client 2v2, real two-Mac LAN, performance, packaged targets and Windows/Linux verification remain open. The automated result establishes connection/map travel only and does not certify replication correctness or describe the complete game as playable.

Coordinator handoff: publish an integrated exact commit; link this checklist from README/SETUP; preserve the documented slot-wrapped build/native/session commands and receipt requirement; rerun match preflight and the full same-source integrated rehearsal before the user's second-Mac session. Central docs/backlog/log and publication stay coordinator-owned.
