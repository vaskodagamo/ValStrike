# ValStrike asset chat handoff

Updated: 2026-11-03. This brief supports a separate asset chat while the main chat builds and verifies gameplay.

Create a separate worktree from the latest committed `codex/bootstrap-fps` branch, using an asset branch such as `codex/environment-kit`. Keep the main workspace on its current branch. Read [AGENTS.md](../AGENTS.md), [README.md](../README.md), [MVP_PLAN.md](MVP_PLAN.md), [BACKLOG.md](BACKLOG.md), and the latest [WORK_LOG.md](WORK_LOG.md) entry before starting.

## First asset batch

**User-confirmed visual target: Valorant-like stylized graphics, not CS2-like photorealism.** Use clean geometric forms, broad controlled color areas, readable silhouettes, simplified materials, and restrained texture detail. Avoid photographic surface scans, dense realistic grime, and a photorealistic lighting treatment. References guide visual principles; create original designs.

Prepare a small original environment kit and a visual direction sheet for the desktop Unreal Engine 5.8.4 MVP. This is preparatory work for `PLAY-02`; it does not complete that milestone or change the map layout.

Start with one restrained palette and one labeled concept contact sheet showing the pieces together. A useful starting direction is warm plaster, muted blue-green painted metal, and darker structural trim, with broad color areas and readable edges. Treat this as an exploration rather than an established final style.

| Piece | Initial dimensions in centimeters | Purpose |
| --- | --- | --- |
| Wall panel | 400 wide × 20 deep × 300 high | Reusable architectural surface |
| Corner pillar | 40 × 40 × 300 | Join and finish wall segments |
| Crate | 100 × 100 × 100 | Simple prop and material reference |
| Low cover block | 200 wide × 100 deep × 120 high | Readability and cover silhouette study |
| Wall vent | 120 wide × 20 deep × 80 high | One reusable decorative detail |

These sizes are starting specifications. The gameplay chat decides where assets belong and verifies cover heights, collision, sightlines, and movement clearance. Keep this batch small; characters, weapons, animation, audio, extra maps, and a full production art pass are outside this handoff.

## Concepts and usable game assets

AI-generated images can supply concepts, palette studies, and suitable texture sources. A rendered image of a crate is not a 3D crate, and a contact sheet is not an Unreal asset pack.

Deliver meshes separately through a real modeling/export workflow, retaining editable source files. Import `.uasset` files through Unreal Editor only. Label each deliverable clearly as concept, source model, exported mesh, imported asset, or runtime-verified asset. If modeling or Unreal is unavailable in the asset worktree, continue the concepts and precise modeling specifications and record the missing verification honestly.

Use original designs rather than reproducing recognizable Counter-Strike or Valorant maps, logos, characters, or props. Make no purchases or paid-service commitments. Record the author/tool, source or reference, date, and applicable license/provenance for every external or generated input.

## File ownership

| Location | Asset chat responsibility |
| --- | --- |
| `Art/Source/Environment/Concepts/` | Palette, labeled contact sheet, reference images, and generation notes |
| `Art/Source/Environment/Models/` | Editable modeling files and exported interchange meshes |
| `Content/Art/Environment/Meshes/` | Native imported static meshes |
| `Content/Art/Environment/Materials/` | Small shared materials and instances |
| `Content/Art/Environment/Textures/` | Only textures required by the kit |
| `docs/assets/MANIFEST.md` | Asset names, dimensions, provenance, triangle/material counts, and verification status |
| `docs/assets/WORK_LOG.md` | Append-only asset-session log, results, gaps, and next action |

Create these directories only when they contain a deliverable. Follow the existing Git LFS rules for binary files. Use unique names such as `SM_VS_Wall_400x300`, `SM_VS_Crate_100`, `M_VS_Plaster`, and `MI_VS_Plaster_Warm`.

The main chat owns gameplay, `Source/`, `Config/`, `Scripts/`, `Tests/`, `Content/Maps/VS_Graybox.umap`, and `Content/Materials/Bootstrap/`. Do not edit binary maps, regenerate the graybox, or replace its bootstrap materials from the asset chat. Keep central plan/backlog/log changes with the main chat; use the dedicated asset log for this task and provide a summary for integration.

## Modeling and acceptance

- Use Unreal's centimeter scale, Z up, and a declared front direction. Apply modeling transforms before export and verify imported dimensions at scale 1.
- Make wall pieces snap on a 10 cm grid. Put architectural pivots at a documented base corner and prop pivots at the bottom center.
- Use simple geometry, clean normals, modest bevels, and a small shared material set. Avoid details that obscure player silhouettes or imitate usable cover without matching collision.
- Provide simple box or convex collision suited to each piece. Keep decorative vent collision off unless gameplay needs it; verify collision after import.
- Use readable materials under ordinary lighting. This kit must not depend on Nanite, Lumen, ray tracing, or third-party plugins.
- Check UVs and texture orientation when textures are used. Report triangle counts, material slots, texture sizes, bounds, and collision type instead of claiming an unmeasured performance result.
- Deliver a contact sheet, manifest, source/export files where created, and import inspection screenshots where Unreal was available. Record missing checks explicitly. Map placement and gameplay acceptance happen in the main chat.

Commit asset work on its own branch and hand back the branch/commit, changed paths, previews, and verification notes. The main chat integrates the kit after reviewing it against the movement and map tests.

## Starting prompt

Paste this into the separate chat after opening its worktree:

```text
Work on ValStrike's first environment asset batch. Use a separate worktree based on the latest committed codex/bootstrap-fps, on codex/environment-kit; do not switch or edit the main chat's workspace.

Read AGENTS.md, README.md, docs/MVP_PLAN.md, docs/BACKLOG.md, the latest docs/WORK_LOG.md entry, and docs/ASSET_BRIEF.md. Follow the asset brief's ownership boundaries. The main chat owns gameplay and VS_Graybox; do not edit or generate maps, gameplay code, project configuration, bootstrap materials, or shared setup scripts.

The user explicitly wants Valorant-like stylized graphics, not CS2-like photorealism: clean shapes, controlled colors, readable silhouettes, simplified materials, and restrained texture detail. Create original designs. Start with a palette and labeled concept contact sheet for the five-piece environment kit. Build actual source/export meshes when the available modeling tools support it, and import native Unreal assets only through Unreal Editor 5.8.4. Clearly distinguish AI bitmap concepts from meshes and runtime-verified assets. Make no purchases. Preserve centimeter scale, documented pivots, simple collision, and provenance.

Keep outputs under Art/Source/Environment and Content/Art/Environment. Maintain docs/assets/MANIFEST.md and your own append-only docs/assets/WORK_LOG.md instead of editing the central work log or backlog. Deliver a small reviewable batch with previews, exact paths, verification results, remaining gaps, and a branch/commit handoff for the main chat.
```
