# ART-14 — Reusable Meshy artist workflow

- Status: Implementation ready; authenticated asset generation unverified.
- Backlog: PLAY-02; user-requested programmatic asset generation and animation workflow.
- Owner: This Codex session, coordinator/art tooling; no additional workers.
- Branch/base: `codex/meshy-agent-workflow`, `5dcf6db2bdc5361dd401bfb50e61058b2b9d6cb3` (`origin/main` at publication). Prepared initially from `b6e4a5f`; only this task's changes are carried onto current main.
- Worktree: `/Users/webproduktion3/.codex/worktrees/10a1/ValStrike`.
- Exclusive paths: this brief and `Art/Skills/valstrike-meshy/`; local installation of that skill for Codex and Claude. No central board, registry, current asset assignment or source-asset edits.
- Read-only dependencies: team workflow/board, Meshy intake instructions, approved art references and official Meshy documentation/tooling. ART-11/12/13 continue in their existing worktrees.
- Result/stopping condition: a portable, locally available skill using the official Meshy CLI, with resumable jobs, bounded costs, artist review and central Unreal integration.
- Out of scope: paid generation, account purchases, asset replacement, a custom API SDK, native import, gameplay changes and builds.
- Validation: V0 skill inspection/validator plus a free CLI planning check; no new tests or Unreal slot. Live account/generation checks deferred until a concrete asset job is assigned.
- Publication: user authorized commit and push on 2026-11-04; coordinator opens the review PR, user retains merges.

## Completion handoff

- Commit/PR: published through `codex/meshy-agent-workflow`; exact commit and PR are recorded in the coordinator's publication response. Merge remains user-controlled.
- Changed: [shared skill](../../../Art/Skills/valstrike-meshy/SKILL.md) (repository path: `Art/Skills/valstrike-meshy/SKILL.md`) installed identically at `~/.codex/skills/valstrike-meshy/` and `~/.claude/skills/valstrike-meshy/`. Invoke as `$valstrike-meshy` in Codex or `/valstrike-meshy` in Claude Code; another machine can copy the repository folder into its provider's skill directory. The pinned CLI is cached through `npm exec`, not installed globally. No other worker was started or reassigned.
- Checks actually run: Node `22.21.1`, Meshy CLI `0.8.2`; official skill validator passed. CLI `make ... --dry-run` returned `ok: true`, `dry_run: true`, `requests_made: 0` and a 30-credit estimate for that specific text preview/refine plan. This was not an asset submission or a quote for all jobs. Authentication status reported `authenticated: false`, no profiles. No credits spent, tests added or Unreal work run.
- Deferred: authenticated API generation/download, artist review, and any later native import/runtime acceptance.
- Next owner: coordinator selects one new asset and a credit allowance; artist uses the skill in the assigned worktree. Do not regenerate already assigned uploads.
