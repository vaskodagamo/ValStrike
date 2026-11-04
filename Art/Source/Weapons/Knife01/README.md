# K01 knife source — ART-20

One original imagegen reference → one Meshy 7.1 textured draft → local Blender finishing. The actual generated grip/UVs remain in the final mesh. After the user rejected the first surface correction, the uneven blade, guard and heel were locally retopologized to the generated design, with clean planes/bevels and a restrained burgundy inset. This is not a procedural-only substitute or a second service generation.

- `Knife01.blend`: editable final asset only: `SM_VS_Knife01`, `Grip`, `Tip`. No character, copied hands, cameras or lights.
- `Exports/SM_VS_Knife01.fbx`: final mesh plus two visual attachment frames; equipped `NoCollision` intent.
- `Textures/`: original generated base color/normal (4096²) and metallic/roughness (2048²); material recipe in `materials.json`.
- `Originals/`: unmodified downloaded GLB and FBX, preserved for provenance/rework.
- `Reference/`: inspected isolated built-in imagegen reference and exact prompt.
- `Raw/`: ignored local CLI journal, signed service results, logs and downloads; not needed by another artist.

Use centimeters, +X along blade, +Z up. Origin and `Grip` sit at the visual grip center; `Tip` is a visual landmark only. All delivered object transforms have unit scale. The FBX uses the repository's -Y-forward/Z-up serialization convention; source axes remain +X/Z. See the measured bounds and attachment matrices in `docs/assets/knife01/`.

Reproduce with Blender 5.2.2 LTS from repository root, one process at a time:

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Knife01/prepare_knife.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Knife01/verify_and_fit.py
python3 Art/Source/Weapons/Knife01/finalize_manifest.py
```

`prepare_knife.py` starts from the preserved GLB and exports locally. `verify_and_fit.py` reopens the source, reimports the actual FBX and reads the frozen Urban Entry pilot for same-scale reference renders, without saving the character. No network request or Unreal command is embedded in these scripts.

Source is ready for art review and later hand posing; the shown inherited rifle hold is not a finished knife grip. Its index finger remains extended and the grip intersects the glove. Blade and guard clearance are separately reported. Retopologized constant-material surfaces have a UV layer, but overlapping edge/trim charts need packing before a future custom texture bake. The generated grip atlas is retained. Separate closed shells overlap inside the assembly; this is a visual game prop, not a watertight manufacturing model.

**Knife combat is not implemented by this art task.** No melee traces, damage, RPCs, equip slots, movement benefit, gameplay animations, native Content, Unreal import/build or runtime acceptance is included.
