# K01 source review

Final ART-20 source restores the original Meshy appearance. The earlier reduction/reconstruction degraded the blade and guard; it is held under `Candidates/Rejected_Retopology/`, not delivered as the final knife. Source-ready for review; dedicated hand posing, native and runtime verification remain pending.

![Final original-preserving source render](Previews/neutral_three_quarter.png)

## Matched preservation review

Same camera, lights, centimeter scale and original PBR setup. FBX shaders are restored by name from the preserved original graph because FBX does not carry every Blender node.

| Untouched Meshy baseline | Final normalized source | Reimported final FBX |
| --- | --- | --- |
| ![Original](Previews/Comparison/original_pbr.png) | ![Source](Previews/Comparison/candidate_pbr.png) | ![Export](Previews/Comparison/export_pbr.png) |
| ![Original common material](Previews/Comparison/original_common_material.png) | ![Source common material](Previews/Comparison/candidate_common_material.png) | ![Export common material](Previews/Comparison/export_common_material.png) |

The second row applies one identical diagnostic material to all three temporary render scenes. [Comparison record](comparison.json) records preservation fingerprints and render differences. [Held reconstruction](Previews/Rejected/Retopology/candidate_pbr.png) and [earlier uneven correction](Previews/Rejected/uneven_blade_correction.png) remain as defect evidence. They are not accepted alternatives.

## Frozen files and measurements

- [Source/export/texture/preview hashes](manifest.json), [readiness](readiness.json).
- [Bounds, topology, UV/normal preservation, Grip/Tip frames](source-inspection.json).
- [Source reopen, actual FBX roundtrip and frozen-character fit](verification.json).
- [Single paid Meshy job](job.json), [exact request options](request.json).
- [Source and reproduction instructions](../../../Art/Source/Weapons/Knife01/README.md).

The final asset retains 70,640 triangles, one original material and its UV layout. No speculative triangle target justifies changing these surfaces. Further optimization, if needed, requires its own measured reason and comparison candidate.

## Same-scale fit evidence

![Actual-material reference hold](Previews/fit_right_hand_pbr.png)

![Opposite hand inspection](Previews/fit_reverse.png)

![Unscaled character and knife](Previews/fit_character.png)

The frozen `UrbanEntry01_Pilot.blend` stays unchanged at scale 1, static rifle hold frame 1. The knife exits the index side of the hand. Exact bone-head-relative matrix, character hash and dimensions are recorded. This is a **reference hold**: the index finger remains extended and the grip intersects the glove. The sampled blade and guard have no detected hand-surface intersections, which is not motion clearance. Do not rescale the character to compensate. FPArms02 still requires its own dedicated pose/attachment review.

Art lead reviews this preservation-first source. The assigned hands/animation owner poses against the frozen manifest. A separately assigned integration owner must import, reconstruct the original packed material channels and verify the actual native axes in a reserved engine slot. INT-ART-01 has no automatic ownership of this new native knife.

No Unreal, packaged-client, dedicated-server or multiplayer verification occurred. Knife combat is not implemented by this art task. Equip logic, damage, melee traces/RPCs, movement benefits and attack animations remain outside scope. Exactly one Meshy job consumed 30 credits; no paid retry or extra stage occurred.
