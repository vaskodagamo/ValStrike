# ValStrike

An original desktop tactical FPS: deliberate movement and round-based combat inspired by Counter-Strike, with a clean, stylized visual direction inspired by Valorant.

**Current state:** development combat and session foundation. The project includes replicated movement, a server-owned rifle, health/death, corrected mouse look, a live HUD and private-server menu. A complete objective match, final weapon presentation and packaged multiplayer remain in progress. Verification and its limits are recorded in the work log.

**Confirmed direction:** desktop first, with a foundation that can grow into a larger game.

**Foundation:** Unreal Engine 5.8.4, C++ gameplay, and small Blueprints for presentation and tuning. The Mac toolchain is pinned to Xcode 26.1.1. Runtime testing is underway.

## Project documents

| Document | Purpose |
| --- | --- |
| [MVP plan](docs/MVP_PLAN.md) | Scope, architecture, milestones, and acceptance criteria |
| [Engine decision](docs/decisions/0001-engine.md) | Recommendation, alternatives, AI workflow, and tradeoffs |
| [Server hosting path](docs/decisions/0002-server-hosting.md) | Proposed dedicated-server stages toward many servers, and the server contract to build first |
| [Setup](docs/SETUP.md) | Observed environment and first implementation steps |
| [Backlog](docs/BACKLOG.md) | Ordered tasks, dependencies, and status |
| [Work log](docs/WORK_LOG.md) | Work completed, verification, issues, and handoff |
| [Asset chat brief](docs/ASSET_BRIEF.md) | Separate environment asset batch and starting prompt |
| [Parallel work history](docs/PARALLEL_WORK.md) | Historical assignments and combat/HUD contract |
| [Playtest assignment history](docs/PLAYTEST_WORKSTREAMS.md) | Earlier six-stream contracts and playtest gates; current ownership is in the team board |
| [Weapon art direction](docs/WEAPON_ART_DIRECTION.md) | Pistol asset and revised rifle/pistol finishes |
| [First tactical map](docs/MAP_BRIEF.md) | One-site layout, map ownership, spawn and objective metadata |
| [Round rules foundation](docs/ROUND_RULES_BRIEF.md) | Isolated match/objective logic, tests and integration boundary |
| [Team workflow](docs/TEAM_WORKFLOW.md) | Shared hierarchy, worktree ownership and centralized validation for Codex, Claude and humans |
| [Team board](docs/TEAM_BOARD.md) | Current owners, prepared worktrees and integration queue |
| [Agent instructions](AGENTS.md) / [Claude entry point](CLAUDE.md) | Shared contributor rules; start here before implementation |

## First playable target

A private 2v2 match on one small map with one objective site, one rifle, fixed loadouts, and a complete plant/defuse round loop. Prove 1v1 networking before expanding to four players. A separate authoritative server decides damage and round outcomes.

The first implementation milestone is a reproducible project build and two clients connected to a server. See tasks `SET-01` through `SET-04` in the backlog. [Setup](docs/SETUP.md) contains the bootstrap commands and distinguishes development editor launches from packaged-server verification.

Run the helper's prerequisite report from the repository root:

```sh
python3 Scripts/ue.py --engine "/Users/webproduktion3/UE_5.8/UE_5.8" doctor
```

This checks the toolchain and map; it is not a gameplay acceptance test. The helper's unit tests run without Unreal:

```sh
python3 -m unittest discover -s Tests -p 'test_ue.py' -v
```
