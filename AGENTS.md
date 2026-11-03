# ValStrike project instructions

## Read before implementation

Read `README.md`, `docs/MVP_PLAN.md`, `docs/BACKLOG.md`, and the latest entry in `docs/WORK_LOG.md`. Read `docs/SETUP.md` when working on builds, and the engine decision when changing architecture.

For parallel work, also read `docs/PARALLEL_WORK.md` and the current `docs/PLAYTEST_WORKSTREAMS.md` assignment, which supersedes first-batch ownership. Use the assigned worktree, respect file ownership, and maintain the stream-specific log; the integration chat owns central documentation and merges.

The user confirmed desktop first and authorized implementation on the Unreal baseline. The initial toolchain is pinned to Unreal Engine 5.8.4 and Xcode 26.1.1 on macOS. Do not silently switch engines or versions, expand the MVP, or describe the game as playable before runtime verification.

## Work in small verified increments

- Select an existing backlog ID, or add a scoped task with a clear completion condition.
- Keep gameplay rules and network validation in reviewable C++. Use Blueprints for asset wiring, presentation, and tuning where useful.
- Use engine character movement and replication. Keep health, ammo, objective state, and round results authoritative on the server.
- Prefer existing engine features over new frameworks. Add dependencies only for a concrete need.
- Treat `.uasset` and `.umap` files as editor-owned binary assets. Do not fabricate them or edit their bytes as text. Avoid concurrent edits to the same asset.
- Use original or appropriately licensed assets and record provenance when adding them.
- Keep test commands, engine/toolchain versions, and platform-specific build instructions reproducible in `docs/SETUP.md` as they become verified.

## Verification and handoff

- Compile and run relevant checks after code changes. Network features need actual server/client testing; a single-player run does not verify replication.
- Test rules that can regress, especially weapon validation and round transitions. Do not add tests that merely mirror trivial implementation details.
- Never invent test results. Distinguish editor, packaged client, dedicated-server, and cross-platform verification.
- At the end of a meaningful work session, update task status and append to `docs/WORK_LOG.md`: changes, evidence, remaining defects or limitations, and the next action.
- Record substantial architecture changes in `docs/decisions/`; preserve the reason for earlier decisions.
- Keep logs append-only apart from explicit factual corrections. Use dates in Europe/Berlin time.
- A task is done only when its stated completion condition is met; partial work remains in progress.
