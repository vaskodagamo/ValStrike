# ART-26 source review — held checkpoint

Reviewed 2026-11-04 (Europe/Berlin), Blender 5.2.2 LTS. Source-only V1; no Unreal operation, build, gameplay/network test, Meshy/account call or credit spend.

## Passed checks

The derived source retains both original meshes (780,956 triangles total), topology/positions, UVs, custom normals and weights by exact signature comparison with frozen ART-24. All 39 bone names, parents and rest matrices are unchanged. The authoring scripts retain the original packed PBR graph and factors; FBX review explicitly rebinds that graph.

Five paired-hand actions use 60 fps and identity root throughout. Equip is 0.40 s; slash impact/total is 0.15/0.55 s; stab impact/total is 0.30/0.90 s. Idle/locomotion loop endpoints and equip/attack-to-idle endpoints match exactly. Actual FBX reimport sampled bone matrices differ by at most 0.0000201; skeletal mesh bounds differ by at most 0.00000573 cm. No unweighted vertices or extra reference meshes were exported. See [export-inspection.json](export-inspection.json) and [animation-contract.json](animation-contract.json).

All 13 source/export image pairs were visually compared: idle front/reverse/above/90-degree FOV; equip middle/end; locomotion; slash windup/impact/recovery; stab windup/impact/recovery. The actual reimports match the source poses and original PBR appearance. Slash is a lateral sweep, stab a forward extension; sampled impact/recovery views keep the off hand separate and the forearm ends outside the camera view. The idle/recovery lower-edge hand crop is removed in the documented camera. Equip deliberately rises from below the view.

Sparse nearest-surface/ray-parity checks find no sampled grip points deeper than 0.05 cm inside K01. Sampled off-hand clearance is at least 13.1369 cm. This does not prove continuous collision clearance or a firm grip. See [contact-inspection.json](contact-inspection.json).

## Held — do not mark source appearance accepted

1. **Firm grip:** [source top](Previews/source_idle_01_above.png) and [actual FBX top](Previews/export_idle_01_above.png) expose a palm-to-handle gap and incompletely wrapped fingers. The thumb/web no longer has the earlier folded presentation, but the current pose still fails the brief's natural firm-grip requirement. A sparse no-penetration result is not a substitute for believable contact.
2. **Windup framing:** [slash windup](Previews/export_slash_05_90fov.png) crops the outer grip at the right edge; [stab windup](Previews/export_stab_10_90fov.png) crops the lower hand. Impact and recovery are readable, but full attack framing is not accepted.
3. **Dense deformation:** idle edge stretch p99/max is 1.6001/35.3565 on the right and 1.4921/21.7175 on the left (rest edges over 0.015 cm). These inherited dense-surface outliers remain held. No geometry smoothing, finger scaling, weight change or structural rig change was used to conceal them.

Next source action is a bounded contact/pose correction using the existing rig and matched top/reverse/FOV evidence, with windup offsets adjusted together. This checkpoint does not establish that structural mesh/rig edits are necessary; if a safe correction requires them, escalate the exact change to the art lead before expanding scope. The producer can freeze this delivery and assign that correction explicitly. Do not admit this draft as final art.

## Pending integration

Native skeleton/material import, attachment basis and camera offsets, animation notifies/cancellation, LMB/RMB authority and timing, locomotion blending, remote visibility/audio and actual server/client acceptance are unverified. The 780,956-triangle pair still needs runtime budget/LOD review. No playable or full-feature completion claim is made. The gameplay/native owners own those checks; the user merges.

## 2026-11-04 — ART-26-C1 superseding source review

The returned contact/framing defects are corrected in the current derived source. See [C1 review](c1-review.md), [current readiness](readiness.json) and [full-action sheet](Previews/c1_full_actions.png). The earlier observations above remain the record for `2b944b3`; use the C1 evidence for the revised source. Art-lead/native acceptance remains separate.
