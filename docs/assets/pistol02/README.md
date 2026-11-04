# P02 Kestrel — ART-18 source handoff

2026-11-04 (Europe/Berlin). Source implementation ready for art review; **native verification pending**. New Meshy-derived compact pistol, not a scaled P01. User merges the PR. No native Content, gameplay, character source or central status was edited.

## Result and evidence

| Assembled envelope, cm | Length X | Width Y | Height Z |
| --- | ---: | ---: | ---: |
| P01 unchanged reference | 23.929 | 4.300 | 18.325 |
| P02 final | 20.959 | 3.346 | 13.806 |
| Reduction | 12.41% | 22.19% | 24.66% |

Three meshes: Body 12,514 triangles; Slide 13,756; Magazine 634. Total **26,904** triangles, two shared materials, one UV channel. Identity mesh transforms; centimeters, +X muzzle-forward, +Z up. The original draft had 454,988 triangles. Local cleanup welded coincident UV-boundary geometry, reduced and lightly relaxed the surface, cut/capped separate slide and magazine floor, and added a concealed simple magazine visual stem. Existing external UV loops remain; cut interiors use the constant interior material. No functional internals.

[Editable packed Blender source](../../../Art/Source/Weapons/Pistol02/Models/P02_Kestrel.blend), [three FBX parts](../../../Art/Source/Weapons/Pistol02/Models/Exports), [measurements](source-measurements.json), [export check](export-verification.json), [source hashes](source-hashes.json).

## Genuine source previews

All images below are Blender renders of the actual cleaned mesh, **not Unreal/game screenshots**. Studio views use orthographic cameras (31 cm span); ruler ticks are 1 cm, total 21 cm. Old/new uses one orthographic camera with a 48 cm span, identical scene units, no individual scaling; P01 is above P02. Three-quarter camera `(7,-65,26)` looks at `(5,0,0)`; side camera `(5,-65,0)` looks at `(5,0,0)`. Character cameras and 90-degree horizontal-FOV camera are recorded exactly in [fit measurements](fit-measurements.json).

![P01 above P02, same scale and camera](Previews/P01_P02_same_scale.png)

![P02 three-quarter with centimeter ruler](Previews/P02_three_quarter.png)

![P02 side](Previews/P02_side.png)

![Frozen 170 cm neutral-height character, supplied crouched rifle-pose frame](Previews/P02_full_character.png)

![Actual right palm and fingers](Previews/P02_grip_contact.png)

![Unmodified support-hand gap](Previews/P02_support_hand_gap.png)

![Weapon-only 90-degree horizontal-FOV composition](Previews/P02_first_person_90deg.png)

## Fit result and limits

The frozen ART-13 blend hash matches commit `4c2887fe7ab34bdff96efc6686cfe9147fb2481b`. Only a task-local ignored copy was loaded. Character neutral height stays 170 cm and scale stays 1. The displayed action is unchanged `A_UE01_Walk_Backward_with_Gun`, frame 1; its crouch is not a shortened character.

The grip cross-section envelope at local Z = -3 ±0.5 cm is **6.801 × 3.136 cm** (X/Y vertex band; not an internal-clearance measurement). The palm is accommodated in the inspected render, but the supplied rifle-pose fingers overlap the lower trigger guard. The support wrist is **23.089 cm** from the weapon Grip datum and does not contact the pistol. A pistol-specific right-finger and support-hand pose remains necessary; no two-hand fit pass or animation readiness is claimed. No character pose was authored or saved.

Residual Meshy waviness remains along slide/edge highlights. The local finish reduces generated micro-normal strength and metallic response, but this is a source art-review candidate, not final close-up surface approval. The magazine is an exterior visual shell plus concealed stem; no detailed inner reload cavity. Native inspection must check material conversion, cap/slide motion, muzzle alignment, import scale, camera framing and hand intersections.

## Checks actually run

Blender 5.2.2 LTS: original GLB inspection; saved final blend reopen; three-part FBX round-trip with matching triangle/UV counts and maximum bounds error **0.000000835 cm**; packed-image presence; actual studio, comparison, full-character, grip, support-gap and FPS render inspection. One failed smoothing preview exposed UV-boundary cracks; welding before smoothing corrected them, and affected source/export/render evidence was regenerated. No test framework was added.

`git diff --check` and task-scoped Git LFS checks accompany publication. No Unreal launch/import/build, multiplayer suite, packaged client, dedicated-server or cross-platform check ran. The coordinator assigns later native import to one integration owner; current INT-ART-01 scope is not automatically extended.

## Provenance and reproduction

Built-in imagegen produced the isolated input: [exact prompt](../../../Art/Source/Weapons/Pistol02/Reference/PROMPT.md). Official Meshy CLI **0.8.2** under Node **22.21.1**, explicit `meshy-7.1`, standard geometry, 4K texture request/PBR, no remesh. Job `01a1a6c5-c95f-77e6-8e57-3e88b38c77ea`, operation `art18-p02-20261104-01`, succeeded; **30 actual credits / 30 authorized**, no retry or extra stage. [Manifest](meshy-manifest.json), [exact options](request.json), [official pricing](https://docs.meshy.ai/en/api/pricing). The base-color and normal exports are 4096px; packed data map is 2048px as returned by Meshy. The 4K request does not mean every returned PBR channel is 4K.

Original GLB/FBX, all four returned texture files and thumbnail are preserved unmodified under `Art/Source/Weapons/Pistol02/Originals/`, with hashes. Inputs are original generated project art; no branded firearm or third-party gun mesh was used. Meshy service/account terms govern generated outputs; no claim of exclusive copyright or separately verified license is made. Character/P01 remain existing project references and are not duplicated in this delivery.

Reproduce local source cleanup with Blender `--factory-startup --background --threads 2 --python-exit-code 1 --python Art/Source/Weapons/Pistol02/Models/cleanup_p02.py`, then `render_p02.py`. These overwrite P02 generated outputs; preserve manual changes first. Fit scripts require the ignored frozen character copy described in `character-reference.json`, including its Textures folder. Run `fit_preview.py`, then `verify_exports.py` sequentially. No script invokes Meshy, starts Unreal or spends credits.
