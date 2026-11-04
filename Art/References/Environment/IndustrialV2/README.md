# Industrial V2 — Environment art reference pack

2026-11-04. The user liked the colors and environment of the generated first-person rifle composition and requested more examples for artists building walls, ground, boxes and props. That image is the [approved style anchor](00_ApprovedStyleAnchor.png). The six new images extend that direction; they are proposed visual references, not approved production meshes or engine screenshots.

## Reference index

| Image | What the artist should take from it |
| --- | --- |
| [01 — Courtyard](Scenes/01_Courtyard.png) | Material hierarchy, daylight, broad wall shapes, dark cover against pale concrete, restrained red/yellow marks |
| [02 — Service lane](Scenes/02_ServiceLane.png) | Wall returns, loading-bay depth, protected door frame, flush drain, compact utility details and warm interior accent |
| [03 — Crates and cover](AssetSheets/03_CratesAndCover.png) | One coherent crate family, visible construction, inset panels/handles, stacking logic and a concrete barrier |
| [04 — Walls and doors](AssetSheets/04_WallsAndDoors.png) | Modular wall, corner, double service door, open loading-bay frame, catwalk/rail and curb |
| [05 — Ground surfaces](AssetSheets/05_GroundSurfaces.png) | Poured paving, rectangular slabs, steel tread, drain, white route mark and yellow safety hatch |
| [06 — Utility props](AssetSheets/06_UtilityProps.png) | Cabinet, conduit junction, lamp, vent, protective bollards and plain fabric marker |

The source prompts are in [generation-prompts.json](generation-prompts.json). Each image was generated with the built-in image tool using the same approved reference. Files, dimensions and hashes are recorded in `manifest.json`. No image was repainted or edited with a script.

## Art direction to preserve

- **Large light surfaces:** warm-gray cast concrete walls; slightly cooler paving. Keep enough midtone separation that dark player silhouettes remain readable.
- **Dark structure and props:** graphite/blue-gray coated metal frames, doors and crates. Use lighter inset face panels and modest edge highlights to reveal construction.
- **Accent hierarchy:** muted dusty crimson for a few broad identity marks, corner protectors and rails; faded warm-yellow safety markings; off-white route/site symbols. Avoid painting every edge red. Decorative color does not assign attacker/defender identity or replace readable site lettering.
- **Shape quality:** credible thickness, modest bevels, inset panels, coherent joins and a few purposeful fasteners. A simple mass still needs designed proportions and edges. Do not reproduce the rejected character work's blockout-quality finish by assembling only flat primitive boxes.
- **Surface quality:** broad quiet values with restrained seam dirt, edge wear and casting variation. Use wear to explain construction; do not cover every surface in procedural scratches, grunge or contrasting cracks.
- **Lighting:** soft neutral daylight and controlled cool shadows. Interior lamps provide a small warm accent without making doorways unreadably black. The images are appearance targets, not prescribed engine exposure/light values.

## Proposed modular inventory

Dimensions below are **artist starting proposals in centimeters**, not measurements extracted from generated images and not permission to change the existing map. Order is width × depth × height. Existing cover, collision, lane, door and objective contracts take priority; adapt visual shells to validated gameplay dimensions.

| ID | Asset | Starting envelope / construction |
| --- | --- | --- |
| W01 | Straight concrete wall | 400 × 20 × 300; offer a 200-wide half module |
| W02 | Outside corner | Two 200-wide wall returns at 90°, matching W01 thickness/height |
| W03 | Service door assembly | 220 × 30 × 260 outer frame; model leaves separately; gameplay decides whether it opens |
| W04 | Loading-bay frame | 400 × 40 × 320 outer envelope; preserve a clear opening and visible jamb depth |
| W05 | Catwalk/rail module | 200-long deck segment, approximately 120 deep; rail height about 110 above deck; traversal only if gameplay permits |
| W06 | Curb/plinth | 200 × 20 × 20; separate end cap/corner only if used |
| G01 | Ground slab | 200 × 200 surface module; visually shallow joints, no player-catching gaps |
| G02 | Drain segment | 100 × 20 surface module; grate detail above simple flush collision |
| C01 | Small crate | 60 × 60 × 60; decoration or low object, not automatically cover |
| C02 | Medium crate | 100 × 100 × 100; shared frame/panel vocabulary |
| C03 | Tall crate | 100 × 100 × 150; shared construction, taller face layout |
| C04 | Stack arrangement | Reuse C02; resolve feet/lid contact and gameplay collision before placement |
| C05 | Concrete cover barrier | 240 × 60 × 100 visual proposal; preserve the existing map's validated cover height when replacing it |
| P01 | Electrical cabinet | 60 × 20 × 90, mounted flush to wall |
| P02 | Junction/conduit assembly | Box about 35 × 15 × 35; conduits follow wall grid |
| P03 | Wall/lintel light | 80 × 15 × 18; material/light setup remains an integration decision |
| P04 | Vent grille | 80 × 12 × 50; visual slats over simple backing |
| P05 | Protective bollard | 30 × 30 × 80; add collision only where deliberately required |
| P06 | Fabric marker | 60 × 120 fabric area and simple bracket; no new simulation system required |

Use the current project's centimeter and axis conventions. Keep structural lengths on a practical 100 cm grid, with 10 cm subdivisions where useful. Use consistent placement pivots: base corner for wall/grid modules, base center for free props, wall-contact reference for mounted pieces. Document pivots and snap points. Do not change these conventions independently across asset workers.

## Materials and construction handoff

Reuse a small material family: cast concrete, paving, graphite coated metal, slate inset metal, crimson paint, yellow/off-white markings and fabric where needed. Palette names describe an appearance target; do not sample lit pixels as final base-color values. Match materials under a neutral inspection light before tuning scene lighting.

Model the silhouette, wall/frame thickness, large bevels, inset panel recesses and visible drain/vent depth. Put fine casting variation, subtle scratches and small fastener detail into appropriate textures/normal detail where that is sufficient. Keep broad painted stripes and reusable floor marks separate as decals or a planned reusable material mask. Do not bake large lighting/shadows or an entire perspective concept image into albedo.

**The ground board is not a seamless texture set or a PBR map pack.** It contains lit construction references. Artists must create actual reusable base-color/normal/roughness maps and check tiling at intended world scale. The asset sheets show multiple objects; for an image-to-3D service, use a clean single-object reference, not the whole sheet.

Crate variants must share a compatible frame, panel inset, handles and stacking contact. The sheet communicates the family, not exact matching dimensions; resolve those mechanically in the source model. Avoid random independent redesigns of each size.

## Production sequence and review

1. First build one representative wall/corner, one medium crate and a small paving patch using the existing map scale. Use shared materials and compare a neutral source preview against these references before multiplying the kit. A polished representative sample is more useful than many unreviewed primitive assets.
2. Review proportions, silhouette, edge treatment, value separation and material response at player-eye distance. Make a targeted art correction if needed. Do not create a new automated test suite for this visual approval.
3. Once the sample is accepted, derive reusable variants, then add doors, drains, lamps, conduits and signage. Keep exact source/native ownership per task; this reference pack does not dispatch new model work or change existing art ownership.
4. Native import/reopen and one focused in-map visual check belong to the central integration slot. Retain original source/export files, provenance, transforms, material slots and collision intent in the asset handoff.

The two scenes propose appearance and composition, not a replacement Site01 layout. No map geometry, cover positions, spawn/site/recovery actors, sightlines, collision or traversal have been tested or changed. Preserve those gameplay contracts. The apparent catwalk and closed door do not authorize new traversal or door mechanics. No native `.uasset`/`.umap`, mesh, tileable texture, performance result or runtime acceptance is delivered by these images.
