# Coordinated work toward the first Mac playtest

Updated: 2026-11-03 (Europe/Berlin). The user explicitly asked the integration chat to curate the existing project chats and keep them on useful work. This assignment supersedes the first-batch file ownership in PARALLEL_WORK.md. Do not create duplicate work or silently expand the single-rifle MVP.

## Common starting point and handoff

The coordinator prepared `codex/playtest-foundation` with reviewed combat/HUD, rules `baf2405`, map `3f0405f` and weapon assets `71c80e9`. Remote main independently received those last three branches in PRs #4–#6 during preparation; their history is included. Foundation verification and the exact start commit are recorded in WORK_LOG.md and each kickoff message.

Every continuing chat gets a NEW checkout below `.parallel-worktrees/` and a new `codex/` branch. Leave completed first-batch checkouts and branches as evidence. Always run commands in the explicitly assigned checkout, even when the app shows the parent project path. The coordinator alone owns shared contracts, central docs/backlog/log, main/integration merges and shared attachment wiring.

Commit each reviewable deliverable locally on its assigned branch, with exact checks and limitations in its own append-only log. The coordinator will inspect commits and handle publication/PRs; a local finished commit is a valid handoff. Do not wait on a push approval, edit another checkout, merge another active stream or ask for account setup. Stop at the scoped deliverable; a bounded review may finish sooner than implementation work.

## Assignments

| Existing chat | New checkout and branch | Task / exclusive ownership |
| --- | --- | --- |
| Implement round and objective rules | `.parallel-worktrees/match-runtime`, `codex/match-runtime` | RULE-01–05 runtime: `Source/ValStrike/Match/`, `Rules/` fixes only with tests, `VSCharacter.h/.cpp`, `Combat/VSHealthComponent.*`, `Combat/VSRifleComponent.*`, new `Tests/Match/`, `Scripts/test_match.py`, `docs/workstreams/match-runtime.md` |
| Build HUD and session controls | `.parallel-worktrees/round-hud`, `codex/round-hud` | PLAY-03 round display: `VSHUD.h/.cpp`, new `UI/Round/`, `Tests/UI/Round/`, `docs/workstreams/round-hud.md` |
| Implement rifle gameplay | `.parallel-worktrees/rifle-presentation`, `codex/rifle-presentation` | GUN-03 visual foundation: new `Source/ValStrike/Presentation/`, `Content/Presentation/Rifle01/`, `Tests/Presentation/`, `Scripts/test_rifle_presentation.py`, `docs/workstreams/rifle-presentation.md` |
| Build the first tactical map | `.parallel-worktrees/mac-playtest`, `codex/mac-playtest` | Mac transfer/launch tools: `Scripts/ue.py`, `Tests/test_ue.py`, new `Scripts/check_playtest.py`, `Tests/Playtest/`, `docs/playtest/`, `docs/workstreams/mac-playtest.md` |
| Create pistol and upgrade weapon colors | `.parallel-worktrees/objective-device`, `codex/objective-device` | ART-04 device asset: `Art/Source/Objectives/Device01/`, `Content/Art/Objectives/Device01/`, `docs/assets/device01/`, `docs/workstreams/objective-device.md` |
| Add the first game assets | `.parallel-worktrees/site-readability`, `codex/site-readability` | Bounded PLAY-01/02 review: only `docs/reviews/site01/` and `docs/workstreams/site-readability.md`; map and asset contents are read-only |

The base player controller, `VSMatchHUDData.h`, `VSHUDData.h`, build/project files, default configuration, existing smoke harnesses and all other files stay coordinator-owned. In particular, rifle presentation must not edit Character/Combat and round HUD must not implement a second GameState or objective RPC path. Record necessary shared changes as a precise handoff rather than taking file ownership.

## Match runtime: critical path

Implement `AVSMatchGameMode` and `AVSMatchPlayerController` under `Match/`, deriving from the existing VS types, with replicated GameState/PlayerState/objective actors. Preserve VS_Graybox practice behavior and its existing smoke suites. Select the match mode through an explicit server URL override; do not edit the map, its WorldSettings or default config in this stream. The reflected game-mode path is `/Script/ValStrike.VSMatchGameMode`.

Own the tested rules model in the authoritative server. Follow its exact sealed timestamp/event ordering, continuous interaction eligibility, replay identity, placement acknowledgement and effect-deduplication contract. Gate damage/fire/reload before applying gameplay effects at expired deadlines; merely changing the score afterward is insufficient. Preserve existing shot validation/history. Gate match movement/combat while retaining practice behavior outside match mode.

Use the actual Site01 actors and fixed map contract v1: role Actor Tags, site A TriggerBox bounds and recovery TargetPoint. Do not read `docs/maps/site01/layout.json` at runtime; docs are not packaged gameplay data. Validate actual distance/site eligibility, collision/floor support and safe reachable device placement. A bounds-only placement check is insufficient.

Create server-owned fresh pawns for real round resets (full health/fixed ammo). Existing DevelopmentReset controls remain opt-in testing tools. Implement team caps, minimum one per team for development/two for 2v2, safe spectators/late joins, carrier drop on death/disconnect, interrupted plant/defuse, all wins, one award, role swap after three, six rounds, draw, explicit rematch and empty-server clear. Use a configurable 3-second result display before the server issues NextRound; do not change model timing values. At MatchComplete, a validated connected participant may request the private-playtest rematch; coalesce duplicates and validate current match state.

Controls are frozen for this batch: hold E to plant/defuse; press E to retrieve an eligible nearby dropped device; G to drop; Enter to request rematch when offered. The match controller owns bindings/RPC validation. Menu opening, death, disconnect, loss of eligibility and key release must cancel the relevant held interaction. A new press is required after cancellation. Keep Escape/session focus behavior intact.

Use a simple primitive for the device until ART-04 lands. Expose an optional device mesh setting so final art does not block gameplay. Override the base controller hooks described below. Run separate server/client tests for each win path and invalid actions, interrupted interactions, role spawns, deadline ties, reset, late join/disconnect, six-round match/rematch and at least ten consecutive rounds. Record actual network evidence; unit tests alone do not finish this assignment. Publish a first compiling adapter commit before extending tests if useful for review, but continue to the scoped handoff.

## Shared HUD contract, version 1

`AVSPlayerController::GetMatchHUDData() const` is a virtual, pawn-independent read-only provider returning `FVSMatchHUDData` from `VSMatchHUDData.h`. Practice mode returns `bAvailable=false`; hide match-only UI in that case. The match controller overrides it using authoritative replicated state, including when no pawn exists or the player is spectating.

Fields cover phase, persistent LocalTeam/AttackingTeam, team scores, completed/total rounds, winner/draw, spectator state, objective state/carrier, eligible interaction prompt, active/progress state and status text. `bHasPhaseDeadline`, `ServerNowSeconds` and `PhaseDeadlineSeconds` share synchronized server seconds; the HUD formats a clamped remaining duration without inventing a local authoritative clock. Team identity must remain stable when roles swap. Unavailable data is not zero-health, a result, or a made-up timer.

`CancelObjectiveInput()` is a virtual base-controller hook already called before menu focus and during restart with an open menu. Its practice implementation is empty; the match controller must cancel local intent and send a validated fresh server cancellation as appropriate. `RequestMatchRematch()` is also virtual/empty in practice; the match controller validates the real request. `bCanRequestRematch` controls presentation eligibility, not server authority.

The HUD chat builds readable waiting/preparation/active/planted/result/spectator/complete displays with timer, scores, roles, objective/interaction progress and the Enter-rematch hint. Preserve existing health/ammo/menu behavior, 960×540 readability and keyboard cues. No new menus/settings, fake state in normal play, or domain/rules logic. It may use explicitly development-only local fixtures in its own test namespace to render all states; label those as presentation tests. Real replication acceptance follows with the match adapter. Record any required shared startup/test-hook wiring for the coordinator.

## Rifle presentation

Deliver an attachable native visual component with an explicit initialization/attachment API. Use the accepted R01 body/magazine assets and existing Grip/Muzzle/SupportHand/Magazine socket contract; preserve their source/native geometry and materials. Own only new presentation code/content; runtime owns Character/Combat changes. The coordinator will attach the component in shared character construction after review. Verify using a self-contained opt-in harness or temporary test level within the assigned scope.

Read `GetHUDData()` and the public rifle `GetState()`/`ShotsFired` accessors. Initialize shot/reload/life caches on attachment so spawn/rejoin never replays old effects; reset cleanly for a new pawn. Cosmetic feedback may follow accepted-shot state in this first deliverable; document the response-latency limitation. Do not add competing shot RPCs or change damage, aim, fire cadence, sensitivity or camera control rotation. Visual weapon kick/sway must not move the authoritative aim.

Show the weapon in first person, a world representation for observers where practical, a muzzle flash and simple magazine/reload movement. Keep collision disabled and owner/remote visibility correct. Any audio must be original or licensed with recorded provenance; absence of suitable audio is an explicit remaining item, not a reason to fetch an unknown copyrighted sound. No pistol mechanics, weapon switching or full arms rig. Capture actual runtime views at neutral/dim light and verify menu/fire release, reload/death cleanup and no duplicate remote effects. Do not claim full GUN-03 from visual attachment alone.

## Mac source checkout and LAN preparation

The other Mac is Apple silicon with Unreal already installed; its exact version/toolchain and real LAN behavior are not yet verified. Prepare a source-project route using pinned UE 5.8.4, Xcode 26.1.1 and a working Metal compiler. Packaging/source-engine access is a separate open gate, not a prerequisite for this editor LAN test.

Add safe allowlisted launch choices for `graybox` and `site01`, preserving graybox defaults and existing tests. Server mode choices are `practice` and `match`; the latter uses only the fixed `/Script/ValStrike.VSMatchGameMode` URL override and must fail clearly if unavailable. Do not accept arbitrary URL/console options. Editor map selection is for inspection; do not silently rewrite world settings. Existing clients join by validated host:port.

Improve preflight to reject unresolved Git LFS pointers/missing map dependencies, confirm exact tools and identify the checkout revision. Test path/argument/error behavior. Rehearse clone + LFS retrieval + native build from a separate temporary checkout, keeping evidence and avoiding generated-file copying. Use the engine slot for build/runtime work. Do not edit another chat's worktree or the system firewall/network configuration.

Write concise instructions/checklist under `docs/playtest/`: clone/LFS, version checks, doctor/build, starting one server and two clients, choosing Site01/match once the class is integrated, host LAN IPv4:7777, inbound app firewall permission and non-isolated LAN. Never use loopback as the remote Mac's host address. Record a failed-join/log collection path and a bug-report template with revision, machine, inputs, expected/actual and screenshots. Local tests are not a real two-machine verification; the user's session will supply that evidence. Keep central SETUP/README edits as a coordinator handoff.

## Objective device art contract

Create one original stylized compact device, matching the established simplified materials/readable forms. Target envelope 30 cm long (X), 18 cm wide (Y), 10 cm tall (Z); bottom-center pivot, +X forward, Z up, scale 1. Native name `SM_VS_Device01` under `/Game/Art/Objectives/Device01/Meshes/`. Keep under roughly 4,000 triangles and report actual bounds/counts. Provide one `Carry` socket at local (0,0,8) cm, identity rotation/scale, and material slots in order `Shell`, `Trim`, `Status`.

Provide a small status surface and restrained material variants for idle/armed/defused feedback; do not rely only on color to communicate gameplay. The runtime actor owns collision/placement, so the visual mesh uses NoCollision. Keep editable source/export/native assets, provenance, saved/reopened measurements and actual neutral/dim previews. No mechanisms, VFX system, gameplay actors or changes to rifle/pistol art. The match chat uses a primitive until this asset is reviewed.

## Bounded Site01 readability QA

Review the existing original map from actual player eye heights and representative first-contact/corner/site positions. Check route/site recognition, visual separation of dark player/weapon silhouettes, cover readability and obvious misleading collision cues. Reuse existing evidence before launching an editor; obtain only the additional native captures needed. A clearly labeled temporary proxy silhouette is allowed in an unsaved test world, not as a claimed final character.

Produce a short evidence-backed list of actionable issues (viewpoint, screenshot, player consequence, smallest suggested fix), plus strengths that should be retained and a verdict for first playtest readiness. No speculative scorecard, second map, broad art pass, production map/material edits, or repeated review when no issue is found. This bounded QA task may finish quickly; stop after a useful report. The coordinator decides any subsequent fixes.

## Shared machine and completion gates

Use `python3 Scripts/with_ue_slot.py -- <command> ...` around every Unreal build, map import/render and multi-process runtime test. It takes one cross-worktree engine slot, fails immediately if busy, and preserves the child exit code. A busy slot means continue source/tests/docs work and retry later; do not poll rapidly, bypass the lock, or launch untracked editors. All new checkouts share a 24 GB Mac. Close only processes you started. Lightweight source/unit tests may run in parallel. Wrap the whole foreground runner, not each nested child; runners remain responsible for cleaning up their own descendants. Reserve loopback ports 7831–7834 for match runtime, 7841–7844 for HUD, 7851–7854 for presentation and 7861–7864 for Mac tooling.

The coordinator checks task snapshots, reviews meaningful finished commits and integrates one scoped stream at a time. Required pre-playtest rehearsal: same-source local server/two clients, useful visible weapon, correct team spawns, objective interactions/wins/reset, round HUD, six rounds/rematch, and clean source/LFS retrieval. The user's second Mac provides the first real LAN result. None of this claims packaged, Windows/Linux or public-service acceptance.
