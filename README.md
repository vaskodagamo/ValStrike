# ValStrike

An original desktop tactical FPS: deliberate movement and round-based combat inspired by Counter-Strike, with a clean, stylized visual direction inspired by Valorant.

**Current state:** bootstrap implementation. The C++ project, movement code, map generator, and development launcher are present. The Mac Editor target compiles and the generated arena loads in a development client. A three-process development movement smoke test passes 45 checks. Rendered movement acceptance and packaged multiplayer verification are pending.

**Confirmed direction:** desktop first, with a foundation that can grow into a larger game.

**Foundation:** Unreal Engine 5.8.4, C++ gameplay, and small Blueprints for presentation and tuning. The Mac toolchain is pinned to Xcode 26.1.1. Runtime testing is underway.

## Project documents

| Document | Purpose |
| --- | --- |
| [MVP plan](docs/MVP_PLAN.md) | Scope, architecture, milestones, and acceptance criteria |
| [Engine decision](docs/decisions/0001-engine.md) | Recommendation, alternatives, AI workflow, and tradeoffs |
| [Setup](docs/SETUP.md) | Observed environment and first implementation steps |
| [Backlog](docs/BACKLOG.md) | Ordered tasks, dependencies, and status |
| [Work log](docs/WORK_LOG.md) | Work completed, verification, issues, and handoff |
| [Asset chat brief](docs/ASSET_BRIEF.md) | Separate environment asset batch and starting prompt |
| [Parallel work](docs/PARALLEL_WORK.md) | Chat ownership, shared combat/HUD contract, and integration |
| [Weapon art direction](docs/WEAPON_ART_DIRECTION.md) | Pistol asset and revised rifle/pistol finishes |
| [First tactical map](docs/MAP_BRIEF.md) | One-site layout, map ownership, spawn and objective metadata |
| [Round rules foundation](docs/ROUND_RULES_BRIEF.md) | Isolated match/objective logic, tests and integration boundary |
| [Agent instructions](AGENTS.md) | How future coding sessions maintain this project |

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
