# ValStrike development setup

Status: inspected on 2026-11-03; implementation has not started. No game build or launch command has been verified.

## Observed local environment

| Item | Observation |
| --- | --- |
| Workspace | `/Users/webproduktion3/Documents/ValStrike` |
| Host | Apple Mac mini, M4, arm64, 24 GB memory |
| Operating system | macOS 15.6 |
| Active developer directory | `/Library/Developer/CommandLineTools` |
| Existing project | Git repository containing only a title in README before this planning session |
| Standard install paths checked | No `Xcode.app`, `Godot.app`, or `Epic Games Launcher.app` under `/Applications`; no `/Users/Shared/Epic Games` directory |
| Remaining uncertainty | Applications could exist elsewhere; available disk space, engine availability, and SDK compatibility are not yet verified |

The Mac is above Epic's currently documented 16 GB minimum memory and below its 32 GB recommendation. Actual editor, compilation, and multi-client performance need measurement. Match Xcode and macOS to the chosen engine release; Command Line Tools alone do not establish a working Unreal development setup. See [Epic's macOS requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine).

## First implementation sequence

1. **SET-01:** Locate any existing engine/toolchain installations and check free disk space. Select a supported stable Unreal Engine 5 release and compatible full Xcode/SDK. Record the exact versions. Install missing prerequisites within the authorized setup task.
2. **SET-02:** Create a small C++ Unreal project and a graybox test map. Establish source control exclusions and Git LFS for binary assets. Compile, run locally, and record the working commands and paths.
3. **SET-03:** Prove server and client execution. First use editor/server processes for a quick check, then build and run a packaged client against a packaged dedicated server. Start two separate clients. The engine's documented dedicated-server workflow requires a source build and a C++ project; verify this path before committing to further gameplay work. [Epic dedicated server guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)
4. **SET-04:** Identify playtest platforms and the real machines/runners that will build and run them. Proposed targets are Windows x64 clients and a Linux dedicated server. Establish those builds early; a successful Mac editor session does not satisfy either target.
5. Run `MOVE-01` through `MOVE-03` to establish networked movement before adding the rifle.

The launcher engine can support an initial editor spike. Do not mistake that for proof that the final dedicated-server packaging path works. Keep client and server engine revisions aligned.

## Build record to fill after verification

| Field | Current value |
| --- | --- |
| Unreal release and source revision | Not selected |
| Engine location | Not located |
| Xcode and macOS SDK | Not selected |
| Project file | Not created |
| Source control asset policy | Planned; Git LFS not configured |
| Local build and run commands | Not verified |
| Dedicated server build and launch | Not verified |
| Windows compiler/SDK and runner | Not selected |
| Linux toolchain and runner | Not selected |
| Client connection command or UI | Not implemented |
| Reference performance machine and settings | Not selected |
| Server tick configuration | Not selected |

Replace these entries with exact commands and observed results as tasks complete. Keep platform-specific instructions separate. Add required build artifacts, ports, and private-test connection steps only when verified. Store future build/test logs under an appropriate ignored output directory, and link concise results from the work log; do not commit generated caches or packaged binaries.
