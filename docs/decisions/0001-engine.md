# Engine recommendation

Date: 2026-11-03

Status: Recommended planning baseline; toolchain validation pending.

## Context

The user confirmed a desktop-first game with a foundation for a larger project.
ValStrike aims for Counter-Strike-inspired tactical combat and original stylized visuals inspired by Valorant.
The first deliverable is a small playable multiplayer FPS, with AI assistance used during development.

## Recommendation

Use Unreal Engine 5. Select and pin the exact release after a compatibility spike on the development Mac.
Keep authoritative gameplay, weapon rules, round logic, and network validation in C++.
Use thin Blueprints for asset references, tuning, animation, effects, and UI composition.
Start with a small project; consult Lyra as a reference without adopting its full architecture.

The deciding advantage is existing networked character movement: prediction, server validation/correction, replay of saved moves, and smoothing are already documented engine features. Custom movement still needs to obey that framework. [Unreal networked movement](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine)

Epic recommends combining C++ foundations with Blueprint classes. C++ also provides text changes that are straightforward to review and merge. [C++ and Blueprint](https://dev.epicgames.com/documentation/en-us/unreal-engine/coding-in-unreal-engine-blueprint-vs-cplusplus)

Lyra supplies useful multiplayer, weapon, bot, and UI examples, but includes modular game features and a customized Gameplay Ability System. Keeping it as a reference limits initial architectural scope. [Lyra sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)

## Alternatives considered

| Option | Why it remains useful | Fit for this project |
| --- | --- | --- |
| Unreal Engine 5 | Native game tooling and integrated networked character movement. | Recommended for the confirmed desktop direction; accept a heavier build and editor workflow. |
| Godot | Human-readable scenes and headless command-line operation make a compact project easy to inspect and automate. | Strong fallback if Unreal setup or iteration costs become impractical; budget additional FPS networking work. |
| Three.js | A flexible JavaScript 3D library; game systems can be assembled around it. | Better matched to a browser requirement; we would own more framework integration. |
| Babylon.js | Browser-focused engine with physics integration, including Havok through Physics V2. | A useful browser alternative if instant web distribution becomes the priority. |

Godot evidence: [text scene format](https://docs.godotengine.org/en/stable/engine_details/file_formats/tscn.html), [command-line tools](https://docs.godotengine.org/en/stable/tutorials/editor/command_line_tutorial.html), and [multiplayer APIs](https://docs.godotengine.org/en/stable/tutorials/networking/high_level_multiplayer.html).
Browser evidence: [Three.js game architecture](https://threejs.org/manual/pages/game.html) and [Babylon.js physics integration](https://doc.babylonjs.com/features/featuresDeepDive/physics/).

The comparison is a project-fit judgment, not a measured ranking of AI coding accuracy or overall engine quality.
Godot's text scenes and JavaScript/TypeScript projects are attractive for file-oriented AI iteration; that advantage does not remove multiplayer implementation work.

## Two meanings of AI

AI-assisted development means generating and reviewing code, investigating failures, and maintaining documentation.
Use small changes with reproducible builds, focused tests, and recorded playtest results.
Visual assets and editor configuration still need inspection; generated code is not evidence of a working game.

In-game bot AI means navigation, perception, decisions, aiming, and reaction times.
Begin with NavMesh navigation and a small state machine; expand into Behavior Trees when needed.
Run bot decisions on the authoritative server and give bots bounded perception and reaction delays.
Runtime language-model calls are unnecessary for the MVP combat loop. [Unreal Behavior Trees](https://dev.epicgames.com/documentation/en-us/unreal-engine/behavior-tree-in-unreal-engine---overview)

## Local setup evidence and first validation

Observed development machine: Apple M4, 24 GB memory, macOS 15.6.
Xcode command-line tools were detected. Standard full-Xcode and engine installation paths were not found in the initial checks.
This is a limited installation check, not proof that no alternative installation exists.

Epic's current Mac requirements list 16 GB minimum and 32 GB recommended memory, with release-specific macOS and Xcode requirements.
The observed machine is between those memory figures; practical editor and build performance must be measured.
Verify the requirements for the selected release rather than assuming the newest release fits the installed tools. [Mac development requirements](https://dev.epicgames.com/documentation/en-us/unreal-engine/macos-development-requirements-for-unreal-engine)

The compatibility spike must establish:

1. An exact supported Unreal, macOS, and full-Xcode combination, plus available storage.
2. Successful C++ compilation, editor launch, and a packaged native Mac test build.
3. Two clients connecting to an authoritative server with visible movement replication.
4. A viable dedicated-server build and hosting path, including the intended server OS.
5. Recorded build time, editor responsiveness, and basic frame-time measurements.

Epic's documented dedicated-server workflow requires a source engine build and a C++ project.
Resolve source access, build tooling, storage, and target-server deployment early; a local listen-server test alone does not complete this validation. [Dedicated server setup](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)

## Consequences and revisit criteria

Treat stylized rendering as an art-direction and performance task. The MVP does not require Nanite or Lumen.
Keep the first map small and measure performance before adding visual complexity.
Use automated checks for gameplay rules and actual multi-client playtests for networking and feel.

Revisit this recommendation if the compatibility spike cannot produce a usable build workflow, if build/editor costs prevent iteration, or if the primary target becomes a browser.
Prefer Godot for a smaller native workflow in the first two cases; reassess Babylon.js and Three.js for a browser target.
Record the evidence and supersede this decision explicitly if the engine changes.
