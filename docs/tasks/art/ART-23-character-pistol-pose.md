# ART-23 — Urban Entry third-person pistol hold

- Date/backlog: 2026-11-04 (Europe/Berlin), PLAY-02; producer-assigned correction to the observed native pistol attachment/pose limitation.
- Owner: existing Codex “Art — Character playtest prep”, `01a1a6b0-8aa8-7373-b8bb-bc5506f451d4`, High. ART-15 is frozen/idle; do not resume its old checkout.
- Branch/worktree: `codex/art23-character-pistol`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art23-character-pistol`; base merged main `6cc438576d3a0f4896292ab73094ce8c5650781c` plus this brief; exact prepared HEAD in dispatch.
- Exclusive writes: `Art/Source/Characters/UrbanEntry01/PistolPilot/`, `docs/assets/urban-entry01/pistol-pilot/`, `docs/workstreams/art23-character-pistol.md`. The brief/central board/log are producer-owned.
- Read-only: frozen ART-15 source `45deb700a96b2aad3084808b1c7809d4589fdbd7`, final handoff `02f6bf221bcb9b24415c11b7b554faadfacd2ca8`; current P01 source/import contract; corrected P02 source `ccb08f06e22d23573697a1494a34436f901fa006`. First-person hands and native integration have separate owners.
- Result: a visibly usable third-person pistol hold on the existing UrbanEntry rig, with measured P01 and P02 attachment recipes and animation-only exports. This closes a source-coverage gap; native wiring/acceptance is a later producer-controlled integration.
- Validation V1: inspect actual pose, hand contact and changed export readback. Reuse prior character normalization/rig/material evidence. No new test suite, full clip resampling, UE operation/build or paid service call. Budget **0**.
- Publication: commit/push owned source/docs; one PR to main; concise handoff and stop. User merges. No additional worker.

## Observed problem and scope

INT-ART-01's live server/two-client visual run renders the character but retains the old remote pistol offset, so the pistol visibly floats below the empty hands. The rifle hold/socket was explicitly not approved for pistols. Producer inspected the actual evidence image at `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/int-art-01-meshy-playtest/Saved/Tests/Weapons/20261104T144844Z_visual_c2deea/observer_pistol.png` on 2026-11-04. The screenshot is defect evidence, not a frozen asset source. Existing smoke assertions passing do not establish visual acceptance.

Reuse the preserved mesh, skeleton, textures and compatible ART-15 pose/action source. Author one conservative stationary two-hand pistol hold with natural wrists/elbows/shoulders, dominant grip and support-hand contact. Fit the currently integrated P01 for the immediate pilot and the frozen corrected P02 for the later replacement, with separate per-weapon offsets/pose variants only if necessary. Do not scale fingers, weapons or the character to conceal fit defects. If the existing character's finger rig/mesh cannot reach a safe contact without structural repair, deliver the closest honest held pose and precise defect instead of inventing a pass.

Preserve all previous character assets, six original clips and ART-15 pilot exports. Work on a derived editable blend under PistolPilot; no base mesh, rig hierarchy/rest pose, normals, UV/material or weapon geometry changes. No first-person pose, crouch/locomotion/reload/recoil/aim-offset/full animation-system expansion. Existing crouch and directional gait limitations remain separate backlog work. No Meshy regeneration/account check; local animation authoring only.

## Deliverable and acceptance

- Editable derived blend and bounded static hold/short constant loop action, with animation-only FBX onto the unchanged rig, centimeter/+X-forward/Z-up conventions and root motion off. No weapon or duplicate character mesh in animation exports.
- Exact frozen weapon and character hashes/revisions, bone/rest compatibility and per-weapon `hand_r`/weapon-component matrices. Explain how integration derives the local offset after measuring the native source-to-engine basis; no blindly copied Blender quaternion or assumption that Python can author a named Unreal socket.
- Whole-body and close grip/thumb/trigger-guard views with actual P01 and P02, plus a representative game-distance view. Fingers clear slide/muzzle, support hand does not intersect the dominant hand, wrist/shoulder pose is believable. Inspect the actual reimported animation on the preserved character and compare affected pose/contact views only.
- A small `readiness.json` with exact included animation/source paths and hashes, per-weapon pass/held/pending state, preserved-source reference and native instructions. Final handoff states commit/PR, checks actually run, limitations and next owner.

Do not change INT-ART-01 while it verifies its current candidate. Source can advance independently; freeze the handoff for the producer to queue one future native update. Passing this task does not authorize a separate build or automatic merge.
