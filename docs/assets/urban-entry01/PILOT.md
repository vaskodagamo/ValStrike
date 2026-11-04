# ART-15 — limited third-person pilot approval

2026-11-04 (Europe/Berlin). **Partial: character, temporary R01 idle/hold and
forward in-place rifle walk approved for native pilot integration.** Native/runtime
and final-production acceptance are pending. The exact frozen revision and SHA-256
allowlist are in [pilot-readiness.json](pilot-readiness.json). ART-13's historical
handoff remains unchanged; this review supersedes its pending *pilot* art decision
only for the subset below.

## Use this subset

| File under `Art/Source/Characters/UrbanEntry01/Exports/` | Approved use |
| --- | --- |
| `SK_VS_UrbanEntry01.fbx` | Original ART-13 character, unchanged; same three textures, 170 cm, 66 bones |
| `Pilot/A_UE01_Pilot_RifleHold.fbx` | Static temporary R01 hold/idle, frames 1–61 at 60 FPS; 1 second; no breathing/recoil |
| `Pilot/A_UE01_Pilot_Walk_RifleHold.fbx` | Forward in-place rifle walk, frames 1–63 at 60 FPS; 1.0333 seconds; frozen upper-body hold |
| `A_UE01_Walking.fbx` | Unchanged unarmed forward in-place walk, optional presentation fallback |

The new [editable pilot source](../../../Art/Source/Characters/UrbanEntry01/UrbanEntry01_Pilot.blend)
contains the original mesh, rig and actions plus the two pilot actions and actual
R01 preview meshes. `UrbanEntry01.blend`, its original seven FBXs, textures and six
supplied clips remain byte-for-byte unchanged. R01 geometry/materials are appended
read-only for fit evidence; the preview weapon/anchor is **not** in character or
animation exports. Weapon sources and first-person presentation were not edited.

The hold starts from frame 1 of `Walk_Backward_While_Shooting`. Its lower body is
replaced by the supplied neutral stance so it does not freeze a walking stride.
The armed torso is rebased at the same pelvis, both wrists are lowered together
10 cm with length-preserving upper/lower-arm pose rotations, and the head is
raised 15 degrees. No bone rest transform, hierarchy, weight, geometry or scale
changes. The result is a temporary raised rifle hold, not a sight-aligned ADS pose.
The forward walk preserves every original Walking pelvis/leg curve and attaches
the fixed hold from `spine_01` upward, with one initial pelvis-basis compensation.
Only these two new actions were exported and round-trip checked.

All motion stays **in place**: retain pelvis as the sole root, retain vertical bob
and original small horizontal sway; root motion disabled, no ground-root addition,
no motion extraction or displacement from animation. CharacterMovement remains the
only movement driver. No traveling clip needed conversion. Do not import the old
traveling animations just because their files accompany the source delivery.

## R01 attachment and hand fit

Use `hand_r` as the parent of a new native third-person socket, e.g.
`R01_Pilot_Grip`. Scale is 1. The rifle body pivot is its existing Grip origin;
attach its magazine at the existing `(14, 0, 2)` cm body-local anchor. The exact
source matrices, quaternion and joint references are in
[pilot-source-validation.json](pilot-source-validation.json), `socket`.

In **Blender pose-bone head-local space**, the body transform is translation
`(4.100887, 13.554956, -6.189259)` cm, quaternion **WXYZ**
`(0.00563946, 0.81492198, 0.57947540, -0.00886966)`, scale 1. This is not a
Blender bone-tail parent offset or a guessed Unreal FRotator. The saved source uses
a Copy Transforms anchor at the bone head to make the convention explicit.

Reconstruct the native socket from the frozen hold reference after checking FBX
axis conversion: at frame 1, the source rifle body is at character-local
`(25.341227, -16.508160, 139.568008)` cm with +8° source Z yaw. Let `C` be the
verified source-to-native component basis, `W` the supplied source rifle matrix,
and `H` the **native** `hand_r` component matrix at that frame. In column-vector
notation, socket local is `inverse(H) * C * W * inverse(C)`. If the importer uses
the expected Y reflection, `C = diag(1,-1,1,1)`; the resulting target translation
is `(25.341227, +16.508160, 139.568008)` and geometric yaw is −8°. Confirm this
against the preview rather than assuming native bone-local axes match Blender.
Use the equivalent Unreal transform composition/order, not literal matrix memory
copying. Preserve the imported skeleton's bind pose and character component offset.

The support hand cups the **rear** handguard, about 7–9 cm behind R01's generic
`SupportHand=(31,0,2.4)` marker. Do not snap it to that unfitted marker. Bone
references in rifle-local space: left wrist `(18.3905,8.2226,1.8416)`, middle-finger
base `(23.5154,-2.6258,2.4970)` cm. These are joint references, not skin contact or
IK targets. Use the authored two-hand pose as supplied; no runtime IK is needed
for this fixed hold. Its sampled support-wrist drift relative to the rifle is
below .00004 cm. The index lies near the trigger guard, not a verified animated
trigger pull. Minor finger/foregrip overlap and lack of a tight stock-to-shoulder
seat remain acceptable only for this pilot distance; not hero/first-person hands.

Fit evidence: [PBR overall](Previews/Pilot/hold_pbr.png),
[right/grip view](Previews/Pilot/hold_hands.png),
[left/support view](Previews/Pilot/hold_support.png). The two close views use
Workbench for contact readability, not material acceptance.

**P01 limitation:** this two-handed R01 pose/socket is not a fitted pistol pose;
the left hand would float. Do not display an R01 when P01 is equipped. Preserve
existing first-person P01 behavior. For the remote pistol, retain the existing
presentation only if INT-ART-01 finds it visually usable; otherwise use the
unarmed/reference-pose fallback and omit the remote pistol model until fitted,
explicitly recording that limitation. No P01-specific art approval is granted.

## Visual review and limits

Reviewed the approved OperatorV2 front, ART-13 neutral PBR front/rear stills and
representative Walking, backward-armed, backward-shooting and crouch frame sheets.
The face/hair, cropped jacket, left metal shoulder, crimson right tab, collar,
gloves, trousers and boots retain the selected identity. The mesh does not need a
replacement or blind decimation for this pilot. The original PBR finish separates
fabric, glove and armor sufficiently; Workbench gloss in old motion sheets is not
the actual PBR finish. Generated face/hair detail remains soft and the dark chest
carrier blends into the jacket. These are final-art debt, not reasons to rebuild
the rig in this pass.

New source perspective views use **1920×1080 and 90° horizontal FOV** at measured
[5 m](Previews/Pilot/distance_5m.png) and [10 m](Previews/Pilot/distance_10m.png),
centered on the body. At 5 m, the raised rifle, shoulder asymmetry and gray shirt
remain readable. At 10 m, face/fingers collapse into small shapes; the broad armed
silhouette survives but crimson is too small to establish team identity. These
are neutral-studio source views, not Site01 lighting, enemy/team readability,
LOD/performance, or runtime-camera acceptance.

Changed walk frames [1](Previews/Pilot/walk_01.png),
[16](Previews/Pilot/walk_16.png), [32](Previews/Pilot/walk_32.png),
[47](Previews/Pilot/walk_47.png), [63](Previews/Pilot/walk_63.png) were inspected.
Hands stay on the rifle across alternating strides; no gross elbow/wrist collapse
or detached garment seam is visible. Deep knee/trouser folds and stiff torso/arm
motion remain visible. This is a deliberately small presentation set, not a full
animation system. Native gait speed/blend tuning must check foot slide.

Held: original Running (unarmed, not needed for this walk pilot), all three
traveling armed clips (unfitted hands/foot drift and seams), Vault (excluded
mechanic), crouch idle/directions, backward/strafe-specific gait, aim offsets,
reload/fire/recoil, jump/fall/land, death and objective animations. A forward gait
may be used by integration as an explicitly temporary directional fallback; it
must not be described as reviewed strafing/backward footwork. No crouch pose is
approved here: native QA must record the existing visual fallback and any
capsule/hit-zone mismatch without altering authoritative rules to disguise it.

## Checks and next owner

[Focused report](pilot-source-validation.json): new FBXs reimported onto the actual
character export with matching names, parentage, rest transforms, animation-only
contents, 60 FPS and frame ranges. Start/middle/end sampled joint/vertex error is
below .00017 cm; hold seam is exact, walk maximum joint seam below .00009 cm.
Pilot source reopened; original rig, vertices, skin weights and action fingerprints
match; all three 4K textures reload. Original export/texture hashes are checked
against prior ART-13 evidence. Reused its full normalization/old-clip verification;
no old-clip resampling, new suite, Unreal run, account check or paid Meshy stage.

Reproduction (sequential, Blender 5.2.2 LTS, two threads):

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/prepare_pilot.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/review_pilot.py
```

These overwrite only the new pilot outputs. Do not run the old full preparation
scripts merely to consume the frozen delivery. The initial sandboxed Blender
launch crashed before file load; authorized source runs outside the sandbox
succeeded. An initial temporary round-trip scene omitted centimeter units; that
check failed, was corrected, and both changed exports then passed.

INT-ART-01 accepts the exact allowlist/revisions, imports/reopens in its assigned
UE slot, creates the native socket with verified axis/bone treatment, and tunes
idle-to-walk presentation from replicated speed. Keep root motion off and existing
capsule/camera/hit zones unchanged. Check material spaces/normals, feet/scale,
weapon switching/visibility, crouch/directional fallbacks, death/respawn and remote
server/client presentation. The 170 cm visual versus existing capsule/hit zones,
foot slide, team readability and missing pistol/crouch coverage are explicit QA
items. User merges; source pilot approval alone does not make the game playable.

## Weapon-scale steering — 2026-11-04

The user finds the current gun too large beside the character. Keep this R01 fit
as a **temporary integration baseline only**, not final weapon art approval. The
existing assembled rifle is 86 × 9.04 × 32.2 cm at scale 1; its length is 50.6% of
the 170 cm character height. The pistol-grip profile spans approximately 10.8 cm
fore/aft, 4.8 cm across and 16 cm vertically; its broad receiver/handguard and grip
look bulky beside these hands. Rear support-hand contact and the loose stock seat
are additional fit limits documented above. Do not change character/hand scale to
compensate. The coordinator assigned separate Rifle02/Pistol02 artists (ART-17/18)
for slimmer replacement meshes and new socket recipes; their native replacement
will be integrated separately. No replacement weapon work belongs to ART-15.
