# Round and objective rules foundation

Updated: 2026-11-03. Independent RULE-01 through RULE-05 domain work; Unreal runtime wiring and multiplayer acceptance remain separate integration steps.

## Assignment and ownership

Work in `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/round-rules` on `codex/round-rules`. Read AGENTS.md, README.md, MVP_PLAN.md, BACKLOG.md and PARALLEL_WORK.md; read SETUP.md for an Unreal build. The round rules in MVP_PLAN.md are the product contract.

Own `Source/ValStrike/Rules/`, `Tests/Rules/`, a uniquely named `Scripts/test_round_rules.py` if needed, and `docs/workstreams/round-rules.md`. Keep any test main function outside Source. Do not modify GameMode, GameState, PlayerState, Character, Combat, HUD, UI, shared contracts, Build.cs, configuration, maps, shared launchers or central documents. Those files either belong to active chats or need a coordinated integration pass.

## Deliverable

Implement a small deterministic C++ rules model that the authoritative Unreal GameMode can own later. It should compile and run focused tests without launching Unreal, and also compile inside the existing Unreal module without new dependencies, exceptions or RTTI. Use a compact API and ordinary data types; avoid a generic event framework or a second game engine. Do not claim a playable or replicated round loop from unit tests alone.

Cover waiting, preparation, active play, planted objective, round resolution and match completion. Support two persistent teams of at most two players, configurable minimum roster of one or two per team, six rounds, role swap after round three, draw and explicit rematch. Defaults: preparation 8 s, round 90 s, plant 3 s, defuse 5 s, detonation 30 s. Preserve scores while waiting for a missing team; clear abandoned matches when both teams leave.

Represent device ownership, drop/retrieve, plant/defuse, interruption and recovery requests. Include elimination, timeout, detonation and defuse wins, late-join spectators, disconnects, round resets and a single award for every resolved round. The world adapter will perform physical drop placement, reachability and actual player/weapon resets; expose explicit outputs for those actions rather than pretending to execute them in the core.

## Integration boundary

- Inputs originate from the authoritative server adapter: monotonic server time, validated participant identity/team and roster events, alive/dead updates, interaction intent, and refreshed world eligibility. Client timestamps, claimed proximity and claimed winners never enter as authoritative facts.
- Use integer time units with documented precision and checked/non-overflowing arithmetic. Define and test monotonic ordering, duplicate/stale requests, invalid identities and invalid state transitions. Starting or continuing an interaction must not refresh its original start time or permit interrupted progress to resume.
- Publish a read-only snapshot with phase, persistent teams/roles, scores, completed rounds, active deadlines, living/eligible participants, device state/carrier and interaction state. Report one-time transition effects with stable identifiers so the future adapter can avoid applying an award/reset twice.
- Document a small proposed adapter API in the stream log. GameMode will drive the model; GameState/PlayerState/objective actor will replicate appropriate snapshots; combat will consult the phase. Do not implement competing versions of those actors in this branch.
- Map contract version 1 is in MAP_BRIEF.md. The core uses site ID `A` and validated eligibility; it does not read layout JSON, world transforms or Unreal assets. The adapter will map persistent teams to the current attacker/defender spawn roles.
- Preparation must expose a combat/movement gate for integration. Spectating late joins must not count as living combatants this round. Explicitly distinguish connected roster, round participants and living players.

Define deterministic handling of simultaneous deaths and competing end events. A plant/defuse succeeds only strictly before its deadline; the deadline wins an exact tie. Resolve once and lock later interactions. After plant, attacker elimination alone cannot award defenders a win. Preserve MVP_PLAN.md's disconnect/empty-server semantics. Document any remaining ambiguity and the chosen reproducible policy alongside tests; do not silently change the product rules.

Time advances must process crossed deadlines in chronological order so a large step behaves consistently with smaller steps. Do not let a late tick retroactively validate an interaction across unobserved eligibility changes. Document the adapter's ordering and continuous-eligibility obligations, including what happens when release, death, disconnect or leaving range coincides with completion.

## Verification and handoff

Test observable behavior: all win paths; exact ties and just-before/after boundaries; interruption and invalid interaction; stale/duplicate commands; carrier death/disconnect and recovery effects; post-plant attacker elimination; deterministic simultaneous events; one award; late join; team limits; underfilled waiting/resume; empty-server reset; role swap; six-round draw/win; rematch; and at least ten consecutive rounds across matches without stale state. Use reproducible tests with explicit time rather than sleeps. Include representative multi-event timelines and invalid configuration handling.

Run the standalone tests and, after the implementation is stable, the pinned Unreal Mac Editor build to verify module compatibility. Use Scripts/ue.py with its mutex, separate checkout outputs and no unnecessary editor launches. Network replication, physical placement, movement/combat gating and end-to-end match tests remain integration work.

Maintain an append-only stream log with commands, results, limitations and integration steps. Commit and push only `codex/round-rules`; do not merge into integration. Hand back the commit, API summary, exact test/build evidence and the runtime work still required. Keep RULE milestones open where acceptance depends on real server/client behavior.
