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

## Entry template

Copy this structure for the next completed session; do not replace earlier entries.

- Date and short outcome:
- Task IDs:
- Changed:
- Verification and evidence, including build/revision and exact test conditions:
- Defects, limitations or blocked dependencies:
- Decisions:
- Next action:
