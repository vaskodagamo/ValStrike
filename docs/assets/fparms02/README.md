# ART-19 FPArms02 delivery

Editable Meshy-derived source and a paired hand rig, for art review. Native import,
runtime presentation and production art acceptance remain pending. Refer to
`readiness.json` for the final included/held state and artifact hashes.

## Provenance and paid scope

The built-in image tool derived one isolated right hand from the frozen glove-pair
study, SHA-256 `a0f38ea2e8b0d2b7ba7c288355fd4d42b2ed98496c04f757e577c93428262a77`.
The exact prompt and selected image are under
`Art/Source/Characters/FPArms02/Reference/`. This was inspected as a single hand,
not submitted as a left/right multi-view pair.

Official Meshy CLI 0.8.2, Node 22.21.1, shared saved login. One image-to-3D task:
`01a1a6d2-0762-71ad-80d6-c8b4049d3e47`; operation
`art19-fparms02-single-draft-20261104`. Explicit Meshy 7.1, standard geometry,
4K texture/PBR, remesh off; GLB/FBX requested. Service reported **30 credits**,
the full ART-19 allowance. No paid retry, remesh, retexture, conversion, rigging,
motion or purchase. [Current pricing checked at submission](https://docs.meshy.ai/en/api/pricing).
`job.json` preserves task/options, original hashes and credit evidence; originals
are retained unchanged. Generated content is project-directed original concept
work; no third-party character/brand reference was added. Meshy account/license
entitlement is not independently audited here.

## Local source work

The selected source retains all **529,894 triangles and 281,289 vertices per hand**
from the draft (1,059,788 triangles for the pair), its UV corner data, supplied
custom normals and original imported PBR graph. Only the documented centimeter
basis/hand-forearm length normalization and necessary skin rig affect it. Supplied
normals are transformed with the inverse transpose of that basis. No weld,
decimation, geometry smoothing, material darkening or synthetic hand replacement
is in the selected source. Coincident UV-split vertices receive the same skin
weights without welding the original surface.

The left mesh reverses winding and corresponding UV/normal corner order, with
proper mirrored normals and an outer-wrist burgundy tab. Original right-hand
asymmetry is mirrored to the corresponding anatomical side, rather than copying
an unreversed right hand. Actual dorsal, palm and deformation views are supplied.
The pair is not claimed as a separately sculpted left-hand anatomy pass.

`Candidates/FPArms02_Reduced.blend` preserves the earlier 46,000-triangle-per-hand
reduction/cloth-relaxation/material-adjustment candidate. It is **held**, not the
selected export. The producer's preservation gate superseded that approach.
`preservation-comparison.json` and the three matching-camera images compare the
normalized original, held reduced candidate and final exported right hand with
the same original PBR graph and lighting. Final topology/UV hashes and transformed
normal/position errors are checked against the original. The preserved mesh is
expensive for a viewmodel; a reviewed performance target and careful retopology/LOD
assignment are still needed, not a nominal triangle target imposed here.

The 39-bone viewmodel skeleton has a root and, per side, forearm, forearm twist,
hand, three bones on each of five digits and a nondeforming weapon attachment
marker. No Meshy humanoid auto-rig. Weights are normalized, at most four influences,
with a corrected thumb/index boundary and local web/joint smoothing. All vertices
are weighted. Bone names, parents, measured head/tail coordinates and source counts
are in `source-inspection.json`; actual export readback is in `export-inspection.json`.

## Scale and attachment contract

Authoring units are centimeters: +X forward/fingers, Z up, right side −Y and left
side +Y. At rest the wrists are `(24, −13, 0)` and `(24, +13, 0)` cm. Hand length
17.8361 cm and forearm length 22.6120 cm derive from the frozen 170 cm UrbanEntry01
right wrist/middle-tip and elbow/wrist measurements at source commit
`45deb700a96b2aad3084808b1c7809d4589fdbd7`. The character is never changed or
stretched. No hand/finger scaling is used to accommodate weapons.

FBX conversion flags are `axis_forward=-Y`, `axis_up=Z`, unit scale applied,
`FBX_SCALE_UNITS`, matching the existing project exporter. Authored +X geometry
is checked through Blender reimport. Animation exports use a temporary neutral
frame 0 for consistent skeleton defaults, exporting only frames 1–30. Native
Unreal handedness/import settings still require explicit verification at scale 1.

Attach the viewmodel root to the first-person presentation transform; do not
attach it to or replace the third-person rig. `weapon_socket_r/l` are markers
parented to `hand_r/l`, not accepted universal weapon sockets. For a frozen weapon,
reconstruct its exact component transform from `weapon-fit.json`, then compute
the attachment offset as inverse posed marker matrix × weapon component matrix.
Verify muzzle direction, grip and trigger contact in the engine before wiring.
R02 is read-only at interim main `627e0745d7328a94fe408e190a6127f6723017d7`. P02 uses the corrected source in merged PR #81 (`b4ecd48`); neither
their transforms nor the UrbanEntry source is edited. K01 still requires a fit against its reviewed frozen handoff; no mutable artist
checkout was read. `frozen-weapon-sources.json` identifies the staged LFS files and hashes.

## Evidence and boundaries

`preview.html` labels all images as **Blender source**, including dorsal/palm,
grip closeups, wrist/forearm twist, and 90° horizontal-FOV first-person studies.
They are actual rendered meshes and deformations, not generated mockups or gameplay.
The fit report records unit-scale component recipes and frozen weapon hashes.
The initial rifle/pistol poses are studies, not full equip/reload/fire animations.
The knife placeholder has no geometry-fit claim. No Unreal import, native material,
Animation Blueprint, runtime camera, multiplayer or packaged acceptance was run.

The source has no new gameplay dependency or test suite. Integration must receive
a separately scoped native assignment from the coordinator; INT-ART-01's current
character/world ownership does not automatically include these hands.

## Final review result — 2026-11-04, ART-19 owner

**Pass for source preservation and serialization; source/rig ready for art review.
Production acceptance held.** The matched original/final export retains the
silhouette, cuff snap, panel seams, knuckle/finger ridges and original material
response. The reduced candidate visibly softens these details and remains held.
The additional uniform-scale original view exposes the changed wrist station,
hand length and width from the assigned character-fit normalization; approval of
those proportions remains with art review.

Actual open, curled grip and wrist/twist poses were inspected on both hands.
The earlier thumb/index strip and pointed thumb-web fold were corrected through
weights and a bounded thumb pose, without altering geometry. The source is still
not production deformation topology: local web/joint compression and high local
edge-stretch outliers remain recorded in `export-inspection.json`. Extreme fist,
wide opposition, reload/equip/slash and broad animation ranges are not approved.
Original UV-split topology remains (31,994 boundary edges per hand in raw index
space); that count is not a claim of physical holes. No watertight or retopology
claim is made.

Corrected P02 PR #81 was rechecked with the same hand scale and contact cameras.
Right index is alongside/below the guard, not a verified trigger pull; reverse
contact shows finger/thenar intersection and imperfect support-hand wrapping.
The static pose is a fit study and is **held for animation/gameplay use**.
R02 fit is **provisional**: ART-17 is correcting its earlier source, per producer
handoff. Recheck only changed grip/contact geometry against that future frozen
revision. Do not carry this interim R02 fit forward as acceptance. K01 has no
frozen dependency in this delivery, so its action remains an anatomical placeholder.

The chosen GLB is the visual master. It embeds JPEG base color, packed
metallic/roughness and normal inputs; these exact byte buffers are extracted to
`Textures/T_FPArms02_GLTF_*.jpg` and also remain packed in the blend. Original
separately downloaded PNGs are preserved alongside them; base-color/normal PNGs
are 4096², metallic/roughness PNGs 2048². See `material-inputs.json` for channel
wiring and hashes. The producer must assign a reviewed optimization/retopology
pass before treating this 1.06-million-triangle pair as a shipped viewmodel.
