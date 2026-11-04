# Urban Entry 01 — Meshy source cleanup

ART-13 derives from the user's MSH-C01 character, not the rejected ART-07 model.
`UrbanEntry01.blend` contains the original detailed mesh, one shared 66-bone rig,
six preserved actions and a neutral-pose source studio. `Exports/` contains one
character FBX and six verified animation-only FBXs. `Textures/` contains the
three supplied PNGs unchanged. Do not separate these texture files from the blend.

See [delivery / integration notes](../../../../docs/assets/urban-entry01/README.md)
and [source previews](../../../../docs/assets/urban-entry01/preview.html).

Reproduce in this assigned worktree with Blender 5.2.2 LTS:

```sh
blender --factory-startup --background --threads 2 --python-exit-code 1 \
  --python Art/Source/Characters/UrbanEntry01/prepare_source.py
blender --factory-startup --background --threads 2 --python-exit-code 1 \
  --python Art/Source/Characters/UrbanEntry01/validate_and_preview.py
python3 Art/Source/Characters/UrbanEntry01/package_previews.py
```

The last command requires Pillow. Run commands sequentially, one Blender process
at a time. They overwrite generated task outputs; preserve any artist edits first.
The scripts verify the exact intake hashes before reading temporary input copies.
`prepare_source.py -- --no-renders` skips the three Cycles stills;
`validate_and_preview.py -- --no-renders` skips the compact Workbench sequences.
`inspect_intake.py` is the standalone initial inventory; it need not be rerun for
routine export. Original FBXs are not committed redundantly; the canonical intake
manifest identifies all ten originals. No script imports into Unreal.

For artist editing, select `SK_VS_UrbanEntry01` and `SKEL_UE01`; the studio
collection is preview-only. The rig opens neutral with no active action. In the
Action Editor choose `A_UE01_<clip>` and its `OBSlot` slot; use the documented
frame range at 60 FPS. All six actions have fake users and are saved in the blend.
There is no NLA arrangement or new animation authored here.

Export caveat: animation-only FBX has no skin BindPose. The exporter creates a
**temporary** neutral frame 0 to establish Model defaults, then exports only
frames 1 through the original last frame. It deletes that temporary action after
export. This keeps the animation-only bind skeleton consistent with the character
without adding a frame, changing the six saved actions or duplicating mesh data.
Blender reimport of these exports uses animation offset `0`, not its default `1`.
