# ValStrike project instructions

## Read before implementation

Read `README.md`, `docs/MVP_PLAN.md`, `docs/BACKLOG.md`, and the latest entry in `docs/WORK_LOG.md`. Read `docs/SETUP.md` when working on builds, and the engine decision when changing architecture.

The user has confirmed desktop first. Unreal Engine 5 with C++ gameplay is the recommended planning baseline; the exact version and toolchain are not pinned yet. Do not silently switch engines, expand the MVP, or describe the game as playable before runtime verification.

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
