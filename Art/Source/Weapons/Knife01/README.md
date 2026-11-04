# K01 knife source — ART-20

Final source preserves the single Meshy job's original geometry, UVs, custom normals and PBR material. A local reduction/reconstruction candidate degraded the design; the user rejected the correction and a matched review confirmed the original looked better. That candidate is held separately and is **not the deliverable**.

- `Knife01.blend`: final editable asset only: `SM_VS_Knife01`, `Grip`, `Tip`. No character or copied hands.
- `Exports/SM_VS_Knife01.fbx`: final unreduced mesh and two visual frames.
- `Originals/K01_Meshy.glb` and `.fbx`: unmodified service downloads.
- `Originals/K01_OriginalNormalized.blend`: original geometry/UVs/normals/material, normalized using only an object matrix for comparison.
- `Textures/Embedded/`: byte-identical maps extracted from the original GLB, also packed in the final blend. Base color and normal are 4096²; packed metallic/roughness is 2048². Preserve this routing in `materials.json`.
- `Textures/texture_0_*.png`: separate unmodified API texture downloads, retained for provenance; the final shader uses the original GLB maps.
- `Reference/`: built-in imagegen reference and exact prompt.
- `Candidates/Rejected_Retopology/`: held historical candidate; never import as final. Its relative texture paths need relinking to the root `Textures/` if opened for forensic review.
- `Raw/`: ignored local CLI journals, service URLs, downloads and logs; not a handoff dependency.

The final mesh has 70,640 triangles and one material. No reduction was needed by the assignment; optimize only in a separately reviewed candidate if a measured runtime budget requires it. Source bounds are 27.8 × 2.399 × 4.651 cm. +X points along the blade; +Z is up. The origin and `Grip` are at the visual grip center. `Tip` is a visual landmark, not an attack volume. Applied object transforms have scale 1. The FBX uses repository -Y-forward/Z-up serialization; source coordinates remain +X/Z.

The source mesh retains 7,980 boundary edges at duplicated imported seam vertices. An ephemeral diagnostic copy with coincident vertices welded has zero boundary/nonmanifold edges; no weld is applied to the deliverable. The UV hash remains unchanged. Normalization preserves vertex positions within 0.000001 cm and custom-normal direction within 0.04 degrees, including explicit transfer of original normals through Blender's apply-transform operation.

Reproduce with Blender 5.2.2 LTS from repository root, one process at a time:

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Knife01/prepare_knife.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Knife01/verify_and_fit.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Knife01/compare_preserved_source.py
python3 Art/Source/Weapons/Knife01/finalize_manifest.py
```

These scripts make no service request or Unreal call. `prepare_knife.py` preserves the original surfaces and creates scale/pivot/frames/export. `verify_and_fit.py` reopens it, imports the actual FBX and reads the unchanged frozen Urban Entry pilot for same-scale fit evidence. The comparison script uses matched camera, lights, scale and original material; a second pass uses one identical diagnostic material for all three versions.

Source-ready for review and later dedicated hand posing. The inherited rifle pose has an extended index finger and grip/glove intersections; blade and guard have no detected hand-surface intersections in that single static reference. This is not full animation clearance. **Knife combat is not implemented by this art task.** No equip logic, melee traces, damage, RPCs, movement benefits, native Content or runtime acceptance is included.
