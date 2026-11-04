# ART-23 — frozen stationary pistol-pose handoff

2026-11-04 (Europe/Berlin). **Implementation/source ready; stationary third-person
pilot fit passes for P01/P02; native/runtime verification pending.** Assigned Codex
character session, `codex/art23-character-pistol`, worktree
`/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art23-character-pistol`.
Prepared HEAD verified: `fd7621cfdec54838254e3f7a7ac049a02129de07`.

- **Publication:** [PR #95](https://github.com/vaskodagamo/ValStrike/pull/95), main.
  Frozen asset commit `887de0a05558169bb6f18717b579d13779449c43`; this subsequent
  handoff commit contains the final [readiness gate](../assets/urban-entry01/pistol-pilot/readiness.json).
  Both must be in the integration ancestry. Branch frozen after publication;
  user merges. No automatic merge or further assignment is active here.
- **Delivery:** one derived editable blend and one compatible animation-only FBX,
  `A_UE01_Pilot_PistolHold`, constant frames 1–61 at 60 FPS. Neutral standing lower
  body, adjusted arm/wrist/finger rotations; pelvis root, root motion off. Separate
  P01/P02 measured `hand_r` offsets, actual weapon meshes in source previews only.
  [Review, exact assembly/native recipe and comparisons](../assets/urban-entry01/pistol-pilot/README.md).
- **Preserved:** original geometry, UVs/custom normals, weights/materials, skeleton
  rest/hierarchy, all six supplied clips and ART-15 actions/files; character,
  fingers and weapons not scaled. Frozen P02 `ccb08f0` source and current P01 hashes
  verified in [source references](../assets/urban-entry01/pistol-pilot/source-references.json).
  Only assigned PistolPilot/docs paths changed; old ART-15 and active INT-ART-01
  checkouts remained untouched. Budget used: **0 credits / 0 service calls**.
- **Checks actually run:** Blender 5.2.2 LTS, two threads. Saved source reopened;
  exact mesh/UV/normal/weight/material/rig and existing-action fingerprints match
  the preserved source. New animation reimported onto actual character FBX: 66
  matching bones/parents/rest, correct FPS/range, frames 1/31/61, max sampled
  joint/vertex error <.000072 cm, exact constant loop. Actual matched original /
  candidate / animation-reimport PBR grip views, whole-body and 5 m views inspected.
  Limited hand/slide/muzzle surface diagnostics recorded, not a collision proof.
  Syntax/JSON/links, exact input/output hashes, 24 LFS pointers/upload and diff/scope
  checks passed. [Measured evidence](../assets/urban-entry01/pistol-pilot/source-validation.json).
  Reused ART-13/15 historical evidence; no full clip reruns, new tests or Unreal work.
- **Limits / held:** stationary third-person only. Angular compressed glove curls,
  small grip hollow and shallow P01 thumb/backstrap contact remain visible in close
  views; no hero/first-person or collision-perfect claim. Crouch, locomotion, reload,
  equip, recoil, trigger-pull and aim-offset coverage are absent. P02 high-detail
  source-fit pass does not approve runtime triangle budget or native replacement.
- **Next owner:** producer reviews and queues a later native follow-up **after the
  existing INT-ART-01 acceptance**. Import the single allowlisted animation onto
  existing UrbanEntry skeleton, keep root motion off, and activate pistol pose plus
  matching attachment together. Measure native `hand_r` matrix/basis and derive
  offset from the supplied matrices; preserve P01/P02 distinct part origins and
  first-person/authority/hit-zone behavior. Verify stationary remote pistol fit,
  switching/visibility and native reopen/runtime centrally. Do not apply source
  files directly to the active candidate or present this as an already-fixed game.
