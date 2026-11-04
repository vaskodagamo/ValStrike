# ART-17 — R02 character-scale rifle

- Status: Ready for new Codex art chat, High reasoning; user approved 30 Meshy credits for this task on 2026-11-04.
- Backlog: PLAY-02, user-requested new rifle to correct oversized appearance beside the character.
- Lead: Codex coordinator/art lead; follow [Weapon V2 brief](WEAPON_V2_BRIEF.md), TEAM_WORKFLOW and current TEAM_BOARD.
- Branch/worktree: `codex/art17-rifle02`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art17-rifle02`.
- Base: main `b8183db` plus coordinator kickoff; exact prepared HEAD supplied at dispatch and recorded before editing.
- Exclusive writes: `Art/Source/Weapons/Rifle02/`, `docs/assets/rifle02/`, `docs/workstreams/art17-rifle02.md`.
- Read-only: R01/P01/shared palette, approved art references and frozen character reference specified in the shared brief; all other checkouts and source files.
- Result: a new, slimmer compact original R02 rifle generated with Meshy then cleaned into editable source with separate magazine, measured character/hand-fit comparisons, full socket recipe and source-ready main-targeted PR. This is real model authoring, not a concept-only or uniform-scale delivery.
- Out of scope: P02, character pose/rig edits, native Content, existing R01 assets, gameplay/presentation code, first-person arms, additional paid stages/retries beyond the proposed single Meshy task.
- Validation: V1 as in shared brief. No Unreal slot assigned, builds or new test suite. Native import/in-game verification deferred.
- Publication: owner commits/pushes/opens PR directly to main; user merges. Stop after a concise `docs/workstreams/art17-rifle02.md` handoff. No recursive delegation.

Use the compact 72–78 cm initial length target and fit the grip, shoulder/stock and support-hand location to the actual character. Treat the 86 cm R01 as a measurement/comparison reference, not an approved replacement silhouette. Preserve future magazine motion as separate visual geometry; functional internals and firearm mechanics are outside this game-art task.

Required workflow: isolated built-in imagegen reference → official Meshy image-to-3D → Blender cleanup/scale/fitting → source handoff. Prepare references, then generate the Meshy draft; do not silently replace that stage with a procedural model. Approved allocation is exactly 30 Meshy credits for this task: one textured image-to-3D job, no extra paid stage or retry. No further user confirmation is needed within this scope. Follow WEAPON_V2_BRIEF and the shared skill for resumable jobs.
