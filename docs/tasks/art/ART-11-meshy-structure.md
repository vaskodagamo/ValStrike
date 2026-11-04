# ART-11 — Meshy Site01 structure cleanup

- Status: Source prepared with repair holds; local source `a4f44f1`, handoff `f9e19da`, 2026-11-04. Not published or native-integrated.

Workflow update: follow [shared Meshy skill](../../../Art/Skills/valstrike-meshy/SKILL.md) for any future assigned programmatic service work. This completed cleanup pass has no paid stages and a zero-credit allowance. Preserve the current handoff; do not regenerate or rerun the task merely to adopt the new workflow.

- Backlog: PLAY-02, approved Meshy world-asset cleanup.
- Owner: Codex chat “Art — Site01 navigation signs” (`01a1a5af-0f13-78f1-8aaa-1903c1392df7`), High reasoning. Previous task is frozen; leave its old worktree/commits unchanged.
- Worktree: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/meshy-structure`; branch `codex/meshy-structure`.
- Base: coordinator dispatch commit descended from `bca21d037077b0a6e8645fbc3ada611e4be49439`; read exact base using `git rev-parse HEAD` before editing and record it in handoff.
- Owns ONLY: `Art/Source/Environment/MeshySite01/Structure/`, `docs/assets/meshy-site01/structure/`, `docs/workstreams/meshy-structure.md`.
- Read-only: canonical `/Users/webproduktion3/Documents/ValStrike/meshy_ai_assets/world_objects/`, intake manifest, map handoff/layout, IndustrialV2 and ModularIndustrialV1 references. Never edit the coordinator checkout or another worktree.
- Input inventory: [manifest](../../assets/meshy-site01-intake.json); verify recorded hashes before reading/importing.

## Exact assigned inputs

- MSH-W01: `Meshy_AI_Concrete_Grid_Panel_20261104_texture_0e1290ef/` (all five files).
- MSH-W05: `Meshy_AI_Industrial_Corner_Barrier_20261104_texture_d31f9a34/` (all five files).
- MSH-W06: `Meshy_AI_Industrial_Corner_Barrier_20261104_texture_e89dd755/` (all five files).
- MSH-W08: `Meshy_AI_Industrial_Portal_20261104_texture_31514e26/` (all five files).
- MSH-W09: `Meshy_AI_Industrial_Security_Barrier_20261104_texture_f32de489/` (all five files).

## Deliverable and stopping condition

Inspect actual imported geometry first; generated folder names are not verified shape identities. Record triangle count, object/material count, bounds, UV/textures and notable defects once per asset. Use installed Blender, preserve the generated visual design, and repair scale, warped flat mating planes, pivots, shading and needless density. Do not replace these inputs with generic primitive artwork or discard their texture/detail quality. Keep originals unchanged in the canonical inbox. Author cleaned editable .blend, game FBX and required textures under your owned source path, with a concise machine-readable per-asset manifest and provenance. Avoid committing duplicate raw input archives.

Start with one representative wall/corner, inspect a neutral render against approved references and preserve a before/after image. If quality matches the approved style and defects are understood, continue the remaining assigned inputs; if it cannot be made acceptable without redesign, stop that asset and report the concrete issue. Derive compatible profiles from one cleaned master. Preserve the approved restrained grey/graphite/crimson palette; use paint as optional material masks where practical. Record normal-map convention and texture usage, not just filenames.

Dimensions in the reference guide are proposals. Align structural mating profiles; document actual envelopes for integration. Existing Site01 cover/route/door/collision requirements win. No map layout changes. Use base-corner structural pivots, top-surface corner for floor tiles, base-center for crates; document axes and export units. Avoid a flat collision hull blocking a doorway. Source collision intent can be authored here; only the future native owner imports it.

Use the prior triangle budgets as starting targets: walls/corners 200–2,000, portals/doors 500–3,000, floor slabs 50–500, crates 2,000–6,000, simple stairs 300–1,500. These are visual-quality budgets, not reasons to destroy silhouette or create a new benchmark. Inspect high-detail originals and preserve a useful source if further optimization is needed. Work at sensible preview resolution; one Blender process at a time in your own task.

## Checks and handoff

V1 source-only work: inspect actual source/render/export, scale/pivots/material assignment and file references, then `git diff --check`. Reuse existing targeted validation only if it catches a concrete defect. No new per-asset test suite, C++ build, Unreal launch/import, full regression or engine-slot retry. No native Content, gameplay, shared material library, central docs or asset register edits. No recursive delegation.

Commit only your assigned paths locally. Hand off commit, cleaned asset paths, real previews, checks run, remaining defects and native-import instructions in `docs/workstreams/meshy-structure.md`. Coordinator reviews/publishes; user merges. Stop after source-ready handoff. A single native integration owner will later combine both batches and place them into existing Site01; native import/runtime verification is pending, not part of this worker's completion claim.
