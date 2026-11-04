# ART-12 — Meshy Site01 props and surfaces

**Four game-mesh sources ready for art review; stairs deferred with detailed source retained.** Source cleanup for five user-uploaded packages, 2026-11-04 (Europe/Berlin). Starting revision `b6e4a5f5db3b62833369121bc5bbc42a3fda1b64`, branch `codex/meshy-props-surfaces`. This delivery is for art review and later native integration; it does not change Site01 or establish runtime acceptance.

## Assets and placement contract

Source root: [PropsSurfaces](../../../../Art/Source/Environment/MeshySite01/PropsSurfaces/). Each ID has an editable `.blend` and a `Textures/` directory. Four ready IDs also have a selected-game-mesh FBX; their blends contain the visible game mesh and a hidden normalized detailed master. MSH-W02 contains only the normalized detailed source and has no game FBX. Raw FBX uploads are not duplicated in Git.

| Input | Actual inspected identity | Nominal X × Y × Z (cm) | Pivot and orientation |
| --- | --- | --- | --- |
| MSH-W02 | Five-step concrete flight with dark treads, pale safety noses and crimson side stripe | 200 × 200 × 100 | Base corner `(0,0,0)`; ascent toward +Y; five proposed 20 cm rises / 40 cm runs. |
| MSH-W03 | Four-panel concrete paving tile | 200 × 200 × 10 | Top-surface corner `(0,0,0)`; thickness extends to Z = −10 cm; repeat on X/Y 200 cm grid. |
| MSH-W04 | Long, two-panel cargo crate | 200 × 100 × 100 | Base center `(0,0,0)`; X is the long axis. |
| MSH-W07 | Thin metal infill panel, **not a cabinet** | 200 × 20 × 300 | Base corner `(0,0,0)`; X spans panel, Y is depth, front faces −Y. Open boundaries require existing wall backing. |
| MSH-W10 | Cube cargo crate | 100 × 100 × 100 | Base center `(0,0,0)`; +Z up. |

Blender uses meters internally (`scale_length=1`, centimeter display), applied object transforms, and +Z up. FBX exports `-Y` forward / `Z` up with unit conversion metadata, no extra global scaling, and only the game mesh selected. Native import should produce the centimeter envelopes above at actor scale 1. Source and FBX reimport measurements are recorded separately; verify Unreal's actual bounds before placement.

These are source art envelopes based on the existing modular guide, not permission to change map geometry. Site01 is currently flat and uses validated route/cover envelopes; do not add stair traversal or replace 120 cm cover with 100 cm crates silently. Native integration owns placement and collision. The panel's 200 × 20 × 300 profile follows the reference wall grid but is not a newly assigned structural opening or a cabinet model.

## Cleanup and retained detail

The actual imported assets were inspected before editing. All 25 assigned inbox files matched the coordinator's byte counts and SHA-256 hashes. Each original had one object, one material and UV0. The tile, stairs and crates were closed; the infill panel already had 506 boundary edges. Per-asset `inspection.json` records original bounds, counts, UVs and materials; `manifest.json` records the final source/game/export data.

The work keeps the uploaded silhouettes, panel construction, fasteners, color hierarchy and texture detail. Source meshes are normalized to the documented envelopes; crate bases receive a small contact-plane correction. Tile plateau deviations are compressed while retaining its seams and bevel strips. Stairs retain their original detailed geometry after normalization; attempted tread/reduction changes were rejected. These local corrections retain distinct vertices instead of collapsing entire surface patches into degenerate faces.

Separate game meshes reduce density. Direct reuse of the original normal/UV maps after reduction visibly distorted the sample surfaces, so the three reduced ready assets receive fresh game UVs and a selected-to-active material rebake. Base color, roughness, metallic and tangent normal are reprojected from the supplied textured detail master at 2048², with 16px bake margin, 1.2 cm cage extrusion and 3.5 cm ray distance. No lighting is baked into the base color; color/scalar channels use emission baking. Custom corner normals transfer from the detailed master, and the FBX includes tangent data. The low-density panel retains its supplied UVs and maps.

Original 4096² color/normal and 2048² scalar maps are preserved byte-for-byte for the detailed masters. Each material remains one slot. No replacement primitive art, new logo, weathering generator or recolor is introduced. Crimson/yellow marks remain part of the supplied color textures; a separate optional paint mask is not claimed. Repeated marked assets should be placed sparingly pending a separately assigned material-variant pass.

## Textures and native handoff

- `T_MSH_Wxx_basecolor.png`: original sRGB color for the detailed master (and panel game mesh).
- `T_MSH_Wxx_normal.png`: original RGB tangent-normal input, interpreted as +Y/OpenGL in Blender. The vendor package did not label its convention; its original interpretation is a source-preview assumption.
- `T_MSH_Wxx_roughness.png` / `_metallic.png`: original linear scalar textures, connected directly to the corresponding Principled inputs. No packed ORM channel convention is assumed.
- `T_MSH_Wxx_Game_<role>.png`: rebaked game maps for the three reduced ready meshes. Game normal baking explicitly uses tangent-space `+X,+Y,+Z` (OpenGL). Base color uses sRGB; all other maps use Non-Color data.

For Unreal 5.8.4, the future native owner imports FBX and textures through the editor, reconstructs the single material slot from the **Game** maps (the four original maps for MSH-W07), and checks the actual result. Use sRGB for base color, linear sampling for roughness/metallic, and normal-map compression for normals. The baked +Y maps require the appropriate green-channel flip for Unreal's DirectX normal convention. Confirm raised/recessed features under a neutral light rather than relying on filenames. Do not import the hidden detailed master or preview furniture.

No UCX collision is included. Recommended intent: reuse the existing validated map collision for visual replacements; a flush simple box for floor tiles if separately needed; deliberate simple crate cover boxes; a separately approved stair collision/ramp only where traversal is intended. The infill panel is a wall-backed visual shell, not a freestanding watertight blocker or doorway. No collision or movement behavior is established by source renders.

## Reproduction and evidence

Run in the assigned worktree with the installed Blender 5.2.2 LTS, one process at a time:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python-exit-code 1 \
  --python Art/Source/Environment/MeshySite01/PropsSurfaces/prepare_assets.py \
  -- produce MSH-W02 MSH-W03 MSH-W04 MSH-W07 MSH-W10
```

The script verifies input hashes before import and copies each FBX to a temporary directory, preventing embedded extraction from writing to the canonical inbox. It writes only this task's source/docs paths and temporary files. `--factory-startup` avoids unrelated installed add-on handlers. On this Mac, sandboxed Blender failed at startup; the successful source work used approved local execution. No Unreal process, engine-slot attempt or native import was run.

Before/after images under [Previews](Previews/) are actual Blender Cycles CPU renders with matched neutral lighting and camera direction. Framing follows each envelope, so before/after images are comparisons of appearance, not measurements of physical scale. The first representative tile and long crate were visually checked before continuing the remaining uploads. Further native import/reopen, in-map lighting/DPI/distance inspection, collision/traversal and runtime acceptance remain pending.

## Provenance

User-supplied Meshy exports from the project's generated concept references. Input paths, original file hashes and sizes are retained in each asset's manifest from [the coordinator intake](../../meshy-site01-intake.json). Generation/account IDs and vendor license evidence were not supplied in that intake; this source work makes no additional licensing claim. Originals remain unchanged in the local canonical inbox. The detailed master preserves the uploaded geometry and original maps through the normalization/cleanup process; derived game UVs and baked maps are authored by this task. No downloaded assets or dependencies were added.

## Review outcome and concrete remaining issues

| Asset | Input triangles | Delivered game triangles | Review outcome |
| --- | ---: | ---: | --- |
| MSH-W02 | 880,472 | **None** | Deferred. Normalized detailed source only; controlled retopology and rebake needed. |
| MSH-W03 | 7,774 | 498 | Tile silhouette, cross seam and neutral material retained. Source tiling preview accompanies the export. |
| MSH-W04 | 286,224 | 6,000 | Long crate panels, handle openings and color hierarchy retained; minor generated edge unevenness remains. |
| MSH-W07 | 3,774 | 3,774 | Infill panel retained with original UV/textures; 506 open boundary edges remain. Requires wall backing and art-lead identity review. |
| MSH-W10 | 347,276 | 6,000 | Cube crate retained with shared 100 cm family height/depth; stacking preview is geometric source inspection only. |

The stair low-budget trial finished at 2,180 triangles and tore the surface severely. A 60,000-triangle trial still produced visible tread and normal/projection defects. Neither is a usable game export. [Rejected reduction evidence](Previews/MSH-W02_RejectedReduction.png) is intentionally retained and labeled; [detailed source preview](Previews/MSH-W02_DetailOnly.png) shows the preserved design. Stop further automatic reduction of this asset: a future owner should construct controlled tread/riser topology around the supplied design and rebake it. No rejected stair FBX or game texture is included.

Crate details soften at the practical 2K game bake resolution, especially small fasteners and narrow edge seams. The original 4K maps and high-detail sources remain available. The long and cube crates share normalized family envelopes but their generated seats/feet are not a mechanically certified interlocking system. The panel is not a cabinet and should not be assigned cabinet gameplay/collision.

Native import, exact material/normal response, source-to-map fit, collision, movement, lightmap needs, platform/performance and runtime acceptance remain unverified. There is one UV channel per mesh; the native owner must decide whether to generate a separate lightmap UV channel for the chosen lighting path. Do not substitute art renders for those checks.

## Checks actually completed

[Source/export evidence](source-export-checks.json) records five saved Blender sources reopened, external material images explicitly loaded and hashed, finite geometry and applied transforms, plus four FBX round-trips. Triangle counts and bounds matched the source within 0.001 cm; each export contains only its intended game mesh and one material/UV channel. The stair source was checked without claiming an FBX. All 25 canonical inputs were rehashed unchanged after authoring. No new test framework or Unreal work was used.

[Source overview](Previews/ART12_SourceOverview.png) and [representative before/after comparison](Previews/ART12_RepresentativeComparison.png) show actual renders. Additional focused source inspections: [2×2 paving patch](Previews/MSH-W03_TilePatch_2x2.png), [cube stack](Previews/MSH-W10_CrateStack.png), [panel back](Previews/MSH-W07_PanelBack.png). Tile repetition showed no gross gaps at the nominal grid; the stack showed bounding-envelope contact without a physics/locking claim. Both panel sides render, but the measured open boundaries still prevent a watertight-volume claim.

To repeat the source inspection and compose labeled sheets:

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --python-exit-code 1 \
  --python Art/Source/Environment/MeshySite01/PropsSurfaces/prepare_assets.py \
  -- verify MSH-W02 MSH-W03 MSH-W04 MSH-W07 MSH-W10
python3 Art/Source/Environment/MeshySite01/PropsSurfaces/compose_previews.py
git diff --check
```

The compositor uses installed Pillow 13.0.0 only to resize/label actual renders. Require the completed JSON report and `ART12_DONE verify` marker, not merely Blender's exit status. An initial inspection assertion encountered Blender's lazy image loading; the completed pass explicitly loads image pixels. An earlier stair source operation was stopped after a process sample identified repeated per-vertex full-mesh normal recalculation; the discarded correction cached normals before editing. Those attempts are not counted as completed verification.
