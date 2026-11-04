# ART-10 — Meshy asset intake for Site01

- Status: Implementation ready; folder/workflow setup complete. Production intake awaits user uploads.
- Backlog: PLAY-02; cleanup/integration of user-generated art for the existing map.
- Owner/lead: This Codex coordinator and art lead.
- Checkout: `/Users/webproduktion3/Documents/ValStrike`, branch `codex/meshy-asset-intake`, base `822644299771318655ef0aecbe71d6cf8c80dda3`.
- Exclusive paths: `meshy_ai_assets/` scaffolding/registry; this brief; coordinator-owned additions to `README.md`, `docs/TEAM_BOARD.md`, `docs/WORK_LOG.md`. User owns the original uploaded files; contributors only read them.
- Result/stopping condition: Existing local upload folder with world/character/animation categories, original-preservation rules, asset register and a bounded Site01 handoff procedure, published for user review.
- Read-only dependencies: team workflow, Site01 map/handoff/layout, approved IndustrialV2/OperatorV2 art references and modular reference PR #51.
- Out of scope now: model edits, imports, gameplay wiring, map edits, paid service calls, worker launches or watching an empty folder.
- Validation: V0; inspect scaffold, links, empty register and Git ignore behavior. No new tests or Unreal builds; no engine slot.
- Publication: coordinator may commit/push/open PR; user merges.

## Follow-on production scope

Once the user confirms uploads are complete, the coordinator inventories the actual files and records exact input paths/hashes, provenance and material/rig dependencies. It then assigns small batches in separate worktrees with exclusive source/export paths and named owners. There is no automatic claim or import trigger.

1. Environment artist: first clean one wall/corner, one crate and a tile for visual review; then continue the approved world-object batch. Preserve the original style, fix scale/pivots/seams/materials and collision intent. Native integration belongs to its named owner.
2. Character/animation artist: inspect the uploaded rig, texture set and clip skeleton compatibility. Preserve original rig/animation exports; preview representative idle/walk/run before processing the rest. Do not replace the rig or resume rejected procedural character work automatically. Character runtime/Animation Blueprint wiring needs explicit path ownership coordinated with engineering.
3. Single map/native integration owner: after source review, adapt the approved art into `/Game/Maps/Site01/VS_Site01`. Preserve layout, cover envelopes, collision intent, routes, spawn/site/recovery metadata and gameplay behavior. VS_Graybox and startup-map config remain outside this assignment. Check the latest map owner and integration queue before allocating exact native paths or a heavy slot.

Production contributors use V1 focused source/visual checks and concise handoffs. One central owner reserves `Scripts/with_ue_slot.py` for imports, save/reopen, representative in-map views and affected collision/traversal checks. No per-asset builds, full regressions or new cosmetic test suites. Source ready, imported, runtime verified and merged remain distinct.

## Completion handoff

- Commit/PR: publication on `codex/meshy-asset-intake`; user merges.
- Changed: upload categories, original-preservation/worktree rules, coordinator-owned register, Site01 follow-on scope and root README/board links.
- Checks: four actual folders and tracked placeholders inspected; representative root/nested ZIP/FBX/texture ignore rules and scaffold exemptions verified; local links and empty CSV checked; diff whitespace checked at publication. No test suite or Unreal work.
- Pending: actual asset inventory, source review/cleanup and native integration. No uploads inventoried or production workers dispatched yet.
- Next action: user drops original packages into the canonical inbox and confirms the batch is complete; coordinator assigns real inputs to bounded art tasks.
