# Urban Entry pistol pilot — ART-23

One static third-person pistol hold on the preserved UrbanEntry rig. Use the
[review, native recipe and readiness gate](../../../../../docs/assets/urban-entry01/pistol-pilot/README.md).
Only this directory and its assigned docs are authored here; original character,
rifle pilot and weapon files remain unchanged.

`UrbanEntry01_PistolPilot.blend` includes all preserved character actions, one new
`A_UE01_Pilot_PistolHold`, and actual P01/P02 preview objects. Character textures
resolve through `../Textures/`; P02 textures remain packed. Hydrate those existing
LFS references before use. The source is centimeters, +X forward, Z up, scale 1.
The only new FBX in `Exports/` is animation-only, frames 1–61 at 60 FPS, pelvis root,
root motion off. Do not export the preview weapon meshes as new weapon assets.

Reproduce sequentially in this assigned worktree, Blender 5.2.2 LTS, two threads:

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/PistolPilot/prepare_pistol_hold.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/PistolPilot/verify_pistol_hold.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/PistolPilot/inspect_contacts.py
```

The first command overwrites only this new blend/animation and affected Workbench
views; `-- --preview-only` omits FBX export while posing. The second reopens this
source, compares the intact master, imports just the new animation onto the existing
character FBX, and renders matched PBR views. The third reports limited skin-vertex
contact observations, not an automatic collision acceptance gate. Do not rerun the
old full normalization/clip pipelines. No Meshy or Unreal operations are involved.
