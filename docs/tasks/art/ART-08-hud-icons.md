# ART-08 — Compact tactical HUD icon set, source delivery

- Owner: new Codex art chat; reasoning **High**, retain the configured model. No extra agents.
- Branch/worktree: `codex/art-hud-icons`, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/art-hud-icons`.
- Base: main `90849f5` plus coordinator assignment commit supplied at dispatch; record starting HEAD.
- Owns only `Art/Source/UI/HUDIcons01/`, `docs/assets/hudicons01/`, `docs/workstreams/art-hud-icons.md`.
- V1 source-only assignment. No Unreal builds/imports/runtime checks or new unit-test suites. Claude INT-01 owns the engine slot.

Create eight original, consistent HUD icons: rifle, pistol, reload, device carried, plant, defuse, eliminated and spectator. Reference the current rifle/pistol/device silhouettes and the existing HUD/round states read-only. Deliver useful vector assets rather than another UI review or redesigned menu. No crosshair redesign, typography system, ability icons, brand marks or expanded game features.

Use clean geometric shapes, readable negative space and a consistent optical weight. Design on one common grid, legible at 24 and 32 display pixels with a monochrome tint; plant/defuse/carried must differ by shape rather than color alone. Keep the project's restrained stylized direction. Source SVG is the preferred artifact; draw it directly as editable vectors. Avoid an image-generation detour or downloaded icon/font dependencies. Export transparent PNGs at a documented practical resolution using an existing local renderer where available; retain the canonical SVGs. No texture atlas/framework unless a real existing import requirement demands it.

Include one contact sheet showing actual exports on light and dark backgrounds at intended sizes. Inspect it visually, correct obvious legibility problems, and document the icon-to-state mapping, canvas bounds, padding, tint/alpha convention and intended Unreal import settings. Source/contact-sheet images are not screenshots of implemented HUD behavior. Write no gameplay, Slate/UMG code, Content packages or central docs. ART-06 owns world signage, so do not create site letters/arrows or edit its files.

Use practical file/XML/dimension/alpha checks and git diff --check; no new tests or screenshot matrix. Commit only owned source/export/docs files locally, respecting Git LFS for PNGs. Write one concise handoff in docs/workstreams/art-hud-icons.md with commit, assets/contact sheet, checks and pending native import/UI hookup. Coordinator handles publication. Stop at the source-ready set; no push/merge, extra agents/chats or engine-slot attempts.
