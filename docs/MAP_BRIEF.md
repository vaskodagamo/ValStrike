# First tactical map

Updated: 2026-11-03. Independent PLAY-01 prototype; full acceptance still requires integrated rounds and a human playtest.

## Assignment and ownership

Work in `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/tactical-map` on `codex/tactical-map`. Read AGENTS.md, README.md, MVP_PLAN.md, BACKLOG.md, PARALLEL_WORK.md and SETUP.md. The integration chat prepares this checkout with completed environment-kit commit `8acb0d4` included.

Own only:

- `Content/Maps/Site01/` with map `/Game/Maps/Site01/VS_Site01` and any map-specific presentation assets.
- `Art/Source/Maps/Site01/` for layout sources and editor generation/inspection scripts.
- `Tests/Maps/Site01/` for meaningful geometry/metadata checks if needed.
- `docs/maps/site01/` for layout metadata, route measurements, previews and handoff.
- `docs/workstreams/tactical-map.md` for an append-only work log.

Reuse the existing environment kit without editing its source meshes or shared materials. The original asset chat retains those files. Do not modify VS_Graybox, bootstrap materials, default-map configuration, shared launchers, gameplay code, project/build files or central documentation. Select the new map explicitly when testing. Do not merge other active workstreams.

## Deliverable

Build one compact original 2v2 layout with two attacker approaches, one meaningful flank, one objective site and four safe spawns. Keep player movement scale and useful sightlines ahead of decorative detail. Use the completed walls, pillars, crates and cover plus simple native geometry where required. A restrained original industrial courtyard is a suitable starting theme; do not reproduce a known competitive map.

The user wants **Valorant-like stylized graphics**, with clean geometric forms, controlled color areas, readable silhouettes and simple materials. Use ordinary lighting without requiring Nanite, Lumen or ray tracing. Keep this a small playable prototype; no new weapon, character, animation or full art-production scope.

Document routes and sightlines in an overhead diagram and capture actual Unreal eye-level views. Show spawn exits, first-contact positions, cover and plant area. Measure route distances and report calculated travel estimates using the current 450 cm/s normal speed separately from actual traversals. Verify bottlenecks, crouched/standing clearance, cover collision, flank usefulness, spawn safety and reachable recovery point. Do not label estimated timings or editor queries as human playtest evidence.

## Map-to-rules contract, version 1

This is authored metadata for the later integration adapter, not working objective gameplay. The rules core consumes server-validated eligibility and does not load map assets or this JSON.

Write `docs/maps/site01/layout.json` with `schema_version: 1`, `map_asset: "/Game/Maps/Site01/VS_Site01"`, `units: "cm"`, and these fields:

- `spawns`: four entries with unique `id`, `role` (`attacker` or `defender`, two each), `actor_label`, `location_cm` ([x,y,z]) and `yaw_degrees`. These are spawn roles, not persistent team identities; teams swap roles after round three.
- `site`: `id: "A"`, `actor_label`, `center_cm` and `half_extents_cm` (positive axis-aligned bounds). Mark one visible plant area; record bounds rather than relying on a decorative decal.
- `device_recovery`: `actor_label` and `location_cm`, on safe reachable ground.
- `playable_bounds`: `center_cm` and `half_extents_cm`, documenting the permitted area for the later recovery check. If simple bounds include unsafe holes, document those and avoid relying on bounds alone for reachability.

Use four real PlayerStart actors: two with Actor Tag `VS_AttackerSpawn` and two with Actor Tag `VS_DefenderSpawn`. Each actor has exactly one role tag and a unique label matching the JSON. Tag a simple editor marker `VS_Site_A` and the recovery marker `VS_DeviceRecovery`; marker labels also match the JSON. Tags belong to Actor Tags, not PlayerStartTag. Site dimensions come from the exported metadata. Markers must not create blocking collision. No new C++ gameplay actors are required for this deliverable.

Save and reopen the map through Unreal Editor. Verify the four spawn transforms, tags, site bounds and recovery marker against the actual saved actors. Include reproducible inspection results and exact launch/generation commands. A baseline GameMode may ignore these tags until integration; do not claim team-specific spawning or plant/defuse works yet.

## Verification and handoff

Use pinned Unreal 5.8.4 and Xcode 26.1.1. Build through Scripts/ue.py with its existing mutex. Keep generated outputs local to this checkout, close only your own processes, and avoid leaving an unnecessary editor running on the shared 24 GB Mac.

Maintain the stream log with evidence, failures and next steps. Record original asset provenance and any new inputs. Commit and push only `codex/tactical-map`; do not merge into integration. Hand back the commit, asset paths, overhead/eye-level previews, measurements, verified collision/clearance results and untested gameplay questions. Full PLAY-01 acceptance remains open until integrated human playtests.
