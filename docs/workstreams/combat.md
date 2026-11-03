# Rifle gameplay workstream

## 2026-11-03 — GUN-01 and independent GUN-02 foundation

**Branch/base:** `codex/rifle-gameplay`, based on `b245939f48be3a16e3d7ad4668c56ef812cd062c`. Integration owns central task status and merging. This stream does not claim GUN-04, round rules, packaged acceptance or rendered playability.

**Scope status:** Rifle state, authoritative hits/health/death, development reset, and focused network verification are implemented. The integration handoff is being finalized after the movement regression.

**Changed paths:** `Source/ValStrike/Combat/VSHealthComponent.h/.cpp`, `VSRifleComponent.h/.cpp`, `VSCombatSmoke.h/.cpp`; `Source/ValStrike/VSCharacter.h/.cpp`; `Scripts/test_combat.py`; `Tests/Combat/test_combat_runner.py`; this stream log. No maps, configuration, build files, UI/controller/game-mode files, art, or shared HUD contract were changed.

**Implementation:** Replicated native components own health and rifle state. Defaults are 100 health, 30 magazine / 90 reserve, 0.12-second shot interval, 2-second reload, 100 m trace range, 30 body / 100 head damage. Default tuning is clamped on authority. Magazine capacity and fire interval replicate along with ammo, reload state and accepted-shot count. LMB requests automatic fire through Enhanced Input; R requests reload. `GetHUDData()` reads the replicated components; `StopCombatInput()` releases local held fire and leaves reload running. Health zero disables character movement, jump/crouch initiation, damage hit regions and firing; death cancels a pending reload without transferring ammunition.

**Authority:** The actor channel enforces the sender connection's ownership and the server also requires current player possession. Ordered bounded shot IDs consume rejected requests and prevent duplicates/stale IDs; history survives development reset. Server checks alive/reload/ammo/cadence, finite unit direction, origin within 100 cm of the authoritative eye, and direction within 20 degrees of its control view. The server always traces from its own eye against current collision and chooses the target and damage. Visibility-only body box/head sphere follow authoritative stance, independent of rendered mesh smoothing. A bounded 20 ms arrival tolerance absorbs tick jitter while advancing the server deadline without sustained fire-rate credit. No victim, damage, ammo or hit result is accepted from clients.

**Development controls:** The C++ smoke harness is opt-in via `-VSCombatSmoke=server|driver|observer`, registered from character BeginPlay, and compiled out in Shipping/Test. Server-only native `DevelopmentReset()` additionally requires `-VSCombatSmoke=server` or `-VSCombatDev`; there is no unrestricted reset RPC or console cheat. Normal launches do not run test actions. The harness uses three separate editor processes, real controller/Enhanced Input for ordinary actions, and opt-in raw requests for invalid-action cases. Temporary test cover is created in memory on the server, never saved as an asset.

**Verification so far:** Unreal 5.8.4 launcher engine (CL 58902052), Xcode 26.1.1 / Apple clang 17.0.0, macOS SDK 26.1, Mac arm64. The first compile succeeded in 26.64 s; final fixture build succeeded in 7.45 s. Build log: `Saved/Logs/RifleBuild.log`. Commands run from this assigned checkout:

```sh
python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
python3 Scripts/test_combat.py --engine /Users/webproduktion3/UE_5.8/UE_5.8
python3 -m unittest discover -s Tests/Combat -p 'test_combat_runner.py' -v
python3 -m unittest discover -s Tests -p 'test_ue.py' -v
```

The build helper includes `-WaitMutex`. Combat smoke passed **55/55 checks**, exit 0: `Saved/Tests/Combat/20261103T151326Z_1bf1c7/result.json` with role logs/reports. Conditions: one headless development editor server and two editor clients, UDP loopback 7793, null RHI, no audio, 60 FPS cap, no artificial latency/loss. All roles observed 30 -> 29 ammo and 100 -> 70 body damage; reload topped off to 30 with reserve 89; headshot killed; dead movement/fire were blocked; development reset restored health, ammo and movement mode; server cover prevented damage; the two-second firing segment accepted 15 additional shots (16 total after the cover shot); magazine stopped at zero; partial top-off conserved ammo; menu hook stopped held fire; death while reloading left 29/90 after the original reload deadline. The shot-cadence check allows 14–17 shots during that timed hold to accommodate process scheduling, so this is not a precise rate/performance benchmark.

Raw client request checks covered duplicate/stale IDs, back-to-back cadence, forged origin, opposite/zero direction, NaN origin, reloading, dead and empty state, with unchanged final gameplay state. The foreign-pawn RPC attempt was dropped by Unreal with `No owning connection ... ServerRequestFire will not be processed`; this exercises ordinary actor-channel ownership, not a custom forged-packet injector. The final report records source/config hashes and module SHA256 `d1e255eff528170ad3c6a69a5e94753c759c7574cbc9aad2063f0f2c33d86de1` against the dirty working tree based on b245939. Thirteen runner validation tests and 21 existing helper tests passed. Independent source review found no remaining definite blocker in this scope.

**Corrected test defect:** The first combat run (`20261103T151055Z_574c69`) failed 15 dependent checks because Unreal refused `SetStaticMesh` on the runtime cover's static component. Set the fixture movable before assigning the mesh and fail explicitly if assignment fails; rerun above passed all 55. This was a harness setup error, not claimed as passing evidence. Existing engine startup test-error/widget-factory messages remain in logs; no warning-free engine launch is claimed.

**Remaining limits:** Round-phase/team/friendly-fire enforcement awaits RULE-01; eligibility must be added to both firing and reload. Current server positions only; no rewind, moving-target measurement, measured RTT, network jitter/loss, cross-machine, four-player or packaged client/dedicated-server verification. GUN-03 recoil, movement inaccuracy, audiovisual/confirmed-hit feedback and final rifle mesh remain separate work. UI integration must visually verify real replicated health/ammo/death and menu focus. The temporary head/body collision must be revisited with final character art. Test behavior is preprocessor-excluded from Shipping/Test, but those configurations were not built here. Engine source access is still an external packaging dependency; no account setup was repeated.

**Integration instructions:** Merge this branch as one scoped gameplay increment after review; do not copy generated Binaries/Intermediate. Rebuild with the command above, run `Scripts/test_combat.py`, rerun `Scripts/test_movement.py`, then integrate the UI branch and verify the existing `GetHUDData` / `StopCombatInput` contract in a rendered session. Integrate rifle assets separately. Preserve the authority checks and shot-ID history when adding rounds/reset flows. Central docs/backlog updates remain with integration.

## 2026-11-03 — Movement regression and handoff verification

**Status:** The scoped GUN-01 and independent GUN-02 implementation is ready for integration review; central backlog status remains integration-owned. No production source changed after the passing combat run.

**Additional verification:** `python3 Scripts/test_movement.py --engine /Users/webproduktion3/UE_5.8/UE_5.8` passed **45/45 checks**, exit 0, on the same compiled module. Evidence: `Saved/Tests/Movement/20261103T151505Z_72b7e1/result.json`, one editor server/two clients on loopback UDP 7789, null RHI/no audio/no artificial latency or loss. Server/owner movement speeds stayed 450 / 202.5 / 180 cm/s; crouch/stand capsule, camera, jump/landing, body alignment, boundary, stairs and cover checks all passed. The test runners closed only their own processes. Git status confirms no map/material changes; generated outputs remain ignored.

**Next action:** Integration reviews and merges this branch, rebuilds/reruns the two smoke commands, then performs rendered HUD/menu and rifle-art integration. Round eligibility and the documented packaged/network-condition gates remain open.
