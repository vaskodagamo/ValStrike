# R01 Kestrel model source

An original fictional ValStrike rifle presentation asset, created with Blender
5.2.2 LTS (`d13f752e3b9c`) on 2026-11-03. It uses angular profile geometry,
single-segment chamfers, broad color blocks, a genuinely open trigger guard,
an open stock silhouette, simple iron sights, and a recessed muzzle. It has no
internal mechanism, firing logic, animation, character hands, or gameplay wiring.

**ART-03 finish revision, 2026-11-03:** work now continues on
`codex/pistol-weapon-finish` in
`/Users/webproduktion3/Documents/ValStrike/.asset-worktrees/pistol-weapon-finish`.
The accepted ART-02 geometry, pivots, mesh names and attachment contract remain
unchanged. The current finish is charcoal, cool gunmetal/steel and crimson.
The original cream/teal concept is retained as historical art direction.

## Deliverables

- `build_rifle01.py`: editable procedural source for both meshes and previews.
- `update_finish.py`: material-only migration of the saved model and FBX files;
  validates exact geometry preservation and serialized material values.
- `finish_validation.json`: before/after geometry fingerprints, actual reopened
  Blender shader values, shared palette hash and updated FBX hashes.
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

The current authority is [weapon_palette.json](../../Shared/weapon_palette.json),
shared with P01. Legacy rifle material and slot names remain unchanged so native
mesh references stay compatible. Their current roles are:

| Material | Current role | sRGB | Roughness | Metallic |
| --- | --- | --- | --- | --- |
| M_VS_R01_Graphite | Structure / matte charcoal | #303944 | 0.70 | 0 |
| M_VS_R01_Teal | Satin gunmetal | #52647B | 0.48 | 0.15 |
| M_VS_R01_Ceramic | Cool steel panel | #8D9FB4 | 0.42 | 0.25 |
| M_VS_R01_Accent | Restrained crimson | #CF3D50 | 0.52 | 0 |

The same shared file defines the ice-blue and copper/ember comparison variants.
Colors are sRGB inputs; both Blender and Unreal decode them to linear constants.
The first ART-03 native preview exposed overly dark high-metallic panels;
the current palette raises value separation and reduces metallic response.
No emission or textures are added. The previous ART-02 values below are retained
only as concept/source history:

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

For the current material-only revision, run from the assigned finish worktree:

```sh
cd /Users/webproduktion3/Documents/ValStrike/.asset-worktrees/pistol-weapon-finish
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 \
  --python Art/Source/Weapons/Rifle01/Models/update_finish.py -- --skip-render
```

This command has passed the source checks for ART-03. It loads the existing
`.blend`, revises only material values, exports the canonical meshes and reads
the real FBX files back. It checks geometry bounds/counts/UV channels and ordered
material names plus actual Base Color, Roughness and Metallic values. After
saving and reopening the `.blend`, it compares vertex/face/UV/material-index/
transform fingerprints for canonical meshes and editable parts, and checks the
saved shader and viewport colors. `finish_validation.json` records measured
values; `VALSTRIKE_RIFLE_FINISH_SOURCE_SUCCESS` is the success marker.
Omit `--skip-render` to refresh both source previews in the same run.

The native rifle meshes must stay byte-identical during this finish revision.
Use the material update and fresh-process verification workflow in
[IMPORT.md](../../../../../docs/assets/rifle01/IMPORT.md); do not reimport the
rifle meshes to apply colors. Updated FBX material payloads legitimately change
source file hashes while the original native geometry remains intact.

The full procedural regeneration command is retained below for source
reproducibility; it rebuilds geometry and is not the material-only migration:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 \
  --python Art/Source/Weapons/Rifle01/Models/build_rifle01.py
```

This regenerates both exports, the `.blend`, validation report, and previews,
overwriting this script's outputs. Preserve any hand edits first. A deliberate
future geometry revision requires its own native reimport and contract review;
ART-03 keeps the native rifle mesh packages unchanged. The script does not write
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
