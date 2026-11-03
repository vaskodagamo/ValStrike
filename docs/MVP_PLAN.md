# ValStrike MVP plan

Planning baseline: 2026-11-03. Desktop first and starting implementation on the Unreal baseline are confirmed by the user. Game-rule values and external platform targets remain proposed.

## Product goal

Build a small, enjoyable tactical FPS that proves movement, shooting, and networked round play. The larger ambition is a competitive team shooter. The first release is a private playtest, not a production competitive service.

Use Counter-Strike as a reference for deliberate movement, lethal weapons, team roles, and objective pressure. The user explicitly confirmed Valorant-like stylized graphics, not CS2-like photorealism. Use Valorant as a reference for readable silhouettes, controlled colors, simple materials, and clear effects. Create our own maps, characters, branding, audio, and assets. Visual quality depends on art direction and animation as well as the engine.

## What the MVP includes

| Area | Initial scope |
| --- | --- |
| Players | Two teams of two; 1v1 supported for development tests |
| Map | One compact original map, two approaches, one flank, one objective site |
| Movement | Mouse look, WASD, walk modifier, crouch, jump, collision |
| Combat | One hitscan rifle, recoil, movement inaccuracy, ammo, reload, body/head damage, death |
| Loadout | Same fixed rifle loadout each round; no economy or armor initially |
| Objective | Carry, drop, retrieve, plant, and defuse a device |
| Match | Six rounds, swap sides after three, draw allowed; rematch available |
| Feedback | Crosshair, health, ammo, team/score, round timer, objective state, hit/death feedback, basic spatial audio |
| Session | Private join by address; up to four players; team assignment and safe disconnect handling |
| Settings | Mouse sensitivity, volume, window/resolution options, exit to menu |
| Server | Separate authoritative server; no dependency on a local host player |

Later features: 5v5, multiple maps, weapon economy, additional weapons, grenades, abilities, advanced bots, ranking, matchmaking, accounts, cosmetics, voice chat, replays, public-service operations, and production anti-cheat. Sprint, sliding, ADS, and complex movement are outside the initial control scheme.

## Round rules

Initial tuning values: 8-second preparation, 90-second round clock, 3-second plant, 5-second defuse, 30-second detonation. Keep these configurable; playtests decide their final values.

1. Both teams spawn with full health and fixed ammo. Preparation prevents combat and movement out of spawn.
2. When the round becomes active, attackers can win by eliminating defenders or detonating the planted device. Defenders can win by eliminating attackers before a plant, reaching the round time limit before a plant completes, or defusing.
3. Planting requires a living attacker carrying the device within the site. Defusing requires a living defender near the planted device. Interaction must remain held and valid throughout; interruption resets progress.
4. A carrier who dies or disconnects drops the device at a valid reachable location. Use a designated recovery location if it leaves playable space. Surviving attackers may retrieve it.
5. After a plant, the detonation clock replaces the round deadline. Eliminating all attackers does not award defenders a win; the device still must be defused. Eliminating all defenders awards attackers a win.
6. Server timestamps decide deadlines. A plant/defuse must complete strictly before its applicable deadline; the deadline wins an exact tie. Resolve an end condition once, award one point, then lock combat and objective interactions.
7. Late joins wait as spectators until the next preparation phase. Disconnects update the living roster and objective ownership; resolve any resulting active-round win once. Before starting another round, wait if either team lacks the configured minimum roster, preserving score and completed-round count. Resume preparation once both teams qualify. If both teams become empty, clear the abandoned match and return to waiting without awarding another point.
8. Reset players, weapons, device, interaction progress, timers, and transient effects each round. Preserve score through the six-round match; clear it on rematch. Swap team roles after round three without changing which players belong together.

A match requires at least one player per team in development mode and two per team for the final MVP playtest. Simultaneous deaths and competing round-end events must have a documented deterministic server resolution in `RULE-01`.

## Technical foundation

Selected engine: Unreal Engine 5.8.4, paired with Xcode 26.1.1 for this Mac. Validate that pair during the toolchain spike. Use a small C++ project with the engine's character movement and replication. The first-person template may supply a starting viewpoint and input, but its gameplay must still be verified for multiplayer. Use Lyra as a reference when useful rather than adopting its whole framework initially.

| Responsibility | Proposed home |
| --- | --- |
| Server-only match rules and round transitions | `GameMode` |
| Replicated round phase, score, and server deadlines | `GameState` |
| Player team, score, and life status | `PlayerState` |
| Input, local settings, camera, HUD | `PlayerController` and UI |
| Movement and player body | `Character` and `CharacterMovementComponent` |
| Weapon validation, ammo, reload, shot cadence | C++ weapon component |
| Health and damage processing | C++ health component, server-owned changes |
| Device ownership and interactions | Replicated objective actor and server validation |
| Sounds, visual effects, mesh/animation wiring | Small Blueprints and assets |

Keep configuration separate from rules. Avoid a custom engine layer, external backend, or ability framework until a concrete feature needs it. Keep source, configuration, assets, and documentation in version control; establish Git LFS before introducing large binary assets.

### Networking policy

- Clients send action requests. The server validates ownership, alive state, round phase, fire cadence, ammo, reload state, view/origin plausibility, and interaction distance.
- Clients never choose the victim, final damage, ammo count, or round winner. The server traces shots against authoritative collision, including cover and head/body hit regions.
- Give local camera, recoil, muzzle-flash, and sound feedback immediately; reconcile gameplay outcomes from server results. Prevent repeated input or replayed shot IDs from producing duplicate damage.
- Use Unreal's character movement prediction and reconciliation. Avoid manually trusting client transforms or duplicating the engine's movement system.
- Initial hit registration uses server-current positions without historical rewind. This is a deliberate private, low-latency MVP limitation. Measure moving-target hit behavior before adding lag compensation; movement prediction alone does not solve it.
- Initial supported private-playtest target: measured RTT at or below 50 ms. Also characterize 100 ms RTT, jitter, and packet loss to expose failure modes. These are proposed test conditions, not verified capabilities.
- A packaged dedicated server and real clients are required. Editor multiplayer is an early check, not the release acceptance test.

Unreal already provides predicted networked character movement, but our weapon validation and match rules remain project work. See [Epic's movement documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine).

### Platforms and rendering

Develop locally on the available Mac. Proposed external playtest client: Windows x64; use a real Windows build/test environment before claiming Windows support. A packaged macOS development client can prove the first local vertical slice. Proposed deployed server: Linux, with a real Linux build/test environment; a local headless server alone does not verify Linux deployment.

Confirm tester machines during setup. A Mac-only first playtest is possible if all testers use Mac, but record that narrower scope rather than silently claiming Windows/Linux completion.

Start with simple lighting, modest geometry, readable silhouettes, and a restrained material palette. Do not require Nanite, Lumen, or ray tracing for the initial look. Polish a small section after movement scale and sightlines pass playtests, then apply that style consistently across the map.

## Milestones and acceptance gates

| Milestone | Deliverable | Required evidence |
| --- | --- | --- |
| M0 Planning | Scope, engine recommendation, backlog, working log | Documents agree; no implementation claimed |
| M1 Build and connection proof | Pinned toolchain, small C++ project, test map, packaged client and headless server | Reproducible build instructions; two separate client processes connect; selected client/server platform builds actually launch |
| M2 Networked gunplay | Movement and one rifle in a graybox arena | Two clients can move, shoot, reload, take damage, die, and restart; server rejects invalid actions; latency/loss results recorded |
| M3 Complete round loop | Teams, device, timers, scoring, side swap, rematch | Every win path and deadline conflict passes; ten consecutive rounds reset correctly; late join and disconnect do not strand the match |
| M4 Stylized playable slice | Map readability, original visual treatment, audio, HUD, menu and settings | A new tester can join and understand the match without developer instructions during play; capture images and performance measurements |
| M5 Private MVP release | Four-player packaged build and setup instructions | Four clients across at least two machines finish a six-round 2v2 match and rematch on the chosen dedicated-server target; no blocking defects |

M1 and M2 resolve the largest technical uncertainty. Keep art investment small until these gates pass. Estimate the rest of the calendar from actual setup, compile, and playtest results rather than assigning unsupported completion dates now.

An unavailable target build environment leaves the corresponding M1 gate incomplete; independent local gameplay tasks can still progress according to backlog dependencies. Before round rules exist, M2 uses a server-side development reset to restore test players and weapon state. Exclude that control from release builds.

## Verification plan

| Check | Evidence to retain |
| --- | --- |
| Build | Engine version, source revision if used, SDK/compiler, platform, exact command, exit result |
| Weapon rules | Cadence, empty magazine, reload interruption, duplicate request, dead-player shot, head/body damage, cover obstruction |
| Objective rules | Plant/defuse eligibility, interrupted interaction, dropped-device recovery, post-plant attacker elimination, timeout/deadline ties |
| Round state | Single score award, all win paths, role swap, death/respawn state, rematch, empty server |
| Multiplayer | Two-client development tests; four-client release session; late join, disconnect, server exit, invalid join address |
| Network conditions | Measured RTT, jitter and loss, actual emulator settings, movement corrections, hit-registration observations |
| Performance | Hardware, build type, resolution, settings, average and low-percentile FPS, frame-time spikes, server frame time |
| User playtest | Join success, comprehension, fun/friction notes, map sightlines, reproducible defects |

Proposed performance goal: 60 FPS at 1080p on an agreed reference machine, with server frame time within its configured tick budget. Select the reference machine and initial tick configuration in M1, then measure. Neither performance nor competitive-grade networking is currently demonstrated.

Automate repeatable weapon and round-rule tests. Use multiplayer functional tests and human playtests for movement feel, readability, and synchronization. Preserve a build/revision identifier with evidence.

## How AI fits the project

For development, use AI to draft and revise small C++ systems, tests, build scripts, and documentation. Compile frequently, check APIs against the pinned engine release, and review network authority explicitly. C++ text diffs support this workflow; Blueprints, maps, and animation still need editor verification. No engine guarantees correct AI-generated code.

For game bots, add a practice opponent after human multiplayer works: navigation, perception/line of sight, patrol/search/engage states, reaction delay, and aim error. Run decisions on the server and use the same weapon rules as players. A runtime language model is unnecessary for the initial combat behavior. Bots are a follow-on task, not a substitute for multiplayer acceptance.

## Working rhythm

Choose a backlog task, implement one observable increment, verify it, update its status, and append evidence and the next action to the work log. Record major technical choices in decision documents. Keep bugs in the backlog with reproduction steps and expected/actual behavior. Maintain a playable verified baseline as soon as M1 exists.

Open setup choices: Windows tester/build access; Linux server build access; reference performance hardware; asset budget. Resolve these as dependencies arise. No paid services or asset purchases are included in this planning baseline.
