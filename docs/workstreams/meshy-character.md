# ART-13 — Urban Entry Meshy source handoff

2026-11-04 (Europe/Berlin). **Implementation ready; art approval and native
verification pending.** Owner: dispatched Codex character session, High reasoning.
Checkout `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/meshy-character`,
branch `codex/meshy-character`; starting HEAD `6afa5738ebcb351ec11a9000137b72bf679d851c`.

- **Source commit:** `4c2887fe7ab34bdff96efc6686cfe9147fb2481b`. This handoff is a subsequent docs-only commit;
  coordinator reviews/publishes, user merges. No push or PR was made here.
- **Delivery:** [editable blend](../../Art/Source/Characters/UrbanEntry01/UrbanEntry01.blend),
  [character FBX](../../Art/Source/Characters/UrbanEntry01/Exports/SK_VS_UrbanEntry01.fbx),
  six animation-only FBXs, three unchanged 4K textures, reproducible source scripts.
  [Asset/rig/clip/integration notes](../assets/urban-entry01/README.md) link every file.
  One 30,970-triangle mesh, one material, 66 bones, 170 cm, max four skin influences.
- **Changes:** common centimeter/+X-forward basis with identity object transforms;
  portable texture links; one shared rig and six preserved actions; corrected
  animation-only FBX bind defaults without adding exported frames or mesh copies.
  Character identity, detail, geometry/UVs/normals/weights and supplied motion retained.
- **Actual previews:** [gallery](../assets/urban-entry01/preview.html),
  [before](../assets/urban-entry01/Previews/01_material_before.png),
  [after](../assets/urban-entry01/Previews/02_material_after.png),
  [rear](../assets/urban-entry01/Previews/03_rear.png); six GIFs and frame sheets.
  Walking/Running reviewed first, then all other clips; every 15 FPS sampled frame
  inspected directly. Browser local-file policy blocked live gallery viewing;
  no browser playback acceptance is claimed. These are Blender source previews.
- **Checks run:** all ten original SHA-256 hashes matched before and after work;
  normalized joint motion compared at every source frame; saved source reopened;
  all six actual animation-only exports reimported onto the actual character export.
  Shared names/parents/rest data, 60 FPS/ranges, no duplicate meshes, sampled deformation
  error <0.00026 cm. [Measured report](../assets/urban-entry01/animation_validation.json).
  Syntax, links, GIF decoding, final export hashes, 26 LFS pointers and
  `git diff --cached --check` passed. No new test suite or Unreal operation.
- **Remaining / next owner:** coordinator/art lead reviews source, then assigns native
  integration. Pelvis is root: no ground-root/IK/twist hierarchy; mixed in-place and
  traveling clips require a movement/retargeting decision. Documented foot drift
  (most noticeable in backward shooting), loop seams, weapon-hand fitting, deep cloth
  compression, generated nonmanifold topology and missing essential clips remain.
  Native scale/handedness/materials, collision/hit zones, animation wiring and runtime
  acceptance are deferred. Vault is source-only; provenance/license evidence remains
  the intake follow-up. Original inputs and rejected ART-07 work remain untouched.
