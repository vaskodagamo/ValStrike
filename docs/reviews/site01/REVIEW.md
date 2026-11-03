# Site01 readability review

2026-11-03 (Europe/Berlin) · `codex/site-readability` · foundation `a520ff20fba6bb9a94d0fc6ae2c59acff3d94606`.

**Verdict: visually suitable for a guided first Mac playtest.** Two small navigation cues would help unfamiliar players. This review does not establish match, multiplayer, performance, packaged-build or second-Mac readiness. The coordinator owns those gates and any fixes below.

## Actionable findings

| Finding / viewpoint | Visible evidence and player consequence | Smallest suggested fix |
| --- | --- | --- |
| **R1 — Service turn is easy to mistake for a dead end.** Camera `(50,1750,152)`, facing +X. | In [05 service flank](../../maps/site01/previews/05_service_flank.png), the blank end wall dominates the view and the left turn around the service building is hidden. A first-time player can stop or backtrack instead of following the flank toward the defender side. The authored route confirms a continuation; this is a navigation cue problem, not a blocked route. | One high-contrast **A + left-turn arrow** near the visible far-wall/return corner. Keep the geometry and route. Useful before an unguided test; explain the turn during a guided test. |
| **R2 — Transformer arrival relies on the amber pad to identify A.** Camera `(-150,-1450,152)`, looking toward `(1000,-650,152)`. | In [08 Transformer exit](../../maps/site01/previews/08_transformer_exit.png), the site pad is visible but the existing wall A faces away. Players who do not know the color convention, or cannot distinguish it reliably, lack a visible site identifier from this approach. | Add one contrasting **A glyph** to the pad or the approach-facing surface. Retain the amber boundary and existing wall marker. |

## Keep, and watch during playtest

Retain the broad matte surfaces, sparse detail, clear low-cover tops and distinct full-height Transformer mass. They preserve the requested stylized direction and make cover classes understandable. The amber boundary remains easy to distinguish from paving. No obvious false doorway, see-through blocking gap, missing face or distracting glare was visible in the reviewed stills.

The **temporary dark proxy** remains recognizable against the Transformer in [09](previews/09_transformer_proxy.png). Separation is weaker against the teal/black crate stack in [10](previews/10_site_proxy.png), particularly along the torso edge, but the full proxy is still identifiable in that close static view. This does **not** justify a broad material change: the proxy is not the final character, and these are approximately 3.8 m / 4.8 m observations, not long-range or motion tests. Watch the stacked-crate position during the first match; if players actually lose dark targets there, trial a lighter crate-facing panel or character edge treatment locally.

The accepted R01 has a visible upper outline and readable light side panel in 09/10. Its fixed placement is an art composition, not the runtime presentation component. View 09's weapon hides part of the nearby proxy's lower body; it cannot establish full-body visibility from that pose. The [standing](previews/11_cover_standing.png) / [crouched](previews/12_cover_crouched.png) pair shows the intended 120 cm cover-height distinction. Cover collision is supported by the earlier map inspection, not newly exercised here.

## Reviewed views

| Step | Screenshot | Assessment |
| --- | --- | --- |
| 01 | [Attacker exit](../../maps/site01/previews/01_attacker_exit.png) | Screen corner and low-cover silhouette readable. |
| 02 | [Gallery contact](../../maps/site01/previews/02_gallery_contact.png) | Uncluttered lane; near cover top readable. |
| 03 | [Transformer contact](../../maps/site01/previews/03_transformer_contact.png) | Full-height obstruction clear; old exposure insufficient for final contrast judgment. |
| 04 | [Site A](../../maps/site01/previews/04_site_a.png) | Boundary and wall A recognizable; dark crates warrant the added proxy view. |
| 05 | [Service flank](../../maps/site01/previews/05_service_flank.png) | R1: missing turn cue. |
| 06 | [Defender exit](../../maps/site01/previews/06_defender_exit.png) | Corner, low cover and Transformer remain distinct. |
| 07 | [Overview](../../maps/site01/previews/07_overview.png) | Route context only; not player-eye evidence. |
| 08 | [Transformer exit](../../maps/site01/previews/08_transformer_exit.png) | R2: site identity depends on learned color. |
| 09 | [Transformer + temporary proxy](previews/09_transformer_proxy.png) | Head/upper body separate from teal; rifle masks lower body. |
| 10 | [Site + temporary proxy](previews/10_site_proxy.png) | Dark torso edge weaker against stack; visible at this close distance. |
| 11 | [Standing at SiteWestCover](previews/11_cover_standing.png) | Eye clears top; no false opening indicated. |
| 12 | [Crouched at SiteWestCover](previews/12_cover_crouched.png) | Cover face blocks forward view as expected. |

## Evidence boundaries and reproduction

The eight supplied previews were explicitly authorized for reuse. Their hashes and the saved map hash match [reused-evidence.json](reused-evidence.json) and the original [editor report](../../maps/site01/editor-report.json). They use 70° horizontal FOV (55° overview) and forced fixed exposure. They are not current runtime captures.

The four supplemental native editor captures use 90° FOV, unchanged saved map lighting, project exposure defaults and no camera postprocess overrides. Their Z coordinates are 154.15 / 90.15 cm: source eye heights 152 / 88 cm plus the 2.15 cm floor gap observed in the prior [single-player traversal](../../maps/site01/runtime-report.json). The camera is positioned explicitly; no player was driven. The proxy is an unsaved 50 × 40 × 144 cm box plus 32 cm sphere, overall 176 cm high, with a transient dark matte material. It is **not a final character**. The source R01 meshes/materials are unmodified; their grip offset is `(32,16,-18)` cm relative to the camera. The cover pair intentionally omits the cosmetic rifle to avoid art-composition intersection with cover.

See [capture-report.json](capture-report.json) for exact positions, source hash, PNG hashes and before/after Content verification. Production maps, materials and meshes were never saved by this script. No image generation, repainting or screenshot retouching was used. No new collision, network or human gameplay acceptance is claimed.

Run from this assigned checkout with UE 5.8.4 / Xcode 26.1.1:

```sh
python3 Scripts/with_ue_slot.py -- python3 Scripts/ue.py \
  --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac

python3 Scripts/with_ue_slot.py -- \
  /Users/webproduktion3/UE_5.8/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor \
  /Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/site-readability/ValStrike.uproject \
  /Engine/Maps/Entry \
  -ExecutePythonScript=/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/site-readability/docs/reviews/site01/capture_readability.py \
  -DisablePlugins=AndroidFileServer -unattended -nosound \
  -abslog=/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/site-readability/Saved/Logs/SiteReadabilityFinal.log
```

Exit 75 means the engine slot is occupied; do not bypass it. Review the JSON result and editor exit code before treating any captures as a successful run. Stop this bounded review after the report; the next useful evidence is the integrated human playtest.
