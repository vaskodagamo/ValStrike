# ART-15 — Urban Entry character pilot handoff

- Status: Ready; new Codex character art chat, High reasoning; coordinator/art lead delegates this bounded visual review and finishing pass.
- Backlog: PLAY-02. Result: an explicitly reviewed character/animation subset that INT-ART-01 can import for the user's first Meshy playtest.
- Branch/worktree: `codex/art15-character-pilot`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art15-character-pilot`.
- Base: main `ee25083298801008291747e710b078bf6ea9563b`, coordinator kickoff, then source `4c2887fe7ab34bdff96efc6686cfe9147fb2481b` and handoff `f9ebe2cab8590fa3d8ac8da86180536efcc9b328` cherry-picked by coordinator. Record the exact prepared HEAD before editing.
- Exclusive writes: `Art/Source/Characters/UrbanEntry01/`, `docs/assets/urban-entry01/`, `docs/workstreams/art15-character-pilot.md`. Old ART-13 source owner is stopped; its checkout remains read-only. Preserve its historical handoff.
- Read-only: approved OperatorV2 references, original Meshy inbox, ART-13 evidence, existing R01/P01 weapon source/export contracts, gameplay movement/camera/hit-zone code. No other worker checkout edits.
- Engine slot: none. No Unreal operations, tests, gameplay or native Content edits. No recursive delegation or new chats.
- Publication: commit, push your branch and open one PR targeting **main**; user merges. Publish the source delivery and this review together, not just the readiness note. Stop with one concise handoff.

## Work and stopping condition

Read TEAM_WORKFLOW, current board, this brief and the existing character handoff first. This is not a request to repeat ART-13's full normalization, export validation or preview generation. Reuse its evidence; inspect actual saved stills and representative motion frames against the approved grounded/stylized character direction. Review visible face/hand/cloth quality, retained silhouette, material response and game-distance deformation. Make bounded corrections only for demonstrated pilot blockers, preserving identity and existing rig compatibility. No primitive replacement, blind decimation or structural rerig.

Prepare a minimum viable presentation set: reviewed character, usable temporary armed idle/hold, and safe in-place locomotion from the supplied clips. You may derive a clearly labeled pilot hold pose/short loop from an existing armed clip and fit it to the actual R01 rifle. Record the hand_r attachment transform and support-hand fit with one source preview; provide a pistol limitation/fallback if it cannot share that pose. Do not invent completed reload, aim-offset or weapon animation coverage. First-person arms and first-person weapon transforms are outside scope.

Preserve the original six exports. If a traveling clip is useful for this pilot, a separate derived in-place export under `Exports/Pilot/` may remove horizontal travel while preserving the coherent pelvis rig, vertical motion and clip semantics. Inspect only changed exports/poses. Do not add a ground root/rename bones or feed animation travel into CharacterMovement. If conversion or pose fitting cannot be made safe within this bounded pass, mark that clip held and provide the usable subset plus the concrete remaining defect. Vault remains excluded; no sprint/vault mechanics. Prefer a visibly usable small subset over new paid animations or an invented full animation system.

Deliver `docs/assets/urban-entry01/pilot-readiness.json` with schema_version, task_id, status (`ready`, `partial`, or `hold`), source_commit (the frozen asset commit preceding the readiness-record commit), exact included FBX/texture paths and hashes, per-clip verdict and root/in-place convention, idle/weapon socket recipe, camera-distance findings, held clips/defects and the INT-ART-01 next action. A `partial` result must state exactly what is safe to integrate. The readiness record and source commit must be ancestors of the final pushed branch. This is delegated **pilot art approval**, not native/runtime or final-production approval.

Use a concise source preview for new work, preserve provenance, check changed source/export integrity and diff/LFS handling. No new suite, full resampling, all-clip rerender or rebuild without a relevant change. Use [valstrike-meshy](../../../Art/Skills/valstrike-meshy/SKILL.md) for future service work; this task has zero paid stages/credits and needs no account checks. Do not regenerate or rerig the user's existing model.

Handoff: commit/PR, approved subset, evidence links, checks actually run, held defects and exact integration instructions in `docs/workstreams/art15-character-pilot.md`. Once published, freeze the branch for INT-ART-01; new edits need a new revision and invalidate only affected evidence.
