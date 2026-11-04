# Modular Industrial V1 — Individual image-to-3D references

2026-11-04 · ART-09 · Sixteen original images generated with the **built-in image_gen tool**. Each asset has one isolated three-quarter view on white, suitable as an individual modeling reference. [Style anchor](00_StyleAnchor.png): the first-person environment selected by the user.

These are modular **design references**, not dimensionally verified meshes. A 3D service will infer unseen surfaces and may distort flat edges or holes. Use the target sizes below during cleanup before map assembly. The current map's validated lane, cover, collision and traversal requirements take priority.

## Image index and target sizes

All sizes are **centimeters**, width × depth × height, including structural trim. They are proposed reconstruction envelopes rather than measurements of the pictures. Main structural grid: 200 cm with 10 cm detail increments; walls: 300 cm high. Use 90-degree rotations. This is a reference-kit proposal, not a change to existing asset contracts.

| Image / upload one at a time | Target size | Assembly note |
| --- | --- | --- |
| [01 — Straight concrete wall](Walls/01_ConcreteWall.png) | 200 × 20 × 300 | Match the same wall end profile and bottom trim on all straight/corner pieces. |
| [02 — Metal infill wall](Walls/02_MetalPanelWall.png) | 200 × 20 × 300 | Same connection envelope as 01; panels need a solid back. |
| [03 — 90-degree concrete corner](Walls/03_CornerWall.png) | 200 × 200 × 300 | L footprint; join straight walls at its two open terminal faces. |
| [04 — Open doorway module](Doors/04_PedestrianDoorFrame.png) | 200 × 20 × 300 | Clear opening 120 × 240; keep all passage collision open. |
| [05 — Separate service door leaf](Doors/05_PedestrianDoorLeaf.png) | 120 × 6 × 240 | Nominal opening-sized blank; fit to 04 with a small working clearance. |
| [06 — Open garage portal](Garage/06_GarageFrame.png) | 400 × 30 × 300 | Clear opening 360 × 260; keep tracks inside the outer envelope. |
| [07 — Separate closed garage shutter](Garage/07_GarageShutter.png) | 360 × 8 × 260 | Nominal curtain size; fit to 06 after rebuilding its tracks. |
| [08 — Flat roof deck tile](Roofs/08_RoofDeck.png) | 200 × 200 × 20 | Repeat in two directions; top at wall height, slab thickness below. |
| [09 — Straight roof parapet](Roofs/09_ParapetStraight.png) | 200 × 20 × 80 | Repeat along a roof edge; match coping profile to 10. |
| [10 — 90-degree roof parapet corner](Roofs/10_ParapetCorner.png) | 200 × 200 × 80 | L footprint; match 09 coping and bottom trim. |
| [11 — Concrete paving tile](Ground/11_ConcreteGroundTile.png) | 200 × 200 × 10 | Top-surface corner is placement reference; thickness extends below ground. |
| [12 — Ground tile with inset drain](Ground/12_DrainGroundTile.png) | 200 × 200 × 10 | 20-wide channel meets two opposite edge midpoints; top stays flush. |
| [13 — Asphalt yard tile](Ground/13_AsphaltGroundTile.png) | 200 × 200 × 10 | Keep the yellow stripe optional; build an unmarked material variant. |
| [14 — Stackable cargo crate](Crates/14_CargoCrateCube.png) | 100 × 100 × 100 | Base-center pivot; verify physical stacking contacts. |
| [15 — Long cargo crate](Crates/15_CargoCrateLong.png) | 200 × 100 × 100 | Match 14 frame/bevel/material proportions and stacking contacts. |
| [16 — Straight stair flight](Access/16_StairFlight.png) | 200 × 200 × 100 | Five 20-high risers / 40-deep treads; validate collision centrally. |

## From image to reusable mesh

1. Upload **one numbered asset image**, not the anchor or several different pieces together. Where the service accepts instructions, describe a single isolated hard-surface game prop, preserving the open holes and straight mating faces; exclude the white background/contact shadow from the mesh.
2. Keep the original generated model, then create a cleaned working source. Rebuild warped planes, wall ends, openings and grate boundaries. Confirm the actual dimensions in Blender; prompting alone does not enforce scale. Derive further sizes from a cleaned master instead of regenerating every variation.
3. Use base-corner pivots for structural modules and base-center pivots for crates. Ground tiles use a top-surface corner so every walking surface shares elevation zero; thickness extends below it. Put a movable pedestrian door's pivot on its hinge axis. Document orientation and apply the project's existing export-axis conventions.
4. Preserve separate door/frame and garage/frame meshes. The pictured door leaf and shutter are nominal opening sizes; trim for clearance and account for hinges/tracks in the source. Do not infer working door mechanics from a concept image.
5. Reuse shared concrete, slate metal, graphite and paint materials. Keep crimson/yellow markings in optional masks or decals, so repeated modules can remain quiet. These lit perspective PNGs are **not** seamless textures or albedo/normal/roughness maps.
6. Assemble two straight walls plus a corner and doorway, a 2 × 2 tile patch, roof edge/corner and two crates in the source scene. Inspect seams from both sides, open holes, surface heights, normals and stacking contact. The assigned integration owner handles Unreal import/collision/visual checks in its scheduled engine slot.

## Visual inspection notes to carry into cleanup

All sixteen images were inspected for a single complete asset, readable silhouette, consistent palette and uncluttered background. No image was edited through a script.

The generated wall corner has taller graphite bottom trim than the plain wall; normalize these to one shared profile. The parapet corner has a bottom band absent from the straight parapet; reconcile them in the source. Coping/end caps and crate stacking seats are visual suggestions, not matched mechanical measurements. Drain and paint alignment must be made exact on the modeled grid. Keep red diagonals and safety hatching optional rather than repeating them on every piece.

Use the sparse warm-gray concrete / cool slate / graphite palette and controlled bevels. Preserve broad quiet surfaces, limited purposeful fasteners and subtle wear. Do not add noisy grunge or neon trim.

## Provenance and delivery

- [Exact generation prompts and nominal sizes](generation-prompts.json).
- [PNG dimensions and SHA-256 hashes](manifest.json).
- Delivery ZIP: `Artifacts/ModularIndustrialV1-ReferencePack.zip` in the project root (local ignored artifact).
- Related broader scene/sheet pack: [PR #47](https://github.com/vaskodagamo/ValStrike/pull/47). This pack is self-contained; it does not require that PR to merge first.

No 3D generation service was used, and no meshes, maps, native Unreal assets, builds or runtime tests were created by this reference task. Production-model approval and in-engine acceptance remain separate.
