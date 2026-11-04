# ART-16 pilot art review

2026-11-04 (Europe/Berlin). Prepared HEAD `09a2e4b8bf1beead613c69d3333e03af2803b761`; branch `codex/art16-props-pilot`. **W03, W04, W07 and W10 are ready for the bounded native pilot.** This is delegated source art approval; native import, material response, map placement and runtime verification remain pending with INT-ART-01.

The unchanged asset revision is `f7a15a1245ccd97ca0e5e2bc0b0bb4d3f7862e6c` (the prepared cherry-pick of ART-12's source delivery). No mesh, UV, texture or export correction was justified by this review. Historical ART-12 reports remain unchanged. The machine-readable [readiness record](pilot-readiness.json) selects the exact FBX and four material textures for each asset; detailed-master textures are not an alternative game-material set for the reduced meshes.

## Findings and verdicts

Reviewed against the [approved Industrial V2 anchor](../../../../Art/References/Environment/IndustrialV2/00_ApprovedStyleAnchor.png): quiet gray surfaces, inset metal construction, restrained crimson corners and limited yellow details. These neutral Blender renders establish source appearance, not a match to Site01's current lighting. Do not compensate for lighting differences by recoloring the supplied maps.

| Asset | Verdict | Observed evidence and limits |
| --- | --- | --- |
| MSH-W03 paving | ready | [Matched before/after](Previews/ART12_RepresentativeComparison.png) retains the cross seam and shallow beveled silhouette. [2×2 patch](Previews/MSH-W03_TilePatch_2x2.png) has no gross gaps, but the uneven seam/crack motif visibly repeats and the outer seams are not mathematically seamless. Accept four tiles only for this pilot. Preserve 200 cm spacing and floor collision at Z=0; overlapping old paving must be masked as described in the recipe. |
| MSH-W04 long crate | ready | [Before/after](Previews/ART12_RepresentativeComparison.png) retains two front panels, corner protectors, handle recesses and safety marks. Mild rail waviness and softer small fasteners remain after the bake; no torn surfaces or missing large panels are apparent. Accept the 6,000-triangle export without another optimization pass. It is 100 cm high, so it is approved here as rooftop decoration, never a replacement for 120 cm half cover. |
| MSH-W07 infill panel | ready | [Before](Previews/MSH-W07_Before.png), [after](Previews/MSH-W07_After.png) and [back](Previews/MSH-W07_PanelBack.png) retain the slim frame, three inset plates and narrow red header. Both sides appear covered in these views, but the unchanged topology evidence records 506 boundary edges. This is a wall-backed visual shell, not a cabinet, opening or freestanding blocker. Its vendor normal convention remains unlabelled; start from the source's +Y interpretation and resolve the native green-channel check before accepting its material. |
| MSH-W10 cube crate | ready | [Before](Previews/MSH-W10_Before.png) and [after](Previews/MSH-W10_After.png) retain the frame, handles and X panels. The after image shows softened fasteners and some wavy edge highlights, acceptable for the pilot rather than close-up production acceptance. [Stack](Previews/MSH-W10_CrateStack.png) supports the 100 cm vertical repeat; it does not establish physical interlocking. Replace existing 100 cm crate visuals only; preserve their collision and total 200 cm site-stack envelope. |

W02 stairs remain **hold/excluded** with no game FBX. ART-11 structures are excluded and were not re-reviewed. Their presence in inherited source commits is not import approval.

## Checks and evidence scope

- Visually inspected the approved anchor, source overview, representative comparison, tile patch, W10 before/after/stack and W07 before/after/back images. Existing images were opened directly; no generated review imagery, new renders or altered evidence.
- Verified the 24 selected files (four blends, four FBXs, sixteen selected textures) against their recorded SHA-256 values and the corresponding Git LFS pointer OIDs. All matched, so the prior evidence still addresses these unchanged sources.
- Reused [ART-12 source/export checks](source-export-checks.json): five source reopens and four FBX round-trips. The four selected exports retain one material and UV channel, applied source transforms, finite geometry and matching bounds within 0.001 cm. Only W07 has recorded boundary edges. These checks were not rerun.
- Read Site01's authoring data, builder and saved actor inventory to resolve real actor labels, pivot locations and visual overlays. The [placement recipe](pilot-placement.md) is a source-derived plan with native preconditions, not an assertion that a new map was inspected.
- Checked diff whitespace and JSON/file correspondence. No Unreal process, build, paid Meshy stage, auth check, pipeline rerun or new test suite.

Material color space and the explicit baked +Y normal convention are specified in the recipe. Original W07 convention uncertainty and native FBX axis/unit handling remain native gates, not reasons to silently reprocess otherwise acceptable source art. Import, save/reopen, oblique-light/material review, collision and rendered server/client acceptance belong to INT-ART-01. Final production polish, human balance, packaged and cross-machine acceptance remain separate.
