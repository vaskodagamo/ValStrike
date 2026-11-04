# ART-18 — P02 character-scale pistol

- Status: Ready for new Codex art chat, High reasoning; user approved 30 Meshy credits for this task on 2026-11-04.
- Backlog: PLAY-02, user-requested new pistol paired with the replacement rifle.
- Lead: Codex coordinator/art lead; follow [Weapon V2 brief](WEAPON_V2_BRIEF.md), TEAM_WORKFLOW and current TEAM_BOARD.
- Branch/worktree: `codex/art18-pistol02`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art18-pistol02`.
- Base: main `b8183db` plus coordinator kickoff; exact prepared HEAD supplied at dispatch and recorded before editing.
- Exclusive writes: `Art/Source/Weapons/Pistol02/`, `docs/assets/pistol02/`, `docs/workstreams/art18-pistol02.md`.
- Read-only: R01/P01/shared palette, approved art references and frozen character reference specified in the shared brief; other checkouts and source files.
- Result: a new compact original P02 pistol generated with Meshy then cleaned into editable source with separate slide/magazine, measured character/hand-fit comparisons, full socket recipe and source-ready main-targeted PR. This is real model authoring, not a concept-only or uniform-scale delivery.
- Out of scope: R02, character pose/rig edits, native Content, existing P01 assets, gameplay/presentation code, first-person arms, additional paid stages/retries beyond the proposed single Meshy task.
- Validation: V1 as in shared brief. No Unreal slot assigned, builds or new test suite. Native import/in-game verification deferred.
- Publication: owner commits/pushes/opens PR directly to main; user merges. Stop after a concise `docs/workstreams/art18-pistol02.md` handoff. No recursive delegation.

Start around 20–22 cm length with a slimmer grip/slide profile; accommodate the actual palm/fingers and a sensible two-hand hold. Use the existing 23.929 cm P01 only as an original-design/measurement reference. The rifle chat owns its own geometry, so coordinate the family through this shared brief and read-only palette, not a shared writer path. Separate parts enable later visual animation; no functional firearm mechanism is requested.

Required workflow: isolated built-in imagegen reference → official Meshy image-to-3D → Blender cleanup/scale/fitting → source handoff. Prepare references, then generate the Meshy draft; do not silently replace that stage with a procedural model. Approved allocation is exactly 30 Meshy credits for this task: one textured image-to-3D job, no extra paid stage or retry. No further user confirmation is needed within this scope. Follow WEAPON_V2_BRIEF and the shared skill for resumable jobs.
