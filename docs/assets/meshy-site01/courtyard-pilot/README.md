# ART-25 — Preserved courtyard structure group

2026-11-04 (Europe/Berlin). **W01, W08 and W09 are source-ready for the bounded decorative Site01 assembly below; native verification is pending.** These are the original supplied meshes with uniform scale, a base-corner pivot and corrected external texture paths. No reduction, smoothing, recoloring, surface snapping, reconstruction or rebake was applied. The old ART-11 and ART-16 sources remain unchanged.

[Readiness allowlist](readiness.json) · [Site01 recipe](placement.md) · [measured source assembly](Previews/Assembly_Dimensions.png) · [exact measurements](assembly-measurements.json)

## Delivered subset and scope

| ID | Actual identity | X × Y × Z (cm) | Triangles | Source verdict |
| --- | --- | --- | ---: | --- |
| MSH-W01 | Concrete grid wall | 194.2425 × 18.2067 × 300 | 4,231 | ready as backed facade cladding; standalone watertight structural use not approved |
| MSH-W08 | Pedestrian doorway frame | 255.2130 × 48.7428 × 300 | 4,388 | ready as a visibly backed facade recess; no new traversable opening, not a garage |
| MSH-W09 | Straight capped parapet | 200 × 37.3730 × 63.5910 | 4,390 | ready as decorative roof-edge modules on the existing inaccessible service roof; not 120 cm cover |

Each asset folder in [CourtyardPilot](../../../../Art/Source/Environment/MeshySite01/CourtyardPilot/) contains an editable `.blend` with the original imported master hidden, one preserved delivery mesh, a selected-mesh FBX, a byte-identical `Original.fbx` recovery copy and four original texture PNGs. Import only the selected FBXs/maps in the readiness allowlist. The standalone [CourtyardAssembly.blend](../../../../Art/Source/Environment/MeshySite01/CourtyardPilot/CourtyardAssembly.blend) holds six proposed visuals, cameras and labeled map-context proxies; its boxes, guide and lights are not game assets or a map export.

The original W01 and W08 boundary counts (203 and 22) remain; W09 is closed. Counts alone were not treated as visible damage. The complete front/back views show retained surfaces; backed placement avoids claiming these sources are watertight structural replacements. No extra geometry was justified for this bounded use. Uniform factors are 3 for W01/W08 and 2 for W09, preserving original proportions rather than stretching them to ART-11's nominal envelopes. Blender uses meters internally, exported unit metadata supplies centimeters. The source pivot is minimum XYZ, source front −Y and up +Z. Native FBX Y-negation is expected from the same exporter already observed by INT-ART-01; the integrator must measure it before placing.

## Actual visual review

The reviewer is the ART-25 assigned art session, 2026-11-04. Source review **pass for the stated decorative subset**; native/runtime **pending**. Matched images use the same camera, framing, uniform scale, lights, AgX view, exposure and supplied material channels. Original mesh data remains untouched; its object presentation transform matches the source for comparison. The FBX column is the actual reimport, with the same supplied PNG channel mapping restored after FBX material recreation. Whole and close views show no new visible dents, lost seam detail, changed paint borders or flattened shading.

- [W01 original/source/FBX](Previews/MSH-W01_Comparison.png): concrete casting, form holes, cross-seam and dark base strip retained. [Back](Previews/MSH-W01_source_back.png) has no gross visible hole in this view; topology remains open.
- [W08 original/source/FBX](Previews/MSH-W08_Comparison.png): full original width, doorway bevel, inset frame, metal cap, crimson stripe and yellow marks retained. [Back](Previews/MSH-W08_source_back.png) is also textured. Source aperture samples at the mid-depth plane are 133.80–134.54 cm wide, with a center lintel at 225.12 cm; these sparse samples do not establish traversability, and the Site01 recipe keeps the wall behind it.
- [W09 original/source/FBX](Previews/MSH-W09_Comparison.png): cap bevels, recessed rail, screws and painted concrete retained. [Back](Previews/MSH-W09_source_back.png) retains the supplied finish. No stretching to an artificial 80 cm height.
- [Full-facade eye-level reference](Previews/Assembly_eye_152cm.png), [actual-route-coordinate source approach](Previews/Assembly_service_approach.png), [overhead](Previews/Assembly_overhead.png), [wall/frame joint](Previews/Assembly_mating.png) and [roof joint](Previews/Assembly_roof_joint.png) are real Blender renders. The blank solid backing inside W08 makes it a blind recess; it is not an open route. The wall/frame reveal has a **1 cm nominal envelope gap but 1.88–4.85 cm sampled profile gaps**; eight of twenty near-front ray samples miss one profile. This is backed decorative mating, not a seamless/watertight assembly. The capped roof modules have deliberate 1 cm end gaps and a visible V at the cap ends. Keep these seams; do not stretch or smooth the sources to hide them.

The palette and construction follow the approved Industrial V2 reference: light concrete, slate caps, limited crimson/yellow paint. Existing irregularities and generated texture artifacts are preserved, not silently polished away. This approval does not establish final-production close-up quality or permit mass repetition across the map.

## Preservation and checks actually run

All fifteen canonical original file hashes matched intake before preparation; twelve copied PNGs and three recovery FBXs remain byte-identical. Original shaders already connect separate base color, tangent normal, roughness and metallic maps. The necessary path correction rebinds embedded JPEG/temp aliases to the accompanying canonical lossless PNG of the same channel, retaining shader factors and channel connections. No ORM packing is assumed. [Original import metadata](original-inspection.json) records the imported shader and authored corner normals.

Three selected sources were saved/reopened and three actual delivery FBXs were reimported. Source UV values equal the originals exactly; original normals were transformed consistently with the positive uniform affine transform. [Export surface inspection](assembly-measurements.json) confirms identical polygon index sequences, zero UV-coordinate error, minimum transformed-corner-normal dot ≥0.99999974 and maximum vertex-position error <0.00004 cm after FBX round-trip. These targeted asset checks and the matched renders are complementary; counts alone are not the visual verdict. [Assembly reopen](assembly-reopen.json) resolves all twelve material image references and six placed art objects.

Blender 5.2.2 LTS, Cycles CPU, four threads, 16 samples. One initial source pass stopped at a Python reference invalidated by reopening the saved W01 file; the reference was fixed and unchanged completed images reused. The successful pass completed all three assets, and the later assembly run completed its measured views and reopen. No Unreal process/build, new test suite, paid Meshy job, account request or service call ran.

## Reproduce only when needed

From the assigned checkout, one local Blender process at a time:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python-exit-code 1 --python Art/Source/Environment/MeshySite01/CourtyardPilot/prepare_sources.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python-exit-code 1 --python Art/Source/Environment/MeshySite01/CourtyardPilot/assemble_courtyard.py
python3 Art/Source/Environment/MeshySite01/CourtyardPilot/compose_evidence.py
```

Require `ART25_SOURCE_DONE` and `ART25_ASSEMBLY_DONE` plus complete reports. The source script's `--resume-unchanged` switch was used only for that code-only reopen correction; never use it after changing geometry, material, lights or camera. The canonical inputs are read-only; FBX embedded extraction takes place in a temporary directory. Other-Mac source editing can use the delivered blends/recovery FBXs/maps without the local inbox. Scripted regeneration currently expects the documented canonical inbox on this Mac.

## Remaining work and named later gaps

INT-ART-02 imports this frozen subset, measures native bounds/front/back, reconstructs the supplied material channels, saves/reopens and checks the proposed section, routes, visibility, camera clearance and current gameplay markers in its own engine slot. Native normal direction, shading, platform performance/LOD, gameplay/runtime and production acceptance remain pending. Do not advertise this source assembly as an implemented or playable map section.

W05 is an L parapet; W06 is a three-sided alcove, not an L corner; W02 stairs need original-tread inspection after prior damaging reduction. A true garage, roof module and missing true corner are named later gaps. None is approved, repaired, regenerated or imported by ART-25.

Provenance: user-supplied Meshy exports from project concept references, [central intake](../../meshy-site01-intake.json). Original paths and hashes are in each manifest. Generation IDs and vendor license evidence remain unspecified in that intake; this pass makes no new licensing claim. Paid stages/service calls/credits: **0**.
