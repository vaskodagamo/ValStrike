# ART-25 Site01 service-facade and approach recipe

2026-11-04 (Europe/Berlin). INT-ART-02 owns native execution. This is a **source-measured plan**, not a changed Unreal map. Import only the frozen [readiness allowlist](readiness.json). Preserve the already integrated ART-16 props, all existing routes/openings, 120 cm cover, spawn/site/recovery tags and collision.

## Existing location and approach

Use the **north-facing side of NorthService**, adjacent to the existing outer service-flank approach. This avoids the ART-16 infill on the south face and its roof crate at X=−200. Keep the existing core, facade shells, roof band and materials. No existing actor is deleted, hidden or moved.

| Existing actor / route | Recorded location (cm) | Retained contract |
| --- | --- | --- |
| `VS_NorthService` | `(600,1375,190)` | Core 1960×210×380; together with facades, service block bounds X [−400,1600], Y [1250,1500], Z [0,380]. |
| `VS_ServicePanel_1_2` | `(400,1500,0)` | Existing north facade over X [400,800]. Keep its visual and collision as backing. |
| `VS_ServicePanel_1_3` | `(800,1500,0)` | Existing north facade over X [800,1200]. Keep its visual and collision as backing. |
| `VS_ServiceRoofBand` | `(600,1375,387)` | 2000×250×14, top Z=394; existing NoCollision decoration. |
| `VS_Floor` | `(0,0,−50)` | Collision top Z=0, unchanged. |
| `north_flank` / Service route | Straight segment `(−1000,1750)` → `(1900,1750)` | New facade front Y=1510 stays 240 cm from the centerline, 206 cm beyond the 34 cm capsule radius. Existing collision boundary stays Y=1500. |
| North boundary | Interior face Y=2050 | Existing physical corridor remains 550 cm deep; the added decorative skin projects 10 cm into its edge, leaving a 540 cm visible corridor. |

Locations are cross-checked against the [saved actor report](../../../maps/Site01/editor-report.json), [INT-ART-01 preexisting snapshot](../../meshy-pilot-integration/site01-preexisting-snapshot.json) and [authoring design](../../../../Art/Source/Maps/Site01/design.py). Recheck the current native candidate before applying: a changed actor/layout invalidates the affected placement. The 240/206 cm values are authoring geometry calculations, not fresh runtime sweeps.

## Six visual actors

All rotations `(pitch,yaw,roll)=(0,0,0)`, scale `(1,1,1)`, **NoCollision**, no physics/nav influence. The expected native transforms below incorporate the FBX import Y-negation already observed by INT-ART-01 for this export convention. Source coordinates use +Y inward and front −Y; expected native coordinates use −Y inward and front +Y. Verify native bounds and front/back first. Do not use negative actor scale or blindly apply the Blender transform.

| Native label | Asset | Native pivot XYZ (cm) | Expected world AABB min → max (cm) |
| --- | --- | --- | --- |
| `VS_ART25_WallWest` | W01 | `(400,1510,0)` | `(400,1491.7933,0)` → `(594.2425,1510,300)` |
| `VS_ART25_DoorRecess` | W08 | `(595.2425,1510,0)` | `(595.2425,1461.2572,0)` → `(850.4555,1510,300)` |
| `VS_ART25_WallEast` | W01 | `(851.4555,1510,0)` | `(851.4555,1491.7933,0)` → `(1045.6980,1510,300)` |
| `VS_ART25_RoofParapet1` | W09 | `(421.8490,1500,394)` | `(421.8490,1462.6270,394)` → `(621.8490,1500,457.5910)` |
| `VS_ART25_RoofParapet2` | W09 | `(622.8490,1500,394)` | `(622.8490,1462.6270,394)` → `(822.8490,1500,457.5910)` |
| `VS_ART25_RoofParapet3` | W09 | `(823.8490,1500,394)` | `(823.8490,1462.6270,394)` → `(1023.8490,1500,457.5910)` |

The full-precision version is [assembly-measurements.json](assembly-measurements.json). Local source assembly `(U,V,Z)` meters maps to the expected native world `(400+100U,1510−100V,100Z)` centimeters. This makes the source eye view at U=3.22849, V=−8, Z=1.52 correspond to world approximately `(722.849,2310,152)`: a reference camera outside the north boundary, **not a player view or route**. The overhead likewise documents geometry, not in-game visibility. The additional [source approach view](Previews/Assembly_service_approach.png) is at source `(−3,−2.4,1.52)`, mapping to **`(100,1750,152)`** on the actual service-route centerline, looking toward `(690,1510,185)`; it uses unchanged source/map-context geometry. For native player inspection use this approach and the existing service route near `(722.849,1750,152)`, looking south, plus oblique approaches from X=400 and X=1050. The source context view omits the north boundary to expose the assembled facade; it does not authorize moving it.

## Mating and placement decisions

The wall/door/wall strip spans 645.698 cm, with 1 cm nominal envelope gaps. Actual sampled profile gaps range 1.88–4.85 cm; eight of twenty near-front samples hit neither a complete pair of side profiles. [Close view](Previews/Assembly_mating.png) shows the resulting reveal and retained solid backing. Do not call this seamless, watertight or a new structural barrier. Backing is required. No vertex reconstruction is needed for this safe decorative use; freestanding/structural mating remains held for a separately bounded repair.

W08 is a **blind service recess** here. The wall remains plainly visible through its aperture, with no new door leaf, interactability, dark void or implied exit. Its original 48.743 cm depth embeds behind the existing wall; only the front 10 cm projects into the corridor. Do not cut the existing wall to expose its source doorway. If native lighting makes the backing disappear or suggests a usable route, hold W08 placement and integrate the two wall pieces and roof modules independently. The gap is not approval to add a fake blocking volume in an open doorway.

The skin is 10 cm ahead of the retained wall plane. With the 34 cm capsule against that plane, the capsule center should remain at Y≥1534, at least 24 cm ahead of the visual envelope; confirm actual camera/body/weapon clipping rather than changing collision. The wall and doorway surfaces must not fight the old facade. If the current native facade has relief protruding through the new skin, hold that placement and report the measured overlap; do not delete original collision or stretch the mesh to hide it.

The three W09 modules form a 602 cm roof run with deliberate 1 cm end gaps, centered above the lower strip. [Roof joint view](Previews/Assembly_roof_joint.png) shows the cap-end V and visible joint. Their bottoms are supported by the existing roof band at Z=394 and their tops reach 457.591 cm. They do not replace 120 cm ground cover or advertise a new accessible roof route. No collision or invisible large blockers are added. Standing/crouched route sightlines lie below the retained 380 cm service building; verify actual views and new shadows centrally.

## Material and import settings

Import the three named delivery FBXs as static meshes with the supplied authored normals/tangents and UV0; one material each. Do not import `Original.fbx`, hidden masters, assembly proxies, guide line, cameras or lights into the map. Disable automatic collision, complex-as-simple and simulation; no Nanite/LOD change is required by this source handoff. Native lighting may need a separate lightmap UV channel; never overwrite source UV0.

Use the four original PNGs identified by role/hash in the readiness record. Base color is sRGB RGB to Base Color. Roughness/metallic are separate linear maps (sRGB off), R directly to their matching input; no inversion, ORM packing, tint or gloss remapping. Tangent normals are linear/NormalMap compression with a Normal sampler. The imported normal node assumes **+Y/OpenGL** at strength 1; vendor convention is unlabelled. Start with Unreal **Flip Green Channel=true** exactly once, then compare raised/recessed details under neutral oblique light against the matched source renders. Do not both flip the texture and invert the material. Hold the affected material if the result remains ambiguous. Retain the imported factors (including normal strength and specular response) documented in the original inspection; use ordinary opaque one-sided Default Lit materials.

## Native stop conditions

INT-ART-02 records the combined candidate and exact source/readiness revisions, imports/saves/reopens, confirms dimensions/front/back/materials and checks the six expected bounds. Inspect at actual service-route eye/crouch positions, close to both joints and the closed recess, and from the roof/overhead only as diagnostic cameras. Compare all preexisting actor transforms, tags and collision before/after. Reuse affected route, floor-support and visibility checks plus the assigned rendered runtime acceptance; source renders do not establish native success.

Hold only the failing asset/placement if there is missing backing, clipping, flicker, ambiguous normal response, changed route/marker/collision, a newly reachable roof or an apparent opening. The other ready members may proceed. No first-person-hands dependency, new gameplay opening, garage, stair route, structure redesign or broader map rebuild is introduced.
