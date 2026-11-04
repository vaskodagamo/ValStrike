# INT-ART-01 — Meshy character and Site01 first playtest

- Status: Reserved for the user's **new Claude integration chat**. Start with engine-free preflight; native work waits for the source readiness gates and INT-04's engine-slot release.
- Backlog: PLAY-02 / PLAY-04. Result: the reviewed Meshy character and approved props visibly working in existing Site01, one actual local server/two-client acceptance run, and exact other-Mac playtest instructions.
- Owner: one new Claude session launched by the user from the prepared checkout. No additional workers. Codex coordinator/art lead owns this assignment and central status; existing Claude engineering lead retains its own sub-board and current gameplay tasks.
- Branch/worktree: `claude/int-art-01-meshy-playtest`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/int-art-01-meshy-playtest`.
- Base: main `ee25083298801008291747e710b078bf6ea9563b` plus coordinator kickoff. Record exact prepared HEAD, accepted art branch heads and final combined candidate before native work. Update to later main only if necessary; assess affected changes.

## Exclusive write ownership

This is the coordinator's explicit native-art/shared-source exception to the ordinary engineering boundary. This single chat owns:

- New native assets under `Content/Art/Characters/UrbanEntry01/` (mesh, skeleton, physics/materials/textures, approved animation sequences/Animation Blueprint) and `Content/Art/Environment/MeshySite01/PropsSurfaces/` (approved four IDs only).
- `Content/Maps/Site01/VS_Site01.umap` and `Content/Maps/Site01/Materials/` for bounded visual integration, preserving all layout/collision/marker contracts.
- New reproducible editor import/placement scripts under `Art/Integration/MeshyPilot01/`; do not rerun the original whole-map generator and erase existing work.
- `Source/ValStrike/VSCharacter.h` and `.cpp` **only** for presentation initialization, replacing/hiding prototype visuals and hooking the native character/animation class. Do not change input, movement/replication, camera, health, authority or capsule rules.
- New `Source/ValStrike/Presentation/VSCharacterPresentationComponent.h/.cpp` and `VSCharacterAnimInstance.h/.cpp` if genuinely needed; prefer existing engine presentation features.
- `Source/ValStrike/Presentation/VSWeaponPresentationComponent.h/.cpp` **only** for world-model attachment to the reviewed hand socket/pose. Preserve first-person transforms, recoil/flash/loadout behavior and authority. Existing rifle/pistol meshes are read-only.
- `docs/assets/meshy-pilot-integration/`, `docs/playtest/meshy-pilot.md`, `docs/workstreams/int-art-01-meshy-playtest.md`.

All other paths are read-only. In particular: gameplay/HUD/Combat/Damage/Match files, gameplay sub-board, Config defaults, art source folders, central docs/registry and other checkouts. ART-15/16 exclusively own source changes until their frozen handoffs. GPT-10 owns HUD reticle edits; GPT-11 owns weapon request validation. Ask the coordinator to resolve any genuinely necessary cross-owner change rather than taking over a path.

## Readiness and scheduling

Read AGENTS.md, CLAUDE.md, TEAM_WORKFLOW, the newest TEAM_BOARD entry and this brief. Inspect the **live** engineering board read-only at `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-gameplay/docs/teams/GAMEPLAY_BOARD.md`; committed root copies may lag. INT-04 currently owns the UE slot for settings verification. This task is next in the art queue after its recorded release; a free-looking lock alone is not permission to interrupt an assigned run. Use `Scripts/with_ue_slot.py` for every Unreal import, preview, build and runtime job. Exit 75 means stop/return to the queue; never loop, poll, bypass or kill someone else's processes.

Prepare a minimal import/animation/placement plan now using the read-only handoffs in the original `meshy-character` / `meshy-props-surfaces` worktrees and current map/presentation code. Do not revalidate those entire source pipelines. Actual import gates:

1. ART-15 branch `codex/art15-character-pilot` has frozen source plus `docs/assets/urban-entry01/pilot-readiness.json` and `docs/workstreams/art15-character-pilot.md` declaring the exact usable subset.
2. ART-16 branch `codex/art16-props-pilot` has frozen source plus `docs/assets/meshy-site01/props-surfaces/pilot-readiness.json` and `docs/workstreams/art16-props-pilot.md` declaring approved props/placement recipe.
3. Accept only those exact source/readiness revisions and IDs. Inspect branch diff/ownership, bring them into **your integration branch only**, retain LFS assets and record resulting candidate SHA. User PR merge is not required to assemble this disposable candidate, but nobody may merge remote main. Do not read mutable uncommitted art as acceptance or blindly combine later changes.

If one batch is held, integrate the other approved batch and report the missing dependency honestly. Do not mark the full pilot ready until both character and approved props are working. If an input or slot is pending, finish independent preflight and record the specific gate; no empty repeated build/check attempts. The coordinator/user can resume on the handoff/release.

## Implementation

Import real editor-owned assets, save and reopen. Verify native centimeter scale, orientation/left-right, material color spaces and normal convention against the source notes. Never fabricate binary assets. Place a representative set of approved tile/crates/panel in existing `/Game/Maps/Site01/VS_Site01` following ART-16's recipe; verify actual placements against map geometry. Held structures and stairs stay out. Preserve cover height, routes, floor elevation, collision and spawn/site/device/recovery metadata. Keep VS_Graybox and default map settings unchanged; use explicit Site01 launch instructions.

Replace third-person prototype body/head with the reviewed character. Keep owner-no-see behavior and the existing first-person camera/weapon presentation. Drive animation from the existing replicated pawn movement state; movement remains server-authoritative CharacterMovement. Use only approved in-place pilot clips/idle/hold. No root-motion-driven displacement, skeleton-wide rerig, vault/sprint mechanics or invented missing animations. Document temporary gait/direction/idle fallbacks. Fit world rifle and pistol attachment without disrupting firing or authoritative hit zones. Preserve collision and hit-zone gameplay; compare visual alignment and report a real mismatch rather than silently changing damage volumes. Do not advertise new first-person arms.

## Verification and delivery

Tier V1 presentation integration with central real multiplayer acceptance. Reuse existing evidence; build the combined candidate **once if C++ changed or its binary is missing/stale**, then only affected checks. Do not add a parallel test framework or run every historical suite. Native asset import/reopen and rendered inspection are required even without C++ changes. Run existing affected presentation/map checks where useful; preserve their original assumptions or explain why a prototype-specific check needs its owning maintainer.

Observe an actual local server and two rendered clients on Site01: the remote new character, idle/movement/crouch/fallbacks, weapon switching and attachment, death/respawn visibility, absence of first-person self-occlusion; walk the changed prop areas and confirm collision/cover and objective/spawn access. Capture concise genuine in-game screenshots/video and record candidate, engine/platform, commands, actual results and limitations. No fabricated screenshots, unit-only replication claim or unobserved network acceptance. Rerun only failures or affected changes. Other-Mac LAN, packaged and cross-platform checks remain separate.

Deliver a pushed main-targeted PR and `docs/playtest/meshy-pilot.md` with exact tested revision/branch, Git LFS checkout steps, pinned UE 5.8.4/Xcode 26.1.1 source-build requirements, verified Site01 launch commands, how a solo tester can see the third-person character, and the known temporary animation limitations. Do not promise the new assets appear by pulling main before the source/native PRs merge. User merges. Use one concise task handoff; no central-status edits or further agents.
