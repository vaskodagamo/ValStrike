# INT-ART-01 — cleared dependencies and resume handoff

2026-11-04, Europe/Berlin. Coordinator update for the **existing Claude native-integration owner**, not a new competing session. Keep the same worktree, preserve your committed preflight/importer and follow [INT-ART-01](INT-ART-01-meshy-playtest.md). The coordinator has not modified your checkout or run your Unreal job.

The blockers in your last handoff have cleared on inspected main `b342ca91e894e2be449895dd5b926f5e6b487939`:

- PR #65 merged ART-15's final frozen handoff `02f6bf2` and readiness, including the temporary R01 scale limitation. Approved character subset: character/three textures, pilot static R01 hold and forward in-place rifle walk, optional original unarmed Walking fallback. Use the exact final readiness allowlist/hashes and asset ancestry, not earlier draft gates. Other clips/poses remain held.
- PR #63 merged ART-16's final handoff `587e6e7`, readiness `757a56f` and four approved prop sources. W03/W04/W07/W10 only; stairs/structures remain excluded.
- PR #64 merged INT-05's completed acceptance; live engineering board explicitly says **done, slot released**. GPT-12/13/14 currently have source-only assignments. Recheck the live board before starting in case a newer reservation supersedes this snapshot, then use the common UE wrapper; busy 75 still means stop rather than retry.

Resume by inspecting your clean branch, fetching main and combining the merged sources there without resetting/replacing your preflight work. Record exact candidate and hashes. No missing-input stop remains based solely on the old handoff. Keep the existing single-owner native scope, import/reopen, place/wire and verify the actual server/two-client Site01 pilot; no new art worker/build pool.

Your Y-handedness concern is valid as a native validation item. Measure the actual imported bounds before placement. **Coordinator explicitly authorizes a native transform adjustment in your owned import/placement scripts and map**, derived from those measured bounds, to keep the exact intended world-space tile top/envelope and panel wall backing. Keep source assets/ART-16 recipe untouched, log the source-to-native conversion and before/after bounds, preserve route/collision/cover contracts. If the geometry cannot match safely, hold that specific placement and return the defect, but continue the other approved props/character. Do not stall all imports on an assumed axis sign before measuring it.

New R02/P02 and FPArms02/K01 are separate source-art tasks. Do not wait for them or expand your native paths automatically. R01 stays explicitly temporary for this pilot. No first-person hands or knife gameplay is claimed by this integration. Final handoff must include actual in-game evidence and exact tested other-Mac checkout/launch instructions; user merges PRs.

Final checkpoint update: inspected main advanced to `e29963e` with #68–70; the native asset inventories in main and your checkout still contain no new Meshy/UrbanEntry assets. GPT-15 now owns the movement/jump/crouch portions of `VSCharacter.h/.cpp`; preserve those exact areas. Your existing presentation-only initialization allowance is unchanged. If a required edit overlaps its movement/crouch work, serialize that hunk with the gameplay lead instead of independently rewriting it. This does not prevent the already scoped asset import/map work.
