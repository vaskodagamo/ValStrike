# ART-13 — Urban Entry Meshy character and animation cleanup

- Status: Assigned, 2026-11-04.
- Backlog: PLAY-02 / replacement for rejected ART-07 prototype; user explicitly requested an artist work on the extracted character.
- Owner: Codex “Art — Third-person player model” (`01a1a5ba-709c-7dd3-bc9e-d70ee4f951bc`), High reasoning. Preserve previous ART-07 checkout and commits; do not resume procedural modeling.
- Worktree: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/meshy-character`; branch `codex/meshy-character`.
- Base: coordinator dispatch commit descended from `f19b5ed833c3d5f6ca4c42a5f5a57f782c778a91`; exact prepared head is supplied in the dispatch message. Record it before editing.
- Exclusive write paths: `Art/Source/Characters/UrbanEntry01/`, `docs/assets/urban-entry01/`, `docs/workstreams/meshy-character.md`.
- Read-only: canonical `/Users/webproduktion3/Documents/ValStrike/meshy_ai_assets/characters/UrbanEntry/Meshy_AI_TacticalCharacterRemeshed_20261104_animate_7c537b23/`; [input manifest](../../assets/meshy-character-intake.json); `Art/References/Characters/OperatorV2/`; current project presentation/skeleton contracts only as needed.
- Inputs: MSH-C01, all ten extracted files listed/hashes recorded in the manifest: base character FBX, six with-skin animation FBXs and three texture images. The downloaded template was UE5 Mannequin, 60 FPS. This is not proof of native skeleton compatibility.
- Publication: local commits and concise handoff; coordinator reviews/publishes, user merges.

## Work to do

1. Verify input hashes, then inspect the actual character in Blender. Inventory mesh/bone/material counts, scale, bind pose, root hierarchy, UVs, texture assignment and skin weights. Preserve the original inputs; work only on copies/derived files in your worktree. Do not run any bundled external scripts.
2. Confirm the six animation files share compatible bone names, hierarchy, rest transforms and scale with the base character. Preview walk/run first, then all other supplied clips; inspect foot sliding, root motion, loop boundaries and shoulder/elbow/wrist/knee deformation. Record actual duration/frame range/root movement and issues per clip. Do not assume selecting a UE5 template makes the mesh drop-in compatible with any project skeleton.
3. Make bounded source fixes to real defects: scale/origin/orientation, material assignment, shading, minor geometry/weight problems and animation consistency. Preserve this character's identity, detail and the selected Urban Entry design. Do not replace it with primitive anatomy, blindly decimate, rename all bones, rerig independently or apply edits that break the supplied clips. If a structural rerig or substantial retopology is necessary, show the defect and hand off the decision before doing that broader change.
4. Deliver an editable .blend and game-ready FBX exports for the character and compatible clips, required textures, per-clip metadata and concise real before/after previews. Avoid duplicating full textured character geometry in every final animation export if a shared-skeleton animation-only export can be verified. Preserve untouched source evidence via input manifest rather than committing seven redundant raw FBXs.
5. Provide the native-integration handoff: bone/root/units conventions, actual compatibility findings, material inputs, animation names/durations/root-motion notes, any suggested weapon socket locations (document only), missing essential clips and remaining defects. No claim of runtime readiness without engine acceptance.

## Limits and validation

V1 source-only. Inspect the real model, representative deformation poses, all six clip playbacks and exports; use a focused round-trip only where needed to establish export integrity. Keep preview output modest and use one Blender process at a time. No new test suite, benchmark or full regression. Git diff/asset references and actual visual evidence are enough for the handoff.

The provided texture set contains base color, metallic and roughness; no separate normal map is present. Do not label an invented texture as a supplied one. Prefer correcting material interpretation over redesigning the finish. Preserve original provenance; generation/license evidence remains a recorded intake dependency.

No Unreal launch/import/build, native Content/map/Animation Blueprint edits, gameplay C++, shared skeleton edits, first-person arms task, coordinator docs/registry edits or recursive delegation. No engine slot reserved. The vault clip remains source-only: do not add vault movement, sprint or other mechanics. Existing gameplay movement/network authority stays unchanged.

Stop after a cleaned source/export/previews handoff in `docs/workstreams/meshy-character.md`, including exact commit, checks actually run, deferred native acceptance and concrete remaining issues. Central integration will schedule the native import and animation wiring in separately owned paths.
