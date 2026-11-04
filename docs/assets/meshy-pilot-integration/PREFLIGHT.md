# INT-ART-01 — engine-free preflight and native plan

2026-11-04 (Europe/Berlin). Owner: INT-ART-01 Claude integration session. Branch
`claude/int-art-01-meshy-playtest`, prepared HEAD `e61bcaa` (main `daf32bf` kickoff merged).
No Unreal process, import, map edit, build or runtime test has been run. Everything below
is a plan derived from committed source records and current code, not native evidence.

## Gate status at preflight

| Gate | State | Evidence |
| --- | --- | --- |
| ART-15 character frozen | **Open** | `codex/art15-character-pilot` head `51d19ee` is the unchanged prepared head. No `docs/assets/urban-entry01/pilot-readiness.json` or `docs/workstreams/art15-character-pilot.md` is committed. Pilot exports/previews exist only as uncommitted files in the ART-15 checkout, which is mutable and not accepted. |
| ART-16 props frozen | **Readiness committed; handoff missing** | `757a56f` adds `pilot-readiness.json` (`status: ready`, W03/W04/W07/W10, source `f7a15a1`), `pilot-placement.md`, `pilot-review.md`. Its brief also requires `docs/workstreams/art16-props-pilot.md` and a pushed PR before freeze; neither exists yet (branch not on origin). |
| Engine slot | **Held by INT-05** | Live `team-gameplay` board lists INT-05 as the current job, release signal = PR for `integration-05.md`. The kernel lock was held by INT-05's `Tests/Damage/run_runtime.py` at preflight. |
| Main drift | Not affecting | `origin/main` `b8183db` adds GPT-10/GPT-11 (VSHUD, crosshair/fire rules, tests, docs). No overlap with INT-ART-01 paths. Merge once when assembling the candidate. |

## Props (ART-16) — native plan

1. Merge the frozen ART-16 revision into this branch only; record the merge SHA. Keep LFS objects; `git lfs pull` before import.
2. Import with [`Art/Integration/MeshyPilot01/import_props.py`](../../../Art/Integration/MeshyPilot01/import_props.py) (allowlist + SHA-256 from the readiness record; no map access; no overwrite). Then reopen with `-VSInspectOnly`.
   - Textures: base color sRGB; roughness/metallic linear masks, R channel; normals `TC_NormalMap`, **Flip Green Channel once** (all four per the record). W07 normal direction is checked visually under oblique light; toggle only W07 if inverted.
   - Meshes: one slot, import normals+tangents, UV0 only, no collision, no Nanite. Site01 lights are all Movable (`build_site01.py`), so no lightmap UV is generated.
3. **Pre-identified native risk — FBX Y handedness.** Unreal's FBX conversion normally maps Blender `+Y` to Unreal `−Y`. W04/W10 are Y-symmetric and unaffected. W03 (`Y 0→200`) and W07 (`Y 0→20`, front `−Y`) are not: if negated, the recipe's world bounds and the W07 front-facing direction would be wrong. Existing project imports (rifle, pistol, device) are all Y-symmetric, so no prior evidence settles this. The importer reports `native_frame_vs_record`; if it reads `y_negated`, W03/W07 placement is held and returned to ART-16 for a corrected recipe or pivot, never compensated by guessed mirroring/rotation here.
4. Placement (after import inspection, separate script, editor-saved `VS_Site01`): exactly the nine actors in `pilot-placement.md`. Confirmed now from `docs/maps/Site01/editor-report.json`: all eleven referenced anchor actors exist at the recorded locations (`VS_Floor`, `VS_GalleryPaving`, `VS_NorthService`, `VS_ServiceRoofBand`, `VS_ServicePanel_-1_0`, `VS_NorthCrate`, `VS_SiteEastCrate(Top)`, three 120 cm covers). Re-inspect in the candidate before editing.
   - Crates: hide render/shadow of the original crate actors only; collision and transforms untouched.
   - Tiles: map-local masked copies of floor and gallery-paving materials for X `[-100,300]`, Y `[750,1150]`; defer the tile patch if the mask fails native inspection.
   - Before/after: spawn/site/device/recovery labels, tags and coordinates must match exactly.

## Character (ART-15) — native plan, pending its readiness record

- Import `SK_VS_UrbanEntry01` (+ skeleton, physics asset off/minimal, material from the three 4K maps) and only the clips ART-15 marks usable, under `Content/Art/Characters/UrbanEntry01/`. Check 170 cm height, +X facing, `*_l` on the character's left after conversion, color spaces.
- Code changes limited to the brief's exception:
  - `VSCharacter`: assign the skeletal mesh/anim class to the existing `GetMesh()` (already at Z −88, NoCollision), set `OwnerNoSee`, yaw −90 only if native facing requires it, and hide or remove `BodyVisual`/`HeadVisual`. **Keep** `UpdatePrototypeVisuals()`'s camera height and `HitZones->RefreshLayout()` calls on crouch. Capsule, movement, input, camera, health untouched.
  - Animation: a small native `UAnimInstance` subclass reading replicated `Velocity`/`IsCrouching()`/`IsFalling()` to pick idle/hold, in-place locomotion, crouch fallback. No root motion. Temporary fallbacks documented.
  - `VSWeaponPresentationComponent`: world model to the reviewed `hand_r` socket transform from ART-15; first-person transforms, recoil/flash, authority unchanged. `WorldGrip` constants live in the rifle/pistol subclasses (not owned); attachment change is made in the base component's world-anchor path only, or escalated.
- Hit zones: compare visual limbs to `VSHitZoneSet` volumes and report mismatch; do not alter damage volumes.

## Verification plan (once both inputs + slot)

One combined build (C++ changes) → props/character import and reopen → map placement save/reopen → existing Site01 inspection (`Art/Source/Maps/Site01/inspect_site01.py`, read-only map checks) → local server + two rendered clients on Site01 (`Scripts/ue.py server --map site01 --mode practice`, two clients), with genuine screenshots of the remote character, crouch, weapon swap, death/respawn, no first-person self-occlusion, and the changed prop areas. Other-Mac instructions go in `docs/playtest/meshy-pilot.md` only after those commands are verified.
