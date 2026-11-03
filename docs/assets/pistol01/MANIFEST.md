# P01 Shrike asset manifest

Updated: 2026-11-03 (Europe/Berlin). **ART-03 — Done as an asset batch.** Branch `codex/pistol-weapon-finish`. Original P01 pistol, stronger shared rifle/pistol finish, actual-material comparisons, native imports/reopening and image inspection completed. Integration owns merge and gameplay acceptance.

The P01 is an original stylized fictional game prop: a compact chamfered slide, a separate dark frame, an open trigger guard, stepped steel panel, modest grip ribs, minimal sights and sparse crimson landmarks. It shares R01's angular planes and controlled color blocks. No existing game weapon, branded firearm, external model, scan or photographic texture was used. No functional internal mechanism is modeled.

## Deliverables

| Item | Location |
| --- | --- |
| Editable Blender model | [P01_Shrike.blend](../../../Art/Source/Weapons/Pistol01/Models/P01_Shrike.blend) |
| Reproducible generation/export/material-update script | [build_pistol01.py](../../../Art/Source/Weapons/Pistol01/Models/build_pistol01.py) |
| Three actual FBX meshes | [Exports](../../../Art/Source/Weapons/Pistol01/Models/Exports/) |
| Source measurements, FBX and reopen evidence | [mesh_validation.json](../../../Art/Source/Weapons/Pistol01/Models/mesh_validation.json) |
| Source collection/command documentation | [Models/README.md](../../../Art/Source/Weapons/Pistol01/Models/README.md) |
| Native meshes/materials | `Content/Art/Weapons/Pistol01/Meshes/` and `Materials/` |
| Native importer/finish verifier | [import_weapon_finish.py](../../../Art/Source/Weapons/Pistol01/import_weapon_finish.py) |
| Reproducible native stage/captures | [inspect_weapon_finishes.py](../../../Art/Source/Weapons/Pistol01/inspect_weapon_finishes.py) |
| Native import and fresh-process reopen | [Import](unreal-import-report.json), [reopen](unreal-import-reopen-report.json) |
| Six studio comparisons and neutral/dim FP views | [Comparison](COMPARISON.md), [capture evidence](unreal-inspection-report.json) |
| Final native P01 views | [Studio](Previews/VS_Pistol01_Unreal_Studio.png), [first-person](Previews/VS_Pistol01_Unreal_FirstPerson.png), [dim](Previews/VS_Pistol01_Unreal_FirstPerson_Dim.png) |
| Exact author/import/reopen/capture commands | [IMPORT.md](IMPORT.md) |

All preview images in this batch are renders of the actual meshes. Blender previews are source renders; files named `Unreal` are native editor captures. No pistol concept image was generated. The original rifle AI concept is preserved and labeled an earlier color study, not current asset/gameplay evidence.

## Measured geometry

**3,296 triangles total.** Assembled envelope **23.9290 × 4.3000 × 18.3250 cm**. Height includes sights and magazine heel; it is slightly above the suggested 18 cm starting envelope. Each native mesh has one LOD, two UV channels and Nanite disabled. Native/source measurements agree within 0.05 cm.

| Mesh | Dimensions X × Y × Z, cm | Triangles | Material slots |
| --- | --- | ---: | ---: |
| `SM_VS_Pistol01_Body` | 22.7854 × 4.2800 × 13.2508 | 1508 | 4 |
| `SM_VS_Pistol01_Slide` | 23.4805 × 4.3000 × 4.3750 | 1604 | 4 |
| `SM_VS_Pistol01_Magazine` | 7.0873 × 4.0500 × 10.8500 | 184 | 3 |

All canonical export transforms are identity. Component shells are closed and intentionally intersect inside the silhouette; no Boolean-solid, light-bake or texture-density acceptance is claimed. UV0 and UV1 use isolated face tiles; no texture maps are needed for this material treatment.

## Attachment and pivot contract

Units: centimeters. **+X muzzle-forward, +Z up.** Unreal FBX conversion reflects Blender Y; all centerline sockets use Y=0. Body origin is the grip/upper-frame attachment reference. Slide origin is its rest attachment anchor; magazine origin is its insertion anchor.

| Native Body socket | Location XYZ, cm | Rotation / scale |
| --- | --- | --- |
| `Grip` | 0, 0, 0 | Identity / 1 |
| `Slide` | 0, 0, 3 | Identity / 1 |
| `Magazine` | 0, 0, 0 | Identity / 1 |
| `Muzzle` | 16.7, 0, 4.75 | Identity / 1; +X forward |

Attach `SM_VS_Pistol01_Slide` to `Slide` and `SM_VS_Pistol01_Magazine` to `Magazine` with **zero relative translation/rotation and unit scale**. The child meshes have no native sockets. The muzzle is a stable body-space visual effect reference; its rest position in slide-local coordinates is (16.7,0,1.75). No moving-slide animation behavior is verified.

Native inspections attach the actual child actors and measure zero attachment error in studio and first-person poses. All three meshes have zero simple/convex collision, disabled triangle-section collision and default `NoCollision`. Equipped components also keep `NoCollision`; these are not shot-validation, player-hit-region or dropped-item physics assets.

First-person preview: camera looks +X, horizontal FOV 85°, grip offset **(34,12,-14) cm**. This is an art composition without arms. The eventual rig, hand pose and runtime camera integration must tune the transform.

## Shared finish and provenance

The authoritative [weapon palette](../../../Art/Source/Weapons/Shared/weapon_palette.json) drives both weapons. Saved default: **Charcoal / Crimson**. Midnight/Ice-blue and Charcoal/Copper-ember use the same geometry/camera/light per weapon and transient native materials for the comparison. No duplicated production variant assets are added.

| P01 material | sRGB | Roughness | Metallic |
| --- | --- | ---: | ---: |
| `M_VS_P01_Structure` | `#303944` | 0.70 | 0 |
| `M_VS_P01_Gunmetal` | `#52647B` | 0.48 | 0.15 |
| `M_VS_P01_Steel` | `#8D9FB4` | 0.42 | 0.25 |
| `M_VS_P01_Accent` | `#CF3D50` | 0.52 | 0 |

Hex swatches are sRGB, decoded into linear BaseColor constants. Source saved/reopened materials, exported FBX materials and native saved/reopened material graphs are checked. There is no emissive input, glow, wear texture, external font or third-party modeling add-on. Geometry/scripts/palette were authored by Codex for this project with Blender 5.2.2 LTS and Unreal 5.8.4. The source script reuses project-owned geometric/export utilities from R01. No purchase or external asset license dependency was introduced.

The initial higher-metallic treatment lost separation in Unreal. Final lower-metallic materials restore slate/steel separation under the existing neutral stage. Crimson is selected for stronger restrained contrast; ice is colder, copper warmer. In the dim stress frame (sun 0.75 / sky 0.3, fixed exposure), broad steel panels and silhouette remain identifiable but near-black structure loses fine rib/rail detail. This is a documented lighting limit, not a guarantee of readability against every gameplay background. No viewmodel fill light or emissive workaround is added.

## Verification and remaining integration

Mac Editor build passed in **22.29 s** through the existing `-WaitMutex` helper. Source shell/UV/transform checks, real FBX round trips including material constants, saved-Blender reopening, native import, material compile, fresh-process native reopening and ten native image captures passed. Exact logs and commands are in IMPORT.md. Captures are checked for fresh PNG output, dimensions and hashes. Source material-only revision fingerprints match before/after/reopen.

Rifle native body and magazine packages are byte-identical to ART-02, including their four body sockets, pivots, materials slots and NoCollision settings. Its source/export material payloads changed, with geometry fingerprints preserved. See the [rifle manifest](../rifle01/MANIFEST.md) and current finish reports.

No unresolved visible mesh artifact was found after replacing the pistol guard's intersecting coplanar pieces with one open extruded ring. Two pre-existing engine startup automation `Condition failed` messages remain; this is not an engine automation-suite result. The Mac Editor/native authoring checks do not establish packaged-client, dedicated-server, Windows/Linux, performance or multiplayer acceptance.

No pistol mechanics, switching, inventory, economy, firing, reload logic, recoil, audio, skeletal rig or animation was implemented. Next action: integration reviews/merges this asset branch, wires the existing rifle appearance separately, and tests camera/hand fit under actual gameplay lighting. Pistol remains an art-ready asset outside the single-rifle gameplay MVP.
