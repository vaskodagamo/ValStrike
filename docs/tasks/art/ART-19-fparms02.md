# ART-19 — Meshy first-person hands/gloves replacement

- Status: Ready; user approved 30 Meshy credits for this hand/forearm task on 2026-11-04, separate from the rifle/pistol allocation.
- Backlog: PLAY-02, replacement for user-rejected ART-05. Owner: new Codex hands art chat, High reasoning.
- Branch/worktree: `codex/art19-fparms02`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art19-fparms02`.
- Base: main `b342ca91e894e2be449895dd5b926f5e6b487939` plus coordinator kickoff; exact clean HEAD supplied at dispatch.
- Exclusive writes: `Art/Source/Characters/FPArms02/`, `docs/assets/fparms02/`, `docs/workstreams/art19-fparms02.md`.
- Read-only: canonical UrbanEntry01 character/rig/hand scale, OperatorV2 glove reference, old rejected FPArms01 rig/contract as technical reference only, existing R01/P01 and any frozen new R02/P02/K01 handoffs. Never change another checkout or the third-person character.
- Validation V1: inspect real source/deformation and changed exports. No new suite or Unreal build/import/preview; native integration remains centralized. No recursive delegation.
- Publication: commit, push, PR directly to main; user merges. One concise handoff, then stop.

## Source and style

The old FPArms01 is a rejected procedural prototype, not a Meshy asset or current visual target. The new work must start from **Meshy geometry**, then receive actual anatomical/topology/material/rig cleanup in Blender. Preserve the selected Urban Entry glove style: fitted charcoal leather/textile, curved integrated knuckle padding, flexible finger joint regions, believable palm/thumb webbing, slate sleeves and small burgundy cuff tabs. No block fingers, oversized pads, robotic joints or generic cylinders.

Existing generated design reference: `Art/References/Characters/OperatorV2/Gloves/01_DorsalPalm.png`. A newer matching pair study is preserved locally in the coordinator's ignored `/Users/webproduktion3/Documents/ValStrike/Artifacts/FPArms02Reference/` with its prompt/hash record; read only. That picture shows two different hands, not two views of one hand. Use the built-in imagegen skill to derive **one isolated right gloved hand and forearm** in a relaxed neutral pose with all five separated fingers, complete silhouette and a plain background. No weapon, ruler, text, torso or contact sheet in the Meshy input. Save the exact selected image/prompt/provenance under your owned source path. Inspect the reference before submission; reference authoring is authorized now.

Meshy returns a draft, not an FPS rig. Clean its silhouette, thumb/knuckles/finger anatomy, joints, UVs/materials and sleeve transition. Construct the paired left hand locally with correct handedness, normals, UVs and cuff asymmetry; do not pay for a second hand or claim a mirrored pair has been anatomically validated without inspection. No humanoid Meshy auto-rig for isolated hands. Fit the pair to the frozen 170 cm UrbanEntry01 hand/forearm measurements; never stretch the character or shrink fingers to fit a weapon.

Author an explicit viewmodel rig with independently bending fingers/thumbs, wrists and forearms, appropriate loops/weights and documented centimeter/+X-forward/Z-up export basis. The old rig layout can inform naming, but the old rejected geometry is not the deliverable. Inspect open/relaxed, curled grip/fist and wrist poses; correct actual collapsing/penetrating joints. Provide initial rifle, pistol and knife hold poses using **frozen** weapon geometry if available, otherwise label anatomical/placeholder holds and document the pending fit instead of reading another artist's mutable work. No invented full reload/equip/slash animation claim.

Deliver editable .blend, skeletal FBX, bounded pose/action exports, texture/material inputs, rig/bone table, scale/attachment recipe, real first-person 90°-FOV closeups and visible deformation evidence. Export only the new hands/rig; reference weapons/character do not become duplicated assets. Source previews must be labeled Blender, not gameplay. Preserve both original generated draft and selected cleaned result with provenance/hashes. Report what is ready versus held in `docs/assets/fparms02/readiness.json` and one handoff. Source-ready is not native/in-game acceptance.

## Meshy stages and budget

Apply [valstrike-meshy](../../../Art/Skills/valstrike-meshy/SKILL.md). Approved allocation: **30 credits for exactly one standard image-to-3D task** for the isolated right hand/forearm. Explicit Meshy 7.1, standard geometry, textured/PBR 4K, no separate remesh, retexture, rig, animation or paid retry. The official pricing checked during the weapon kickoff is 30 credits for this configuration; confirm matching options before create. CLI 0.8.2 standard-model pinning uses `--data`; its `--ai-model` flag is for smart-topology. Download the available GLB/FBX/textures; artist cleanup/rigging is local.

**User allowance approved on 2026-11-04: “Approve 60 additional credits.” This task owns exactly 30 of that allowance**, for one textured hand/forearm image-to-3D job. ART-20 owns the separate other 30. These credits are additional to the rifle/pistol allocation; no pooled borrowing, extra stage, paid retry or purchase. Reference preparation, the single Meshy job and local cleanup/rigging may proceed without another confirmation. No repeated login flow; use the shared verified account.

Submit once with async/task/operation ID and preserve the job record. Resume after a timeout; never duplicate the create. Record actual credits and output hashes without credentials/signed URLs. If the draft is unusable, retain it and identify the concrete repair/retry need without spending again. No procedural-only substitution or paid pipeline expansion.
