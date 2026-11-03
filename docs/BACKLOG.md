# ValStrike backlog

Updated: 2026-11-03. `Ready` means work can start; `Queued` means prerequisites are incomplete; `In progress` means work has started; `Done` requires evidence; `Deferred` means outside this MVP. Use `Blocked` only with a named dependency and a next action.

## Planning

| ID | Task | Status | Completion condition |
| --- | --- | --- | --- |
| PLAN-01 | Inspect workspace and clarify platform | Done | Empty project established; user confirmed desktop first |
| PLAN-02 | Compare engines and recommend a foundation | Done | Official sources reviewed; Unreal recommendation and tradeoffs recorded |
| PLAN-03 | Establish scope, milestones, backlog, log, and handoff | Done | Documents created and checked for consistency and local links |

## M1 Build and connection proof

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| SET-01 | Pin and prepare engine/toolchain | Done | PLAN-02 | Compatible release, full Xcode/SDK, disk capacity, and install locations recorded; toolchain launches |
| SET-02 | Create C++ project and source control setup | Done | SET-01 | Test map runs; project compiles; generated outputs excluded; binary asset storage configured |
| SET-03 | Prove packaged server/client connection | Queued | SET-02 | Source-engine server build path works; two client processes connect to packaged headless server; steps repeatable |
| SET-04 | Establish target-platform and performance baseline | Queued | SET-03 | Tester platforms chosen; actual target client/server builds launched; reference hardware, rendering settings, and tick configuration recorded |

## M2 Networked gunplay

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| MOVE-01 | Build graybox movement arena | In progress | SET-02 | Collision, player scale, spawns, stairs and cover are usable |
| MOVE-02 | Add replicated FPS controls | In progress | SET-03, MOVE-01 | Walk, crouch, jump, mouse look and remote body state work for two clients |
| MOVE-03 | Validate movement under latency/loss | Queued | MOVE-02 | Measured RTT/jitter/loss and corrections recorded; no repeatable stuck or divergent movement on supported conditions |
| GUN-01 | Add server-owned rifle state | Queued | MOVE-02 | Cadence, ammo, reload, fire state and tuning data agree across server/clients |
| GUN-02 | Add hits, health, death and development reset | Queued | GUN-01 | Server validates traces/damage; cover blocks shots; dead players cannot act; server-only development reset restores players/ammo for repeated tests |
| GUN-03 | Add immediate local feedback | Queued | GUN-02 | Recoil, crosshair, firing audio/effects, confirmed hits and death feedback work without duplicate effects |
| GUN-04 | Verify weapon authority and network behavior | Queued | GUN-03, MOVE-03 | Invalid/repeated requests rejected; rule tests pass; moving-target hit behavior measured and limitations documented |

Movement evidence (2026-11-03): a three-process loopback smoke test passed 45 checks covering normal/slow/crouched speeds, replicated stance/body bounds, camera height, jump/landing, walls, stairs and cover. MOVE-01/02 remain in progress until the rendered checks are complete; MOVE-03 latency/loss and SET-03 packaging are still open. See WORK_LOG.md and the repeatable command in SETUP.md.

## M3 Complete round loop

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| RULE-01 | Implement teams and round state | Queued | GUN-04 | Preparation, active, round-end, match-end states; deterministic contested-event resolution and one score award |
| RULE-02 | Implement objective carry/drop/interact | Queued | RULE-01 | Plant/defuse eligibility and interruption validated; death/disconnect drop and recovery work |
| RULE-03 | Complete win conditions and match flow | Queued | RULE-02 | Elimination, timeout, defuse and detonation pass; six rounds, side swap, draw/win and rematch work |
| RULE-04 | Handle joins and disconnects | Queued | RULE-03 | Late joins wait safely; teams capped; underfilled rosters pause before the next round with score preserved; empty server clears abandoned match |
| RULE-05 | Run round regression checks | Queued | RULE-04 | All rule paths tested; ten consecutive rounds reset cleanly; edge-case evidence recorded |

## M4 Stylized playable slice

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| PLAY-01 | Turn test arena into one-site map | Queued | RULE-05 | Routes, timing, cover and spawn safety survive human playtest |
| PLAY-02 | Establish original art treatment | Queued | PLAY-01 | One polished section sets palette/materials/lighting; consistent readable map and team silhouettes follow |
| PLAY-03 | Complete HUD, menu, settings and sound | Queued | RULE-04 | Join flow and game state understandable; settings persist; footsteps/shots/objective audio communicate action |
| PLAY-04 | Profile and conduct usability playtest | Queued | PLAY-02, PLAY-03, SET-04 | Evidence includes screenshots, reference-hardware frame times, server timing, and new-tester observations |

## M5 Private MVP release

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| REL-01 | Produce repeatable target builds | Queued | PLAY-04 | Packaged client/server built from recorded revision; development reset excluded; connection and startup instructions verified |
| REL-02 | Complete full 2v2 acceptance session | Queued | REL-01 | Four clients on at least two machines finish a match and rematch; disconnect and join checks pass |
| REL-03 | Resolve blocking defects and record release | Queued | REL-02 | No known blockers; limits and build identifier recorded; installation/playtest handoff works |

## Follow-on work

| ID | Task | Status | Entry condition |
| --- | --- | --- | --- |
| NEXT-01 | Simple practice bot | Deferred | Human multiplayer MVP accepted |
| NEXT-02 | Historical hit rewind / broader latency support | Deferred | Current hit-registration measurements justify design and acceptance criteria |
| NEXT-03 | Second weapon, economy and tactical utility | Deferred | Playtest feedback establishes priorities |
| NEXT-04 | 5v5 and additional maps | Deferred | Four-player performance and round flow established |
| NEXT-05 | Public sessions, identity, matchmaking and anti-cheat | Deferred | Separate service scope, operating budget and requirements agreed |

## Bug record format

Add discovered bugs with an ID, severity, build/revision, platform, reproduction steps, expected result, actual result, evidence, status, and next action. Runtime findings and remaining test coverage are recorded in WORK_LOG.md.
