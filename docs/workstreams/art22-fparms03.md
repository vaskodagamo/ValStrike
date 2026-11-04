# ART-22 — Universal gloves with bare forearms

## 2026-11-04 — Source handoff (Europe/Berlin)

Implementation/source package ready for art review; **production deformation and weapon contact held; native verification pending**. Source commit `bcd377d6ad8002b107f610d719842582adf88986`, branch `codex/art22-fparms03`. [PR #92 to main](https://github.com/vaskodagamo/ValStrike/pull/92) is open and unmerged; 47 LFS objects (308 MB) uploaded.

ART-19 remains frozen at `afa582a` / `45cf24d`, PR #90; no further writes there. Its glove/rig was inspected first. The modeled sleeve could not supply anatomical bare skin through clean reuse. Reused the glove direction and 39-bone naming/hierarchy; one authorized new right-hand/bare-forearm draft supplies the intact geometry. Mirrored left has corrected winding, UV corner ordering and normals. Uniform centimeter transform only; full geometry, UVs, authored normals and original PBR retained. New rest landmarks/rolls require retarget/import validation despite name compatibility.

Meshy task `01a1a702-abc1-76dc-97f3-b5ad96d100e1`, reservation **ART-22-FPArms03-draft-1**, terminal **30/30 credits**. One explicit Meshy 7.1 standard image-to-3D, 4K/default texture/PBR, no extra stages or retries. [Manifest and terminal charge evidence](../assets/fparms03/meshy-job.json). Producer alone binds/settles its ledger; this worker did not edit it.

Delivered packed editable blend, preserved originals, skeletal FBX, six bounded actions (open, curl, wrist, rifle, pistol, knife), exact embedded maps, bone table/dimensions, attachment recipe and real source/contact/FOV previews. Only arms and rig exported; frozen weapons are ignored read-only staging. Pair: 780,956 triangles, deliberately retained as full-detail source. [Source instructions](../../Art/Source/Characters/FPArms03/README.md).

Frozen fit dependencies: corrected R02 `67f985be0099ac870ed2b0c6ed87443a07380406` / #89 (not old #76); corrected P02 `ccb08f06e22d23573697a1494a34436f901fa006` / #81; preserved K01 `d774eafb710af68bf5fa23666e5d0ddacb651d3e` / #85. Exact file hashes in [reference record](../assets/fparms03/frozen-weapon-sources.json).

Checks actually run: Blender 5.2.2 LTS source reopen and actual mesh/six-action FBX round trips; names/parents, scale/bounds, weights and sampled pose checks passed. Mesh bounds error ≤0.000006 cm; sampled bone-position error ≤0.000023 cm. Matched original/source/reimported-FBX whole and glove/cuff views inspected: preservation pass, zero transformed position difference, identical original/source UV/topology hashes, tiny normal-storage differences. Original PBR was explicitly rebound for comparison; native shader recreation is not proved. Python syntax, owned-path/diff inspection and LFS publication checks performed. [Export evidence](../assets/fparms03/export-inspection.json), [preservation evidence](../assets/fparms03/preservation-comparison.json).

**Held findings:** dense finger/MCP/web weighting stretches locally (hold p99 edge ratios around 2.2–2.4, larger isolated outliers); stronger knife thumb opposition folds the web. Pistol support fingers/thumb visibly intersect the grip; rifle trigger/guard/support contact needs refinement. Knife off hand is separate, but exact dominant grip/guard contact and swept clearance are unproven. Knife 90° view crops the hands near the bottom. No accepted universal production grip, firing pose, reload, equip or melee animation is claimed. [Readiness and image evidence](../assets/fparms03/readiness.json).

Deferred: production deformation/contact and camera approval, native import/materials/skeleton/sockets, animation transitions, LOD/performance and centralized runtime/network acceptance. No Unreal operation, build, new test framework, native Content/code, central board/log or other contributor checkout changed. No workers created.

Next owner: producer/art lead reviews this bounded source package and scopes the held deformation/contact follow-up before native integration. User retains merge authority. Stop after publication.
