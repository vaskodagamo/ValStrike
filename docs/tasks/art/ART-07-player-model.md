# ART-07 — Third-person player model, source delivery

- Owner: new Codex art chat; reasoning **High**, retain the configured model. No extra agents.
- Branch/worktree: `codex/art-player-model`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art-player-model`.
- Base: main `90849f5` plus coordinator assignment commit supplied at dispatch; record starting HEAD.
- Owns only `Art/Source/Characters/Player01/`, `docs/assets/player01/`, `docs/workstreams/art-player-model.md`.
- V1 source-only assignment. No Unreal builds/imports/runtime checks or new unit-test suites. Claude INT-01 owns the engine slot.

Produce one original stylized third-person tactical player model with editable Blender source, a modest deform skeleton and FBX export. This fills the visible-player art gap; no full cosmetic roster or animation library. Use clean readable forms, separated head/torso/limbs, simplified tactical clothing, gloves and boots, restrained charcoal/slate/steel colors with a small crimson accent. Avoid photoreal scans, copied game characters, brand logos, dense grime and uniformly black silhouettes. Aim around 15k triangles or less; record actual counts and material slots.

Read current VSCharacter capsule/mesh settings and Damage/VSHitZoneLayout.* as references for scale/proportions and expected bone names. Preserve gameplay collision and rules: do not edit source code or promise the visual rig verifies hit-zone alignment. Include practical root/pelvis/spine/head/arm/leg bones, skin weights and a clear bone-name mapping. Use documented centimeter scale, +X forward and Z up export conventions. Make one neutral/reference pose and one simple weapon-ready inspection pose using accepted weapon geometry read-only. No animation clips, facial rig, IK framework, game wiring or Content packages.

ART-05 owns separate first-person arms/gloves under FPArms01; do not duplicate its high-detail first-person work, modify its files or depend on its unfinished output. Your full-body hands are only third-person detail. Match the shared stylized palette without claiming identical rigs. All other source/art/maps and central docs are read-only.

Deliver a saved .blend, reproducible export instructions/script, FBX, concise palette/provenance/rig/scale notes and at most three useful Blender source previews (front/three-quarter, rear, weapon-ready). Reopen source/export once, inspect gross joint deformation and actual images. Label source renders clearly; native import/retargeting/collision/readability remain pending central integration. Use modest renders and at most two Blender render threads while the other art sessions work. No new test framework or broad asset audit.

Commit only owned files locally with Git LFS and git diff --check. One short handoff at docs/workstreams/art-player-model.md: commit, files, measurements, previews, source checks and native-integration requirements. Coordinator handles publication/review. Stop at the source-ready deliverable; do not push, merge, start more chats, poll the engine slot or invent a follow-on task.
