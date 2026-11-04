# ART-23 — stationary third-person pistol hold

2026-11-04 (Europe/Berlin). **Source pass for stationary third-person pilot
presentation with P01 and corrected P02; native/runtime verification pending.**
This replaces the missing source-pose coverage, not the currently running native
candidate. Producer queues a later integration after its existing acceptance run.

One constant one-second action, `A_UE01_Pilot_PistolHold` (frames 1–61, 60 FPS),
uses the unchanged 66-bone UrbanEntry skeleton. Pelvis remains the sole root;
root motion is off and the character stays at 170 cm. No locomotion, crouch,
reload, equip, recoil, trigger-pull or aim-offset coverage is added. The muzzle is
forward at chest/ready height, not a sight-aligned aiming pose.

[Readiness allowlist](readiness.json) identifies the frozen source revision and
hashes. [Source/export checks and exact matrices](source-validation.json) are the
numeric authority. [Contact diagnostics](contact-diagnostics.json) document their
limited scope; they are not a continuous collision proof.

## Delivered source

- [Editable derived blend](../../../../Art/Source/Characters/UrbanEntry01/PistolPilot/UrbanEntry01_PistolPilot.blend).
  Preserves all existing actions, including the six supplied clips and ART-15
  rifle pilot actions. P01 is the default visible preview; toggle only the
  `PREVIEW_ONLY_P01/P02` collection visibility to inspect the other pistol.
- [Animation-only FBX](../../../../Art/Source/Characters/UrbanEntry01/PistolPilot/Exports/A_UE01_Pilot_PistolHold.fbx).
  Import on the existing UrbanEntry skeleton. No weapon or character mesh is
  duplicated in this export; no new native skeleton is intended.
- [Reproduction instructions](../../../../Art/Source/Characters/UrbanEntry01/PistolPilot/README.md).
  Frozen P01/P02 geometry is embedded only as read-only preview reference in the
  derived blend. Existing native weapon assets remain the integration inputs.

The stance uses neutral pelvis/legs/torso and length-preserving shoulder/elbow
rotations to position both wrists. The supplied rifle-hand curl provided a starting
point; the support-hand rotations and individual finger rotations were adjusted
for pistol contact. No bone rest/hierarchy, mesh, UV, custom normal, weight,
material channel or scale change. Original character and weapon files are untouched.
The initial missing-texture draft was rejected and corrected by saving portable
paths; only the final textured source/export comparisons support this handoff.

## Observed visual result

| Weapon | Whole body / distance | Actual reimported grip views | Verdict |
| --- | --- | --- | --- |
| P01, current pilot pistol | [body](Previews/P01_candidate_body.png), [5 m](Previews/P01_distance_5m.png) | [right / index / guard](Previews/P01_reimport_right.png), [left / thumbs / support](Previews/P01_reimport_left.png) | Pass, stationary third-person pilot only |
| P02, corrected frozen source | [body](Previews/P02_candidate_body.png), [5 m](Previews/P02_distance_5m.png) | [right / index / guard](Previews/P02_reimport_right.png), [left / thumbs / support](Previews/P02_reimport_left.png) | Pass, source-fit reference only; native replacement/optimization separate |

Both actual pistols sit in the dominant grip instead of below empty hands. The
support hand cups the dominant fingers. Elbows remain bent/down and wrists retain
a readable line into the palms. The dominant index is outside the trigger guard;
this is a ready pose, not firing. The slides and muzzle area are visibly clear.
The 5 m views use 1920×1080, 90° horizontal FOV and the same source studio, not
Site01 lighting or a runtime camera. A pistol and joined two-hand silhouette remain
readable; fine fingers are not independently readable at that distance.

Matched comparison, identical camera/light/material/pose:
[P01 untouched source](Previews/P01_original_same_pose.png) →
[P01 candidate](Previews/P01_candidate_right.png) →
[P01 actual animation reimport](Previews/P01_reimport_right.png);
[P02 untouched source](Previews/P02_original_same_pose.png) →
[P02 candidate](Previews/P02_candidate_right.png) →
[P02 actual animation reimport](Previews/P02_reimport_right.png).
“Untouched source” means the preserved character mesh/material in this **same new
pose**, not an unrelated neutral-pose image. Weapon references are identical in
each comparison. Original character PBR was rebound to the imported character so
this does not assert automatic FBX/native shader conversion.

**Held limits:** hero/first-person hand quality, animated trigger pull and all
unassigned animation coverage. The preserved glove mesh has angular compressed
finger surfaces, stiff curls and a small hollow within the two-hand grip. Shallow
thumb/backstrap contact remains on P01 (an exploratory nearest-normal diagnostic
flagged approximately 0.31 cm; it is not a reliable inside test for multipart gun
meshes). No collision-free or anatomical-perfect claim is made. These remain
visible in the close views and are not hidden by scaling fingers or weapons.
Source approval is limited to the stationary third-person pilot distance; close
hero treatment needs a separately scoped finger/weight review. P02's existing
461,412-triangle high-detail reference is not a runtime optimization approval.

## Attachment recipe — do not reuse R01 or old pistol offsets

All values below are centimeters in the preserved source character frame: +X
forward, +Z up. Scale 1 throughout. A preview anchor copies **the head matrix of
`hand_r`**, with no Blender bone-tail parenting offset. The same action serves
both weapons; only the weapon transform differs.

| Weapon | Weapon component location XYZ | Component rotation | Blender head-local translation XYZ |
| --- | --- | --- | --- |
| P01 | `(38, -3, 137)` | Identity | `(4.258268, 12.900201, -5.286075)` |
| P02 | `(38, -3, 134)` | Identity | `(4.282030, 12.958613, -2.286747)` |

Use the **full `weapons.P01/P02.hand_r_local_matrix`** from the validation report,
not just the translations. The report also records both hand and weapon component
matrices and bone references in weapon space. Small ~1e-7 components are floating
point roundoff, not intended nonuniform scale or a deliberate component rotation.

For native integration, evaluate this animation at frame 1, measure native
`hand_r` component matrix `H`, and establish the actual source-to-native component
basis `C`. With source weapon matrix `W`, the desired local transform in
column-vector notation is `inverse(H) * C * W * inverse(C)`. If the verified
conversion is source Y reflection, `C=diag(1,-1,1,1)`, the native component target
translations become P01 `(38,+3,137)` and P02 `(38,+3,134)` with identity rotation.
Use the native API's equivalent multiplication order. Do not copy a Blender
quaternion into Unreal Pitch/Yaw/Roll or apply both the old character offset and
this new hand-local attachment. This task creates no named Unreal socket.

P01: Body Grip datum at zero; Slide body-local `(0,0,3)`, Magazine `(0,0,0)`,
identity child rotation/scale. P02: Body, Slide and Magazine already share the
Grip datum and assemble at **identity**. P02's named Slide/Magazine markers are not
part-mesh origins; do not apply their marker translations a second time. Preserve
the existing source/import contract for each weapon and their NoCollision policy.

The integration owner must activate the pistol action and its matching attachment
together. Switching only the socket under ART-15's rifle pose is not this result.
The current pilot's movement/crouch fallbacks remain separately unresolved; this
stationary pose does not authorize changing authoritative movement or hit zones.

## Focused checks and next owner

Blender 5.2.2 LTS, two threads. Saved derived source reopened. Exact character
positions/topology/UV/corner-normal/weight/material-index hashes, rig/rest hash,
material graph/texture hashes and all existing action fingerprints match the
frozen pilot source. The single new animation was imported onto the actual
character FBX: 66 matching bones/parents, rest error <0.000031, 60 FPS and frames
1–61, sampled frames 1/31/61; max joint/vertex error <0.000072 cm and exact loop seam.

Selected hand-surface diagnostics found no negative-normal hand-to-hand samples
deeper than .05 cm; nearest right-hand vertex to the support surface is about
.0088 cm. Minimum selected skin-vertex distance to slide surface is 3.32 cm (P01)
and 1.17 cm (P02); hands remain about 10.91/9.71 cm behind the muzzle anchor in X.
These are finite vertex/nearest-surface observations, not a physics or all-triangle
collision test. The actual affected source/reimport views were inspected.

Reused ART-13/15 normalization, original clip and material evidence; no historical
clip reruns. No service/account call, spending, Unreal operation/build, new tests,
first-person work or active integration checkout change. Producer reviews this
frozen source and queues a later native update. That owner imports/reopens,
reconstructs the appropriate hand transform, verifies remote pistol switching and
stationary pose after the existing INT-ART-01 acceptance, and records native/runtime
results separately. User merges.
