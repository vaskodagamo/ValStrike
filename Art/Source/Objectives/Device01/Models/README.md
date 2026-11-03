# D01 Relay model source

Original compact ValStrike objective prop authored in Blender 5.2.2 LTS (`d13f752e3b9c`) on 2026-11-03 (Europe/Berlin). Clean slate shells, two protective steel rails, broad base feet and a small status screen make the silhouette readable. There are no functional mechanisms, animations or gameplay behavior.

## Source and reproduction

- `build_device01.py`: original procedural mesh, status-mask formulas, export, validation and previews.
- `D01_Relay.blend`: individual editable parts, canonical export mesh, Carry marker, linked preview mesh, camera and lighting. All three status mask images are packed and retain `//Textures/` references.
- `Exports/SM_VS_Device01.fbx`: one real FBX mesh in the default Idle state; no collision meshes.
- `Textures/T_VS_D01_StatusIdle.png`, `T_VS_D01_StatusArmed.png`, `T_VS_D01_StatusDefused.png`: original 256 × 256 mathematical masks, generated through Blender's image API.
- `mesh_validation.json`: measured geometry, FBX round-trip, saved-Blender reopen, material/status/UV and raw mask evidence.
- `Previews/` and `preview_validation.json`: actual source mesh renders and their hashes. These are Blender compositions, not native Unreal or gameplay captures.

Run from the assigned objective-device checkout:

```sh
/Applications/Blender.app/Contents/MacOS/Blender \
  --background --factory-startup --threads 4 --python-exit-code 1 \
  --python Art/Source/Objectives/Device01/Models/build_device01.py
```

Append `-- --verify-only` to check existing saved source/material/mask values without writing any asset or preview.

Append `-- --skip-render` to generate/export/validate without rendering. Append `-- --render-only` to verify and render the saved source without rewriting the blend, FBX, masks or mesh report. Full generation overwrites this script's outputs; preserve hand edits before rebuilding. Native Unreal import is a separate step.

The hidden `SM_VS_Device01_EDITABLE_PARTS` collection contains individually editable originals. The hidden canonical mesh is the export authority. `PREVIEW_ONLY_linked_mesh` holds the linked display copy and presentation helpers. Unhide selected editable parts for manual work; changing the blend alone does not update the reproducible generator.

## Fixed asset contract

One mesh named `SM_VS_Device01`, with **910 triangles / 492 source vertices** and exact source bounds **(−15,−9,0) to (15,9,10) cm**. Its envelope is **30 × 18 × 10 cm**. Source and export transform are identity, pivot is bottom center `(0,0,0)`, +X is forward, +Z is up. FBX's native scene conversion reflects source Y. The Carry marker is `(0,0,8)` cm with identity rotation/unit scale; Unreal creates the actual socket.

Material slots are exactly ordered **Shell, Trim, Status**. No collision is exported; equipped/placed visual components must use `NoCollision`. Runtime owns actual collision and placement.

The status face is a single +Z-facing quad at Z = 8.81 cm, spanning X = −4…4 and Y = −2.5…2.5 cm. It becomes two triangles. UV0 is `U=(X+4)/8`, `V=(Y+2.5)/5`, covering the full mask once. Source −Y is the reading side; after the native Y reflection that corresponds to Unreal +Y. The asymmetrical Defused check has its short lower stroke toward source −X and its long rising stroke toward source +X/+Y. Native image orientation must preserve that readable check.

## Appearance and status glyphs

`../appearance.json` is authoritative. Hex colors are sRGB and are decoded to linear constants in Blender and Unreal. Shell is matte slate; Trim is restrained cool steel. Status uses a dark background and a separate color/mask pair per state, with constant **0.15 glyph-only emissive strength**. There is no pulse, countdown, bloom strip or animated behavior.

| State | Glyph | sRGB |
| --- | --- | --- |
| Idle | Hollow ring and short horizontal bar | `#91ABC2` |
| Armed | Warning triangle and exclamation mark | `#F2B24D` |
| Defused | Checkmark inside a square | `#8FD4C8` |

These states remain distinguishable by shape without color. The script defines circle-distance and line-segment-distance formulas directly; no font or borrowed symbol artwork is used. The 8:5 physical surface aspect is compensated in the formulas so the ring is physically circular. The 256px masks use 3 × 3 coverage supersampling, equal RGB channels, black background, white glyph interiors and opaque alpha. Import as linear data (`sRGB=false` in Unreal); do not treat them as colored image textures.

The native status shader must be recreated from mask/background/state color. Arbitrary Blender shader-node graphs do not transfer through FBX. The native importer assigns the corresponding Unreal materials to the preserved three slot names.

## Validation and limits

Source checks cover exact bounds, identity transforms, slot order, positive closed-shell volume, nondegenerate triangles/UV triangles and UV bounds. All shells are closed and manifold except the intentional two-triangle status surface, whose four perimeter edges are open. Shells intersect internally by design. Both UV channels exist; UV1 and non-status UV0 faces use isolated face tiles, with no light-bake or texture-density acceptance claim.

Actual FBX reimport verifies bounds, triangle count, slot order, two UV channels and the full status UV orientation. Saved-Blender reopening checks geometry, material values, default Idle selection, all packed images, relative paths and state shader inputs. Raw saved PNG checks cover values, channels, alpha, hashes and the asymmetrical check's reference samples. Source previews show all three states under matched neutral lighting and the default Idle under dim lighting.

No external meshes, textures, scans, fonts, purchases or third-party modeling add-ons were used. Model and graphics are original project-authored assets. Native asset persistence, actual in-game readability, replicated state selection, interaction/placement, animation, multiplayer and performance are separate acceptance work.
