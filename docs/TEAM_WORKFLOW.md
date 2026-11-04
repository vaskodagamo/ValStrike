# Team workflow

Effective 2026-11-04 (Europe/Berlin). Applies equally to Codex, Claude, other assistants and humans. This replaces earlier blanket instructions to compile/test in every contributor session, old ownership tables and automatic coordinator merges. Product requirements and recorded evidence remain valid; historical logs are not rewritten.

## Responsibility hierarchy

```text
User / game director — priorities, creative decisions, final merge
└── Coordinator / producer — task board, dependencies, assignments, batch selection
    ├── Engineering lead — gameplay, networking, UI, tooling contracts and review
    │   └── Coding contributors — one bounded implementation each
    ├── Art lead — stylized direction, asset specifications and visual review
    │   └── Asset contributors — models, materials, animation, map art
    └── Integration / QA owner — combined build, runtime checks, defects, release evidence
```

These are responsibilities, not a requirement for seven running chats. The user assigned Codex to art and Claude to gameplay on 2026-11-04. This Codex coordinator acts as art lead; the user-started Claude session acts as engineering lead and assigns one integration/QA owner. Use only as many contributors as useful independent tasks justify. Add workers only when disjoint, useful tasks exist. QA planning can happen while contributors work; heavy verification waits for a coherent batch. Do not spawn reviewers recursively or keep sessions busy with invented work.

The sole recurring producer is **Direktor (2)**, identified in [PRODUCER.md](PRODUCER.md) and [the worker registry](producer/control.json). The user authorized a check every day/every 30 minutes, bounded follow-up dispatch within the current milestone, and 300 new Meshy credits per Europe/Berlin day. Target three active contributors across providers after the grandfathered batch finishes. The producer owns new task admission and cross-team scheduling; leads retain discipline review and their existing assignments. A merged PR or an idle prepared worktree does not authorize another task.

The user decides scope and merges PRs. Leads resolve routine implementation choices within that scope. Contributors hand off to their lead; the coordinator sends a reviewed batch to integration. Integration returns failures to the existing owner with reproduction/evidence. It does not independently rewrite a contributor's branch. The coordinator can implement a shared integration fix on its own branch with explicit ownership.

## Assign before starting

The coordinator is the sole writer of [TEAM_BOARD.md](TEAM_BOARD.md). The explicitly delegated Claude engineering lead owns its sub-board at `docs/teams/GAMEPLAY_BOARD.md` and prepares isolated engineering assignments within reserved department boundaries. Queue new dispatch through the producer for capacity and shared-path coordination; finish already-assigned work. This delegation does not grant art/native Content or central-document ownership. A task brief based on [TASK_TEMPLATE.md](TASK_TEMPLATE.md) must identify:

- One result and a clear stopping condition; existing backlog ID where applicable.
- One named owner/session, provider, branch, absolute worktree path and base commit.
- Exact owned paths, read-only dependencies and shared-file exceptions. A department name is not a blanket path claim.
- Validation tier, existing checks to reuse, and which checks integration owns. New tests need a concrete uncovered failure described here; routine regression coverage needs no extra user approval.
- Handoff destination and any dependency that prevents integration.

The coordinator reserves paths before dispatching, checking all in-flight tasks and `git worktree list`. Overlapping source edits are serialized or split at an agreed interface. Binary assets, maps and their source files have one owner across all tasks, regardless of separate worktrees. Once a handoff is frozen, new changes need a new commit and invalidate only affected evidence.

Contributor states: `Ready → In progress → Implementation ready → In review`. Integration then records `Verification pending → Verified`. `Merged` is a separate user-controlled Git state. A task can be merged with verification pending; never report that as a tested release. `Blocked` names the missing dependency. Idle contributors stop and report availability.

## Validation proportional to the change

| Tier | Typical change | Contributor work | Central acceptance |
| --- | --- | --- | --- |
| V0 | Docs, labels, formatting, a visual/tuning value with no new rule | Inspect the diff and relevant reference/preview; `git diff --check`. No new test or build. | Review; reuse existing evidence if applicable. |
| V1 | UI behavior, isolated implementation, asset pipeline or new asset | Run the narrow existing engine-free check if useful. For assets, inspect source/export metadata and an assigned native import/reopen/preview. | One combined compile for C++ changes; targeted rendered/asset check where behavior needs it. |
| V2 | Authority, RPC/input validation, damage/economy/round rules, persistence or lifecycle | Reuse focused tests; add only missing cases for concrete regressions. Record runtime scenarios for QA. | Combined build, affected rule tests and actual server/client scenarios. Preserve release gates. |

A one-line damage/round/authority change can be V2; file size is not risk. A cosmetic rename need not become V2 because it is C++. If a check is unavailable, record it as pending and hand off the implementation. Do not create a new runtime harness to manufacture a contributor completion claim. Prefer extending shared test infrastructure when necessary, with one assigned test owner.

The existing GitHub engine-free CI is the shared automated gate. Do not manually repeat its entire matrix in each session. Run locally only the affected fast check that catches a likely defect; use CI for the broad matrix. Existing meaningful tests remain in place. This policy reduces duplicated work, not correctness requirements.

## One integration batch

1. Coordinator chooses related ready commits/PRs and records the exact revisions, owners and affected checks in the board's batch entry. Review diffs before expensive runs.
2. Integration uses its own checkout. For already merged work, fast-forward to the selected `origin/main`. For unmerged work, create a disposable `codex/integration-<batch>` branch from the agreed base and combine selected branches there; never merge remote main on the user's behalf. Record the resulting candidate SHA. Do not copy Binaries, Intermediate or build receipts between trees.
3. Reserve the heavy slot, then build the candidate once with the pinned standard configuration. Do not silently disable unity to declare a normal build passed. Run only relevant existing suites; shared gameplay changes warrant the combined combat/movement/match smoke checks once for that batch, not once per contributor.
4. A failure produces a short defect for its owner: candidate SHA, command, actual/expected result and evidence path. Preserve failing evidence. After a fix, rebuild affected code and rerun failed/impacted checks, not every unrelated suite without cause.
5. Record one acceptance report with commands, source/module identity, platform and limits. Coordinator updates the board/work log and PR descriptions. The user merges; integration checks the resulting main revision when merges or concurrent changes affect the tested candidate.

Do not run a native build for a documentation-only batch such as this workflow change. Separate two-machine LAN, four-player and packaged acceptance still apply when claimed.

## Heavy work and art authoring

Only the assigned integration owner builds/runs multiplayer by default. A contributor compile is an explicit task exception for a concrete compile-dependent investigation, not a routine final step. Art leads may schedule the editor/import/render needed to produce a native asset; this is productive asset authoring, not a full gameplay regression.

On this machine every heavy Unreal command, including asset imports and previews, must run through:

```sh
python3 Scripts/with_ue_slot.py -- <foreground-command-and-arguments>
```

The coordinator reserves a slot for one named job/owner. The wrapper enforces mutual exclusion across this repository's worktrees; it does not enforce roles or queue priorities. Exit 75 means report `waiting for engine slot` and return to independent work or stop. Retry only after a release notification or a newly assigned slot, never in a sleep/retry loop. Close only processes you own. All agents on this Mac use this common clone; separate clones do not share its lock. Other machines need their own slot owner.

## Worktrees and provider handoff

One concurrent writer gets one Git worktree and branch. A worktree isolates files, not overlapping design decisions or Unreal resource use. Existing `.asset-worktrees/` and `.parallel-worktrees/` are retained; never delete/reset/rebase another writer's work. No shared Binaries, Intermediate, Saved or copied test evidence.

The prepared gameplay/art/integration slots are listed in the board. They are idle directories, not running agents. Reserve one for a task before use; if another writer owns it, create another worktree. Start Codex or Claude with that directory as its working directory and paste the task brief. The provider has no ownership privilege. A provider handoff keeps the same branch only after the previous writer stops and the coordinator transfers ownership.

Coordinator recipe for an additional task (replace the example ID/provider; use a unique name):

```sh
git fetch origin
git worktree list
git worktree add -b codex/task-id .parallel-worktrees/task-id origin/main
# Claude work can use claude/task-id with the same worktree layout.
```

Use a base containing this policy. Before its PR merges, the prepared worktrees contain it; other new worktrees can use `codex/team-workflow` as their explicit temporary base. After merge, start from current `origin/main`. In an existing chat, explicitly change to the assigned directory and re-read AGENTS.md; old chat instructions do not grant new tasks. The coordinator must deliver a short policy/task update when reassigning an old chat because files alone cannot update its remembered context.

Commit only task-owned changes. Prepare a concise PR with implementation behavior and pending verification, not a copied transcript. Use the provider-neutral PR template. Push/open PRs when the assignment or user authorizes publication. No force-push or branch/worktree removal as routine cleanup. The user retains merges.

## Token and context discipline

Read the task brief and relevant code first. Search targeted paths; do not load the whole project or every log. Use one concise completion handoff, not repeated near-identical progress logs. Link existing build/test evidence instead of reproducing it. One lead review is enough unless a concrete risk needs another specialty. Batch independent reads and checks; do not add an agent merely to reread the same diff. Do not create tests for documentation, trivial getters, decorative constants or assertions that only restate implementation.

All handoffs include: commit/PR, observable change, checks actually run, checks deferred to the batch, remaining issue and next owner. The coordinator owns central documents; contributors own their task handoff. Logs remain append-only with factual corrections clearly marked. The recurring producer supplies bounded coordination through the configured chat schedule; the existing UE lock supplies machine-level exclusion. Neither proves a worker completed or a game passed. Record only material changes, not an idle log/commit on every wake-up.
