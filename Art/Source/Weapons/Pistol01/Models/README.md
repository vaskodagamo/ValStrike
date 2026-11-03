# P01 Shrike model source

Original fictional ValStrike pistol art authored procedurally in Blender 5.2.2 LTS (`d13f752e3b9c`) on 2026-11-03 (Europe/Berlin). The pistol uses angular profile geometry, clean chamfers, an open extruded trigger guard, a short recessed muzzle, simple sights, broad matte grip ridges and restrained accents. It was designed directly as editable 3D geometry; no generated concept image, downloaded mesh, texture, scan, font or third-party modeling add-on was used.

## Files and regeneration

- `build_pistol01.py` is the reproducible modeling, export, validation and preview script.
- `P01_Shrike.blend` contains separate editable part collections, three canonical export meshes, socket markers and linked presentation copies.
- `Exports/` contains the actual body, slide and magazine FBX meshes.
- `mesh_validation.json` records measurements, material values, FBX round trips and saved-Blender reopening checks.
- `Previews/` contains actual Blender studio and first-person composition renders at 1600 × 1000. These are source renders, not native Unreal or gameplay captures.

From the assigned pistol/weapon-finish worktree root:

```sh
/Applications/Blender.app/Contents/MacOS/Blender \
  --background --factory-startup --threads 4 --python-exit-code 1 \
  --python Art/Source/Weapons/Pistol01/Models/build_pistol01.py
```

Append `-- --update-finish` to revise the saved source materials and FBX constants without rebuilding geometry; it fingerprints topology, positions, UVs, assignments and transforms before/after/save-reopen, then updates the source report and previews.

Append `-- --skip-render` to regenerate and validate the source/FBX without rendering; append `-- --render-only` to render the saved source without rewriting the blend, exports or source report. A full generation overwrites the script's outputs, so preserve hand edits before rebuilding. Native Unreal imports are a separate step.

The `*_EDITABLE_PARTS` collections retain individually editable original pieces. Canonical meshes are hidden from the presentation; `PREVIEW_ONLY_linked_meshes` displays linked copies at the assembly offsets. Unhide individual parts when editing. The procedural source is the reproducible authority; hand editing only the saved blend does not update the script.

## Geometry and attachment

All modeling coordinates use centimeters, +X forward through the muzzle and +Z up. The export meshes have identity transforms. FBX's Unreal scene conversion reflects source Y; all attachment anchors lie on the centerline. The assembled envelope is **23.9290 × 4.3000 × 18.3250 cm** with **3,296 triangles**.

| Mesh | Triangles | Material slots | Local minimum XYZ, cm | Local maximum XYZ, cm |
| --- | ---: | ---: | --- | --- |
| `SM_VS_Pistol01_Body` | 1,508 | 4 | −7.22903, −2.14, −10.35081 | 15.55632, 2.14, 2.9 |
| `SM_VS_Pistol01_Slide` | 1,604 | 4 | −6.78045, −2.15, 0.1 | 16.7, 2.15, 4.475 |
| `SM_VS_Pistol01_Magazine` | 184 | 3 | −6.62062, −2.025, −10.85 | 0.46666, 2.025, 0 |

Body sockets (all identity rotation and unit scale):

| Socket | Body-local position, cm | Purpose |
| --- | --- | --- |
| `Grip` | 0, 0, 0 | Upper grip presentation origin |
| `Slide` | 0, 0, 3 | Slide's local origin at rest |
| `Magazine` | 0, 0, 0 | Magazine insertion origin, coincident with the grip reference |
| `Muzzle` | 16.7, 0, 4.75 | Stable body-local visual effect anchor, +X forward |

Attach slide and magazine to their named body sockets at zero relative translation/rotation and unit scale. The slide-local equivalent muzzle location is `(16.7,0,1.75)` cm at rest; the delivered Muzzle socket belongs to the body. The source separates pieces for future visual animation but includes no rig, animation or internal firearm mechanism. Hand and camera placement need later gameplay validation.

## Materials and checks

The shared authoritative palette is `Art/Source/Weapons/Shared/weapon_palette.json`. The generator reads its selected default and maps roles to `M_VS_P01_Structure`, `M_VS_P01_Gunmetal`, `M_VS_P01_Steel` and `M_VS_P01_Accent`. sRGB hex values are decoded into linear base-color constants; roughness and metallic are copied unchanged. No image textures or emissive surfaces are supplied.

Final native-readability tuning (same values in source and native materials):

| Material role | sRGB | Roughness | Metallic |
| --- | --- | ---: | ---: |
| Structure | `#303944` | 0.70 | 0.00 |
| Gunmetal | `#52647B` | 0.48 | 0.15 |
| Steel | `#8D9FB4` | 0.42 | 0.25 |
| Accent | `#CF3D50` | 0.52 | 0.00 |

The previous higher-metallicity values darkened excessively in the native inspection scene. This revision raises useful value separation and reduces reliance on environmental reflections while retaining restrained crimson accents. `mesh_validation.json` records identical geometry fingerprints before, after and after reopening this material-only change.

Generation asserts closed manifold shells, positive signed volume, nondegenerate faces and UV triangles, UV bounds within 0–1, and identity canonical transforms. Both UV channels use isolated face tiles; no texture-density or light-bake acceptance is claimed. Deliberate closed-shell intersections occur within the silhouette; the trigger guard itself is a single open extruded ring to avoid coplanar visible joints.

Actual FBX reimports verify bounds, triangle counts, two UV channels, material names and decoded Base Color/Roughness/Metallic values. Saving and reopening the blend verifies canonical transforms, bounds, triangle counts and material constants. Native import, native material checks, attachment tests and runtime verification are separate evidence. No collision mesh is exported; equipped Unreal visual components must use `NoCollision`.
