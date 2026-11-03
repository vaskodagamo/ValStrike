# ValStrike work log

Record meaningful work sessions here in chronological order. Use Europe/Berlin dates. Keep task status in [BACKLOG.md](BACKLOG.md), current scope in [MVP_PLAN.md](MVP_PLAN.md), and verified setup commands in [SETUP.md](SETUP.md).

## 2026-11-03 Planning baseline

**Tasks:** PLAN-01, PLAN-02, PLAN-03.

**Request and confirmed direction:** Plan a working FPS MVP with Counter-Strike-like tactical play and a Valorant-inspired visual style; compare engines and support continued development with a working log. The user chose desktop first as the foundation for a larger game.

**Completed:** Inspected the repository and local development environment. Reviewed official engine documentation, including Unreal networking, C++/Blueprint workflow, dedicated-server setup, macOS requirements, Godot text scenes, and Three.js/Babylon.js capabilities. Recommended Unreal Engine 5 with C++ gameplay and small Blueprints; exact release remains unselected. Defined a private 2v2, single-map, one-rifle plant/defuse MVP with networking and packaging gates early in the plan. Created project documentation, ordered backlog, and agent handoff instructions.

**Observed evidence:** The starting repository contained only `README.md` with the project name; initial Git status was clean. The host reports Mac mini M4, 24 GB memory, macOS 15.6, arm64. `xcode-select -p` points to Command Line Tools. Standard Unreal/Epic and full Xcode locations were absent; installations elsewhere remain possible. A restricted `sysctl` memory read failed; `system_profiler` provided the hardware facts. No toolchain installation or machine-setting change was made.

**Verification:** Seven Markdown files and 31 unique backlog tasks checked for local links, task references, and whitespace errors; checks passed. Independent scope review clarified mandatory target-platform build evidence, waiting when one team becomes underfilled, and a development reset before full round rules exist. No game compilation, automated gameplay tests, multiplayer execution, performance test, or packaged build was possible or claimed in this planning-only session.

**Decisions and limits:** Desktop first is user-confirmed. Unreal is the recommended baseline; toolchain validation remains pending. Initial hit registration uses current server positions; production lag compensation is deferred. Windows client and Linux server targets are proposed, not verified. Bots, economy, abilities, 5v5 and public services remain outside the first MVP.

**Next action:** SET-01 — establish the exact Unreal/Xcode combination, disk capacity and installation locations, then create the smallest C++ project and prove server/client packaging. Resolve target tester/build-machine access during M1.

## 2026-11-03 Bootstrap implementation and toolchain setup

**Tasks:** SET-01, SET-02, MOVE-01, MOVE-02 (in progress).

**Authorization and repository:** The user requested committing, pushing, and starting implementation. Planning commit `49e218f` was pushed to `origin/main`. Continued development on `codex/bootstrap-fps`.

**Changed:** Added the Unreal project and Game/Editor/Client/Server targets; C++ character movement, first-person camera, programmatic Enhanced Input, temporary remote-player body geometry, spawn selection, and HUD hints. Added conservative rendering/input defaults, Git ignore rules and LFS attributes. Added an editor-only graybox/material generator and Python CLI for prerequisite checks, native builds, map generation, and separate development server/client launches.

**Local setup:** Selected Unreal 5.8.4 plus Xcode 26.1.1 (17B100) for the existing macOS 15.6. Installed Git LFS 3.8.1 and Epic Games Launcher 20.3.3; enabled LFS only in this repository. The user signed in to Apple and Epic, downloaded the official Xcode archive after Chrome blocked the automated download, and accepted Xcode's first-launch agreement. Xcode passed signature and Gatekeeper checks and was installed in Applications. Repaired the missing Epic manifest directory reported by `IS-0007`; engine download resumed. Downloaded Apple's Metal Toolchain 17B54 after detecting that the existing `metal` executable was only an unusable stub.

**Verification:** 19 launcher-helper unit tests pass; project JSON, Python syntax, and whitespace checks pass. Independent static review found no definite C++/config/map integration blocker. Tests include refusing stale or partial map-generation evidence, preserving dry-run behavior, validating native target/path arguments, and detecting the missing Metal component. No Unreal compilation, generated map, gameplay, or multiplayer result has yet been claimed at this checkpoint.

**Limits and dependencies:** Unreal installation is still in progress. Source access for authenticated GitHub user `vaskodagamo` still returns 404; a launcher sign-in alone does not enable source access. Packaged dedicated-server and Windows/Linux validation remain open. Movement tuning and camera/collision behavior need runtime tests. No weapons or round rules have been added.

**Next action:** Finish the engine installation, compile ValStrikeEditor, generate and inspect the arena, then connect two development clients. Record runtime results in the next entry and update the setup/backlog evidence.

## 2026-11-03 First runtime checkpoint and asset handoff

**Tasks:** SET-01 and SET-02 complete; SET-03, MOVE-01 and MOVE-02 remain incomplete.

**Changed:** Generated the native graybox map and four bootstrap materials through Unreal Editor. The first rendered view exposed an overexposure defect: sunlight at 5000 with fixed exposure washed out the arena. Changed the generator to intensity 3.0 (generator version 2), migrated the existing map through Unreal Python, saved/reopened it, and visually confirmed readable floor, walls, cover, sky and HUD. Disabled the unused default AndroidFileServer plugin and removed its automatically generated machine-specific settings. Development server/client helpers now write distinct absolute log files without GUI log windows. Added docs/ASSET_BRIEF.md with a separate worktree workflow and environment-kit starting prompt. The user explicitly reconfirmed Valorant-like stylized graphics, not CS2-like photorealism; this is explicit in the plan and asset prompt.

**Toolchain evidence:** Launcher engine 5.8.4, changelist 58902052, installed at `/Users/webproduktion3/UE_5.8/UE_5.8`. Xcode 26.1.1 (17B100), macOS SDK 26.1, Apple clang 17.0.0 and Metal 32023.830 execute successfully. The helper doctor passed. Initial Editor target build passed UHT, compilation and linking in 29.25 seconds (`Saved/Logs/BootstrapBuild.log`). About 382 GiB free after installation.

**Runtime evidence:** Generation saved/reopened 24 actors with four spawn points and Map Check zero errors/warnings. The lighting migration reports `VALSTRIKE_LIGHTING_FIXED` in `Saved/Logs/LightingFix.log`. A separate headless editor development server accepted two distinct client connections on loopback port 7777 at 15:34:29 and 15:34:54 Europe/Berlin; both completed map travel. Logs: `Saved/Logs/VS_Server_7777.log`, `VS_Client_8143.log`, `VS_Client_10185.log`. Rendered client inspection confirmed the corrected arena/HUD and basic forward movement; jump input was exercised but a complete movement/collision/replicated-body test was not completed. These are development editor processes, not packaged game/server acceptance.

**Checks:** 21 helper tests pass, including distinct runtime logs and dry-run preservation; JSON/Python syntax and whitespace checks pass. Native binary assets are tracked with Git LFS. No weapon or round-rule implementation is claimed.

**Open findings:** Initial editor logs contain two startup automation condition errors whose origin is unverified, plus audio-device/sample-rate warnings. Headless editor logs include editor widget-factory warnings. They did not prevent the observed map generation or client joins; audio and a warning-free runtime have not been demonstrated. Crouch, slow-walk speed, stairs, collision, visible remote movement and latency/loss still need systematic tests. GitHub source access last returned 404 even though the user reports linking GitHub in Epic; the separate organization invitation may still be pending. Packaged dedicated-server, Windows and Linux builds remain unverified.

**Next action:** Complete the two-client movement checklist and source-access check, then prove packaged client/server builds. The separate asset chat can start with concepts and a small original environment kit using docs/ASSET_BRIEF.md; it owns separate asset folders and its own log.

## 2026-11-03 Repeatable three-process movement verification

**Tasks:** MOVE-01, MOVE-02 verification; SET-03 source-access check.

**Changed:** Added an opt-in development movement harness and `Scripts/test_movement.py`. It launches a loopback editor server and separate driver/observer clients, sends simulated keys through PlayerController/Enhanced Input, and checks authoritative and replicated state. Test-only server positioning isolates floor, boundary, stair and cover sections. Normal launches do not enable the harness; Shipping/Test configurations compile out its behavior. Results preserve exact commands, engine Build.version, source hashes and dirty-checkout status. The runner closes its processes and treats missing/malformed evidence as failure.

**Verification:** Editor build passed in 7.90 seconds; 21 existing launcher-helper tests passed. The first runtime attempt failed because the online platform nickname replaced the URL's test name. Corrected the harness to request names through Unreal's existing SetName/ServerChangeName path after login; gameplay code was unchanged. The next three-process run passed all 45 checks on this M4 Mac with UE 5.8.4, null rendering, no audio, loopback UDP 7789 and no artificial latency/loss. Evidence: `Saved/Tests/Movement/20261103T145256Z_c3d9dc/result.json` plus role logs. It tested the working tree based on d076442; source hashes identify the tested C++ files.

**Measured results:** Server/owner speeds were 450 cm/s normal, 202.5 slow walk and 180 crouched. The observer saw 450/203/180 cm/s (replication quantization). Capsule half-height changed 88 to 56 cm; remote values 87.99/55.99 reflect proxy shrinking. Body-foot error was at most 0.01 cm in sampled stance windows, and the owner camera matched its standing/crouched height. Jump peak was about 90 cm. The server stopped the pawn at X=1915.899 against the east boundary and X=-634.101 against cover. All three processes recorded grounded samples on the 80 cm platform, and the same player ID was authoritative on the server, autonomous on the owner and simulated on the observer.

**Limits:** This is a scripted development smoke test, not rendered visual acceptance, packaged-build proof, a performance benchmark or a network-emulation test. Mouse feel, body visibility, focus changes, diagonal/opposing inputs, every stair/corner direction and latency/loss remain to be checked. MOVE-01/02 stay in progress rather than claiming their full gates. Source access for the existing GitHub account still returned 404, so packaged dedicated-server proof remains pending. No rifle, damage or round rules have been implemented.

**Next action:** Finish the brief rendered movement checklist, then implement GUN-01 server-owned rifle state and GUN-02 authoritative damage/death. Keep the stylized environment asset work independent until integration. Resolve Epic source access for SET-03 in parallel.

## Entry template

Copy this structure for the next completed session; do not replace earlier entries.

- Date and short outcome:
- Task IDs:
- Changed:
- Verification and evidence, including build/revision and exact test conditions:
- Defects, limitations or blocked dependencies:
- Decisions:
- Next action:
