# ART-28 — R02 shoulder and hand fit

`UrbanEntry01_Rifle02Pilot.blend` is a derived source scene on the unchanged 66-bone, pelvis-root UrbanEntry rig. It appends only the final full-size R02 body and magazine for fit preview. Existing character/rifle source files, geometry, weights, UVs, custom normals, PBR and prior actions stay unchanged. R02 remains **75.000 × 7.251 × 25.455 cm**, scale 1, with 448,486 body and 11,860 magazine triangles. The character remains its existing 170 cm scale. No structural rig work or paid generation.

The upper torso turns 35° to bring the supporting shoulder forward; the neck compensates to keep the head facing forward. Arm rotations place the stock against the right shoulder/chest pocket, move the dominant hand to the grip, and cup the fore-end with the support palm. Finger rotations clear the receiver and grip. The trigger finger is indexed outside the guard; this is a ready/hip-fire presentation hold, not an ADS or trigger-pull animation. No mesh/hand/weapon scaling or bone stretching is used.

| Animation-only FBX/action | Frames at 60 FPS | Duration | Lower body |
| --- | --- | --- | --- |
| `A_UE01_R02_Hold` | 1–61 | 1.000000 s | Neutral, constant |
| `A_UE01_R02_Walk` | 1–63 | 1.033333 s | Exact original `A_UE01_Walking` curves |
| `A_UE01_R02_Run` | 1–39 | 0.633333 s | Exact original `A_UE01_Running` curves |

Frame 1 is time zero. All loops close; root-motion extraction stays **off**. The pose freezes the fitted upper body over the original pelvis/leg cycles; no gait, crouch or directional-motion redesign is included. Native gait-rate tuning and aim/reload/transition blends remain unverified.

## Assembly and expected native basis

[Assembly contract](../../../../../docs/assets/urban-entry01/rifle02-pilot/assembly-contract.json) contains exact matrices, expected native quaternion and component placement. The source uses centimeters, +X forward, Z up, character left +Y. Matrices use column vectors and the **`hand_r` bone head**, not its tail. Source assembly is `W_rifle = W_character × H_hand_r × S`. The preview uses a Copy Transforms empty on `hand_r` and the constant local matrix `S` for the body.

Keep the existing R02 body and separate magazine. Magazine relative to body is translation **(15, 0, −2.5) cm**, identity rotation/scale. Muzzle **(52, 0, 7.05)**, Grip **(0,0,0)** and SupportHand **(29,0,2.4)** markers remain unchanged. The authored support palm is fitted to the actual fore-end surface; its wrist origin is not the SupportHand marker and should not be snapped directly to it. Do not apply the magazine offset twice. No weapon geometry is included in these animation-only FBXs.

INT-ART-04 measured the native component/mesh/bone basis as `C = diag(1, −1, 1, 1)`. The expected new socket is **`C × S × C`**. The authoring script checks that this conversion reproduces the existing INT-ART-04 R02 offset and quaternion before calculating the new values. The full precision result is in JSON; it is a measured-basis prediction, not a new native verification.

At Hold frame 1, expected native rifle component position is **(20, 15, 129) cm**, identity rotation/scale. Import the three animation files onto the existing UrbanEntry skeleton with no added root, retarget scale or automatic bone reorientation; preserve the listed 60 FPS/ranges. Replace the old hold/walk/run selection and `hand_r` offset together. Changing only an offset on the old pose reproduces the original defect.

The later native owner verifies the actual imported bone bases, stock contact, face/receiver clearance, both hands, aim/reload blends and runtime locomotion. Retain the already imported original R02 PBR/material recipe and authored normals. Do not infer correct gameplay aim/collision from the source barrel position.

## Source reproduction

Run sequentially from the assigned worktree with Blender **5.2.2 LTS**, CPU threads 2:

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/Rifle02Pilot/prepare_rifle02_pose.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/Rifle02Pilot/review_rifle02_pose.py
```

Authoring options: `-- --preview-only` saves/renders without export; `-- --hands-only` saves and renders just the hands without export; `-- --measure-only` saves the candidate/contract without export or rendering. Any changed pose requires fresh exports/readback before delivery. The review reuses existing source helpers/fingerprints; it reimports the actual new animations on the original character export and renders only changed fit/gait poses. Original PBR is rebound to the imported character for matched shading; prior R02 mesh-export evidence is reused.

[Visual evidence, source checks and limits](../../../../../docs/assets/urban-entry01/rifle02-pilot/README.md). No Unreal, build, multiplayer, native Content or gameplay work is included.
