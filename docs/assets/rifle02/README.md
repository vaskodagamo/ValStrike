# ART-17 / R02 source handoff

2026-11-04 (Europe/Berlin). **Implementation ready for art review; native verification
pending.** R02 is a new Meshy-derived compact rifle, locally cleaned in Blender,
with a continuously fitted grip width and independently movable magazine. The user
rejected the first cleanup: its reduction, shape cutoffs, and replacement shading
degraded a substantially cleaner Meshy output. This revision restores the original
PBR and full surface detail; it supersedes that rejected version. No existing R01,
character, palette, C++, map or native Content file was changed.

## Deliverables and measured scale

[Editable source and exports](../../../Art/Source/Weapons/Rifle02/README.md) include a
packed `.blend`, two FBXs, four PBR maps, exact image prompt and reference. The
[measurement manifest](source-measurements.json) is authoritative for bounds and
counts; the [export report](export-verification.json) records the final round-trip.

| Assembly | Length | Width | Height |
| --- | ---: | ---: | ---: |
| Existing R01 | 86.000 cm | 9.040 cm | 32.200 cm |
| New R02 | 75.000 cm | 7.251 cm | 25.455 cm |

R02 is 12.8% shorter, 19.8% narrower and 20.9% lower in overall envelope. The
palm-region cross section, measured over local Z −5.5 to −4.5 cm, is about 3.22 cm
wide and 6.89 cm fore/aft including the curved transition. This is a local shape
change to a new generated model, not a nonuniform object scale. The character
remains at its frozen 170 cm reference scale.

The source has two material slots per part, one UV channel and packed maps. Exact original GLB JPEG bytes are extracted to `Textures/GLB/` for portable
material reconstruction: base color, normal and ORM. The four separately
downloaded PNG maps remain unchanged as original outputs. Base
color and normal are 4096²; metallic and roughness are 2048² as actually returned by
Meshy. The requested 4K option controls base-color resolution, not every map size.
The original 458,064-triangle draft remains hidden in the `.blend`; final export
triangle counts are **448,486 body + 11,860 magazine = 460,346 total**. Both final
parts have zero boundary/non-manifold edges; the maximum FBX bounds error is
0.0000017 cm. No geometry reduction is applied: the count increase over the draft
comes from the magazine cut and caps. 8,551 sampled corner-normal comparisons
survived FBX export within 0.028 degrees. No LODs, collision or animation assets are supplied.

## Original / corrected / export quality gate

[Preservation review settings](preservation-review.json): all three views use the
same 75 cm scale, orthographic camera, lights, 16-sample seed and original PBR graph.
The FBX is actually reimported and given the same material graph so geometry, UV and
normal loss cannot be hidden by a different shader.

- [Untouched Meshy geometry/material baseline](Previews/R02_original_control.png)
- [Corrected source](Previews/R02_corrected_control.png)
- [Actual FBX reimport](Previews/R02_fbx_control.png)
- [Actual exported magazine removed 10 cm, showing the closed insertion caps](Previews/R02_magazine_separation.png)

The receiver/rail/stock detail is retained. Only the palm-region width adjustment
and connected magazine cut/cap differ from the baseline. The earlier reduction to 9% of the draft,
receiver narrowing, abrupt grip-depth change, color tint and PBR overrides have
been removed. No new generation or credits were used.

## Genuine source previews

All images below are **Blender compositions**, not Unreal/game screenshots. Cameras,
lenses, resolution and positions are recorded in [preview-cameras.json](preview-cameras.json).
The side and comparison views are orthographic with centimeter rulers and 10 cm ticks.

- [Old/new, one camera and one scale](Previews/R01_R02_same_scale.png)
- [Side with ruler](Previews/R02_side.png)
- [Three-quarter studio](Previews/R02_threequarter.png)
- [Full frozen character and 170 cm ruler](Previews/R02_full_character.png)
- [Close hand contact](Previews/R02_grip_contact.png)
- [90° horizontal-FOV first-person composition](Previews/R02_firstperson_90fov.png)
- [Early actual generated draft inspection](Previews/draft_side.png)

The first-person composition uses an 18 mm lens on a 36 mm horizontal sensor,
1400 × 788 pixels; source camera at (−30, 12, 19) cm looking +X. It supplies no arms
and does not establish native camera placement or screen occupancy.

## Actual frozen-character fit and limits

The character `.blend` hash matches the LFS object at frozen ART-13 commit
`4c2887fe7ab34bdff96efc6686cfe9147fb2481b`. Only staged copies were imported;
[reference paths/hashes](character-reference.json) identify the exact dependencies.
Use the supplied `A_UE01_Walk_Backward_While_Shooting`, frame 1 at 60 FPS, without
changing its bones, fingers, mesh, action or scale. R02 is rigidly placed at world
**(19.110096, −20.056599, 142.509323) cm**, XYZ rotation **(0, 0, 0)°**, scale **1**.

[character-fit.json](character-fit.json) supplies the full hand-r-relative 4×4
matrix and bone-to-surface measurements. The right palm contact target is
(15.610096, −20.056599, 137.109329) cm, matching weapon-local (−3.5, 0, −5.4) cm.
The sampled right middle-finger landmarks are approximately 0.12/0.13 cm from the
body surface, index tip 0.65 cm, thumb tip 1.07 cm; left middle-finger landmarks
1.53/0.93 cm. These are nearest-surface distances, not signed penetration tests.
Wrist origins are not palm surface points.

**Fit is not animation-approved.** The unchanged pose shows fingers that need
retuning, stock/face overlap in projection, and no clean shoulder contact. The stock
marker is 24.75 cm from the right upper-arm head; that joint distance is a diagnostic,
not a skin-gap measurement. Do not stretch the rifle or enlarge the character to
force this moving clip into a final hold. Art/animation ownership must resolve wrist
roll, finger closure, support-hand position and stock weld together; integration
then checks the actual camera and sockets. No drop-in compatibility with R01 is claimed.

## Native import and socket recipe — for the later assigned owner

Import the two FBXs separately as static meshes with **Import Uniform Scale 1**, scene
unit conversion enabled and normal FBX axis conversion. Source centimeters, +X muzzle
forward, +Z up; FBX exporter `axis_forward=-Y`, `axis_up=Z`, `FBX_SCALE_UNITS`, global
scale 1. Both FBXs round-trip to their source-local bounds at identity object
transforms. Do not combine parts, auto-generate collision, enable visual collision,
or use these meshes for authoritative hit geometry. The later owner must confirm
native orientation and dimensions before replacing any gameplay reference.

The saved Blender magazine is assembled at its socket offset; its FBX is exported
at its own origin. Create these named sockets on the **body**:

| Name | Source XYZ cm | Source XYZ rotation degrees | Scale |
| --- | --- | --- | --- |
| Grip | (0, 0, 0) | (0, 0, 0) | (1, 1, 1) |
| Muzzle | (52, 0, 7.05) | (0, 0, 0) | (1, 1, 1) |
| SupportHand | (29, 0, 2.4) | (0, 0, 0) | (1, 1, 1) |
| Magazine | (15, 0, −2.5) | (0, 0, 0) | (1, 1, 1) |

These are presentation/contact anchors, not an IK solver contract. Attach magazine
to `Magazine` with identity relative TRS. Blender-to-Unreal handedness reflects Y;
these centerline anchors and zero rotations remain unchanged. For the nontrivial
hand attachment, use the provided matrix and explicit basis conversion, then inspect
in the engine; do not paste Blender Euler components into Unreal Rotator fields.
R01 had Muzzle (56,0,8.2), SupportHand (31,0,2.4), Magazine (14,0,2): migrate the
attachment coordinates and camera/animation tuning explicitly.

Recreate the **original GLB material graph**; FBX does not preserve its entire graph.
BaseColor uses the unmodified sRGB map, without a color multiplier. The packed ORM
uses G for roughness and B for metallic, both directly, without clamps or multipliers.
Normal map strength is **1.0**; OpenGL tangent convention, so check/flip green for
Unreal's DirectX normal convention. ORM/metallic/roughness/normal are Non-Color.
Preserve **custom imported normals and tangents**; do not automatically regenerate
or smooth them without an equivalent rendered comparison. `M_R02_SeamInterior`
is linear (0.035,0.043,0.055), roughness 0.8, metallic 0. The original source PBR is
packed in the `.blend`. Use `Textures/GLB/T_R02_base_color.jpg`,
`T_R02_orm.jpg` and `T_R02_normal.jpg` for the exact inspected material inputs.
Their bytes match the original GLB images; no texture recompression is performed.

## Provenance, verification and next owner

[meshy-job.json](meshy-job.json) records task
`01a1a6c6-03ec-7521-8b6c-382a26f3c320`, CLI 0.8.2, pinned `meshy-7.1`, standard
geometry, textures/PBR on, requested 4K textures, remesh off, GLB/FBX outputs.
One asynchronous submission, resumed by ID after a 60-second wait timed out;
**30 credits consumed / 30 allowed**, no paid retries or other stages.
[Official pricing](https://docs.meshy.ai/en/api/pricing) was checked before submission.
The exact built-in ImageGen prompt is
[Reference/prompt.txt](../../../Art/Source/Weapons/Rifle02/Reference/prompt.txt).
No third-party weapon mesh or branded design was used; provenance is AI-generated
reference and Meshy output plus project-local cleanup, not a claim of legal exclusivity.

Actual checks: rendered-source inspection; exact frozen-reference hash; saved-source
reopen; packed textures; part/anchor bounds; explicit FBX triangle/UV/pivot/scale
round-trip; diff and LFS checks. The repair removed the rejected reduction and broad cleanup operations, and
retained a clean connected magazine seam with plain interior caps. See the report for actual topology.
The original/corrected/export visual comparison is the quality gate, alongside the
custom-normal round-trip. Do not repeat the rejected destructive reduction to reach
a nominal triangle budget. The full-detail source needs separately scoped runtime
profiling/LOD acceptance; another paid generation is not authorized.

Deferred: native import/reopen, material reconstruction, in-game world/first-person
appearance, attachment/animation fit, performance/LOD acceptance, packaged and
network verification. No Unreal process, build, multiplayer runner or test suite
was run. Next owner: coordinator/art lead reviews source; an explicitly assigned
integration/animation owner handles replacement import and fit. User merges the PR.
