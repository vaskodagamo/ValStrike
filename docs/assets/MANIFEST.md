# Environment 01 asset manifest

Updated: 2026-11-03 (Europe/Berlin). Branch: `codex/environment-kit`, based on `d5a00fa`. Worktree: `/Users/webproduktion3/Documents/ValStrike/.asset-worktrees/environment-kit`.

**ART-01 — Done:** one original stylized concept sheet, five editable/exported environment meshes, native Unreal imports, fresh-process reopening, and native visual inspection evidence are complete. Final source hashes and imported measurements agree; editor collision queries passed. Parent task `PLAY-02` remains queued; map placement, gameplay, and the polished-map milestone are separate.

## Visual direction and deliverables

The user confirmed **Valorant-like stylized graphics**: clean shapes, broad controlled color areas, readable silhouettes, simple matte materials, and restrained detail. This original kit uses warm plaster, muted teal metal, and dark trim. The generated concept is an exploration; the real models simplify some decorative shapes.

| Deliverable | File | Status |
| --- | --- | --- |
| Bitmap concept sheet | [VS_Environment01_Concept.png](../../Art/Source/Environment/Concepts/VS_Environment01_Concept.png) | Generated and visually inspected; not geometry |
| Concept provenance and exact prompt | [GENERATION.md](../../Art/Source/Environment/Concepts/GENERATION.md) | Built-in OpenAI image generation; no reference images |
| Palette specification | [palette.json](../../Art/Source/Environment/Concepts/palette.json) | Four sRGB colors |
| Editable modeling source | [VS_Environment_Kit.blend](../../Art/Source/Environment/Models/VS_Environment_Kit.blend) | Blender source with canonical mesh collections and a separate presentation collection |
| Reproducible modeling script | [build_environment_kit.py](../../Art/Source/Environment/Models/build_environment_kit.py) | Blender 5.2.2 LTS; builds and round-trips FBX exports |
| Actual mesh preview | [VS_Environment_Kit_Actual_Meshes.png](../../Art/Source/Environment/Models/Previews/VS_Environment_Kit_Actual_Meshes.png) | Blender render of real meshes; native Unreal appearance must be inspected separately |
| Native Unreal preview | [VS_Environment01_Unreal.png](Previews/VS_Environment01_Unreal.png) | Captured and visually inspected in Unreal 5.8.4; unsaved inspection scene |
| Native import and inspection workflow | [IMPORT.md](IMPORT.md) | Unreal Editor 5.8.4 |

## Mesh inventory

Dimensions are width X × depth Y × height Z in centimeters, at scale 1. Counts below are verified LOD0 triangles and assigned material slots from the final native import, fresh-process reopening, and source report. One LOD per asset; two UV channels; zero texture files. Total: **1,628 triangles**.

| Native asset / FBX basename | Dimensions (cm) | Triangles | Slots | Collision policy |
| --- | --- | ---: | ---: | --- |
| `SM_VS_Wall_400x300` | 400 × 20 × 300 | 220 | 3 | One full-envelope UCX convex box |
| `SM_VS_Pillar_40x300` | 40 × 40 × 300 | 220 | 4 | One full-envelope UCX convex box |
| `SM_VS_Crate_100` | 100 × 100 × 100 | 484 | 4 | One full-envelope UCX convex box |
| `SM_VS_Cover_200x120` | 200 × 100 × 120 | 264 | 4 | One full-envelope UCX convex box |
| `SM_VS_Vent_120x80` | 120 × 20 × 80 | 440 | 2 | None; decorative, `NoCollision` |

FBX exports are in [Models/Exports](../../Art/Source/Environment/Models/Exports/). Native meshes are in `Content/Art/Environment/Meshes/` and load under `/Game/Art/Environment/Meshes/`. Native materials are in `Content/Art/Environment/Materials/`.

Source geometry deliberately uses disconnected, overlapping closed beveled shells. These are static hard-surface props, not deformation meshes. The solid collision hulls fill the whole bounding envelope, including small bevel recesses; they are conservative approximations rather than exact surface collision. Vent collision is disabled at the asset default and on all sections.

## Coordinates, pivots, and UVs

- Blender: centimeters, Z up, front **−Y**. Applied transforms; canonical source objects remain at identity transforms.
- Unreal FBX conversion reflects Y: centimeters, Z up, native front **+Y**.
- Wall and pillar: architectural base corner. Blender pivot is minimum X/Y/Z; native Unreal pivot is **minimum X, maximum Y, minimum Z**. Native wall bounds are `(0, -20, 0)` to `(400, 0, 300)`; pillar bounds are `(0, -40, 0)` to `(40, 0, 300)`, within import tolerance. Both dimensions and pivots support a 10 cm grid.
- Crate, cover, and vent: bottom-center pivot, with centered X/Y bounds and base Z = 0.
- UV0 and UV1 contain separate face tiles with gutters. They are basic nonoverlapping layouts, not art-directed texture layouts. No textures are used; baked-lighting quality is not verified.

## Shared materials

All four native materials are opaque matte surfaces with constant base color, roughness, and metallic values. Colors below are sRGB; the importer converts them to linear values for shader constants.

| Material | sRGB hex | Roughness | Metallic | Use |
| --- | --- | ---: | ---: | --- |
| `M_VS_Plaster` | `#C8B89E` | 0.86 | 0 | Broad architectural surfaces |
| `M_VS_PaintedMetal` | `#416F70` | 0.70 | 0 | Painted panels and structure |
| `M_VS_Trim` | `#303C41` | 0.78 | 0 | Structural edges and recesses |
| `M_VS_Accent` | `#D39B58` | 0.74 | 0 | Sparse graphic detail |

The kit has no texture, Nanite, Lumen, ray-tracing, or third-party plugin dependency. No performance result follows from those choices.

## Provenance

| Input | Author/tool and date | Source/provenance |
| --- | --- | --- |
| Mesh geometry and modeling script | Codex using Blender 5.2.2 LTS, 2026-11-03 | Original procedural shapes created for ValStrike; no external models or textures |
| Concept sheet | Codex with built-in OpenAI `image_gen.imagegen`, 2026-11-03 | Original generated exploration from the project brief; exact prompt and tool record linked above |
| Palette and native materials | Codex, 2026-11-03 | Project-authored values; Unreal shader assets created through the editor |
| Studio previews | Blender / Unreal Editor | Generated from this kit; preview ground, lights, and cameras are inspection aids, not kit assets |

No external images, scanned surfaces, purchased assets, or paid-service commitments were introduced. No new copyright license or ownership guarantee is asserted. Existing game references supplied visual principles only.

## Evidence and open checks

- [mesh_validation.json](../../Art/Source/Environment/Models/mesh_validation.json): source bounds, topology, transforms, material slots, and real FBX round-trip measurements.
- [unreal-import-report.json](unreal-import-report.json): final native import on Unreal `5.8.4-58902052`; five correct dimensions, material assignments, two UV channels, four convex collision hulls, and collision-free vent.
- The isolated Mac Editor target compiled with Xcode 26.1.1 in 25.79 seconds. This does not verify a packaged client or dedicated server.
- [unreal-reopen-report.json](unreal-reopen-report.json): all saved mesh measurements and final FBX SHA-256 values match the import-time report in a fresh editor process.
- [unreal-inspection-report.json](unreal-inspection-report.json): 10 center visibility queries plus 20 front-face probes passed. Four solids block simple/complex traces; vent blocks neither. Front-face impact deviations are within 0.2 cm.
- [Native screenshot](Previews/VS_Environment01_Unreal.png): 1600 × 1000, all five pieces visible with clean surfaces, readable materials, sun and captured-sky lighting. No texture or static-lighting bake was used.
- See [WORK_LOG.md](WORK_LOG.md) for exact local logs, resolved authoring issues, startup diagnostics, and integration notes.

No map was placed or regenerated for this batch. Runtime movement, collision feel, player visibility, sightlines, multiplayer, packaged builds, and performance remain unverified. The next integration action is review of the native preview and collision evidence, followed by controlled placement and movement/sightline testing by the gameplay workstream.
