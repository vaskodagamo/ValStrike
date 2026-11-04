# K01 source review

ART-20 supplies a compact fictional knife visual. Source-ready for review; native and runtime verification pending. The user rejected an earlier uneven surface correction; `Previews/Rejected/uneven_blade_correction.png` preserves that evidence. The current source locally retopologizes the blade and trim while retaining the actual generated grip and UVs.

![Current neutral source render](Previews/neutral_three_quarter.png)

![Opposite side](Previews/neutral_reverse.png)

## Frozen files and measurements

- [Source, export and preview hashes](manifest.json)
- [Measured mesh, bounds and Grip/Tip frames](source-inspection.json)
- [Source reopen, actual FBX roundtrip and frozen-character fit](verification.json)
- [Readiness and next owner](readiness.json)
- [Single paid Meshy job and provenance](job.json), [exact request options](request.json)
- [Source and reproduction instructions](../../../Art/Source/Weapons/Knife01/README.md)

## Same-scale fit evidence

![Actual-material reference hold](Previews/fit_right_hand_pbr.png)

![Opposite hand inspection](Previews/fit_reverse.png)

![Unscaled character and knife](Previews/fit_character.png)

The character is the unchanged frozen `UrbanEntry01_Pilot.blend`, static rifle hold frame 1. Character, rig and weapon remain at scale 1. The knife exits the index side of the hand; `verification.json` records the exact bone-head-relative matrix and source hash. This is a **reference hold**, not knife animation acceptance: the index finger remains extended and there are grip/glove surface intersections. Blade/guard intersection diagnostics are recorded separately. Do not scale the character to compensate. FPArms02 requires its own later pose/attachment review against this frozen knife.

## Limits and next action

Art lead reviews the revised source and silhouette. The assigned hands/animation owner then makes a dedicated grip and checks both sides, finger contact and motion clearance. A separately assigned integration owner must import through Unreal, reconstruct materials and attachment frames under the actual imported axes, and inspect the result in a reserved engine slot. INT-ART-01 has no automatic ownership of this new native knife.

Source inspection and Blender renders are not native, packaged-client, dedicated-server or multiplayer evidence. Knife combat is not implemented by this art task. Equip logic, damage, melee traces/RPCs, movement benefits and attack animations are outside scope. No paid retries or extra service stages occurred.
