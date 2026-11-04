# ART-28 — R02 shoulder and hand fit review

**Implementation ready for art-lead review; native/runtime verification pending.** Source-owner inspection: 2026-11-04, Europe/Berlin. [Frozen revision and hashes](readiness.json), [assembly matrices and animation contract](assembly-contract.json), [editable source/native recipe](../../../../Art/Source/Characters/UrbanEntry01/Rifle02Pilot/README.md).

The corrected ready pose seats R02 against the right shoulder/chest pocket, clears the head, indexes the dominant trigger finger outside the guard, and cups the fore-end with the support palm. The torso turns to bring the supporting shoulder forward without extending the rig. Hold, walk and run use the same fitted upper body; the original lower-body cycles remain unchanged. Character, rifle and bones retain scale 1.

## Observed defect and correction

The producer identified the unseated stock at the cheek in actual [INT-ART-04 before/after runtime evidence](../../weapon-replacement02/runtime/remote_before_after.png) and [Site01 remote rifle](../../weapon-replacement02/runtime/site01_remote_rifle.png). The old native offset faithfully reproduced the unfinished source fit. The following comparison renders that old pose/offset and the corrected source under the **same Blender camera, lighting and material**; neither image below is a runtime capture.

| Old source fit, known defect | Corrected pose and attachment |
| --- | --- |
| ![Before](Previews/before_known_defect.png) | ![Corrected](Previews/reimport_right.png) |

Both the animation and the `hand_r` offset must change together. Moving only the old offset is not this correction. This is a ready/hip-fire presentation pose; sights are not aligned to the eye for ADS.

## Preserved surfaces and actual exported motion

Original character/R02 surfaces are posed identically to the candidate. The actual new animation FBX is then reimported onto the original character FBX, with original PBR rebound for matched shading. Existing R02 mesh-export verification is reused; the weapon mesh is unchanged.

| Original surfaces in corrected pose | Candidate | Actual animation FBX reimport |
| --- | --- | --- |
| ![Original right](Previews/original_right.png) | ![Candidate right](Previews/candidate_right.png) | ![Export right](Previews/reimport_right.png) |
| ![Original left](Previews/original_left.png) | ![Candidate left](Previews/candidate_left.png) | ![Export left](Previews/reimport_left.png) |

Inspect [dominant grip](Previews/reimport_grip.png), [opposing grip](Previews/reimport_grip_reverse.png), [support palm](Previews/reimport_support.png), [stock/face right](Previews/reimport_stock_right.png), [stock/face reverse](Previews/reimport_stock_left.png) and [top view](Previews/reimport_top.png). [Hold](Previews/hold_5m.png), [walk](Previews/walk_5m.png) and [run](Previews/run_5m.png) are unzoomed 5 m / 90° horizontal FOV / 1920×1080 source views.

![Actual export locomotion extremes](Previews/motion_overview.png)

Source-owner result: **pass for the bounded third-person source handoff, with the contact/deformation limits below**. The shoulder seat, head clearance and two hand contacts are visible from opposing views; the full-size rifle remains readable at the reviewed game distance. Original surface detail, normal response and PBR are retained. Art-lead review is separate; no native or runtime approval is implied.

## Checks actually run

- Blender **5.2.2 LTS**, CPU threads 2: saved source reopened; fingerprints equal the frozen character positions/topology, UVs, custom normals, weights, materials, 66-bone rest/hierarchy and all eight prior pilot/source actions plus the rest action. Both R02 part meshes and both materials remain exact. [Input identities](source-references.json), [detailed results](source-validation.json).
- Three actual animation-only FBXs imported onto the original character. Hold **1–61**, Walk **1–63**, Run **1–39**, all **60 FPS** and root motion off. Thirteen endpoint/midpoint/gait-extreme samples agree within **0.000131 cm at joints** and **0.000144 cm at sampled skin vertices**. Loop seams are below 0.00009 cm. Walk/Run lower-body F-curves are exactly retained.
- Sampled real stock-pad vertices contact the shoulder/chest surface across the checked extremes; nearest distance stays below **0.00046 cm**. Nearest-normal signs indicate up to **0.302 cm** of local clothing/pad overlap, and the curved chest does not touch the whole flat pad. The head's sampled vertices remain at least **16.27 cm** from the rifle surface. These are surface samples, not joint-to-joint estimates or continuous/blended clearance proof.
- Final hand diagnostics indicate at most **0.219 cm** right and **0.066 cm** left nearest-normal negative depth. Large buried pads were corrected through palm placement/finger rotations. Residual millimeter-scale contact is disclosed rather than called collision-free. Open/multipart character surfaces and nearest-normal signs do not establish exact penetration volume; opposing visual views remain the quality gate.
- Expected native `C × S × C` basis conversion was checked against INT-ART-04's existing measured R02 offset/quaternion. The new pose/offset has not been imported in Unreal. Python syntax, JSON/local links, frozen input/artifact/LFS hashes, owned scope and diff whitespace are checked before publication. No new test framework, account/API call, paid generation, Unreal launch, build or multiplayer run.

## Limits and next owner

The preserved glove has angular finger pads/creases and small close-up cavities. These remain visible in the honest contact views; this is **not a hero-hand or first-person approval**. The ready stance is constant, with inherited walking/running body motion. Existing clothing bends, foot timing and gait rate were not redesigned. R02 retains its full **460,346 triangles** and original **75.000 × 7.251 × 25.455 cm** envelope; no runtime performance/LOD acceptance is added.

Art lead reviews the frozen source. A later bounded native task after INT-ART-03 must import the three actions, change the socket and animation selection together, confirm both imported bases, and check aim/reload/crouch/airborne/switch blends, contact through transitions, locomotion rate and rendered server/client appearance. This artist does not edit native assets or gameplay. The user retains merge authority.
