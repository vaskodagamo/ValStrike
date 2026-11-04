# P02 Kestrel — corrected high-detail source

2026-11-04 (Europe/Berlin). User-requested quality correction to ART-18 / [PR #73](https://github.com/vaskodagamo/ValStrike/pull/73). **Corrected source for visual review; runtime optimization and native verification pending.** The previous 26,904-triangle export was rejected for degraded surface quality and is superseded.

## What changed

Rebuilt from the unchanged original Meshy GLB. Removed the 5.5% decimation, general smoothing and PBR overrides. Kept the original geometry, UVs, PBR maps and custom corner normals. The muzzle-forward transform now explicitly updates both positions and normals. Part separation preserves per-corner normals through cutting; only newly filled cut caps use flat interior normals. The magazine retains its original exterior floor plus a concealed visual stem.

The new source is **21.000 × 3.352 × 13.833 cm**, with separate Body (211,980 triangles), Slide (241,968) and Magazine (7,464): **461,412 triangles total**. The original had 454,988; added cut boundaries/caps/stem explain the increase. This is a high-detail authoring/reference export, **not an optimized runtime LOD**. No further reduction should be accepted without a same-camera visual comparison and shading validation.

Two shared materials, one UV channel, identity mesh transforms, centimeters, +X muzzle-forward, +Z up. [Packed editable blend](../../../Art/Source/Weapons/Pistol02/Models/P02_Kestrel.blend), [FBX parts](../../../Art/Source/Weapons/Pistol02/Models/Exports), [measurements](source-measurements.json), [source hashes](source-hashes.json), [socket/import recipe](IMPORT.md).

## Quality comparison — actual source renders

The corrected and untouched-original renders below use the same camera, lighting, material maps, scale and resolution. The rejected export is retained as historical visual evidence. These are **Blender source renders, not Unreal screenshots**.

| Rejected export (`ae5cf6e`) | Corrected separated source |
| --- | --- |
| ![Rejected P02](Previews/P02_rejected_ae5cf6e.png) | ![Corrected P02](Previews/P02_three_quarter.png) |

![Untouched Meshy original under the identical camera and lighting](Previews/P02_original_same_camera.png)

![Final FBX reimport with source PBR rebound, same camera and lighting](Previews/P02_fbx_same_camera.png)

The final-export view uses reimported FBX geometry/normals and explicitly rebinds the source PBR slots for the same material setup. It does not claim automatic FBX or Unreal material conversion.

The corrected source visually retains the original slide, serrations, bevels and panel boundaries. Image analysis measured mean absolute RGB difference **0.021/255** between corrected and original renders; **0.073%** of pixels differ by more than 8/255 in any channel. This supports the inspected comparison only; it is not a runtime or general topology test. [Measurement](quality-comparison.json). The remaining generated shape/texture limitations belong to the original Meshy result; this correction preserves its detail rather than claiming exact reproduction of the concept image.

## Scale and fit evidence

| Envelope, cm | Length X | Width Y | Height Z |
| --- | ---: | ---: | ---: |
| P01 unchanged | 23.929 | 4.300 | 18.325 |
| Corrected P02 | 21.000 | 3.352 | 13.833 |

![P01 above P02, one camera and identical scale](Previews/P01_P02_same_scale.png)

![Corrected side view](Previews/P02_side.png)

![Frozen 170 cm neutral-height character, supplied crouched rifle pose](Previews/P02_full_character.png)

![Right palm and fingers](Previews/P02_grip_contact.png)

![Unmodified support-hand gap](Previews/P02_support_hand_gap.png)

![Weapon-only 90-degree horizontal-FOV composition](Previews/P02_first_person_90deg.png)

Studio orthographic span is 31 cm; ruler ticks are 1 cm over 21 cm. Three-quarter camera `(7,-65,26)` looks at `(5,0,0)`; side camera `(5,-65,0)` looks at `(5,0,0)`. P01/P02 uses one 48 cm orthographic camera without individual scaling. Character/FPS camera transforms are in [fit measurements](fit-measurements.json).

The frozen ART-13 source hash matches commit `4c2887fe7ab34bdff96efc6686cfe9147fb2481b`. Only an ignored task-local copy was loaded. Neutral height stays 170 cm at scale 1. The unchanged `A_UE01_Walk_Backward_with_Gun`, frame 1, is crouched. No character or pose edits were made.

The grip envelope at local Z = -3 ±0.5 cm is **6.834 × 3.166 cm** (X/Y vertex band, not internal clearance). The palm is accommodated in the inspected view, but frozen fingers still overlap the lower trigger guard and the support wrist remains **23.089 cm** from Grip. A pistol-specific finger/support-hand pose remains necessary; no two-hand fit or animation acceptance is claimed.

## Checks and remaining work

Blender 5.2.2 LTS: reopened the saved corrected source; reimported all three FBXs; matched triangle/UV counts and bounds within **0.000000716 cm**. Sampled 4,537 source corner normals; over 99% located after coordinate rounding in each FBX, with maximum matched angular difference **0.035 degrees**. [Export verification](export-verification.json). Inspected corrected/original and rejected comparisons and the final FBX with identical source PBR, plus the regenerated scale/character/grip/FPS views. Packed images and unchanged original hashes checked. Task-scoped diff/LFS checks accompany publication. No new test framework.

Art/user visual review, runtime LOD budget and any later reduction/rebake remain pending. Native import must check normals, material channels, scale/socket/camera placement, part motion and hand intersections. No Unreal launch/import/build, multiplayer suite, packaged client, dedicated-server or cross-platform check ran; no UE slot used. The coordinator assigns a later single integration owner. This does not extend INT-ART-01 automatically.

## Provenance and reproduction

Built-in imagegen input and [exact prompt](../../../Art/Source/Weapons/Pistol02/Reference/PROMPT.md) are unchanged. Official Meshy CLI 0.8.2 / Node 22.21.1, pinned `meshy-7.1`, standard geometry, 4K textures/PBR, no remesh. Task `01a1a6c5-c95f-77e6-8e57-3e88b38c77ea`; **30/30 credits originally, 0 additional credits for correction**. No paid retry or extra stage. [Manifest](meshy-manifest.json), [request](request.json), [pricing](https://docs.meshy.ai/en/api/pricing). Base-color/normal maps are 4096px; metallic/roughness are 2048px as supplied. Original GLB/FBX, four maps and thumbnail remain untouched under `Originals/`.

Original generated project art; no branded weapon or external gun mesh was copied. Meshy account/service terms govern generated outputs; no exclusive-copyright or separately verified license claim. Existing character/P01 references are not duplicated in this delivery.

Run Blender `--factory-startup --background --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Pistol02/Models/cleanup_p02.py`, then `render_p02.py`, `fit_preview.py`, and `verify_exports.py`, sequentially. These overwrite task outputs; preserve manual edits first. Fit scripts need the ignored frozen reference copy and Textures folder listed in `character-reference.json`. No script calls Meshy, spends credits or starts Unreal. The historical rejected render is retained from `ae5cf6e`; it is not regenerated by the corrected pipeline.
