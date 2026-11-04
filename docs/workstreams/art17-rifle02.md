# ART-17 — R02 compact rifle

## 2026-11-04 — Source handoff (Europe/Berlin)

**Implementation ready for art review; verification pending in Unreal.**
Implementation commit: `134ae071cf3b105075e1346446444cada32cd4cc`.
[PR #76, targeting main](https://github.com/vaskodagamo/ValStrike/pull/76).
Branch `codex/art17-rifle02`; assigned worktree `.parallel-worktrees/art17-rifle02`;
prepared HEAD `e0071ce295c78cfdfa453af86b83596dc4cfa2ec`.

Delivered a new ImageGen → Meshy → Blender rifle, not a resized R01. Packed editable
source, original dense draft, two FBX parts, PBR textures, reference/prompt and all
required real source previews are confined to the assigned paths. R02 measures
**74.995 × 6.599 × 25.448 cm**; palm-region grip width approximately **3.20 cm**;
**39,614 body + 1,844 magazine triangles**. Magazine has an independent insertion pivot.
[Source, actual previews, measured fit and full socket/import recipe](../assets/rifle02/README.md).

One Meshy 7.1 standard image-to-3D task:
`01a1a6c6-03ec-7521-8b6c-382a26f3c320`; **30 credits consumed of 30 allowed**.
Submitted once asynchronously, recorded immediately, resumed after the wait timeout,
downloaded/preserved GLB, FBX, thumbnail and four maps; no paid retry or extra stage.
[Model/options, prompt/reference and original hashes](../assets/rifle02/meshy-job.json).

Checks actually run: early/final rendered source inspection; frozen ART-13 blend
hash matched its commit; local cleanup corrected duplicated caps/zero-thickness fins;
saved-source reopen and actual FBX round-trip passed with zero boundary/non-manifold
edges, matching triangles/UVs/pivots/scale, maximum bounds error 0.0000017 cm; Python
compilation, `git diff --cached --check`, path-scope review and LFS pointer/upload checks.
[Final export evidence](../assets/rifle02/export-verification.json).

Fit limitations: unchanged 170 cm character, shooting clip frame 1, no pose or scale
edits. Right middle-finger samples are ~0.12/0.13 cm from the mesh; index tip 0.36 cm,
thumb 1.29 cm; left middle samples 1.63/0.73 cm. These are unsigned surface distances.
Stock/face/shoulder alignment is visibly unfinished; stock-marker-to-upper-arm-head
is 24.75 cm (joint diagnostic, not a skin-gap measurement). No fit pass is claimed.
Generated receiver undulation and rail/sight softness remain visible for art review.

Deferred: native import/reopen/material recreation, world/first-person camera,
socket/animation/finger/shoulder acceptance, LOD/performance and runtime/network checks.
No Unreal operation, build or test suite was run. No character, R01, palette, Content,
code, map, central board/log or another checkout was modified. No workers created.

Next owner: coordinator/art lead reviews the source; a separately scoped animation
and integration owner resolves hold/stock fit and imports the replacement. The user
retains PR merge authority. Stop at this source handoff.

## 2026-11-04 — User-requested source-preservation repair

**Supersedes the rejected visual result above.** The untouched Meshy output was
substantially cleaner; the initial local reduction, abrupt grip-depth adjustment
and replacement shading caused the visible degradation. The earlier description
of the dents/softness as remaining generated quality was misleading.

Corrected source commit: `67f985be0099ac870ed2b0c6ed87443a07380406`.
[Follow-up PR #89 to main](https://github.com/vaskodagamo/ValStrike/pull/89), branch
`codex/art17-rifle02-preserve`, same owned worktree/paths. #76 was already merged,
so this correction is published separately. **Source-preservation review: pass by
ART-17 owner on 2026-11-04; producer/user art review and native verification pending.**

Recovered the original full surface detail, UVs, authored normals and PBR. No
reduction, recolor, roughness remap or normal-map weakening remains. Only 75 cm
normalization with matching normal transforms, continuous palm-width fitting, and
connected magazine separation/caps are applied. Exact embedded texture bytes are
portable in `Textures/GLB/`; the untouched master remains in the packed `.blend`.
Original GLB SHA-256: `dc937134bebde584a32d5586db37eab7b393165f1aabb08f010d41ec34b7594b`.
Delivered blend SHA-256: `fb282abdc17285f50e8a30efdced65a5a5c62f1896c6ec3f19c7336f0830afa2`.

Inspected [matched original/source/actual-FBX views and detached caps](../assets/rifle02/README.md#original--corrected--export-quality-gate)
with identical camera, lights, scale and original PBR; refreshed the scale, character
and FOV evidence. The [focused export check](../assets/rifle02/export-verification.json)
passes: 448,486 + 11,860 triangles, no boundary/non-manifold edges, matching UVs and
pivots, bounds error ≤0.0000017 cm, 8,551 sampled custom normals within 0.028°,
packed/external texture bytes identical. Python/diff/LFS checks passed.

Final envelope **75.000 × 7.251 × 25.455 cm**, grip width **3.22 cm**. The magazine
socket is now **(15, 0, −2.5) cm**, identity rotation/scale; other anchors unchanged.
The existing frozen-pose shoulder/finger limitations remain documented. Full-detail
source is deliberately retained; native/material/runtime/LOD acceptance stays with
assigned integration. No Unreal, build, new test suite, shared skill edit or worker.
**Zero additional credits; original task remains 30/30 credits, no retry.**

Next owner: Direktor (2)/producer reviews #89 and assigns integration/animation work;
user merges. Stop at this corrected source handoff.
