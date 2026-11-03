# Live match runtime workstream

## 2026-11-03 — RULE-01–05 runtime adapter started

Assignment: `.parallel-worktrees/match-runtime`, branch `codex/match-runtime`, common base `a520ff20fba6bb9a94d0fc6ae2c59acff3d94606`. Read the project instructions, product plan, backlog, latest work log, parallel/current playtest assignments and setup/engine decision. This stream owns the Match namespace, scoped Character/Combat gates, its new native/Python tests and this append-only log. The earlier rules checkout is untouched. Central contracts, HUD, map, defaults, shared launchers and publication remain coordinator-owned.

Implementation in progress: authoritative `AVSMatchGameMode` with explicit URL selection, replicated GameState/PlayerState/objective, match-derived E/G/Enter controller, server deadline gates for fire/reload/damage, replicated movement gates, fresh-pawn preparation resets and actor-based Site01 validation. Practice behavior remains the default outside match mode. The optional device mesh has a primitive fallback. No live network outcome has yet been verified at this entry.

Deterministic adapter policy: collect current-server-millisecond events, seal only through `Now - 1`, and flush earlier timestamps before processing combat requests. Deadline checks also reject gameplay at the exact unsealed deadline. Hold intervals record their server-observed start and first invalidation; cancellation wins competing same-millisecond starts. Each movement update and world tick audits held interactions, including pending starts. Missing observations longer than 100 ms cancel rather than retroactively certifying eligibility. Placement uses actual Site A TriggerBox/role tags/recovery TargetPoint; it never reads documentation JSON. Drop candidates come from a collision-checked grounded trajectory from the role spawn, use the same actual flat arena-floor actor, require standing capsule clearance and floor support, and fall back to the validated recovery marker. Plant effects retain the last eligible site-ground location so later movement cannot relocate an earlier completion outside A.

Early review found and corrected preparation-login spawn lookup (Unreal requests an initial start before PostLogin assigns an identity), same-timestamp release/start progress retention, and retroactive movement cancellation of an earlier completion. The initial admission start is a harmless spectator fallback; role-specific fresh spawns happen only at preparation. Health gating is independent of the weapon gate so planting/defusing does not grant damage immunity.

Lightweight evidence: the new Python runner's 22 unit cases passed in the subtask review (18 initial cases plus four real auxiliary-connection lifecycle cases); root reran the initial 18 before the auxiliary extension. The runner requires actual server/driver/observer reports and an auxiliary late client that is connected and deliberately disconnected, with strict report validation, source/module/map hashes and owned-process cleanup. Native runtime reports are pending.

Heavy-work discipline: all attempts use `python3 Scripts/with_ue_slot.py -- ...`. Two initial build requests returned 75 while Site01 readability capture held the shared slot; no bypass/editor launch was attempted. The next slot acquisition began the pinned Editor build. Captured build stdout currently lives at `/private/tmp/valstrike-match-runtime-build.log`; final evidence will be copied into this checkout's ignored Saved directory. Git status initially failed because its LFS clean filter could not write shared `.git/lfs/tmp` under the sandbox; this is metadata access, not an asset modification.

Next: compile the combined adapter, run separate server/client scenarios including all wins, interruption, resets, late join/disconnect and twelve rounds across rematch, retain failures, then commit a reviewed local handoff for coordinator publication. No playable/LAN/packaged acceptance is claimed before those results.

## 2026-11-03 — First native build and twelve-round run

Pinned Mac Development Editor compilation succeeded using Unreal 5.8.4/Xcode 26.1.1 with `-DisableUnity`; log `Saved/Tests/Match/Builds/build-nonunity-08.log` records exit 0 and 30.72 seconds. Earlier compile failures are retained: incorrect module-root include assumptions were fixed with relative includes; the native report actor's replication macro parameter and two one-iteration loops were corrected. The normal unity build also exposed pre-existing anonymous-namespace symbol collisions across coordinator-owned files: `Smoke` in `UI/VSUISmoke.cpp` versus `Combat/VSCombatSmoke.cpp`, and `Ink`/`Muted` in `VSHUD.cpp` versus `UI/SVSSessionMenu.cpp`. No shared source/build files were changed. Coordinator action: give these translation-unit-local symbols distinct names or explicitly configure the module's unity policy. The installed UBT supports the temporary `-DisableUnity` command-line option used here.

Reproduction from this worktree (the whole build/test remains inside the cross-worktree engine slot):

```sh
python3 Scripts/with_ue_slot.py -- /bin/bash /Users/webproduktion3/UE_5.8/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh ValStrikeEditor Mac Development -Project=/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/match-runtime/ValStrike.uproject -WaitMutex -NoHotReloadFromIDE -DisableUnity
python3 Scripts/with_ue_slot.py -- python3 Scripts/test_match.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --port 7831
```

First native result: `Saved/Tests/Match/20261103T173107Z_17c227/result.json`, **FAIL, 39/40 checks passed**. This was an actual editor server plus driver/observer processes and a third late client, all null RHI over loopback. Twelve preparations/awards and 24 fresh primary pawns completed, including a 3:3 draw, real rematch RPC and 6:0 second match. The exact defuse/detonation boundary fixture failed because the living planter stood over the planted device and obstructed the defender's validated line of sight; no defuse token started. The fixture now moves that planter away before scheduling the boundary interaction. This failure is retained and is not counted as passing runtime evidence.

Additional review strengthened the harness: held-E repeat events after menu focus returns; actual W/jump/crouch/fire input during preparation/result with server position/ammo assertions; real client firing/reload before timeout plus development-only calls through the actual reload completion/start paths at the expired deadline. Those helper calls require authority and exact `VSMatchSmoke=server`, and are absent from Shipping/Test. The 22 runner unit tests passed again after expanding the required runtime checks to 43. Domain rules remain unchanged; standalone rules rerun passed 31 cases/973 checks (`Saved/Tests/RoundRules/20261103T171919Z_ac8c53/result.json`). Final native/regression evidence remains pending at this entry.

## 2026-11-03 — Verified local runtime handoff

Status: the scoped authoritative runtime deliverable is ready for coordinator review and integration. Changed paths are `Source/ValStrike/Match/` (server adapter, replicated actors, derived controller, native harness), `VSCharacter.h/.cpp`, `Combat/VSHealthComponent.cpp`, `Combat/VSRifleComponent.h/.cpp`, `Scripts/test_match.py`, `Tests/Match/test_match_runner.py` and this log. No Rules, map, asset, base-controller, shared-HUD, project/build/configuration or central-document files changed.

Final evidence, all in this checkout's ignored `Saved/Tests/` and tied to the tested source hashes:

| Check | Result | Evidence |
| --- | --- | --- |
| Pinned Mac Development Editor, non-unity | PASS, exit 0, 32.68 seconds | `Match/Builds/build-nonunity-09.log` |
| Separate match server, two primary clients and late-client connection | PASS, 43/43 checks, 122.66 seconds | `Match/20261103T173559Z_91af93/result.json` |
| Unchanged practice combat server/two-client regression | PASS, 55/55 checks | `Combat/20261103T173820Z_2bc154/result.json` |
| Unchanged practice movement server/two-client regression | PASS, 45/45 checks | `Movement/20261103T173951Z_6f4771/result.json` |
| Match runner lifecycle/evidence unit tests | PASS, 22 cases | `python3 -m unittest discover -s Tests/Match -v` |
| Unchanged standalone deterministic rules | PASS, 31 cases/973 checks | `RoundRules/20261103T171919Z_ac8c53/result.json` |

Regression commands, each executed from the assigned checkout under the shared slot:

```sh
python3 Scripts/with_ue_slot.py -- python3 Scripts/test_combat.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --port 7832
python3 Scripts/with_ue_slot.py -- python3 Scripts/test_movement.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --port 7833
```

The final match and combat reports identify module SHA-256 `166d1d8f7578de6c1e40030b5532e3e651506b0c0dfd444a0bd3e8a6315458b6`. The match report records Site01 SHA-256 `6798356d8d029112a0dabfa3a5fa2789eebeb79b114959836070801bb989f3aa`, engine 5.8.4 changelist 58902052, all 58 source/config/test hashes and the real command/PID/cleanup evidence. All four owned match processes exited 0 during requested cleanup; server/primary clients stayed alive until cleanup. Source hashes were checked again after the regressions with no differences. Runtime testing preceded the local handoff commit, so its recorded revision is base `a520ff2` with a dirty checkout; the per-file hashes identify the actual tested changes.

Coverage includes server phase gates and actual client movement/fire attempts, real E/G/fire/rematch requests, short-hold/menu-repeat cancellation, invalid defender intent, floor placement and device replication, death after plant, defuse/detonation/timeout/rifle-elimination wins, simultaneous deaths, late join followed by actual carrier disconnect/drop/retrieval, team role swap, six-round draw, explicit rematch, twelve consecutive awards and 24 distinct primary pawns. Exact plant/defuse completion ties use development-only server-scheduled, world-validated interaction starts at precise model milliseconds; they do not fabricate scores or bypass the rule resolver. Fire/damage/reload probes run at the first expired pre-actor world tick (17 ms after the deadline in the final run), before the adapter's normal Tick. The reload was begun by actual client input; the fixture invokes its real pending completion/start paths to prove no ammunition transfer at that expired deadline.

Integration instructions: merge/review this branch centrally, resolve the existing unity-name collisions above, then build and rerun the match/regression commands on the combined source. Select Site01 explicitly with `/Game/Maps/Site01/VS_Site01?game=/Script/ValStrike.VSMatchGameMode`; the default practice map/mode is untouched. Normal play must omit every `VSMatchSmoke`/`VSCombatSmoke` flag: model timings remain 8-second preparation, 90-second round, 3-second plant, 5-second defuse, 30-second detonation, and the adapter displays results for 3 seconds. Trusted server command-line `-VSMatchMinimum=2` selects a two-per-team starting minimum; default 1 supports the development 1v1 session. Controls are E hold plant/defuse or press retrieve, G drop and Enter rematch. The derived controller supplies the existing `GetMatchHUDData`, `CancelObjectiveInput` and `RequestMatchRematch` hooks. Final device art can be wired through `ObjectiveClass` and its optional `DeviceMesh`; the primitive is the current fallback.

Limitations/next action: these are local Mac editor, null-RHI, no-audio, no-latency/loss tests, not packaged dedicated-server, rendered UX, two-machine LAN or cross-platform acceptance. The native scenario reaches three concurrent players; a complete four-client 2v2 admission/minimum-two session and all-connections-gone cleanup have implementation/domain coverage but were not separately exercised by this native runner. Round HUD, rifle presentation and final device art must be integrated by their owners and verified together, followed by the user's other-Mac LAN rehearsal. The prior first-run failure remains in Saved as diagnostic evidence. Publication and PRs remain coordinator-owned; this handoff is a local commit only.
