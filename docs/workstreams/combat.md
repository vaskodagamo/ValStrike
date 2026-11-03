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

## 2026-11-03 — BUG-01 vertical mouse-look correction

**Request:** Integration relayed the user's reversed-mouse report as a priority while this stream was finishing the rifle handoff. Verified both axes rather than flipping both.

**Cause and smallest correction:** `Config/DefaultInput.ini` disables legacy input scales. UE 5.8.4 `SceneViewport.cpp:928–931` preserves screen-right X and converts screen-up to positive MouseY; `PlayerController.cpp:6082–6087` applies those signs directly when legacy scales are disabled. The character's extra negative sign inverted pitch. Removed only that unary minus in `AVSCharacter::LookPitch`; yaw and sensitivity are unchanged. Engine `BaseInput.ini:30–31` additionally supplies MouseX/MouseY sensitivity 0.07, and the character retains its 0.12 multiplier.

**Isolated fix commit:** `8e81e84` changes one line in `Source/ValStrike/VSCharacter.cpp`. It can be cherry-picked before rifle commit `df3fea5`. A `git merge-file -p` three-way check against b245939 produced exactly the baseline character with this one sign change and no combat additions; no integration checkout or branch was modified. Merge the full branch later rather than manually copying character files.

**Focused evidence:** Added the opt-in, Shipping/Test-excluded `VSCombatLookSmoke.h/.cpp`, wired through the existing combat smoke bootstrap, plus `Scripts/test_combat_look.py` and six runner tests. Ordinary look input uses simulated MouseX/MouseY axis events through PlayerController and Enhanced Input, with resets only between scenarios. It measures the actual rendered camera and control rotation, captures eight required PNGs, and fails if evidence is incomplete. This differs from the movement driver, which forces rotation every tick and cannot validate mouse look.

Before-fix run `Saved/Tests/CombatLook/20261103T152145Z_fcd796/result.json` reproduced correct horizontal signs but negative pitch for positive MouseY and positive pitch for negative MouseY. Its initial expected magnitude omitted inherited 0.07 sensitivity, so it reported seven failures; that expectation was corrected without changing game sensitivity. Fixed run `Saved/Tests/CombatLook/20261103T152431Z_ad5d0b/result.json` passed **8/8**, exit 0, with actual rendered camera yaw +8.4/right and -8.4/left, pitch +8.4/up and -8.4/down for injected ±1000 input; limits +89.9/-89.9; ignored focus input stayed zero; restored focus accepted both axes. Inspected the direction and pitch-limit PNGs: geometry shifts consistently with the logged view direction; upper/lower limits show sky/ground. The client was a single rendered standalone Editor game, 1000×700 window, 60 FPS cap, no audio. Build succeeded in 13.21 s with the pinned toolchain; log `Saved/Logs/MouseLookBuild.log`. Six new runner tests passed.

```sh
python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
python3 Scripts/test_combat_look.py --engine /Users/webproduktion3/UE_5.8/UE_5.8
python3 -m unittest discover -s Tests/Combat -p 'test_combat_look_runner.py' -v
```

**Verification limits:** This rendered test injects engine axis events; it does not move a physical mouse or prove macOS raw-device behavior. The native input sign path was inspected in the pinned engine source. The focus check exercises IgnoreLookInput/reset/GameOnly; this checkout does not include the UI stream's Escape menu, so actual menu/capture return is pending integration. Only read-only native window inspection was performed; no input or edits were sent to the asset stream's editor. Integration should verify physical right/left/up/down plus Escape/resume after cherry-picking `8e81e84`.

**Push authorization:** Automatic approval initially rejected the upload as requiring explicit destination/payload authorization. The user then explicitly approved pushing this branch's source, tests and handoff log to the existing `github.com/vaskodagamo/ValStrike` repository. Only this stream branch is to be pushed; integration retains merge ownership.


## 2026-11-03 — Final combined build verification

**Final evidence after BUG-01:** The same final Editor module passed all three runtime suites: rendered look **8/8** (`CombatLook/20261103T152431Z_ad5d0b`), movement **45/45** (`Movement/20261103T152555Z_3a1b20`), and combat **55/55** (`Combat/20261103T152730Z_a1a889`), under `Saved/Tests/`, each with exit 0 and its own result.json. The final combat hold accepted 16 additional shots (17 total), with replicated state agreement and all prior authority/reload/cover/death checks passing. Final source/runner hashes were verified against the combat report before committing; module SHA256 is `7cd61c09d9b758669e1e0c169e5e326b0715928868801cdd2eacff52b752327c`. Nineteen Combat runner tests (13 combat + 6 look) and the 21 existing helper tests passed. No additional source changes followed these results.

**Handoff:** `codex/rifle-gameplay`; rifle implementation `df3fea5`, independently cherry-pickable mouse fix `8e81e84`, followed by the rendered regression harness and this log. Source changes are confined to the assigned character/combat/test/script paths. Integration may take `8e81e84` first, then merge the branch and rerun the documented commands. Physical-mouse/Escape-resume acceptance, round-phase enforcement, rendered multiplayer/art feedback, latency/loss and packaged/platform gates remain explicitly pending. This stream did not merge anything.
