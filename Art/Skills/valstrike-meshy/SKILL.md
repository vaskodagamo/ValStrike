---
name: valstrike-meshy
description: Generate, resume, rig or animate assigned ValStrike art assets through the official Meshy CLI, then hand off editable source for artist cleanup and centralized Unreal integration. Use for programmatic Meshy work in this project.
---

# ValStrike Meshy workflow

Run from the assigned ValStrike worktree. Read `docs/TEAM_WORKFLOW.md`, the current task brief and relevant approved art reference. The task must identify asset ID, exclusive source/docs paths, desired dimensions, outputs, allowed stages and credit allowance. Existing authorization carries forward. If a paid scope or allowance is missing, prepare inputs and a costed proposal first; resolve only that missing information before submission. This skill does not assign workers or own `Content/`.

## Tool and account

Use the official Meshy CLI, pinned here to **0.8.2** (Node >=22.12). The portable runner avoids changing a machine-wide installation:

```sh
npm exec --yes --package=meshy-cli@0.8.2 -- meshy --version
```

In commands below, `meshy` means that runner, or a verified compatible installed CLI. Use `--format json --no-update-check` for machine-readable commands. Check the command's `--help` for flags; use the [official generation skill](https://github.com/meshy-dev/meshy-3d-agent/tree/bcfc7e9c12663d600b2fcde2e6a89876ca96e4a2/skills/meshy-3d-generation) for detailed recipes. This workflow is provider-neutral: Codex and Claude run the same commands and keep the same job record.

```sh
meshy auth status --format json --no-update-check
meshy balance --format json --no-update-check
```

Reuse verified credentials. When account connection is needed, follow the official [device sign-in instructions](https://github.com/meshy-dev/meshy-3d-agent/blob/bcfc7e9c12663d600b2fcde2e6a89876ca96e4a2/INSTALL.md): `meshy auth login --device --format json --no-update-check`. Show the returned URL/code, leave browser approval to the user, and await that same command. Never request secrets in chat or store them in the repository. Use bounded tool waits so progress remains visible. Verify authentication afterwards; login alone does not complete an asset job.

### First connection and a free agent check

There is no project API key or checked-in authentication config. On each Mac, run:

```sh
npm exec --yes --package=meshy-cli@0.8.2 -- meshy auth login --device --format json --no-update-check
npm exec --yes --package=meshy-cli@0.8.2 -- meshy auth status --format json --no-update-check
npm exec --yes --package=meshy-cli@0.8.2 -- meshy balance --format json --no-update-check
```

Complete the browser approval before the two verification commands. The CLI manages credentials at `~/.config/meshy/credentials.v2.json`; do not read, copy or commit that file. Codex and Claude under the same OS user/configuration share the login across worktrees. Other Macs, containers and OS users sign in separately. `MESHY_CONFIG_DIR` or `MESHY_CREDENTIALS_PATH` overrides change that location; keep them consistent across sessions.

For existing API-key setups, the CLI also reads `MESHY_API_KEY` from its process environment. It overrides the stored browser session. A project `.env` is **not automatically loaded**. Prefer browser login for this interactive workflow; do not ask users to paste keys into chat or add one to this skill.

To test access without generating an asset, ask an agent: “Use valstrike-meshy. Verify authentication and available credits only; do not submit generation or animation tasks.” No asset assignment is needed for this read-only connection check. A paid asset pilot still needs the bounded assignment described above.

## Choose the shortest useful pipeline

| Asset | Pipeline and artist responsibility |
| --- | --- |
| Prop / environment piece | Approved isolated image → image-to-3D → inspect → cleanup/export. Artist checks true dimensions, modular seams, pivot, UVs, shading, material palette and collision intent. |
| Full-body character | Matching A/T-pose references → textured mesh → inspect anatomy → rigging if needed → required motion clips. Artist checks weights, joints, fingers, foot contact and retargeting. Reuse existing rigged inputs. |
| First-person hands / weapon | Mesh draft only from a suitable isolated reference. Artist owns finger/arm rig, weapon alignment, separate moving parts and reload/equip/recoil animation. Meshy's humanoid auto-rigger is not an isolated-hand rigging solution. |
| Animation for an existing character | Reuse its rig and available clips. Check bundled walking/running before paying for another task; query the current animation catalog. Use text-to-motion only for a missing action, then retarget and inspect. |

Keep the original stylized tactical direction: grounded equipment, natural anatomy, readable silhouettes, charcoal/slate and restrained burgundy accents. For Urban Entry preserve the approved cropped jacket and single shoulder plate. Read that asset's reference pack for actual details; do not invent a new art direction. Preserve stylized shading rather than adding photographic grunge.

Multi-image input accepts 1–4 views **of the same object**. Do not submit a contact sheet, a first-person rifle composition or left/right hands as different views of one hand. Start hand generation with a dedicated one-hand reference; preserve intentional asymmetry when the artist constructs the pair. Generated geometry remains a draft until inspected.

## Submit once, resume by ID

1. Compare the exact stages/options with [current API pricing](https://docs.meshy.ai/en/api/pricing) and the assigned allowance. A `make ... --dry-run` estimate is valid only for that matching pipeline, not arbitrary multi-image/rigging/motion stages. Check the authenticated balance; a web subscription does not establish API credit availability. Do not buy credits. Assign separate allowances to concurrent artists rather than giving each the entire team budget.
2. Use a fresh job directory under the assigned worktree's `Artifacts/Meshy/<asset-id>/<job-id>/`. It is a local, ignored staging area, not a cross-Mac handoff. Pass its absolute path as `--workspace` for file-writing commands. Never overwrite another job's originals.
3. Submit each approved stage with `create --async`. Immediately record `result.submission.task_id`, its resource and parent task ID in the task-owned job manifest. Explicitly record model/version/options instead of relying on a moving `latest` alias. Do not add optional remesh, retexture, conversion or retry stages without coverage in the allowance/scope.
4. Resume with the matching `RESOURCE get TASK_ID` or `RESOURCE wait TASK_ID --timeout 60`, using JSON output. A wait timeout is not a failed generation. After an uncertain submission, reconcile existing tasks before another paid create; never resubmit merely because the client timed out. Stop on insufficient balance or an exhausted allowance.
5. On success, list outputs with `meshy download --resource RESOURCE --task-id TASK_ID --list`. Download the needed GLB/FBX, textures and preview using `--output` and `--workspace`; preserve original exports. Retain files promptly before service URLs expire. A motion result may be GLB-only: inspect the returned assets, do not fabricate an FBX path.
6. Record actual `consumed_credits` when present, otherwise `unknown`. Inspect an available preview and the mesh in the assigned art tool before marking source ready. Preview appearance alone does not establish topology, deformation or game readiness.

The compact job manifest belongs in the task-owned `docs/assets/` path, with: asset/task ID; reference paths and SHA-256; prompt; CLI/model/settings; each stage's resource, task ID, parent ID and status; allowance/estimate/actual credits; downloaded filenames/hashes; source/provenance; actual checks, limitations and next owner. Exclude credentials and temporary signed URLs from committed records. Resume this record across sessions instead of regenerating inputs.

## Animation and Unreal handoff

Meshy's rigging expects a suitable textured humanoid with clear limbs. Check the [current rigging constraints](https://docs.meshy.ai/en/api/rigging); upstream examples may lag API limits. glTF forward is +Z; document conversion to the project's centimeter, +X-forward/Z-up export contract. An FBX extension does not establish compatibility with the project's skeleton.

Retrieve action IDs from `meshy animation-catalog list`; never invent them. [Animation tasks](https://docs.meshy.ai/en/api/animation) can apply a preset, a batch of presets (charged per action), or a completed [text-to-motion](https://docs.meshy.ai/en/api/text-to-motion) clip. Confirm root motion versus in-place movement against the gameplay contract. Custom reload timing, finger contact, aim offsets, sockets and first-person camera composition require dedicated artist/gameplay review.

After review, place selected editable source, exports and textures in the assigned `Art/Source/` paths using the project's LFS rules. Preserve downloaded originals and provenance. Write one concise task handoff with credits, files, preview evidence, defects and deferred checks. The coordinator owns the central registry/status. Integration owns native import, retarget/Animation Blueprint wiring and runtime acceptance through `Scripts/with_ue_slot.py`. Do not run a contributor build or generate tests for a visual-only change.

Implementation/source ready, art reviewed, imported and runtime verified are distinct states. This is an offline authoring tool; no Meshy service call belongs in the shipped game.
