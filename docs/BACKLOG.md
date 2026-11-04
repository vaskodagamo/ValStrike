# ValStrike backlog

Updated: 2026-11-04. `Ready` means work can start; `Queued` means prerequisites are incomplete; `In progress` means work has started; `Done` requires evidence; `Deferred` means outside this MVP. Use `Blocked` only with a named dependency and a next action.

## Team operating model

Current ownership and verification queue live in [TEAM_BOARD.md](TEAM_BOARD.md), under [TEAM_WORKFLOW.md](TEAM_WORKFLOW.md). Historical stream assignments below do not grant active ownership. Implementation-ready, runtime-verified and merged are separate states; existing partial acceptance remains open.

| ID | Task | Status | Completion condition |
| --- | --- | --- | --- |
| TEAM-01 | Establish provider-neutral hierarchy and isolated contributor workspaces | Done | Shared rules and Claude entry point, worktree slots, proportional validation and central integration queue prepared, checked and published for user merge |
| INT-01 | Verify the combined current-main gameplay batch | Ready | One selected revision built and affected checks executed centrally; failures assigned, evidence and limits recorded |

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
| SET-05 | Prepare source-checkout Mac playtest tools | In progress | SET-02 | Real LFS assets verified in fresh checkout; pinned toolchain/build and selectable-map launch verified; LAN checklist and evidence collection ready |
| SET-04 | Establish target-platform and performance baseline | In progress | SET-03 | Tester platforms chosen; actual target client/server builds launched; reference hardware, rendering settings, and tick configuration recorded |

SET-04 partial (2026-11-04): server tick rate chosen at 60 Hz and measured against 30 Hz on the Editor binary; see WORK_LOG.md. Tester platforms, target builds and reference hardware remain open.

## M2 Networked gunplay

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| MOVE-01 | Build graybox movement arena | In progress | SET-02 | Collision, player scale, spawns, stairs and cover are usable |
| MOVE-02 | Add replicated FPS controls | In progress | SET-03, MOVE-01 | Walk, crouch, jump, mouse look and remote body state work for two clients |
| MOVE-03 | Validate movement under latency/loss | Queued | MOVE-02 | Measured RTT/jitter/loss and corrections recorded; no repeatable stuck or divergent movement on supported conditions |
| GUN-01 | Add server-owned rifle state | Done | MOVE-02 | Cadence, ammo, reload, fire state and tuning data agree across server/clients |
| GUN-02 | Add hits, health, death and development reset | Done | GUN-01 | Server validates traces/damage; cover blocks shots; dead players cannot act; server-only development reset restores players/ammo for repeated tests |
| GUN-03 | Add immediate local feedback | In progress | GUN-02 | Recoil, crosshair, firing audio/effects, confirmed hits and death feedback work without duplicate effects |
| GUN-04 | Verify weapon authority and network behavior | Queued | GUN-03, MOVE-03 | Invalid/repeated requests rejected; rule tests pass; moving-target hit behavior measured and limitations documented |

Movement evidence (2026-11-03): a three-process loopback smoke test passed 45 checks covering normal/slow/crouched speeds, replicated stance/body bounds, camera height, jump/landing, walls, stairs and cover. MOVE-01/02 remain in progress until the rendered checks are complete; MOVE-03 latency/loss and SET-03 packaging are still open. See WORK_LOG.md and the repeatable command in SETUP.md.

Integrated development foundation (2026-11-03): GUN-01/02 have server/two-client evidence, and the independent HUD/session subset of PLAY-03 has rendered integration evidence. The original branches are `codex/rifle-gameplay` and `codex/hud-session`. GUN-03/04, round eligibility, network impairment, final sound/settings and packaged acceptance remain open. Ownership and chat references are in PARALLEL_WORK.md.

## M3 Complete round loop

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| RULE-01 | Implement teams and round state | In progress | GUN-04 | Preparation, active, round-end, match-end states; deterministic contested-event resolution and one score award |
| RULE-02 | Implement objective carry/drop/interact | In progress | RULE-01 | Plant/defuse eligibility and interruption validated; death/disconnect drop and recovery work |
| RULE-03 | Complete win conditions and match flow | In progress | RULE-02 | Elimination, timeout, defuse and detonation pass; six rounds, side swap, draw/win and rematch work |
| RULE-04 | Handle joins and disconnects | In progress | RULE-03 | Late joins wait safely; teams capped; underfilled rosters pause before the next round with score preserved; empty server clears abandoned match |
| RULE-05 | Run round regression checks | In progress | RULE-04 | All rule paths tested; ten consecutive rounds reset cleanly; edge-case evidence recorded |

Parallel foundation started on 2026-11-03: `codex/round-rules` owns a deterministic rules model and focused tests covering RULE-01 through RULE-05. Runtime actors, replication, physical objective placement and networked acceptance remain integration dependencies. See ROUND_RULES_BRIEF.md; starting isolated model work does not waive GUN-04 or any milestone gate.

## M4 Stylized playable slice

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| PLAY-01 | Turn test arena into one-site map | In progress | RULE-05 | Routes, timing, cover and spawn safety survive human playtest |
| PLAY-02 | Establish original art treatment | Queued | PLAY-01 | One polished section sets palette/materials/lighting; consistent readable map and team silhouettes follow |
| PLAY-03 | Complete HUD, menu, settings and sound | In progress | RULE-04 | Join flow and game state understandable; settings persist; footsteps/shots/objective audio communicate action |
| PLAY-04 | Profile and conduct usability playtest | Queued | PLAY-02, PLAY-03, SET-04 | Evidence includes screenshots, reference-hardware frame times, server timing, and new-tester observations |

Parallel map prototype: `codex/tactical-map` builds a separate Site01 map from the completed environment kit. VS_Graybox remains the regression arena. Metadata, collision checks and route measurements precede integrated human playtests; full PLAY-01 acceptance still depends on RULE-05. See MAP_BRIEF.md.

## M5 Private MVP release

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| REL-01 | Produce repeatable target builds | Queued | PLAY-04 | Packaged client/server built from recorded revision; development reset excluded; connection and startup instructions verified |
| REL-02 | Complete full 2v2 acceptance session | Queued | REL-01 | Four clients on at least two machines finish a match and rematch; disconnect and join checks pass |
| REL-03 | Resolve blocking defects and record release | Queued | REL-02 | No known blockers; limits and build identifier recorded; installation/playtest handoff works |

## Asset work in parallel

| ID | Task | Status | Completion condition |
| --- | --- | --- | --- |
| ART-03 | Create pistol and revise rifle/pistol finishes | Done | Original pistol source/export/native assets; consistent cooler weapon palette; inspected actual studio/first-person previews; rifle geometry/socket contract retained; integration limits recorded |
| ART-04 | Create an original objective device asset | In progress | Source/export/native mesh with fixed scale/pivot/material/socket contract, provenance and inspected previews; gameplay integration remains separate |
| ART-05 | First-person arms and gloves: source-ready rigged asset | In progress | Original editable source, FBX, rifle/pistol hold poses, concise source checks/previews; native import and runtime wiring remain separate |
| ART-06 | Site01 service-turn and approach-A signs | In progress | Two original source/export signs and safe additive placement recipe for R1/R2; native map changes and in-game visibility remain separate |


The user explicitly requested pistol assets on 2026-11-03 after rifle asset commit `cac8151`. This art increment does not add pistol gameplay to the single-rifle MVP. See WEAPON_ART_DIRECTION.md.

## Current coordinated follow-ups

The user authorized curating all six existing project chats on 2026-11-03. PLAYTEST_WORKSTREAMS.md assigns match runtime, round HUD, rifle presentation, Mac/LAN tooling, a small objective-device asset and a bounded read-only Site01 readability review. Source branch foundations are now merged; this does not complete runtime match or real LAN acceptance.

| ID | Task | Status | Completion condition |
| --- | --- | --- | --- |
| PLAY-05 | Review Site01 readability before first playtest | In progress | Actual viewpoint evidence and concise actionable findings; no unscheduled map edits or claim of human balance acceptance |

## Continuous integration

Engine-free checks run on GitHub-hosted runners so parallel branches are verified without the shared local Unreal slot. They do not replace editor, packaged, dedicated-server or network verification.

| ID | Task | Status | Completion condition |
| --- | --- | --- | --- |
| CI-01 | Run engine-free checks on every PR and push to main | In progress | `.github/workflows/checks.yml` merged to main; an observed GitHub Actions run passes the Python suites, standalone C++ rules/presentation tests (normal and sanitizer, g++ and clang++), `git diff --check` and LFS checks |
## Server and hosting path

Proposed on 2026-11-03 in [decision 0002](decisions/0002-server-hosting.md): one disposable server process per match, grown in stages. SRV-01/02 unblock the existing SET-03 packaged-server gate; they do not replace it.

| ID | Task | Status | Depends on | Completion condition |
| --- | --- | --- | --- | --- |
| SRV-01 | Obtain Unreal source access | Blocked | Account owner links Epic and GitHub and accepts the EpicGames organization invitation | `EpicGames/UnrealEngine` is reachable from the project account; the exact 5.8.4 source reference and commit are recorded in SETUP.md |
| SRV-02 | Establish a Linux server build environment | Queued | SRV-01 | A recorded Linux CI runner or VM (official Epic image or pinned source build) packages `ValStrikeServer` for Linux from a recorded revision; exact commands, disk use and build time in SETUP.md |
| SRV-03 | Add the provider-neutral server contract | Done | SET-02 | Allowlisted startup parameters, lifecycle states with a logging `Local` provider, opt-in exit-when-done, pluggable `PreLogin` admission, startup version line and structured match result; existing smoke suites still pass; a server/client run shows a clean exit after match completion and after an empty timeout |
| SRV-04 | Run several matches on one host | Queued | SET-03, SRV-03 | Two or more packaged Linux servers on one machine with separate UDP ports and their own clients complete concurrent matches; per-match CPU, memory and bandwidth measured at the recorded tick rate |
| SRV-05 | Choose fleet orchestration and backend | Deferred | REL-03, NEXT-05 | Player demand, regions and budget agreed; orchestrator, matchmaking and identity choice recorded in a new decision |

SRV-03 evidence (2026-11-03): implemented on `claude/srv-03-server-contract`. The lifecycle runner (invalid launch, empty timeout, match completion), the movement, combat and match suites, and the rendered UI suite pass on the Editor binary. Packaged and Linux server behavior remain SRV-01/02 and SET-03 work. See WORK_LOG.md and the server section of SETUP.md.

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

### BUG-01 — Reversed mouse look

- Severity: P1 for the current playtest controls.
- Status: Fixed in `8e81e84`, included in the combat/UI integration. Rendered axis-event checks pass for all four directions, pitch limits and focus return. Physical mouse and manual Escape/resume acceptance remain part of the broader movement/UI checks.
- Reported: 2026-11-03 by the user. Exact running build/affected axis not yet confirmed; the same mouse-look functions exist in integration `4fbef78` and the inspected gameplay worktree.
- Reproduction: enter the game and move the mouse horizontally and vertically. User reports reversed camera movement.
- Expected: right/left input turns right/left; up/down input looks up/down with normal non-inverted FPS defaults.
- Verified cause: Unreal supplies positive MouseY for upward movement with legacy scales disabled; `VSCharacter::LookPitch` negated it again. Removed that extra sign; yaw and sensitivity are unchanged.
- Completion: minimal fix, compiled C++, rendered verification of all four directions and pitch limits, focused evidence and separate commit where practical. Existing scripted movement checks force camera rotation and do not cover this defect.
- Next action: complete the physical-mouse/menu acceptance pass on the integrated build. Keep the rendered regression for future input changes.
