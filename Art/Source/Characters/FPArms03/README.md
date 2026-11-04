# FPArms03 — bare forearms and fitted gloves

ART-22 / PLAY-02. Editable source for art review; native/runtime verification pending. One generated right hand and anatomical bare forearm, paired locally with a compatible 39-bone rig. No jacket sleeve or substituted cylindrical arm. Full original surface, UVs, custom normals and PBR are preserved.

## Deliverables

- `FPArms03.blend`: only the two skinned arms and rig, packed original materials. Open in Blender 5.2.2 LTS. Select `SKEL_FPArms03` and an `A_FPArms03_*` Action; use frames 1–30 at 30 fps.
- `Exports/SK_FPArms03.fbx`: paired skeletal mesh. `A_FPArms03_Open`, `Grip`, `Wrist`, `RifleHold`, `PistolHold`, `KnifeHold` are bounded source checks/static hold clips, not gameplay reload/equip/melee animation.
- `Original/`: unmodified Meshy GLB/FBX, API maps and thumbnail; `MeshyDraft.blend` preserves the imported original with its inspection scene.
- `Textures/Embedded/`: exact original GLB JPEG bytes. Image 0 is sRGB base color; Image 1 is linear packed metallic/roughness (G roughness, B metallic); Image 2 is linear tangent normal. Preserve all original factors and channel routing in the material report. The API PNG copies are retained separately.
- `Reference/`: approved isolated image and exact prompt/provenance. `Reference/Frozen/` is ignored staging; dependencies are recoverable from the exact Git revisions/hashes in the report.

## Rig and coordinates

Centimeters, +X along the relaxed fingers, +Z up; right side −Y and left +Y. FBX uses repository −Y-forward/Z-up serialization. Both hand objects and rig use identity transforms. Total elbow-cut-to-middle-tip length is 40.448 cm, forearm landmark 22.612 cm and hand landmark 17.836 cm. These are authored landmark distances, not a medical measurement. The wrist joint lies within the glove cuff. The original mesh receives one uniform scale and basis transform, without stretching fingers to fit weapons.

Bone names/parents match ART-19: root; each side forearm → forearm_twist → hand, five three-bone digit chains, and a nondeforming weapon_socket. Root also does not deform. See the complete bone table in `docs/assets/fparms03/source-inspection.json`. New anatomical landmarks necessarily change rest joint locations and rolls: name compatibility is not bind-pose or animation interchangeability. Reimport/retarget validation remains with integration. Each vertex has at most four normalized influences; mirrored winding, corner UVs and custom normals are handled explicitly. The left glove places its mirrored burgundy tab outboard; no invented logo/text is mirrored.

## Attachment and fit

The three hold clips use actual corrected, unit-scale R02/P02/K01 sources, recorded in `docs/assets/fparms03/frozen-weapon-sources.json`. Weapon objects remain at their authored grip-local frames, including part offsets. Their geometry is temporary read-only fit staging and is absent from this blend/FBX. `weapon-fit.json` records component-space forearm/hand transforms for reconstructing the source study. Do not apply the exported hold plus that matrix twice. For native attachment, place the weapon grip frame in the same component space, derive the relative weapon transform from the chosen `weapon_socket_r`, and verify it with the native skeleton/camera. No runtime socket offset is accepted by this source task.

Rifle uses a forward support hand; pistol uses a two-hand pose; knife uses a curled dominant hand and an open off hand clear of the blade. The contact and 90° horizontal-FOV images are actual Blender renders, not gameplay. Consult `docs/assets/fparms03/readiness.json` for pass/held findings before using any pose as a production animation.

## Reproduction and scope

Run one Blender process with two threads. From the assigned worktree: `prepare_source.py`, `fit_weapons.py`, `export_and_inspect.py`, `preview_source.py`, then `compare_preservation.py`, each through `blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/FPArms03/<script>`. Rebuilding overwrites authored outputs. Stage the frozen reference blends first from the listed commits using Git LFS; no mutable other checkout is required. These scripts make no service call or Unreal invocation. `inspect_reuse.py` and `inspect_draft.py` document the earlier decisions.

The optional single Meshy draft consumed 30/30 credits under reservation ART-22-FPArms03-draft-1. Original task ID, options, input/output hashes and provenance are in `docs/assets/fparms03/meshy-job.json`. No paid retry, remesh, retexture, rigging, animation or conversion was requested.

Both arms retain 390,478 triangles each (780,956 total). This is a full-detail recoverable source, not an accepted runtime budget. LOD/deformation/performance, native materials/import, sockets/camera and animation transitions remain separate integration/art decisions. No Unreal build, native Content change or runtime verification is included.
