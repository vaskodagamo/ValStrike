# ValStrike project instructions

## Read before implementation

Start with `docs/TEAM_WORKFLOW.md`, your assignment in `docs/TEAM_BOARD.md`, and your task brief. These are the current operating rules for Codex, Claude, and human contributors. `docs/PARALLEL_WORK.md` and `docs/PLAYTEST_WORKSTREAMS.md` are historical assignments, not permission to resume old work or build independently.

On a new task, read the README and relevant MVP/backlog sections. Read only the relevant workstream handoff; do not reload every log on each turn. Integration reads the latest central work-log entry. Consult setup for assigned build work and decisions for architecture changes.

The user confirmed desktop first and authorized implementation on the Unreal baseline. The initial toolchain is pinned to Unreal Engine 5.8.4 and Xcode 26.1.1 on macOS. Do not silently switch engines or versions, expand the MVP, or describe the game as playable before runtime verification.

## Ownership and scope

- The user sets priorities and merges PRs. The coordinator assigns bounded tasks and exclusive paths; engineering and art leads review their own disciplines. One integration/QA owner runs combined builds and runtime acceptance. Roles may share one session; do not create management agents just to fill the hierarchy.
- One writer per branch/worktree, one task at a time. Codex and Claude follow the same rules. Never edit another contributor's checkout or concurrently edit the same binary asset.
- No active assignment means no implementation. Report availability; do not invent another feature, review, test suite, or benchmark to stay busy.
- Shared files/contracts need an explicit owner in the task brief. Hand off cross-owner changes rather than implementing overlapping versions.

## Work in small increments

- Select an existing backlog ID, or add a scoped task with a clear completion condition.
- Keep gameplay rules and network validation in reviewable C++. Use Blueprints for asset wiring, presentation, and tuning where useful.
- Use engine character movement and replication. Keep health, ammo, objective state, and round results authoritative on the server.
- Prefer existing engine features over new frameworks. Add dependencies only for a concrete need.
- Treat `.uasset` and `.umap` files as editor-owned binary assets. Do not fabricate them or edit their bytes as text. Avoid concurrent edits to the same asset.
- Use original or appropriately licensed assets and record provenance when adding them.
- Keep test commands, engine/toolchain versions, and platform-specific build instructions reproducible in `docs/SETUP.md` as they become verified.

## Verification and handoff

- Contributors do **not** run Unreal builds, packaging, full regressions, or multiplayer runners by default. Submit implementation with `verification pending` to integration. Asset authoring/import/preview is allowed only when assigned; it uses the same reserved engine slot and does not imply a C++ build.
- Use the task's validation tier. Small documentation, visual, or constant-only edits need diff/inspection checks, not new tests. Behavior changes reuse focused existing checks. Add a test only for a meaningful uncovered regression, authority boundary, lifecycle, or deterministic rule; no duplicate per-feature harnesses.
- Integration builds the combined candidate once and runs the affected suites once. Rerun only for relevant source changes, failures, or missing evidence. Network acceptance still requires actual server/client testing, centrally; single-player or unit results do not prove replication.
- All heavy Unreal work uses `Scripts/with_ue_slot.py` in a scheduled slot. A busy exit 75 means return to the queue; do not loop, poll, bypass the lock, or repeatedly ask to build.
- Never invent test results. Distinguish editor, packaged client, dedicated-server, and cross-platform verification.
- Contributors write one concise handoff in their assigned task/workstream file: commit, changes, checks actually run, checks deferred, risks and next action. The coordinator alone updates central status and `docs/WORK_LOG.md`. Prefer links to existing evidence over copying logs.
- Record substantial architecture changes in `docs/decisions/`; preserve the reason for earlier decisions.
- Keep logs append-only apart from explicit factual corrections. Use dates in Europe/Berlin time.
- `Implementation ready`, `Verified`, and `Merged` are separate states. A merged PR can still need runtime verification. User authorization to merge is not delegated to agents by these rules.
