# ART-21 — Preserve Meshy source quality during cleanup

- Date: 2026-11-04 (Europe/Berlin). Backlog: PLAY-02.
- Assignment: user requested a fix after artists degraded good textured Meshy outputs. Direktor (2) delegated this bounded correction to the outgoing coordinator; producer ownership remains with Direktor (2).
- Owner: Codex Direktor, this ART-21 assignment only.
- Branch/worktree: `codex/art21-meshy-preservation`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art21-meshy-preservation`.
- Base: `627e074` (`origin/main` when the worktree was created).
- Exclusive paths: `Art/Skills/valstrike-meshy/SKILL.md`, `Art/Skills/valstrike-meshy/references/source-preservation.md`, and this task/handoff.
- Read-only dependencies: team workflow/board, existing Meshy skill/account setup, ART-17/18/19/20 source/scripts and artist feedback. No writes to central documents or artist worktrees; no asset mutation, service spending or Unreal work.
- Result: shared Codex/Claude instructions preserve the original geometry, UVs, custom normals and PBR response by default, and require a matched visual comparison for necessary edits and exports. Keep authentication, costs and resumable job handling intact.
- Validation: V0; existing skill validator, linked-file inspection and `git diff --check`. No new tests, builds or generation.
- Publication: main-targeted PR; user merges. Producer steers current artists and owns consistent rollout of the reviewed skill to both local providers.

## Evidence and correction

ART-18 reported reduction from 454,988 to about 27,000 triangles, broad smoothing and lost custom normals; its owner is rebuilding from the preserved original. ART-17 reported softened receiver/rail edges and a grip notch. This assignment inspected ART-17's matched `01_untouched_meshy.png` and `02_rejected_delivery.png` under its `Artifacts/Meshy/R02/revision/` directory and confirmed the visible damage. Its artist owns the repair.

Inspection of ART-20's in-progress `prepare_knife.py` also found global material overrides and geometry replacement; these are a reason to require an original comparison, not a claim that its next revision fails. The producer received these findings and owns follow-up for all four active artists. No existing asset handoff is promoted to accepted by this documentation change.

The shared skill now activates for local Meshy cleanup as well as service work. The linked guide distinguishes baseline preservation, assigned necessary edits, separately justified optimization and matched original/source/export review. Numerical checks cannot substitute for the visual comparison. Existing paid-job and native-integration boundaries remain unchanged.

## Handoff

- State: implementation ready; publication pending. The first task commit contains the complete correction; its exact revision will be recorded in the publication handoff.
- Checks run: existing `quick_validate.py` passed using `PYTHONPATH=/private/tmp/valstrike-meshy-validation-deps` (system Python lacks PyYAML); linked local guide exists; instruction/diff review completed. No dependencies were installed, tests added, assets changed or engine work performed.
- Checks deferred: individual artists' corrected renders/exports, art acceptance and native runtime verification. No asset quality claim is made here.
- Next owner: Direktor (2) reviews the published revision, updates both installed skill copies consistently, records central status and follows current artist corrections. Active artist chats must receive the rule explicitly; a file update alone does not refresh their context.
