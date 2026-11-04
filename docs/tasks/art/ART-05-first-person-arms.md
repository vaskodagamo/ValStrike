# ART-05 — First-person arms and gloves, source delivery

- Owner: new Codex session “Art — First-person arms and gloves”; coordinator/art lead is the parent chat.
- Checkout: `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/team-art`; branch `codex/team-art`.
- Code/art baseline: main `66428e5ed0fb3dc34ed9f93c8c5fb61a6f6c3a08`, plus the coordinator kickoff commit supplied at dispatch. Record actual starting HEAD in the handoff.
- Validation: V1 source-asset work. No Unreal build, editor import, multiplayer run or new test suite in this assignment.
- Owns only `Art/Source/Characters/FPArms01/`, `docs/assets/fparms01/`, `docs/workstreams/art-first-person-arms.md` in this checkout. Read all existing weapon assets, gameplay code and other folders without editing.

## Result

Produce an original pair of stylized first-person forearms and gloved hands that can be posed on the accepted R01 rifle and P01 pistol. This is a source-ready asset delivery for a later native import, not a promise of working in-game arms. Do real modeling and export work, not a concept-only report.

Keep the Valorant-inspired readable, simplified graphic style and the project's charcoal/cool slate/steel palette with restrained crimson accents. No copied characters, logos, photoreal skin, scans, dense grime, paid assets or costume expansion. Preserve the existing guns and their accepted palette. Both hands have believable fingers/thumbs, clean glove shapes and useful joint loops; do not substitute mitten/box proxies for the deliverable.

Use existing Blender tools and original editable source. Include a compact deform rig with forearm/wrist/finger control appropriate to static grip posing and later animation; name/document bones and transforms. Export mesh and skeleton in FBX with centimeter scale, +X forward / Z up documented consistently. Keep modest first-person geometry (aim around 12k triangles or less for the pair; report actual count) and a small material set. No full character, IK framework, animation library, C++, Blueprint wiring or changes to weapon sockets.

Inspect the existing weapon source/native manifests and presentation offsets before modeling. Reference R01 Grip/SupportHand/Muzzle and P01 grip/slide/magazine geometry to create one rifle hold and one pistol hold in the source scene. Keep trigger fingers, support grip and wrists readable with no obvious major weapon penetration; do not move or remodel the guns to make the fit work. Document any pose/attachment assumption that gameplay must resolve. Reference weapon files without copying them into the deliverable.

## Finish and stop

Deliver editable `.blend`, reproducible modeling/export steps or script, FBX, small palette/provenance/rig-and-attachment note, and at most three useful source-rendered previews (rifle hold, pistol hold, hand detail). Label every image as Blender/source preview, not Unreal/gameplay. Existing procedural tools may be reused; do not make a new asset framework.

Reopen the saved source/export once and inspect scale, rig/skinning and gross pose deformation; record practical measurements and issues. Use modest render settings and at most four Blender render threads. Inspect actual images. No new unit tests or exhaustive screenshot matrices; `git diff --check`, source/export inspection and relevant existing tools are enough here. If a preview exposes a real defect, correct it and re-render only the affected view.

Commit only owned paths locally, respecting Git LFS. Write one concise handoff at `docs/workstreams/art-first-person-arms.md` with commit, asset paths, source checks, previews, rig/pose contract and explicit pending native import/camera/animation acceptance. Publication and PR creation are coordinator-owned for this batch. Stop after the bounded source delivery; no additional agents or chats, no engine-slot polling, no edits in another checkout.
