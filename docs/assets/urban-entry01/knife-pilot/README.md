# ART-27 — K01 third-person source review

**Implementation ready for art-lead/native review; runtime verification pending.** Six actual animation exports add K01 hold, equip, walking, running, slash and stab to the preserved UrbanEntry rig. Source-owner inspection: 2026-11-04, Europe/Berlin. Exact frozen revision and hashes are in [readiness](readiness.json).

![Equip, slash, stab and inherited locomotion poses](Previews/motion_overview.png)

The upper body lifts the knife from the right hip, holds it with a closed index finger, cuts across the front and recovers, or thrusts forward and withdraws. The guard hand remains to the character's left. Slash impacts at **0.15 s**, finishes at **0.55 s**; stab impacts at **0.30 s**, finishes at **0.90 s**; equip is **0.40 s**. All use 60 FPS with frame 1 = time zero. [Exact names/ranges and attachment matrices](animation-contract.json); [source/native assembly recipe](../../../../Art/Source/Characters/UrbanEntry01/KnifePilot/README.md).

## Preservation and actual export comparison

The intact original mesh/rig/material and K01 are posed identically to the candidate, under the same camera, studio and exposure. The last column uses the actual new animation FBX on the existing character FBX; original PBR is rebound for consistent shading.

| Original surfaces in the new pose | Candidate | Actual FBX reimport |
| --- | --- | --- |
| ![Original](Previews/hold_original_matched.png) | ![Candidate](Previews/hold_candidate_matched.png) | ![Reimport](Previews/hold_reimport_matched.png) |

[Reverse grip](Previews/hold_reimport_reverse.png), [slash impact](Previews/slash_side_10.png), [slash follow-through](Previews/slash_side_16.png), [stab impact](Previews/stab_side_19.png). [Hold at 5 m](Previews/hold_5m.png), [slash at 5 m](Previews/slash_5m.png), [stab at 5 m](Previews/stab_5m.png) use 1920×1080 and 90° horizontal FOV, without crop/zoom. These are Blender source previews, not game screenshots.

Source-owner result: **pass for the bounded third-person source handoff**. Matched views retain the original texture detail, knife edge, shading and silhouettes. Contact/whole-body/impact/recovery views were inspected; attack wrists were corrected after the initial side views exposed excessive flexion. The final slash has a lateral sweep, while the stab advances/withdraws with a slightly rising blade. Knife/hand silhouettes remain discernible at the reviewed 5 m distance. This does not establish readability at every distance, native lighting or runtime blend state.

## Checks actually run

- Reopened the saved `.blend`; exact fingerprint equality for character positions/topology, UVs, custom normals, weights, material indices/PBR graph, 66-bone hierarchy/rest matrices, original eight pilot/source actions plus rest action, and K01 mesh/PBR. Existing separate PistolPilot and all base files remain untouched. [Frozen input hashes](source-references.json).
- Imported all six actual animation-only FBXs onto the preserved character export. Verified names/hierarchy, rest matrices, ranges and 60 FPS; sampled endpoints/midpoints/impact poses differ by at most **0.000101 cm at joints** and **0.000121 cm at sampled skin vertices**. Hold/walk/run loops close within 0.00009 cm at sampled joints; both attack endpoints match their starts. [Detailed report](source-validation.json).
- Walking/Running lower-body F-curves are exactly unchanged. Pose scales stay 1; no root translation is added. Existing gait motion is retained with root-motion extraction off.
- Discrete surface-crossing checks across all **277 exported frames** found no blade/character, guard/right-hand or knife/body-excluding-right-hand crossing. This includes the guard hand. These are surface checks, not continuous-time collision or proof of zero internal grip penetration. Visual review remains part of the result.
- Blender 5.2.2 LTS, CPU threads 2. Original whole-source review ran once; only the two changed attacks were reimported/rerendered after the wrist correction. Python syntax, JSON parsing, local links, owned-path diff/LFS hashes and whitespace were checked before publication. No new test framework, account/API calls, paid jobs or Unreal operations.

## Limits and next owner

The supplied glove retains angular knuckle/finger creases, a small thumb-side cavity and compressed finger pads at close range. This is **not a hero/first-person hand approval**. No geometry, weights, hand size or knife size was changed to disguise these inherited limits. The character retains existing clothing deformation and the unoptimized K01 retains its original 70,640 triangles.

The ready stance is intentionally constant; locomotion supplies the existing body movement. Stationary attack/equip exports have neutral lower bodies. Native must layer their upper body from `spine_01` over existing moving/crouched/airborne locomotion, match gait rate to movement speed, and verify transitions/cancellations. Source checks do not prove runtime foot-lock or blended clearance.

Art lead reviews the frozen source. INT-ART-03 owns actual native axes/socket calibration, PBR import, animation wiring, accepted cosmetic-event timing, switch/death visibility, combined build and rendered server/client acceptance. Knife reach/damage remain authoritative gameplay rules, independent of the visual blade. No native, packaged-client, dedicated-server, multiplayer or performance acceptance is claimed. The user retains the merge.
