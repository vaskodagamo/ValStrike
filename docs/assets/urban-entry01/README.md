# Urban Entry 01 — ART-13 source handoff

2026-11-04 (Europe/Berlin). **Source cleanup and export implementation ready;
art approval and native integration pending.** This is the user's Meshy character,
with its detailed face, hair, asymmetric shoulder treatment, cropped jacket,
shirt/carrier, gloves, trousers and boots preserved. No procedural replacement,
decimation, independent rerig or new animation was made.

## Delivery and evidence

- [Editable source](../../../Art/Source/Characters/UrbanEntry01/UrbanEntry01.blend)
  and [reproduction instructions](../../../Art/Source/Characters/UrbanEntry01/README.md).
- [Character FBX](../../../Art/Source/Characters/UrbanEntry01/Exports/SK_VS_UrbanEntry01.fbx).
  The six animation-only exports are linked in the clip table below.
- [Preview gallery](preview.html): six GIF playbacks and six frame sheets, plus
  [before](Previews/01_material_before.png), [after](Previews/02_material_after.png)
  and [rear](Previews/03_rear.png) Cycles stills.
- [Intake inspection](intake_inspection.json), [preparation measurements](source_preparation.json),
  [export/motion validation](animation_validation.json),
  [60 FPS joint trajectories](motion_samples.json), [preview sampling](preview_manifest.json).

The stills are actual Blender source renders (600×720, CPU Cycles 24 samples,
two threads). The motion previews use the actual full mesh in Blender Workbench
at 480×560, sampled at 15 FPS from the 60 FPS actions, over a 50 cm grid.
Workbench's studio highlights are **not** a PBR-material acceptance preview.
The rendered sequences were reviewed in order: Walking, Running, then all four
remaining clips. Every sampled frame was visually inspected as contact sheets.
Browser policy blocked opening the local gallery, so no browser/live-player
acceptance is claimed. GIFs are provided for playback; they deliberately replay
traveling and one-shot clips with a visible reset rather than hiding their motion.

## What changed, and what did not

All ten originals matched [the coordinator's manifest](../meshy-character-intake.json)
by SHA-256. They were read-only; Blender imported temporary copies inside this
worktree, avoiding any embedded-image extraction beside the originals.

The supplied rig imported with a uniform 100× object scale and a rotated basis.
One common transform converts it to identity object transforms in centimeters,
with geometric +X forward and Z up. Rest data and every local translation channel
receive the same unit conversion; bone names, hierarchy, rotations and animation
trajectories remain compatible. Every source frame's joint positions matched
before/after under that common coordinate change: maximum error <0.00013 cm.
Representative deformed vertices also matched at each clip's start/middle/end.

The three supplied texture files are copied byte-for-byte to portable paths.
Base color uses sRGB, metallic/roughness use Non-Color. The material has explicit
connections and no unused normal-map node. The finish is intentionally unchanged;
the before/after images are not a claim of a redesign. No separate normal texture
was supplied or invented. The first portable-path attempt produced missing-map
renders; that path issue was corrected, the final renders inspected, and all
three texture paths/dimensions verified after reopening the saved source.

Six identical with-skin rigs/meshes were consolidated to one saved skeleton and
six actions. Final animation exports contain **no mesh or textures**. A temporary
neutral frame outside the exported range fixes animation-only FBX bind defaults;
it is absent from the saved actions and final exported frame ranges. This fixed
an actual rest-transform mismatch caught by reimport. The resulting seven FBXs
total approximately 6 MB, instead of seven approximately 25 MB raw input FBXs.

## Mesh, scale and skeleton

| Property | Measured result |
| --- | --- |
| Mesh | `SK_VS_UrbanEntry01`; one mesh, one material section |
| Geometry | 39,934 imported/exported vertices; 30,970 triangles |
| UVs / shading | One `UVMap`; supplied custom normals preserved |
| Height / neutral bounds | 170 cm; X approximately ±15.136 cm, Y ±42.542 cm, Z 0–170 cm |
| Transforms | Mesh and armature object matrices identity; scene Metric, scale length .01 |
| Skin | No unweighted vertices; sums within 1e-4 of 1; maximum four influences |
| Rig | `SKEL_UE01`; 66 bones; `pelvis` is the sole root, at about Z 101.165 cm |
| Bind | Supplied neutral stance; not a replacement Unreal mannequin bind |
| Textures | Three 4096×4096 PNGs; unchanged source bytes |

The source's +Y side contains `*_l`, and −Y contains `*_r`. Native FBX handedness
conversion must be inspected; do not swap names to match an assumed axis sign.
The `headfront` marker points toward +X. The 170 cm character is not stretched to
the existing 176 cm gameplay capsule, and no collision or hit-zone values changed.

The rig has three spine bones, neck/head, clavicles, arms/hands, articulated
finger chains, legs/feet/balls and the supplied end markers. It has **no ground
`root` bone, IK bones or twist chains**. The Meshy “UE5 Mannequin” export setting
does not establish Manny/Quinn or project-skeleton drop-in compatibility.
All six supplied FBXs had identical bone names, parents and rest matrices before
cleanup. The [bone table](source_preparation.json) records their retained names,
parents and normalized head coordinates. Existing hit-zone names `head`,
`spine_03`, `spine_01`, `upperarm_*`, `lowerarm_*`, `thigh_*`, `calf_*`, `foot_*`
are present; their presence does not verify visual or authoritative alignment.

There are no degenerate faces. The imported surface includes many split UV/normal
seams (38,042 raw boundary edges). A coordinate-weld diagnostic on a disposable
copy still has 35 boundary and 60 total nonmanifold edges. The delivered geometry
was **not** welded: blindly joining these could damage UVs, custom normals, small
finger details or weighted surfaces. No gross open seam was apparent in the
sampled full-body views, but this is not watertight-topology or close-up acceptance.
Any structural retopology needs its own approved scope; the report exposes the
remaining topology debt rather than treating it as solved.

## Supplied animation findings

All exports retain 60 FPS. Duration is `(last − first) / 60`; the last sample is
included. Travel is the **pelvis** end-minus-start in normalized source centimeters,
not approved Unreal root motion. Near-zero values below are rounded.

| Clip / animation-only FBX | Frames / seconds | Pelvis travel XYZ, cm | Source inspection and loop boundary |
| --- | --- | --- | --- |
| [Walking](../../../Art/Source/Characters/UrbanEntry01/Exports/A_UE01_Walking.fbx) · [preview](Previews/Walking.gif) · [frames](Previews/Walking_frames.png) | 1–63 / 1.0333 | 0, 0, 0 | Unarmed in-place walk; shoulder/elbow/wrist/knee bends retain connected silhouette. Start/end pose matches within .001 cm. Visible stance-foot travel requires matching capsule velocity; no foot-lock claim. |
| [Running](../../../Art/Source/Characters/UrbanEntry01/Exports/A_UE01_Running.fbx) · [preview](Previews/Running.gif) · [frames](Previews/Running_frames.png) | 1–39 / .6333 | 0, 0, 0 | Unarmed in-place run, strong knee flex and visible airborne phase. Deep elbow/knee cloth compression needs native-distance review. Start/end pose matches within .001 cm. No sprint mechanic is introduced. |
| [Crouch Walk Left with Gun](../../../Art/Source/Characters/UrbanEntry01/Exports/A_UE01_Crouch_Walk_Left_with_Gun.fbx) · [preview](Previews/Crouch_Walk_Left_with_Gun.gif) · [frames](Previews/Crouch_Walk_Left_with_Gun_frames.png) | 1–77 / 1.2667 | .025, 105.424, −.108 | Traveling crouch; arms retain an aiming posture, but palms/fingers are not fitted to R01. Root-relative seam differs by .733 cm / 1.33° maximum. Low toe-motion proxy suggests several cm of stance drift. |
| [Vault with Rifle](../../../Art/Source/Characters/UrbanEntry01/Exports/A_UE01_Vault_with_Rifle.fbx) · [preview](Previews/Vault_with_Rifle.gif) · [frames](Previews/Vault_with_Rifle_frames.png) | 1–49 / .8000 | 220.308, .330, −.658 | One-shot travel, pelvis peaks at Z 125.792 cm. Knees tuck; feet lift and return. No obstacle or rifle contact verified. Not a seamless loop; source-only vault, with no gameplay authorization. |
| [Walk Backward While Shooting](../../../Art/Source/Characters/UrbanEntry01/Exports/A_UE01_Walk_Backward_While_Shooting.fbx) · [preview](Previews/Walk_Backward_While_Shooting.gif) · [frames](Previews/Walk_Backward_While_Shooting_frames.png) | 1–77 / 1.2667 | −87.526, .298, .077 | Traveling aim/recoil motion. Hands need weapon-fit work. Most noticeable potential stance slip: left toe moves ~21.6 cm horizontally during low-height frames 1–18. Root-relative seam differs by 2.685 cm / 5.11°; blending/clip cleanup remains. |
| [Walk Backward with Gun](../../../Art/Source/Characters/UrbanEntry01/Exports/A_UE01_Walk_Backward_with_Gun.fbx) · [preview](Previews/Walk_Backward_with_Gun.gif) · [frames](Previews/Walk_Backward_with_Gun_frames.png) | 1–61 / 1.0000 | −95.670, 0, 0 | Traveling, low aiming stance. No gross wrist or knee collapse in sampled frames; fingers/contact remain unverified. Root-relative seam differs by .489 cm / .72°. Several cm of toe drift remain. |

The toe-motion observations use `ball_l/r` trajectories within 1.5 cm of each
clip's minimum toe height for at least four frames. They are **bone proxies**,
not verified sole-contact intervals. For moving clips, maximum interval travel
was roughly 5.7 cm (crouch), 4.6 cm (vault), 21.6 cm (backward shooting), and
6.6 cm (backward gun). In-place walk/run deliberately move their stance feet
relative to a stationary origin; subtracting that travel blindly would destroy
the intended locomotion. Source trajectories and samples are included for review.

## Focused verification performed

The final `.blend` was reopened and its three external textures reloaded.
The character FBX was imported once, then each of the six animation-only FBXs
was imported and its action assigned to that imported character skeleton.
Names, parents, rest transforms, 60 FPS ranges and absence of duplicate mesh
geometry matched. Character bind-origin error was below .00004 cm; maximum
start/middle/end sampled joint/vertex error across clips was below .00026 cm.
This proves the tested Blender source/export pairing; it does not prove Unreal
retargeting, animation compression or game behavior. Reimport uses `anim_offset=0`
to preserve the exported 1-based frame labels rather than adding Blender's default
offset. Checks and commands are reproducible in the source scripts.

## Native integration decisions and remaining work

1. Import/reopen the character and six clips under the centrally assigned Unreal
   5.8.4 slot. Check centimeters, +X facing, left/right conversion, skin normals,
   texture inputs and the FBX armature-node treatment. No Content asset exists here.
2. Decide the movement/retargeting contract before enabling root motion. These
   files mix in-place motion and pelvis travel and lack a ground root. Converting
   to in-place motion or introducing a ground-root skeleton must be a coherent,
   separately owned change across all clips. Preserve server-authoritative
   CharacterMovement; do not wire pelvis travel directly into gameplay here.
3. Review the source foot drift, loop seams, deep cloth compression and generated
   topology at game camera distance. No surgical weight/geometry edits were made
   without a demonstrated safe correction; no structural rerig was undertaken.
4. Fit the accepted R01 weapon in a representative armed pose. `hand_r` is the
   candidate attachment parent; its neutral joint is approximately
   `(−3.488, −35.419, 94.099)` cm in source space. That is a **joint reference**,
   not a verified socket offset. Determine grip orientation, palm offset and
   support-hand contact using the actual weapon; no socket was added.
5. Obtain essential missing animation coverage: usable idle/armed idle,
   forward armed locomotion, remaining strafe/crouch directions, crouch idle,
   jump/fall/land, rifle fire/reload, death and objective interaction coverage.
   Existing walking/running clips are unarmed. No new clips or mechanics are implied.
6. Verify the 170 cm visual against the capsule/hit-zone contract, team readability,
   third-person camera distances and runtime multiplayer presentation centrally.

Provenance: user-generated Meshy MSH-C01 from the extracted package recorded in
the coordinator intake manifest, derived from the selected Urban Entry direction.
Generation/account/license evidence remains the intake's recorded follow-up;
this delivery does not invent licensing evidence or claim a new license.
All original inputs, the rejected ART-07 checkout and other contributors' files
remain unchanged. No Unreal launch/import/build, engine-slot attempt, gameplay,
map, Animation Blueprint or shared-skeleton edit occurred.
