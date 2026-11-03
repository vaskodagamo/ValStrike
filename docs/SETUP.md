# ValStrike development setup

Status: implementation started on 2026-11-03. Unreal 5.8.4 and Xcode 26.1.1 are pinned. No game compilation or launch has been verified yet.

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
| Engine installation | 5.8.4 downloading to `/Users/Shared/Epic Games/UE_5.8` |
| Remaining setup | Metal Toolchain component download, engine installation, first compile and runtime checks |

The Mac is above Epic's documented 16 GB minimum memory and below its 32 GB recommendation. Actual editor, compilation, and multi-client performance need measurement. The selected pair follows [Epic's macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine) and [Apple's Xcode compatibility table](https://developer.apple.com/xcode/system-requirements). [Epic's 5.8.4 release announcement](https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775) confirms the selected hotfix is released.

The Xcode archive was downloaded from Apple in the user's signed-in browser. `codesign --verify --deep --strict` passed and `spctl --assess --type execute` reported `accepted`, source `Apple System`, before moving the app into Applications. The initial Epic install failed with `IS-0007` because its reported `Manifests/Pending` directory was absent; that directory was created and the launcher resumed downloading. No broad permissions were changed.

## First implementation sequence

1. **SET-01:** Finish installation of Unreal 5.8.4 and the Xcode 26.1.1 Metal Toolchain. Verify the active compiler, SDK and engine Build.version. A successful `xcrun --find metal` only locates a stub; `xcrun --sdk macosx metal --version` must actually run.
2. **SET-02:** Create a small C++ Unreal project and a graybox test map. Establish source control exclusions and Git LFS for binary assets. Compile, run locally, and record the working commands and paths.
3. **SET-03:** Prove server and client execution. First use editor/server processes for a quick check, then build and run a packaged client against a packaged dedicated server. Start two separate clients. The engine's documented dedicated-server workflow requires a source build and a C++ project; verify this path before committing to further gameplay work. [Epic dedicated server guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)
4. **SET-04:** Identify playtest platforms and the real machines/runners that will build and run them. Proposed targets are Windows x64 clients and a Linux dedicated server. Establish those builds early; a successful Mac editor session does not satisfy either target.
5. Run `MOVE-01` through `MOVE-03` to establish networked movement before adding the rifle.

The launcher engine can support an initial editor spike. Do not mistake that for proof that the final dedicated-server packaging path works. Keep client and server engine revisions aligned.

## Build record to fill after verification

| Field | Current value |
| --- | --- |
| Unreal release and source revision | 5.8.4 selected; source revision not yet accessible |
| Engine location | `/Users/Shared/Epic Games/UE_5.8` installation in progress |
| Xcode and macOS SDK | Xcode 26.1.1 (17B100); SDK/compiler execution checks pending Metal component |
| Project file | `ValStrike.uproject`; engine association 5.8, helper enforces patch 5.8.4 |
| Source control asset policy | `.gitattributes` covers Unreal maps/assets and binary art/audio; local LFS hook installed |
| Local build and run commands | Helpers written and unit-tested; Unreal execution pending |
| Dedicated server build and launch | Not verified |
| Windows compiler/SDK and runner | Not selected |
| Linux toolchain and runner | Not selected |
| Client connection command or UI | Not implemented |
| Reference performance machine and settings | Not selected |
| Server tick configuration | Not selected |

Replace these entries with exact commands and observed results as tasks complete. Keep platform-specific instructions separate. Add required build artifacts, ports, and private-test connection steps only when verified. Store future build/test logs under an appropriate ignored output directory, and link concise results from the work log; do not commit generated caches or packaged binaries.

## Bootstrap commands

Run from the repository root after installation completes. These are the current commands to validate, not evidence of a successful Unreal build. `--engine` and `--dry-run` go before the action. `UE_ROOT` can be used instead of repeating `--engine`.

```sh
export UE_ROOT="/Users/Shared/Epic Games/UE_5.8"
python3 Scripts/ue.py doctor
python3 Scripts/ue.py build --target Editor --platform Mac
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

These are editor processes. They do not cook/package the game and do not satisfy the packaged dedicated-server gate. `build --target Game` builds the standalone game target; cooking and packaging remain separate work. Client/Server build targets require the source-engine path. The helper deliberately rejects other-platform builds from this Mac; use an actual Windows or Linux build environment for those targets.

Controls: WASD, mouse look, Space jump, Left Ctrl crouch, Left Shift slow walk. Slow walk currently scales analog acceleration as well as speed; feel and camera behavior need runtime testing. The graybox is a movement test arena, not the final objective map. No weapons or round rules are implemented yet.

## Checks without Unreal

```sh
python3 -m unittest discover -s Tests -p 'test_ue.py' -v
git diff --check
```

The helper tests check path/argument handling, missing prerequisites, rejected target mismatches, dry-run behavior, and generation evidence. They do not compile or validate C++ gameplay. `--dry-run` still requires the relevant engine/files but executes no external process.

## Source access and target machines

The local GitHub CLI is authenticated as `vaskodagamo`. The source repository `EpicGames/UnrealEngine` currently returns HTTP 404 for that account. Signing in to the launcher does not itself prove source access. Complete [Epic's GitHub linking procedure](https://www.unrealengine.com/ue-on-github?lang=en-US), including the invitation, then verify the actual 5.8.4 source release reference and commit. Do not invent a tag or substitute an untrusted engine mirror.

Windows client and Linux server build/test machines are still unselected. Their acceptance gates remain open.
