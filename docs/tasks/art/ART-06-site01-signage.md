# ART-06 — Site01 navigation signs, source and placement delivery

- Owner: new Codex session “Art — Site01 navigation signs”; coordinator/art lead is the parent chat.
- Checkout: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art-site-signage`; branch `codex/art-site-signage`.
- Code/art baseline: main `66428e5ed0fb3dc34ed9f93c8c5fb61a6f6c3a08`, plus the coordinator kickoff commit supplied at dispatch. Record actual starting HEAD in the handoff.
- Validation: V1 source-asset work. No Unreal build, editor import, multiplayer run or new test suite in this assignment.
- Owns only `Art/Source/Environment/Signage01/`, `docs/assets/signage01/`, `docs/workstreams/art-site-signage.md` in this checkout. Existing map, map generator, Content packages, gameplay, map review and other art are read-only.

## Result

Address the two concrete findings R1/R2 in `docs/reviews/site01/REVIEW.md`: an A + left-turn cue on the service flank and an approach-visible A marker on arrival from Transformer. Produce the original source/export sign assets and a precise additive placement recipe for central native import. Deliver useful asset work, not another general map review.

Keep Site01's broad matte stylized forms and restrained palette. Use geometric original glyphs, strong value contrast and shape/arrow direction rather than color alone. Make the sign legible at the documented player-eye viewpoints; do not change lighting, routes, cover, spawn/objective tags, collisions or the site pad. Two small signs are the scope; no full signage pack or environment redesign.

Use editable Blender geometry or a simple existing project-compatible source method and FBX exports. Keep centimeter scale, clear forward-facing axis, pivots and a small material set. Visual-only signs will have NoCollision. Avoid external fonts/licensing work; a simple original geometric A and arrow are sufficient. Read the existing Site01 source/layout and review evidence to determine appropriate dimensions and transform proposals. Do not modify or regenerate the production map.

Provide a small idempotent import/placement recipe or script under your owned source folder, designed to add only the two named visual actors/materials when integration runs it. Guard the intended map identity; never clear/rebuild the world. Record actor names, target map, transforms, expected dependencies and the native asset destination proposed under `/Game/Art/Environment/Signage01/`. Script execution/native asset creation remains pending; do not claim a source-only inspection proves actual native placement or collision.

## Finish and stop

Deliver editable source, FBX exports, palette/provenance note, placement instructions and at most two useful source previews. Reuse original map evidence for context; label any composition/mockup as a proposal, never as a screenshot of the modified game. Reopen source/export once; check dimensions, face direction, glyph shape and metadata. Inspect actual source images. Use modest render settings and at most four Blender threads.

No new unit-test suite, screenshot matrix or runtime harness. Use `git diff --check`, source/export inspection and syntax checking for any new import recipe. Native import/reopen and two actual in-map views belong to the central art integration batch after source review.

Commit only owned paths locally, respecting Git LFS. Write one short handoff at `docs/workstreams/art-site-signage.md`: commit, files, source checks, the two proposed placements and pending native validation. Publication and PR creation are coordinator-owned for this batch. Stop after this source-ready deliverable; no additional agents or chats and no engine-slot polling.
