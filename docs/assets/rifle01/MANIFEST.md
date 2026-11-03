# R01 Kestrel rifle asset

Updated: 2026-11-03 (Europe/Berlin). Branch: `codex/rifle-asset`, based on `d076442`.

**ART-02 — Done:** original stylized visual rifle, editable source model, body and separate-magazine FBX exports, native Unreal 5.8.4 imports, verified attachment sockets, fresh-process reopening, and studio/first-person composition previews completed. Source/native measurements and final visual inspection agree.

The user requested the first gun after the environment kit, extending this asset workstream to one rifle. The MVP's single-rifle gameplay scope remains unchanged. This is a visual asset increment; server weapon state, firing, reload logic, recoil, damage, animation, audio, and multiplayer verification remain the gameplay workstream's tasks.

The original design uses clean angular forms, graphite and muted teal surfaces, a restrained warm panel and small amber accents. It follows the user's Valorant-like stylized direction. No existing game weapon or real branded firearm is copied. The rifle is a game prop, with no functional internal mechanism.

## Deliverables

| Class | Path | Status |
| --- | --- | --- |
| AI concept sheet | [VS_R01_Kestrel_Concept.png](../../../Art/Source/Weapons/Rifle01/Concepts/VS_R01_Kestrel_Concept.png) | Original stylized direction study; not mesh or gameplay evidence |
| Exact concept prompt and provenance | [GENERATION.md](../../../Art/Source/Weapons/Rifle01/Concepts/GENERATION.md) | Built-in OpenAI image generation, no external reference images |
| Editable model | [R01_Kestrel.blend](../../../Art/Source/Weapons/Rifle01/Models/R01_Kestrel.blend) | Canonical meshes, individual editable parts, socket markers, separate presentation collection |
| Modeling source | [build_rifle01.py](../../../Art/Source/Weapons/Rifle01/Models/build_rifle01.py) | Reproducible Blender 5.2.2 model/export/validation workflow; render-only option preserves exports |
| Interchange meshes | [Exports](../../../Art/Source/Weapons/Rifle01/Models/Exports/) | Body and separate magazine, both verified by Blender FBX round-trip |
| Actual source previews | [Studio](../../../Art/Source/Weapons/Rifle01/Models/Previews/R01_Kestrel_Actual_Meshes.png), [first-person composition](../../../Art/Source/Weapons/Rifle01/Models/Previews/R01_Kestrel_FirstPerson_Composition.png) | Visually inspected Blender renders of the real model |
| Native meshes and materials | `Content/Art/Weapons/Rifle01/` | Created and saved through Unreal Editor 5.8.4 |
| Native Unreal previews | [Studio](Previews/VS_Rifle01_Unreal_Studio.png), [first-person composition](Previews/VS_Rifle01_Unreal_FirstPerson.png) | Both captured and visually inspected; editor compositions, not gameplay |
| Reproduction commands | [IMPORT.md](IMPORT.md) | Build, import, reopen, capture, and integration constraints |

## Measured geometry

Dimensions below are X length × Y width × Z height in centimeters at scale 1. Native measurements match the frozen source exports within 0.05 cm. Each mesh has one LOD, two UV channels, no textures, and disabled Nanite. Total geometry: **3,768 triangles**. Assembled envelope: **86 × 9.04 × 32.2 cm**.

| Mesh | Dimensions (cm) | Triangles | Material slots | Visual collision |
| --- | --- | ---: | ---: | --- |
| `SM_VS_Rifle01_Body` | 86 × 9.04 × 28.3451 | 3,280 | 4 | None |
| `SM_VS_Rifle01_Magazine` | 13.6564 × 6.6 × 18.8 | 488 | 3 | None |

The body uses shaped profile prisms, an open stock and trigger-guard silhouette, a recessed muzzle, and simplified iron sights. Closed chamfered shells intentionally intersect inside the silhouette. The source checks confirm manifold shells, positive volume, nondegenerate geometry and UV triangles, applied transforms, and actual exported geometry. UVs use separate face tiles; no baked-lighting or texture-density acceptance is claimed.

## Coordinate and attachment contract

Both source and native art use **+X muzzle-forward, +Z up, centimeters**. Unreal's FBX conversion reflects source Y. The symmetric width bounds cannot independently identify a handed side; centerline sockets remain unchanged.

The body pivot is its main grip/receiver attachment at local zero. The magazine pivot is its top insertion anchor. Native body bounds are `(-30, -4.52, -12.9451)` to `(56, 4.52, 15.4)`; native magazine local bounds are `(-4.6, -3.3, -18.8)` to `(9.0564, 3.3, 0)`.

| Body socket | Position XYZ (cm) | Purpose |
| --- | --- | --- |
| `Grip` | 0, 0, 0 | Main presentation attachment reference |
| `Muzzle` | 56, 0, 8.2 | Visual effect anchor, +X forward |
| `SupportHand` | 31, 0, 2.4 | Provisional support-hand pose reference |
| `Magazine` | 14, 0, 2 | Detachable magazine insertion anchor |

All socket rotations are identity and scales are 1. Attach the magazine to `Magazine` with zero relative transform and unit scale. Hand locations are art references to tune with a later character rig. There is no skeletal mesh or animation clip in this batch. Authoritative shooting remains server-side gameplay logic; the muzzle marker does not provide shot validation.

Both native meshes have zero simple/convex collision, disabled section collision, and `NoCollision` defaults. Equipped visual components should keep that profile. These are not character hit regions or dropped-item physics assets.

## Materials and provenance

The shader constants and Blender materials use the same sRGB palette decoded to linear. There are no image textures, normal maps, external meshes, scanned surfaces, fonts, or purchased inputs.

| Material | sRGB | Roughness | Metallic |
| --- | --- | ---: | ---: |
| `M_VS_R01_Graphite` | `#28343B` | 0.62 | 0.15 |
| `M_VS_R01_Teal` | `#416F70` | 0.58 | 0 |
| `M_VS_R01_Ceramic` | `#C8B89E` | 0.72 | 0 |
| `M_VS_R01_Accent` | `#D39B58` | 0.55 | 0 |

Geometry, modeling scripts, palette adaptation, and native material authoring: Codex for ValStrike, 2026-11-03, using Blender 5.2.2 LTS (`d13f752e3b9c`) and Unreal 5.8.4 (`58902052`). The bitmap concept was generated with built-in OpenAI `image_gen.imagegen`; its exact prompt is linked above. The real model interprets that original art direction, with a more open stock and handguard. No third-party asset license dependency is introduced or new license/ownership guarantee asserted.

## Verification and remaining work

- [mesh_validation.json](../../../Art/Source/Weapons/Rifle01/Models/mesh_validation.json): source bounds, counts, topology, UVs, transforms, socket layout, assembly envelope, and real FBX round-trip results.
- [unreal-import-report.json](unreal-import-report.json): saved native geometry, material assignment, two UV channels, collision defaults, source hashes, and exact sockets.
- The isolated Mac Editor target built successfully in 22.46 seconds using Xcode 26.1.1. This is not a packaged-client or dedicated-server result.
- [unreal-reopen-report.json](unreal-reopen-report.json): saved measurements, four body sockets, zero magazine sockets, and both source SHA-256 hashes exactly match the import-time report.
- [unreal-inspection-report.json](unreal-inspection-report.json): native actor attachment passed at scale 1 with zero measured magazine position error; muzzle +X direction and equipped `NoCollision` profiles passed in both views. Two fresh 1600 × 1000 captures and their hashes are recorded.
- Both native previews were visually inspected for silhouette, readable color panels, seated magazine, open guard, visible sights, and lower-right first-person framing. The first-person offset is a provisional art composition, not a final animated viewmodel.
- See [WORK_LOG.md](WORK_LOG.md) for exact logs, resolved authoring issues, and limits.

Runtime camera/hand attachment, animations, firing, reload, recoil, audio, hit validation, multiplayer, packaged builds, and performance remain unverified. This asset does not complete `GUN-01` through `GUN-04`. The next integration step is visual attachment in the gameplay workstream, followed by the separate server-owned rifle implementation and its multiplayer tests.
