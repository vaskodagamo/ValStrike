# New Claude chat — Meshy playtest integration

Prepared worktree: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/int-art-01-meshy-playtest`, branch `claude/int-art-01-meshy-playtest`, initial HEAD `7157124c1625d40708eea1cc740cbdee0c3acead`. The user starts this chat; Codex has not launched Claude. The full [INT-ART-01 brief](../tasks/integration/INT-ART-01-meshy-playtest.md) is already committed in that checkout.

Paste into the new Claude session:

```text
You are ValStrike's sole INT-ART-01 Meshy playtest integration owner. Work in /Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/int-art-01-meshy-playtest on claude/int-art-01-meshy-playtest; this checkout is already prepared. Do not use another contributor's checkout or create more workers.

Read AGENTS.md, CLAUDE.md, docs/TEAM_WORKFLOW.md and docs/tasks/integration/INT-ART-01-meshy-playtest.md. That brief grants your exact native asset/map/presentation paths and defines acceptance. Consult the coordinator's current root TEAM_BOARD and the live engineering board in .parallel-worktrees/team-gameplay read-only.

Start engine-free preflight now. Codex ART-15 (codex/art15-character-pilot) is preparing the reviewed character/animation subset; ART-16 (codex/art16-props-pilot) is preparing the four props and placement recipe. Reuse their existing evidence. Combine only their frozen pilot-readiness/source revisions into your integration branch, never mutable uncommitted assets. No new paid generation.

After the source gates and the current engineering batch's recorded engine-slot release (INT-05 at kickoff), import the approved assets, place the props in existing Site01, replace the third-person prototype and wire the approved animations/world-weapon attachment. Preserve server movement, hit zones, map layout/collision, first-person view and gameplay. Follow the full brief; held structures/stairs and vault mechanics remain excluded.

Use Scripts/with_ue_slot.py for every Unreal operation. A busy exit 75 means stop, not poll/retry. Build once if needed and verify the combined result with actual server/two-client rendered evidence. No per-asset test suites or repeated full regressions. Publish a main-targeted PR, concise handoff and exact other-Mac playtest instructions. The user merges. If a source or engine gate is pending, finish independent preflight and report that exact dependency without claiming the game is ready.
```
