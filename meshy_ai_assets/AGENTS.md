# Meshy intake rules

Read the root AGENTS.md, docs/TEAM_WORKFLOW.md, current task assignment and this folder's README before using uploads.

- This folder holds user originals. Never overwrite, rename, delete or extract over them. Read originals only; copy assigned inputs into your own worktree before editing/extracting. Do not execute scripts bundled with a download as part of asset inspection.
- The canonical inbox on this Mac is `/Users/webproduktion3/Documents/ValStrike/meshy_ai_assets`. Ignored uploads are not shared into other Git worktrees or other Macs. Workers read the exact canonical paths supplied in their task, not an empty inbox in their own checkout.
- No self-assignment. The coordinator alone updates ASSET_REGISTER.csv and assigns exact input IDs, source/export/native paths and one writer per asset. No upload automatically starts an agent, import or build.
- Record input hashes and source/provenance at intake. Put edited source/exports under the task-owned Art/Source paths; native Unreal content is editor-owned and needs its own assigned owner/slot. Never force-add the raw inbox; commit reviewed production inputs/source through the existing LFS workflow, adding tracking for any newly adopted binary format before staging.
- Preserve existing Site01 gameplay/layout/collision contracts. Character/skeleton/Animation Blueprint/runtime wiring requires its own explicit assignment and engineering coordination.
- Source inspection and modest authoring previews are allowed when assigned; Unreal builds/imports/runs require the integration owner and reserved engine slot. No repeated per-asset builds/tests or lock polling.
