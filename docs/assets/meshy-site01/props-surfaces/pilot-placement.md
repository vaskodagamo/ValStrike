# ART-16: bounded Site01 placement recipe

2026-11-04 (Europe/Berlin). Target `/Game/Maps/Site01/VS_Site01`; implement only through INT-ART-01's editor-owned native assets/map. The [readiness record](pilot-readiness.json) is the import allowlist. All coordinates below are **centimeters in Unreal world XYZ**; rotations are `(pitch,yaw,roll)` in degrees, all `(0,0,0)`, and every new visual uses scale `(1,1,1)`. Source FBX axes/unit conversion must first produce the documented local bounds. Do not compensate for an incorrect import by guessing actor scale or mirroring.

## Preconditions and retained actors

Locations below come from [design.py](../../../../Art/Source/Maps/Site01/design.py), [build_site01.py](../../../../Art/Source/Maps/Site01/build_site01.py) and the actual saved labels/locations in [editor-report.json](../../../maps/Site01/editor-report.json). They are the baseline in this branch, not a fresh observation of the integration candidate. Inspect these actors in the candidate before changing them; if geometry or labels differ, stop the affected placement and report the mismatch. Do not rerun the whole-map builder.

| Existing actor | Recorded actor location | Relevant existing envelope / role |
| --- | --- | --- |
| `VS_Floor` | `(0,0,-50)` | 5200×4200×100; collision top Z=0. |
| `VS_GalleryPaving` | `(0,950,0.35)` | Decorative 3000×450×0.5 box, XY `[-1500,1500]×[725,1175]`, top Z=0.6; NoCollision. |
| `VS_NorthService` | `(600,1375,190)` | Core 1960×210×380, with facade completing design envelope X `[-400,1600]`, Y `[1250,1500]`, Z `[0,380]`. |
| `VS_ServiceRoofBand` | `(600,1375,387)` | 2000×250×14; top Z=394; already NoCollision. |
| `VS_ServicePanel_-1_0` | `(0,1250,0)` | Corner pivot and yaw 180; facade envelope X `[-400,0]`, Y `[1250,1270]`, Z `[0,380]`. Retain wall and its collision. |
| `VS_NorthCrate` | `(1150,950,0)` | Base-centered 100 cm crate, bounds `(1100,900,0)` to `(1200,1000,100)`. |
| `VS_SiteEastCrate` / `VS_SiteEastCrateTop` | `(1460,-300,0)` / `(1460,-300,100)` | Two base-centered 100 cm crates; combined bounds `(1410,-350,0)` to `(1510,-250,200)`. |

Keep `VS_NorthHalfCover`, `VS_SouthHalfCover` and `VS_SiteWestCover` at their validated 120 cm height. Keep all route geometry, spawn actors/tags, `VS_Site_A`, `VS_DeviceRecovery`, site pad/lines and recovery support unchanged. No new collision volumes, steps, stack locations or physics simulation are proposed.

## Place this representative set

| New visual label | Asset | World pivot location | Expected world bounds min → max | Collision / relationship |
| --- | --- | --- | --- | --- |
| `VS_MSH_W03_Gallery_00` | W03 | `(-100,750,0)` | `(-100,750,-10)` → `(100,950,0)` | NoCollision; floor keeps all support/trace collision. |
| `VS_MSH_W03_Gallery_10` | W03 | `(100,750,0)` | `(100,750,-10)` → `(300,950,0)` | Same. |
| `VS_MSH_W03_Gallery_01` | W03 | `(-100,950,0)` | `(-100,950,-10)` → `(100,1150,0)` | Same. |
| `VS_MSH_W03_Gallery_11` | W03 | `(100,950,0)` | `(100,950,-10)` → `(300,1150,0)` | Same. |
| `VS_MSH_W04_ServiceRoof` | W04 | `(-200,1375,394)` | `(-300,1325,394)` → `(-100,1425,494)` | NoCollision; one decorative crate on existing inaccessible roof band, 75 cm setback from both long roof edges. |
| `VS_MSH_W07_ServiceInfill` | W07 | `(-300,1230.5,0)` | `(-300,1230.5,0)` → `(-100,1250.5,300)` | NoCollision; front faces −Y; last 0.5 cm embeds in existing facade. Wall remains visible behind open edges. |
| `VS_MSH_W10_North` | W10 | `(1150,950,0)` | `(1100,900,0)` → `(1200,1000,100)` | NoCollision new visual over retained `VS_NorthCrate` collision. |
| `VS_MSH_W10_SiteBase` | W10 | `(1460,-300,0)` | `(1410,-350,0)` → `(1510,-250,100)` | NoCollision new visual over retained `VS_SiteEastCrate` collision. |
| `VS_MSH_W10_SiteTop` | W10 | `(1460,-300,100)` | `(1410,-350,100)` → `(1510,-250,200)` | NoCollision new visual over retained `VS_SiteEastCrateTop` collision. |

**Tile overlap must be resolved before acceptance.** At Z=0 the original floor is coplanar and the Gallery paving at Z=0.6 would obscure the tiles. For this one 400×400 patch, create map-local masked copies of the existing floor and Gallery-paving materials, assigned only to those two actors. Preserve their original shading outside the rectangle. Use absolute world X/Y to mask their rendered surfaces inside X `[-100,300]`, Y `[750,1150]`; do not alter transforms or collision. The four closed tile meshes supply the visible surface at the original collision elevation Z=0. Leave all other paving and site graphics intact. Check exact mask edges at grazing angles: no old surface bleed, missing strip, flicker or visible void. Do not lift tiles 10 cm, displace vertices, disable floor collision or hide the whole floor/paving actor to solve this. If the bounded material mask cannot pass native inspection, defer the tile placement and report it; do not redesign the floor.

**Crate replacement is visual only.** Keep each named original crate actor/component and its existing collision responses/transform; disable only its rendering and hidden shadow contribution. Add the new NoCollision visual at the listed base pivot. Verify the original blocking bounds are no larger than the new visible 100 cm envelope, including the top crate. If native bounds disagree, keep that original visible and defer its replacement. Do not add a 120 cm invisible blocker over a 100 cm visual. The new stack uses the same existing 200 cm total height; the source stack preview does not approve another stack elsewhere.

**The long crate is decorative.** This location is on the existing service roof, not on a player route or a cover replacement. Its base stays at the roof band's Z=394. It has no physics, nav influence or collision. Existing ground-level sightlines at 152 cm and crouched height remain below the service structure; nevertheless inspect its silhouette and shadow from Gallery/service viewpoints. Do not make the roof reachable or substitute this crate for half cover. Reject this placement if the current candidate has made the roof a player route.

**The panel needs its backing.** Retain `VS_ServicePanel_-1_0` and the solid core. Do not remove a wall, create a doorway, add a blocker or enable two-sided rendering merely to hide its 506 boundary edges. The proposed panel sits 19.5 cm in front of the existing wall and is 100 cm inset from each end of that 400 cm facade module. Its front at Y=1230.5 leaves 380.5 cm to the Gallery route centerline at Y=850; it does not cross that route or the service-flank route north of the building. The 34 cm player capsule should stop against the retained wall before the camera can enter the panel, but native close-wall movement and camera/body clipping still need inspection. The panel ends at Z=300, below the existing 380 cm wall. It does not cover `VS_ServiceSign` at X=600. Use one panel only; this is not permission to replace other facades.

## Import and materials

- Import only each allowlisted game FBX, one static mesh/material slot. Import the supplied normals and tangents; preserve UV0. No automatic collision generation, complex-as-simple collision, Nanite requirement or hidden detailed master import. New visuals use NoCollision and do not affect navigation. Confirm local bounds and pivot before placement.
- W03/W04/W10 use their four **Game** maps. W07 uses its four original maps. Exact relative paths and SHA-256 values are in the readiness record; do not mix original normals with rebaked game UVs.
- Base color: sRGB enabled, RGB to Base Color, no tint/extra contrast. Roughness and metallic: sRGB disabled, linear scalar R directly to the respective input, no inversion or packed-ORM assumption. Preserve these authored maps rather than replacing them with constant gloss.
- Normals: tangent-space, sRGB disabled, normal-map compression and Normal sampler. W03/W04/W10 game bakes explicitly use `+X,+Y,+Z` / OpenGL: set Unreal texture **Flip Green Channel=true**, exactly once. Do not also invert green in the material. W07's vendor convention is unknown; its Blender preview assumed OpenGL. Start with the same flip, then compare raised/recessed seams under a neutral oblique light against the source image. If inverted, toggle only W07 and record the native decision; hold its material if unresolved.
- Opaque Default Lit, tangent-space normals and one-sided rendering for props; no emissive/translucency or new light. The two old surface materials alone become masked for the floor patch. Existing lighting remains the reference for in-map review. If the candidate requires baked lighting, generate a separate native lightmap UV channel without overwriting UV0; source meshes have only one UV channel.

## Native acceptance owned by INT-ART-01

Import/save/reopen the four assets and verify centimeter dimensions, orientation, bounds, pivots, UVs, material channels and normal direction. Record actual native transforms/bounds and screenshots from the existing Gallery, site and service viewpoints. Inspect tile seams/mask at standing and crouched grazing angles, crate contact/stack seams, panel edges/backing/close-wall clipping, and rooftop visibility/shadow. No source render proves these results.

Preserve existing collision responses and run affected floor/support, cover/Visibility and route checks once on the combined candidate; walk the tile area and replaced crates in the actual server/two-client acceptance run. Verify spawn/site/recovery tags and coordinates before/after. Any native defect may hold that placement without invalidating the other approved sources. Final production polish, human balance, packaged and other-Mac acceptance remain separate.
