# FPArms02 — Meshy-derived first-person hands

ART-19 source package. The original single right hand is preserved in `Original/`;
`FPArms02.blend` contains the preserved paired mesh and explicit finger/wrist/forearm
rig. This is a new Meshy-derived asset, not the rejected FPArms01 prototype.

Open the blend with Blender 5.2.2 LTS. Select `SKEL_FPArms02`, enter Pose Mode,
and select an `A_FPArms02_*` action in the Action Editor. All actions use frames
1–30 at 30 fps. Open is neutral; Grip and Wrist are demonstration excursions
and return to neutral. RifleHold/PistolHold are static fit studies. KnifePlaceholder
is an anatomical grip, without knife geometry or accepted knife contact.

`Exports/SK_FPArms02.fbx` contains only the new rig and two skinned hands.
The six `A_*.fbx` files contain animation only. No reference weapon or character
is duplicated into these exports or the final saved blend. The imported original
Meshy PBR graph and its packed images are preserved in the blend; `Textures/` holds
the four supplied maps plus exact extracted GLB JPEG buffers for matching native material authoring. FBX does not preserve every
Blender shader node. Use the source material and actual map resolutions as authority.
The older reduced/tinted candidate is separate and held under `Candidates/`.

See [delivery and limits](../../../../docs/assets/fparms02/README.md),
[actual Blender previews](../../../../docs/assets/fparms02/preview.html),
and [readiness record](../../../../docs/assets/fparms02/readiness.json).

Fit reproduction first requires the exact R02/P02 sources in the task-local ignored
`Reference/Frozen/` paths, verified against `frozen-weapon-sources.json`. Retrieve
the specified LFS objects from that commit, never another artist's mutable files.

Reproduction, sequentially, one modest Blender process at a time:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/FPArms02/prepare_source.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/FPArms02/preview_source.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/FPArms02/fit_weapons.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/FPArms02/export_and_inspect.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/FPArms02/compare_preservation.py
```

These local scripts overwrite generated task outputs; preserve artist edits first.
They do not call Meshy, rebuild gameplay, launch Unreal or import native assets.
The recorded paid task must never be recreated by these commands.
