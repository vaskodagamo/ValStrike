# ValStrike development setup

Status: local Unreal 5.8.4 / Xcode 26.1.1 toolchain verified on 2026-11-03. The Editor target compiles and the generated map loads in a development client. Packaged builds and complete multiplayer movement acceptance remain open.

## Observed local environment

| Item | Observation |
| --- | --- |
| Workspace | `/Users/webproduktion3/Documents/ValStrike` |
| Host | Apple Mac mini, M4, arm64, 24 GB memory |
| Operating system | macOS 15.6 |
| Active developer directory | `/Applications/Xcode.app/Contents/Developer` |
| Free disk at setup start | Approximately 446 GiB; recheck before a source-engine build |
| Xcode | 26.1.1, build 17B100, installed at `/Applications/Xcode.app`; first-launch agreement completed by the user |
| Git LFS | 3.8.1 installed and configured locally in this repository |
| Epic Games Launcher | 20.3.3 installed under `/Applications`; user signed in |
| Engine installation | 5.8.4 installed at `/Users/webproduktion3/UE_5.8/UE_5.8` |
| Remaining setup | Source-engine access/build, packaged client/server and full movement checks |

The Mac is above Epic's documented 16 GB minimum memory and below its 32 GB recommendation. Actual editor, compilation, and multi-client performance need measurement. The selected pair follows [Epic's macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine) and [Apple's Xcode compatibility table](https://developer.apple.com/xcode/system-requirements). [Epic's 5.8.4 release announcement](https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775) confirms the selected hotfix is released.

The Xcode archive was downloaded from Apple in the user's signed-in browser. `codesign --verify --deep --strict` passed and `spctl --assess --type execute` reported `accepted`, source `Apple System`, before moving the app into Applications. The initial Epic install failed with `IS-0007` because its reported `Manifests/Pending` directory was absent; that directory was created and the launcher resumed downloading. No broad permissions were changed.

## First implementation sequence

1. **SET-01 (verified):** Unreal 5.8.4 and Xcode 26.1.1 with Metal Toolchain 17B54 are installed. The compiler, SDK, engine Build.version and helper doctor passed. A successful `xcrun --find metal` only locates a stub; `xcrun --sdk macosx metal --version` must actually run.
2. **SET-02:** Create a small C++ Unreal project and a graybox test map. Establish source control exclusions and Git LFS for binary assets. Compile, run locally, and record the working commands and paths.
3. **SET-03:** Prove server and client execution. First use editor/server processes for a quick check, then build and run a packaged client against a packaged dedicated server. Start two separate clients. The engine's documented dedicated-server workflow requires a source build and a C++ project; verify this path before committing to further gameplay work. [Epic dedicated server guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)
4. **SET-04:** Identify playtest platforms and the real machines/runners that will build and run them. Proposed targets are Windows x64 clients and a Linux dedicated server. Establish those builds early; a successful Mac editor session does not satisfy either target.
5. Run `MOVE-01` through `MOVE-03` to establish networked movement before adding the rifle.

The launcher engine can support an initial editor spike. Do not mistake that for proof that the final dedicated-server packaging path works. Keep client and server engine revisions aligned.

## Verified build record and open gates

| Field | Current value |
| --- | --- |
| Unreal release and source revision | Launcher 5.8.4, changelist 58902052, compatible changelist 55116800; source revision not yet accessible |
| Engine location | `/Users/webproduktion3/UE_5.8/UE_5.8` |
| Xcode and macOS SDK | Xcode 26.1.1 (17B100), macOS SDK 26.1, Apple clang 17.0.0, Apple metal 32023.830; execution verified |
| Project file | `ValStrike.uproject`; engine association 5.8, helper enforces patch 5.8.4 |
| Source control asset policy | `.gitattributes` covers Unreal maps/assets and binary art/audio; local LFS hook installed |
| Local build and run commands | Editor build succeeded in 29.25 s; generator saved/reopened 24 actors; doctor passed |
| Dedicated server build and launch | Not verified |
| Windows compiler/SDK and runner | Not selected |
| Linux toolchain and runner | Not selected |
| Client connection command or UI | CLI below; local development client joined VS_Graybox on 127.0.0.1:7777 |
| Reference performance machine and settings | Not selected |
| Server tick configuration | Not selected |

Replace these entries with exact commands and observed results as tasks complete. Keep platform-specific instructions separate. Add required build artifacts, ports, and private-test connection steps only when verified. Store future build/test logs under an appropriate ignored output directory, and link concise results from the work log; do not commit generated caches or packaged binaries.

## Bootstrap commands

Run from the repository root. The Editor build and map-generation commands below were executed successfully on this Mac. `--engine` and `--dry-run` go before the action. `UE_ROOT` can be used instead of repeating `--engine`.

```sh
export UE_ROOT="/Users/webproduktion3/UE_5.8/UE_5.8"
python3 Scripts/ue.py doctor
python3 Scripts/ue.py build --target Editor --platform Mac
# Only if Content/Maps/VS_Graybox.umap is absent:
python3 Scripts/ue.py generate-map
python3 Scripts/ue.py editor
```

Doctor reports the absent map until generation succeeds; that does not prevent the initial editor build. The generator uses Unreal's editor Python APIs to author real map/material assets, saves and reopens them, and writes `Saved/Bootstrap/graybox-success.json` only on success. It refuses to overwrite existing map/material assets. On a partial failure, inspect the generated assets in the editor before retrying. Never manufacture `.umap` or `.uasset` files outside Unreal.

For the first network test, use three terminals with the same `UE_ROOT`:

```sh
# Terminal 1: development server using the editor binary
python3 Scripts/ue.py server --port 7777

# Terminals 2 and 3: separate development clients
python3 Scripts/ue.py client --address 127.0.0.1:7777
```

Server logs are written to `Saved/Logs/VS_Server_<port>.log`; each client gets `Saved/Logs/VS_Client_<helper-pid>.log`. They do not open extra log windows.

These are editor processes. They do not cook/package the game and do not satisfy the packaged dedicated-server gate. `build --target Game` builds the standalone game target; cooking and packaging remain separate work. Client/Server build targets require the source-engine path. The helper deliberately rejects other-platform builds from this Mac; use an actual Windows or Linux build environment for those targets.

Controls: WASD, mouse look, Space jump, Left Ctrl crouch, Left Shift slow walk, left mouse fire, R reload, Escape session menu. The menu supports private join by host:port, resume, disconnect and quit. The graybox remains a regression arena; there is no objective match or automatic round respawn yet. Rifle visuals, recoil, movement inaccuracy, sound and round eligibility remain separate work. Slow walk currently scales analog acceleration as well as speed; final physical input/feel acceptance is pending.

## Repeatable development movement test

After building the Editor target, run:

```sh
python3 Scripts/test_movement.py --engine "/Users/webproduktion3/UE_5.8/UE_5.8"
```

This launches a loopback server and two separate editor clients on UDP port 7789, runs simulated key input through the normal player controller, collects server/owner/observer state, and closes the processes. It uses null rendering and no audio. Results, exact commands, engine build, source hashes, and per-process logs are saved under `Saved/Tests/Movement/<run-id>/`; `result.json` must report `passed: true`. Exit zero is reserved for a completed passing run. Use `--port` to select a free local port.

The development-only `VSMovementSmoke` flag enables the harness. Normal launches do not enable it, and Shipping/Test builds exclude its behavior. The server positions players for each test section; movement during each section uses the existing input and CharacterMovement paths. Checks cover network roles, spawn separation, run/walk/crouch speeds, crouch capsule/body/camera state, jump/landing, boundary collision, stairs, and cover. The runner deliberately fails if evidence is missing or malformed.

Verified on 2026-11-03: 45/45 checks passed on the local M4 Mac. See the latest work-log entry for the run identifier and measurements.

This does not verify rendered remote-player appearance, mouse feel, focus changes, all collision corners, artificial latency/loss, packaged builds, or cross-machine play. Keep those acceptance checks separate.

## Checks without Unreal

```sh
python3 -m unittest discover -s Tests -p 'test_ue.py' -v
python3 -m unittest discover -s Tests/Server -p 'test_*.py' -v
python3 Scripts/test_server_contract.py
python3 Scripts/test_server_contract.py --sanitize
git diff --check
```

The helper tests check path/argument handling, missing prerequisites, rejected target mismatches, dry-run behavior, and generation evidence. They do not compile or validate C++ gameplay. `--dry-run` still requires the relevant engine/files but executes no external process.

## Source access and target machines

The local GitHub CLI is authenticated as `vaskodagamo`. The user reports GitHub is connected in Epic; a separate GitHub organization invitation completes source access. The source repository `EpicGames/UnrealEngine` currently returns HTTP 404 for that account. Signing in to the launcher does not itself prove source access. Complete [Epic's GitHub linking procedure](https://www.unrealengine.com/ue-on-github?lang=en-US), including the invitation, then verify the actual 5.8.4 source release reference and commit. Do not invent a tag or substitute an untrusted engine mirror.

Windows client and Linux server build/test machines are still unselected. Their acceptance gates remain open.

## Local verification evidence

The first Editor build passed UHT and all C++ compilation/link steps; captured output is `Saved/Logs/BootstrapBuild.log`. The generator saved four materials and the arena, reopened it with 24 actors and four PlayerStarts, and wrote `Saved/Bootstrap/graybox-success.json`. Map Check reported zero errors/warnings. Full initial Unreal editor diagnostics are under `~/Library/Logs/Unreal Engine/ValStrikeEditor/`; initial client diagnostics are under `~/Library/Logs/ValStrike/`. Captured stdout is not the complete engine log.

Two startup automation condition errors and an audio-device/sample-rate warning appeared in the first editor log; no project automation suite was invoked, and their origin remains unverified. Audio is not tested. Windows/Linux SDK discovery failures reflect absent target toolchains. About 382 GiB remained free after installation.

Two separate editor clients joined the local development server on 127.0.0.1:7777 on 2026-11-03. Both completed map travel; full visible replication and movement acceptance are still pending. The corrected graybox lighting and basic forward movement were visually inspected. The initial sunlight setting was reduced from 5000 to 3.0 for fixed exposure.

## Combat and session regression commands

After building the Editor target, run these from the checkout being verified. Each runner closes only its own processes and writes exact commands, source hashes and results under `Saved/Tests/`. Use free ports if another chat is testing. These are development Editor tests, not packaged acceptance.

```sh
python3 Scripts/test_combat.py --engine /Users/webproduktion3/UE_5.8/UE_5.8
python3 Scripts/test_movement.py --engine /Users/webproduktion3/UE_5.8/UE_5.8
python3 Scripts/test_combat_look.py --engine /Users/webproduktion3/UE_5.8/UE_5.8
python3 Tests/UI/test_session.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 --rendered
```

The UI suite uses actual combat snapshots and firing/reload input on the integrated branch. The mouse-look runner injects engine axis events and checks rendered camera directions; it does not move a physical mouse. Combat tests use current-position hit registration over loopback without artificial latency/loss. See the latest WORK_LOG.md entry for verified combined results and `docs/workstreams/combat.md` / `ui.md` for the original branch evidence.

## Dedicated server contract (SRV-03)

Every dedicated-server process (`-server`, including the Editor-binary development server) runs `UVSServerSubsystem` under `Source/ValStrike/Server/`. Clients, PIE and standalone games never create it. It implements the provider-neutral contract in [decision 0002](decisions/0002-server-hosting.md):

- **Allowlisted startup switches.** Only these `-VSServer*` switches are accepted. Any other `-VSServer*` switch, a duplicate or an invalid value stops startup with `VSSERVER_EXIT code=2`.

  | Switch | Values |
  | --- | --- |
  | `-VSServerSession=<id>` | 1–64 letters, digits, `-` or `_`; default `local-<UTC time>-<pid>` |
  | `-VSServerMaxPlayers=<n>` | 1–4 |
  | `-VSServerExitWhenDone` | no value |
  | `-VSServerEmptyTimeout=<s>` | 1–86400 (default 60); requires `-VSServerExitWhenDone` |
  | `-VSServerResultGrace=<s>` | 0–3600 (default 15); requires `-VSServerExitWhenDone` |
  | `-VSServerProvider=local` | `local` only |

  The server also refuses any map or game mode outside the launcher allowlist. Allowed maps are VS_Graybox and Site01; allowed modes are practice and match, and match requires Site01. Development builds also allow the rifle presentation test mode on the graybox.
- **Lifecycle.** States run Starting → Ready → Allocated → InMatch → Ending → Exited. The `local` provider logs each transition as a `VSSERVER_STATE` line, and logs a `VSSERVER_HEALTH` line every 10 s with players, frame time and tick rate. Without `-VSServerExitWhenDone`, nothing changes from earlier behavior: the server runs until stopped. With it, the server closes after a completed match once the rematch grace passes or everyone leaves, or after it has stayed empty for the timeout. Clients get a return-to-menu message, and the process exits with code 0. A startup failure (invalid switch, map or mode, or an invalid Site01 match contract) always exits. Linux and Windows return its non-zero code; the Mac Editor binary returns 0 for any requested exit, so check the `VSSERVER_EXIT` line there.
- **Admission.** `AVSGameMode::PreLogin` asks the server subsystem after the engine's checks. Joins are refused while the server is starting or ending; otherwise the development `open` admission accepts everyone, as before.
- **Version and results.** At startup the server logs a `VSSERVER_VERSION` line with the project version, Git revision, engine version and changelist, build configuration, target type and network version. `ValStrike.Build.cs` writes the revision into the untracked `Intermediate/VSBuildInfo/VSBuildInfo.h` header; a new commit triggers its regeneration on the next build. Uncommitted edits are not reflected in it. Each completed or abandoned match is logged as a single-line `VSSERVER_RESULT` JSON record and also saved to `Saved/ServerResults/<session>_match<n>.json`. The record holds per-round winners and reasons, scores, the result and team sizes, with no player names or addresses.

Launcher flags map one-to-one onto the switches:

```sh
python3 Scripts/ue.py server --map site01 --mode match --session-id lan-01 --exit-when-done --result-grace 15
```

Runtime verification uses loopback ports 7871–7874. Build the Editor target first, then run:

```sh
python3 Scripts/with_ue_slot.py -- python3 Scripts/test_server_lifecycle.py --engine "$UE_ROOT"
```

The runner checks three scenarios, each with its own server:
- **invalid:** an unknown switch fails startup.
- **empty:** a practice server with one player slot admits one client, refuses a second as full and exits 0 after a measured 30 s empty timeout.
- **match:** a Site01 match server with the development-only `-VSMatchFastTimings` switch runs six timeout rounds with two idle clients. It writes a 3–3 draw result, closes both clients after the 2 s grace and exits 0.

Evidence goes to `Saved/Tests/ServerLifecycle/<run-id>/result.json`. It checks that the logged revision equals `HEAD`, so commit and rebuild before running it. These are Editor-binary processes on one Mac; they do not verify a packaged or Linux dedicated server (SRV-01/02, SET-03).
