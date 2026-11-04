# ART-16 — Four Meshy props for the Site01 pilot

- Status: Ready; new Codex environment art chat, High reasoning; coordinator/art lead delegates this bounded visual review and finishing pass.
- Backlog: PLAY-02. Result: reviewed MSH-W03 tile, W04 long crate, W07 wall-backed infill panel and W10 cube crate, with an actionable Site01 placement recipe for INT-ART-01.
- Branch/worktree: `codex/art16-props-pilot`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art16-props-pilot`.
- Base: main `ee25083298801008291747e710b078bf6ea9563b`, coordinator kickoff, then source `901b558692c072a8d7bb6cdd4582235af8751c8f` and handoff `2bddfdd1e7b92d2204ef98ab66d5193e51a03dd5` cherry-picked by coordinator. Record the exact prepared HEAD before editing.
- Exclusive writes: `Art/Source/Environment/MeshySite01/PropsSurfaces/` except `MSH-W02/`; `docs/assets/meshy-site01/props-surfaces/`; `docs/workstreams/art16-props-pilot.md`. Do not overwrite historical ART-12 evidence or run a generator that rewrites held W02. Old ART-12 owner is stopped; its checkout is read-only.
- Read-only: canonical inbox, IndustrialV2/ModularIndustrialV1 references, ART-12 handoff/evidence, `docs/maps/Site01/` and `Art/Source/Maps/Site01/` layout. No other worker checkout edits.
- Engine slot: none. No Unreal launch/import/build, native assets/map, gameplay, shared docs/register or new tests. No recursive delegation/new chats.
- Publication: commit, push your branch and open one PR targeting **main**, including the existing source delivery and the review/fixes; user merges. Freeze with one concise handoff.

## Work and stopping condition

Read TEAM_WORKFLOW, board, brief and ART-12 handoff. Reuse the five Blender reopen/four FBX round-trip results; do not rerun the old pipeline as a review ritual. Inspect actual saved before/after, tile patch, crate stack and panel-back renders against the approved style. Check source-specific risks that affect a first native import: visible reduction/bake damage, tiling seams, roughness/normal interpretation, dimensions/pivots and the open-backed panel. Make bounded fixes only where a real defect prevents the pilot; inspect/export only affected assets.

Keep the current appearance and texture detail. Give a per-asset `ready` or `hold` verdict with evidence. No paid Meshy stage or auth check (zero credits); use the shared skill only for future assigned programmatic service work. W02 stairs and all ART-11 structures remain excluded. Do not restart retopology or optimize already acceptable assets merely to hit an arbitrary triangle number.

Provide a small, concrete placement recipe for the existing Site01 map: current actor labels/coordinates, target transform, expected envelope and material/normal settings for a few representative uses of each approved asset. Preserve current floor elevation, 120 cm cover, sightlines, spawn/site/recovery tags and routes. A 100 cm crate does not replace 120 cm cover silently; select a matching existing envelope or safe decorative location. Keep the panel backed by a wall. Retain validated collision and record whether the visual should be collision-disabled; do not propose invisible blocking volumes larger than the visible cover. Do not rebuild or redesign the map.

Deliver `docs/assets/meshy-site01/props-surfaces/pilot-readiness.json` with schema_version, task_id, status (`ready`, `partial`, `hold`), source_commit (frozen asset commit preceding the readiness-record commit), exact approved asset IDs/FBX/texture paths/hashes, dimensions/pivots, normal convention, per-asset findings, placement recipe path and remaining native checks. Source and readiness commits must be ancestors of the final pushed branch. This delegates **pilot art approval**, not native/runtime or final-production approval.

Checks: existing visual evidence plus targeted inspection of actual changes, source/export correspondence only where changed, diff and LFS handling. No new suite, benchmark, wholesale render/export repeat or build. Handoff in `docs/workstreams/art16-props-pilot.md`: source/readiness commits, PR, approved subset, evidence, checks run, deferred native checks and exact next action for INT-ART-01. Freeze after publication.
