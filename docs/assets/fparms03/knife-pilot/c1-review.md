# ART-26-C1 — grip and framing correction

Reviewed 2026-11-04 (Europe/Berlin). Same branch/worktree and draft [PR #109](https://github.com/vaskodagamo/ValStrike/pull/109). Source correction prepared for art-lead review; native/full-feature verification remains pending. The unprefixed reports/images describe the earlier held `2b944b3` checkpoint, not this correction.

## Returned defects and correction

The hand previously held K01 beyond the knuckles with insufficient finger closure. The derived wrist-to-knife relationship now seats the handle against the distal palm/pads; each finger uses coordinated base/middle/tip closure. The thumb opposes the handle behind the guard. Compare [actual top](Previews/c1-export_idle_01_above.png), [actual reverse](Previews/c1-export_idle_01_reverse.png) and [actual front](Previews/c1-export_idle_01_front.png) with the original held views. The source and actual FBX top/reverse/90-degree images match. The former broad floating gap and open wrap are corrected; the remaining central palm cavity follows the cupped hand rather than standing in for absent pad contact.

A thumb/web spike came from residual hand influence on the inner thumb and a narrow thumb/index classification boundary. The permitted local weight repair changes **25,613 right-hand vertices** in that dense region. It blends hand influence into the existing thumb chain continuously and preserves the tiny index-boundary influences; at most four normalized influences remain. The left weights and every right vertex outside the recorded delta are unchanged. [Summary](c1-weight-summary.json) gives region/group counts; [exact before/after weights](c1-weight-changes.json) identifies every affected vertex. No geometry, UV, custom-normal, material, bone-name, rest-matrix or hierarchy change; no smoothing, finger/weapon scaling or weapon redesign. No structural rig change is needed for this correction.

Slash windup moved inward; stab windup no longer pulls the grip below the frame. The hold is raised 2 cm and slash follow-through lifted to keep intermediate locomotion/recovery frames visible. Timings and impact frames remain unchanged. The [full-action sheet](Previews/c1_full_actions.png) covers all five actions, including intermediate poses; intentional equip entry is below frame. Slash remains a lateral sweep and stab a forward extension, with off-hand separation and readable recovery. At extreme slash follow-through the blade is foreshortened/partly hand-occluded, not viewport-clipped.

## Focused evidence actually inspected

- Saved derived source and changed actual skeletal/animation FBX imports; 3 current source contact/FOV images, 12 actual export contact/extreme/recovery images and the 30-panel action sheet were inspected.
- Original surface/UV/custom-normal signatures and all 39 rest bones/hierarchy pass. Every weight difference matches the explicit allowed delta. Current sampled actual-FBX bone matrix error is at most 0.00002003; no extra reference meshes or unweighted vertices. [C1 export inspection](c1-export-inspection.json).
- Prior all-frame identity-root/timing evidence is reused; unchanged clip ranges/markers and return/loop endpoints are checked in the current focused export pass. Equip 0.40 s; slash impact/total 0.15/0.55 s; stab 0.30/0.90 s; 60 fps. [Current attachment/animation contract](animation-contract.json).
- All **199 actual exported hold/attack frames** keep every dominant right hand/finger vertex and K01 vertex inside the fixed 90-degree viewport. Lowest lower-edge margin is **6.51%** (about 47 pixels at 720p). Forearms and intentional equip entry are excluded from that numeric bound; the visible arm ends were checked in the sheet/extreme images. [Framing evidence](c1-framing.json).
- Sparse contact observations show sampled off-hand clearance at least 12.758 cm. **5 sampled fingertip points** remain deeper than 0.05 cm, maximum 0.0981 cm (under 1 mm); thumb samples have none over that threshold. These small residual pad contacts are recorded, not described as a zero-penetration pass. [Contact observations](c1-contact-inspection.json).
- Five scripts syntax checked; Git diff/scope and LFS inspection. Blender 5.2.2 LTS, one process at a time, two threads. No new test framework or general regression suite.

## Remaining gates

Art lead reviews natural grip and animation quality using the current images before source admission. Dense skinning outliers remain: right idle edge stretch p99/max 1.8234/24.3546, left 1.4921/21.7175. This is not a general deformation/optimization approval; the unchanged 780,956-triangle pair still needs runtime budget review.

Native skeleton/material import, exact attachment/camera conversion, notifies/cancellation, gameplay authority, remote presentation/audio and combined server/client acceptance belong to their owners. No Unreal/build/gameplay test was run. Zero Meshy/paid-service/account calls and zero credits. No playable/full-feature completion claim; user merges.
