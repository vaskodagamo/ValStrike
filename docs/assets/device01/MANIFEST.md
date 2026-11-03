# D01 Relay objective-device asset

Updated: 2026-11-03 (Europe/Berlin). ART-04. Branch `codex/objective-device`, starting at `a520ff20fba6bb9a94d0fc6ae2c59acff3d94606`. Coordinator owns publication/merge and runtime integration.

**ART-04 asset batch complete and verified.** One original compact stylized device, built to the fixed playtest art contract. It uses a chamfered slate shell, protective cool-steel rails, broad feet and an inset status surface. No downloaded model, scanned/photo texture, font or purchased asset was used.

## Files

| Deliverable | Location |
| --- | --- |
| Editable Blender source | [D01_Relay.blend](../../../Art/Source/Objectives/Device01/Models/D01_Relay.blend) |
| Reproducible model/masks/export/validation | [build_device01.py](../../../Art/Source/Objectives/Device01/Models/build_device01.py) |
| Actual FBX | [SM_VS_Device01.fbx](../../../Art/Source/Objectives/Device01/Models/Exports/SM_VS_Device01.fbx) |
| Source geometry and serialized-material evidence | [mesh_validation.json](../../../Art/Source/Objectives/Device01/Models/mesh_validation.json) |
| Original mask images | [Textures](../../../Art/Source/Objectives/Device01/Models/Textures/) |
| Authoritative appearance | [appearance.json](../../../Art/Source/Objectives/Device01/appearance.json) |
| Native authoring/checks | [import_device01.py](../../../Art/Source/Objectives/Device01/import_device01.py) |
| Fresh native reopen/capture workflow | [inspect_device01.py](../../../Art/Source/Objectives/Device01/inspect_device01.py) |
| Native authoring and fresh-process reopen evidence | [Import](unreal-import-report.json), [reopen](unreal-reopen-report.json) |
| Eight actual native previews and capture evidence | [Status comparison](PREVIEWS.md), [inspection report](unreal-inspection-report.json) |
| Exact commands and integration instructions | [IMPORT.md](IMPORT.md) |
| Append-only evidence | [workstream log](../../workstreams/objective-device.md) |

Source images are actual Blender mesh renders. Eight verified native images are labeled Unreal editor compositions. There is no generated concept image or gameplay capture in this task.

## Fixed geometry and attachment contract

One mesh: `/Game/Art/Objectives/Device01/Meshes/SM_VS_Device01`.

| Property | Contract / measured source |
| --- | --- |
| Envelope | **30 × 18 × 10 cm** |
| Bounds min / max | **(-15,-9,0) / (15,9,10) cm** |
| Pivot | Bottom center `(0,0,0)` |
| Coordinates | +X forward, +Z up, centimeters, identity transforms |
| Triangles / source vertices / native render vertices | **910 / 492 / 2,038** |
| UV channels | UV0 and UV1_Lightmap |
| Material slots | Exactly **Shell, Trim, Status**, indices 0, 1, 2 |
| Socket | Exactly **Carry**, `(0,0,8)` cm, identity rotation, unit scale |
| Visual collision | NoCollision; no simple/convex/section collision |
| Native LOD / Nanite | One LOD / disabled |

Native measured dimensions are 30.000002 × 18.000004 × 10.000000 cm; the few-millionths centimeter difference is FBX floating-point precision. Native render vertices split along hard edges/UV seams.

All component shells are closed/manifold except the intentional two-triangle status surface with four boundary edges. No collision geometry is exported. Intersecting internal shells are intentional. The full screen UV0 uses U along source+X and V along source+Y; native FBX conversion reflects Y. Other UV faces use isolated tiles; no light-bake or texture-density acceptance is claimed.

To align Carry with a hand/reference anchor, account for its 8 cm offset above the bottom pivot. Runtime must not add another scale factor. The mesh's bottom plane supports floor-placement calculations, but the runtime actor still owns collision, floor traces and reachability.

## Status treatment

| State | Shape cue | sRGB glyph color | Native material instance |
| --- | --- | --- | --- |
| Idle | Hollow ring and horizontal bar | `#91ABC2` | `MI_VS_D01_StatusIdle` |
| Armed | Triangle and exclamation mark | `#F2B24D` | `MI_VS_D01_StatusArmed` |
| Defused | Checkmark inside square | `#8FD4C8` | `MI_VS_D01_StatusDefused` |

Three original 256×256 masks share a single small 8 × 5 cm status face. Shapes remain distinct without color; colors are secondary. Slot2 is Idle by default. All instances reference `M_VS_D01_Status` and vary both GlyphMask and GlyphColor. The screen uses background `#101923`, roughness 0.65 / metallic 0 and constant 0.15 glyph-only emission. There is no pulse, countdown, bloom strip or VFX system. Shell is `#303F50` (roughness 0.70 / metallic 0), Trim `#8D9FB4` (0.50/0.15). Shader constants decode sRGB to linear. Masks are linear data with opaque alpha and clamp sampling.

At normal player distance the whole device is necessarily small; the status surface complements runtime interaction/HUD cues rather than replacing them. Replicated gameplay state chooses the appropriate material. Art material selection alone is not an authoritative game event.

## Verification and limits

The assigned Mac Editor target built successfully in 44.86 seconds through the shared engine-slot wrapper. Source assertions cover exact bounds/transforms, one canonical mesh, triangle budget, ordered slots, closed shells except the declared screen, nondegenerate geometry/UVs, actual FBX round-trip measurements and screen orientation. Saved Blender reopened with matching geometry/materials and all three packed masks. Raw PNG validation checks actual values, alpha, RGB equality, hashes and asymmetric checkmark anchors. Four final source renders were inspected after correcting a coplanar front-trim artifact.

Native import and a fresh editor process verified the fixed bounds, ordered slot names/default material paths, exactly one Carry socket, NoCollision defaults/sections, two UVs, one LOD and disabled Nanite. Actual material values, connected parameter/mask/UV0 graphs, linear texture settings and source hashes matched on reopening. All eight fresh 1600×1000 captures completed and were visually inspected. Neutral/dim state comparisons use matched cameras; the checkmark is upright and unmirrored. The source/FBX geometry was also compared triangle-by-triangle after the final export.

The final FBX SHA-256 is `f93e72c5f2bacd794dd49f2b5d482400fd648e23c4ba8b58c8f93cccca182520`. Reproduction and logs are in IMPORT.md and the append-only workstream record. Four shared-slot busy exits deferred native work; no lock bypass occurred. The two existing engine startup automation `Condition failed` diagnostics remained, while both asset scripts completed without Python errors.

In the dim stage the slate shell loses some recess detail, while trim edges and status glyphs remain identifiable. Player-eye inspection confirms the device and warning triangle are small at the fixed physical size; the inner exclamation mark needs close viewing. Carry view has no arms and crops the low feet. These are documented composition/readability limits, not gameplay or accessibility certification.

No gameplay classes, map/config edits, placement/collision implementation, state replication, interaction, animation, hands/rig, network tests, packaged-build, second-Mac or performance verification is included. Pistol/rifle art remains unchanged. The coordinator can integrate this asset through the runtime's optional mesh setting and select state instances on slot 2; that wiring is separate work.
