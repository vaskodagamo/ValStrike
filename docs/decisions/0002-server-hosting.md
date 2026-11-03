# Dedicated server and hosting path

Date: 2026-11-03

Status: Proposed. The user asked on 2026-11-03 for a server path that can grow to many servers if the game succeeds. Nothing in this record is implemented or verified yet. It supplements [0001](0001-engine.md) and does not change the engine, the MVP scope or the deferred NEXT-05 public-service work.

## Context

- **Current server.** `Scripts/ue.py server` and `Tests/Playtest/run_session.py` start the Unreal *Editor* binary with `-server -nullrhi`. One process hosts one map/mode on one UDP port (default 7777). Clients join by a validated `host:port`. `AVSMatchGameMode` admits at most four players (`PreLogin`/`PostLogin`). When the server empties, the match clears and waits; the process keeps running. This is correct for development and LAN tests, but it is not a shippable server.
- **Dedicated server target.** `Source/ValStrikeServer.Target.cs` exists, but Epic's dedicated-server workflow requires a source-built engine. The launcher engine cannot build it. SET-03 is queued, and `EpicGames/UnrealEngine` still returns 404 for the project's GitHub account (see [SETUP.md](../SETUP.md)). [Epic dedicated server guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine)
- **Target platform.** [MVP_PLAN.md](../MVP_PLAN.md) proposes a Linux deployed server and Windows x64 clients. `Scripts/ue.py` deliberately refuses other-platform builds from a Mac. A Linux server needs a real Linux build environment.
- **Official build images.** Epic publishes Linux development images under `ghcr.io/epicgames/unreal-engine`. Pulling them needs the same Epic–GitHub account link as source access, plus a GitHub token with `read:packages`. Epic labels the feature Beta. Its example packages Linux client/server builds with `RunUAT BuildCookRun -platform=Linux`. Epic's quick-start page does not list the exact 5.8.4 tag; verify it once access works. [Epic container quick start](https://dev.epicgames.com/documentation/en-us/unreal-engine/quick-start-guide-for-using-container-images-in-unreal-engine)

## Proposed decision

**One server process runs one match.** Processes are disposable: a hosting system starts one, gives it a match, and it exits when the match is over. Persistent data (accounts, results, rankings) never lives inside the game server process. Any host, from a script on one machine to a cloud fleet, can then run many servers without changing gameplay code.

Grow in stages. Each stage starts only when its entry condition is met.

| Stage | What runs | Entry condition | Acceptance evidence |
| --- | --- | --- | --- |
| 0. Development (current) | Editor-binary server, LAN or loopback | — | Existing runners and LAN checklist |
| 1. Packaged Linux server | `ValStrikeServer` packaged for Linux from the pinned engine source, shipped as a container image | Source access (SRV-01) and a Linux build environment (SRV-02) | Headless Linux server starts from a recorded revision; two packaged clients finish a match (SET-03, REL-01) |
| 2. Several matches per host | N server processes on one Linux machine, each on its own UDP port; static server list | Stage 1 and the server contract below (SRV-03) | Concurrent isolated matches; measured CPU, memory and bandwidth per match at the chosen tick rate (SRV-04) |
| 3. Fleet and matchmaking | An orchestrator starts servers on demand across regions; a backend issues join tickets and stores results | Private MVP released and real demand/budget agreed (SRV-05, NEXT-05) | Decided then; out of MVP scope |

Build the Stage 1 server in Linux CI or a cloud VM, not on a development Mac. A source engine plus build images needs far more disk than the current Macs can spare. Measure the real size when the environment is set up. Clients and servers must come from the same revision and engine changelist.

For Stage 3, compare candidates when its entry condition is met: [Agones](https://agones.dev/site/docs/guides/client-sdks/) (open source, self-run on Kubernetes, Unreal SDK plugin), [Amazon GameLift Servers](https://docs.aws.amazon.com/gamelift/latest/developerguide/unreal-plugin.html) (managed, Unreal plugin), and other managed hosts. Epic Online Services is the natural candidate for player identity, lobbies and anti-cheat on Unreal. Do not adopt any of them now: there is no packaged server and no load measurement, so the choice would be a guess with lock-in.

## Server contract to build first (SRV-03)

These small, provider-neutral pieces work at Stage 0. They make Stages 2–3 an adapter task instead of a gameplay rewrite. Keep them in reviewable, server-authoritative C++ under a new `Source/ValStrike/Server/` area.

1. **Startup parameters.** Map, mode, port, maximum players and a session identifier come from launch arguments through an allowlist, consistent with `Scripts/ue.py`. Clients still cannot add URL options or console commands.
2. **Lifecycle states.** Starting → Ready → Allocated → InMatch → Ending → exited, owned by a server-only subsystem with a small provider interface. The default `Local` provider only logs transitions. Later providers map the same states to their SDK:

   | ValStrike state | Agones SDK | GameLift Servers SDK |
   | --- | --- | --- |
   | Ready | `Ready()` | `ProcessReady()` |
   | Allocated | Allocation by the fleet | `OnStartGameSession` → `ActivateGameSession()` |
   | Health while running | `Health()` | Health-check callback |
   | Ending | `Shutdown()` | `ProcessEnding()` |

   Agones' plugin cannot run inside the Editor. Provider code therefore stays optional and is verified on packaged Linux servers.
3. **Exit when done.** An explicit opt-in launch flag ends the process with exit code 0 after match completion with no rematch, or after the server has been empty for a configured time. Without the flag, today's wait-and-clear behavior is unchanged, so practice, LAN tests and existing runners keep working.
4. **Join admission.** `AVSMatchGameMode::PreLogin` asks a pluggable admission check. The development check accepts everyone, as today. A later matchmaker supplies a server-validated join ticket through the client's join flow, never through text the player types.
5. **Version identity.** The server logs its source revision, engine changelist and build configuration at startup. Mismatched clients are rejected by the engine's network version check rather than joining a divergent build.
6. **Results out, no state in.** At match end the server writes a structured result (log line or JSON under `Saved/`). Stage 3 posts it to a backend instead. The game server has no database access.
7. **Tick rate and health.** Choose and record `NetServerMaxTickRate` (`[/Script/OnlineSubsystemUtils.IpNetDriver]`) with SET-04. Periodically log server frame time and player count. Tick rate drives hosting cost and must be measured, not assumed.

## Alternatives considered

| Option | Why not now |
| --- | --- |
| Listen server (a player hosts) | MVP requires a separate authoritative server; host advantage and trust problems |
| Peer-to-peer | Same authority and cheating problems |
| Ship Editor-binary servers | Needs uncooked content and the Editor; heavy and not a release artifact |
| Pick a managed host now | No packaged server or measurements; premature lock-in. The neutral contract keeps the choice open |
| Build the Linux server on a Mac | Unsupported by the helper's platform policy; insufficient disk on current Macs |

## Consequences and revisit criteria

- Source access (SRV-01) is the critical blocker. It needs the account owner to link Epic and GitHub and accept the EpicGames organization invitation.
- A Linux build environment with large disk is new infrastructure. It costs nothing until it is used; Stage 2 adds the first running hosting cost.
- SRV-03 touches `Match/` behavior (admission, exit). Coordinate ownership with the match-runtime stream and keep all existing smoke suites passing.
- If source access cannot be obtained, no dedicated server can ship. Revisit this record and 0001 rather than falling back silently to listen servers.
- Revisit the Stage 3 choice with real player counts, regions and budget. Record that choice in a new decision.
