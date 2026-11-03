# R01 Kestrel model source

An original fictional ValStrike rifle presentation asset, created with Blender
5.2.2 LTS (`d13f752e3b9c`) on 2026-11-03. It uses angular profile geometry,
single-segment chamfers, broad color blocks, a genuinely open trigger guard,
an open stock silhouette, simple iron sights, and a recessed muzzle. It has no
internal mechanism, firing logic, animation, character hands, or gameplay wiring.

## Deliverables

- `build_rifle01.py`: editable procedural source for both meshes and previews.
- `R01_Kestrel.blend`: native editable model, separate source parts, export
  meshes, socket markers, and linked preview copies.
- `Exports/SM_VS_Rifle01_Body.fbx`: the rifle body, 3,280 triangles.
- `Exports/SM_VS_Rifle01_Magazine.fbx`: separate magazine, 488 triangles.
- `mesh_validation.json`: measured geometry, material, socket, and FBX
  round-trip evidence.
- `Previews/R01_Kestrel_Actual_Meshes.png`: studio render of the actual meshes.
- `Previews/R01_Kestrel_FirstPerson_Composition.png`: lower-right composition
  study rendered in Blender, with no arms. This is not a gameplay screenshot.

The `*_EDITABLE_PARTS` collections contain the individual original shaped
parts. They are hidden individually; unhide the desired parts for editing.
Canonical export meshes are also hidden. The `PREVIEW_ONLY_linked_meshes`
collection displays linked copies and includes all cameras, lights, and ground.
No preview helpers or source socket markers are included in the FBX files.

## Dimensions and attachment coordinates

Source units are centimeters, with **+X forward through the muzzle**, +Z up,
and Y across the width. Applied export transforms are identity. Unreal's FBX
scene conversion reflects Blender Y; the centerline socket coordinates below
are unchanged by that reflection.

| Item | Source minimum XYZ, cm | Source maximum XYZ, cm |
| --- | --- | --- |
| Body | -30, -4.52, -12.9451 | 56, 4.52, 15.4 |
| Magazine, local | -4.6, -3.3, -18.8 | 9.0564, 3.3, 0 |

Assembled at rest, the rifle envelope is **86 × 9.04 × 32.2 cm**. The body
pivot is the main grip/receiver attachment, and the magazine pivot is its top
insertion anchor. These are presentation anchors to tune against eventual hand
poses and reload animation, not a verified animation rig.

| Socket on body | Location XYZ, cm | Rotation |
| --- | --- | --- |
| Grip | 0, 0, 0 | Identity |
| Muzzle | 56, 0, 8.2 | Identity; +X forward |
| SupportHand | 31, 0, 2.4 | Identity |
| Magazine | 14, 0, 2 | Identity |

Attach the magazine at the `Magazine` socket with identity relative transform
and scale 1. No visual collision, UCX geometry, physics body, or authoritative
hit geometry is supplied.

## Materials and provenance

Four shared materials use sRGB palette inputs decoded to linear in Blender.
Broad color planes carry the style; no image textures or normal maps are used.

| Material | sRGB | Roughness | Metallic |
| --- | --- | --- | --- |
| M_VS_R01_Graphite | #28343B | 0.62 | 0.15 |
| M_VS_R01_Teal | #416F70 | 0.58 | 0 |
| M_VS_R01_Ceramic | #C8B89E | 0.72 | 0 |
| M_VS_R01_Accent | #D39B58 | 0.55 | 0 |

Geometry is original procedural work by Codex for this project. No downloaded
mesh, external texture, external font, or third-party modeling add-on was used.
No third-party asset license or attribution dependency is introduced.

## Regenerate or render

Run from the rifle asset worktree root with the verified local Blender binary:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 \
  --python Art/Source/Weapons/Rifle01/Models/build_rifle01.py
```

This regenerates both exports, the `.blend`, validation report, and previews,
overwriting this script's outputs. Preserve any hand edits first. Coordinate
regeneration with the native importer and reimport afterward. It does not write
Unreal `.uasset` or `.umap` files.

To rerender the saved scene without rewriting the FBX exports, validation
report, or saved model:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 \
  --python Art/Source/Weapons/Rifle01/Models/build_rifle01.py -- --render-only
```

The source checks verify closed manifold triangle shells, positive signed
volume, nonzero triangle areas, identity transforms, and both UV channels
inside 0–1 with nondegenerate UV triangles. Each original face has its own
separate UV tile. A real Blender FBX reimport verifies actual bounds, triangle
counts, two UV channels, and material counts. Both meshes deliberately combine
closed shells that intersect inside their silhouette; they are not single
watertight Boolean solids. No light bake or texture-density acceptance is
claimed. Native Unreal import and in-game animation, placement, visibility,
performance, firing, and reload checks remain separate work.
