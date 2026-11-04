# R02 compact rifle — ART-17

Editable source for art review; native verification pending. Built from the isolated
built-in ImageGen reference through one Meshy 7.1 image-to-3D job, followed by local
Blender cleanup. This is a new model, not a resized R01.

Open `R02_Compact.blend` in Blender 5.2.2 LTS. The visible body and magazine are the
final source geometry; the hidden `00_ORIGINAL_MESHY_DRAFT` collection retains the
complete 458,064-triangle generated draft, normalized to centimeters/+X forward.
Textures are packed. Original downloaded files remain in the ignored local archive;
their task ID and SHA-256 values are recorded in the job manifest. The generated
reference and exact prompt are in `Reference/`.

- `Exports/SM_VS_Rifle02_Body.fbx`: grip-local body, identity export transform.
- `Exports/SM_VS_Rifle02_Magazine.fbx`: insertion-local magazine, identity export transform.
- `Textures/`: unchanged downloaded base color, metallic, roughness and normal maps.
- `cleanup_r02.py`: reproducible local cleanup from the archived Meshy GLB. It
  normalizes orientation/scale, shapes receiver/grip width and grip depth, separates
  and closes the magazine seam, reduces geometry, removes malformed cap fins, and
  creates the source/materials/FBXs. Rebuilding overwrites later artist edits.
- `preview_r02.py`: six actual Blender compositions using the frozen character and
  R01 as read-only staged references. It does not save or alter character poses.
- `verify_exports.py`: focused saved-source, pivot, UV and FBX round-trip inspection.
- `inspect_draft.py` / `inspect_fit.py`: early draft and frozen-pose inspections.

The source is **74.995 × 6.599 × 25.448 cm** assembled. The grip's palm-region width
is approximately **3.20 cm**, with local fore/aft shaping; source/object scale is 1.
The original dense draft and cleaned source remain editable; no functional internal
mechanism is modeled. The receiver has some generated surface undulation, visible
in the real close views. This is not a final hand-retopologized production LOD set.

[Delivery, fit and import recipe](../../../../docs/assets/rifle02/README.md),
[measured source](../../../../docs/assets/rifle02/source-measurements.json),
[Meshy provenance](../../../../docs/assets/rifle02/meshy-job.json).

To reproduce, restore the matching original GLB to the manifest's ignored archive
and stage the frozen ART-13 character files as described in the delivery notes.
Run sequentially from this assigned worktree, one Blender process at a time:

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Rifle02/cleanup_r02.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Rifle02/preview_r02.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Rifle02/verify_exports.py
```

These are source-authoring commands only. No Unreal work is authorized here.
