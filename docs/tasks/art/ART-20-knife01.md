# ART-20 — Meshy K01 knife visual asset

- Status: Ready; user approved 30 Meshy credits for this knife task on 2026-11-04, separate from the rifle/pistol allocation.
- Backlog: PLAY-02, user-requested knife alongside the new rifle/pistol/hands assets. Owner: new Codex knife art chat, High reasoning.
- Branch/worktree: `codex/art20-knife01`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art20-knife01`.
- Base: main `b342ca91e894e2be449895dd5b926f5e6b487939` plus coordinator kickoff; exact clean HEAD supplied at dispatch.
- Exclusive writes: `Art/Source/Weapons/Knife01/`, `docs/assets/knife01/`, `docs/workstreams/art20-knife01.md`.
- Read-only: approved Urban Entry character/glove reference, existing weapon palette and frozen character hand size; active FPArms02/R02/P02 sources remain owned by their artists.
- Validation: V1 source/scale/grip and export inspection only. No Unreal operations, new test suite or recursive delegation.
- Publication: commit/push one PR targeting main; user merges. Stop at a concise source-ready handoff.

## Result and visual direction

Create an original **fictional game knife prop using Meshy**, followed by Blender cleanup and hand-fit review. A compact, readable fixed-blade silhouette, sculpted charcoal grip, cool satin steel blade, subtle gunmetal separation and small burgundy accent should belong beside R02/P02 and Urban Entry. Keep clean bevel highlights and shaped surfaces; no branded copy, ornate fantasy silhouette, giant blade, photoreal grime or voxel blocks. Initial total visual envelope around 26–29 cm, then confirm proportions and hand clearance against the actual character rather than treating numbers as acceptance. This task supplies visual geometry, not functional fabrication details.

Use built-in imagegen for one isolated assembled knife in a clear three-quarter view on a plain background, entire silhouette visible, no hand/sheath/text/ruler/contact sheet. Save prompt and image; inspect it against the shared tactical style before submission. The input must be the weapon alone. Meshy produces the draft; clean geometry/materials/UVs/shading and grip fit locally, preserving its design. Do not substitute a procedural-only model.

Deliver editable .blend, game FBX, required textures/materials, neutral and same-scale hand-fit previews, measured bounds/units/triangle/material counts, source/export hashes and provenance. Use centimeters, +X along the blade and +Z up, a clear grip-local pivot and an explicit `Grip` attachment frame with a visual `Tip` marker. Record handedness and suggested hold orientation; these are presentation markers, not authoritative attack volumes. No scaled/edited character or copied hands in the final weapon asset. Communicate through a frozen manifest/readiness record, not shared edits to the hands artist's worktree.

The handoff must explicitly say **knife combat is not implemented by this art task**. Do not add damage, melee traces/RPCs, equip slots, movement benefits or gameplay animations. It should be ready for later hand posing and a separately owned native/gameplay integration task. If a reference-only knife hold can be shown safely, label it; do not claim runtime or complete attack-animation readiness.

## Meshy stages and budget

Apply [valstrike-meshy](../../../Art/Skills/valstrike-meshy/SKILL.md). Approved allocation: **30 credits for exactly one standard textured image-to-3D task**, explicit Meshy 7.1, standard geometry, 4K/PBR, no additional paid remesh/retexture/convert/rig/motion/retry stage. Check the current matching price before submission; CLI 0.8.2 standard-model pinning uses `--data`. Local cleanup/export follows. Submit once async with task/operation ID; resume recorded jobs, preserve raw output/provenance and record actual credits/hash/settings without secrets/signed URLs.

**User allowance approved on 2026-11-04: “Approve 60 additional credits.” This task owns exactly 30**, for one textured knife image-to-3D job; ART-19 owns the other 30. The earlier rifle/pistol allocation remains separate. Reference preparation, this single Meshy task and local cleanup may proceed without another confirmation. No extra charged stage, retry, borrowing or purchase. An unusable draft is evidence for a later decision, not permission for another create.
