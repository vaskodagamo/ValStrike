# D01 Relay authoring, native verification and integration

Run from `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/objective-device` on `codex/objective-device`. Pinned tools: Blender 5.2.2 LTS (`d13f752e3b9c`), Unreal 5.8.4 (`58902052`), Xcode 26.1.1 (`17B100`). Do not use another checkout's generated outputs or editor.

Every heavy Unreal command below uses the shared engine-slot wrapper. Exit 75 means another stream owns the slot: continue lightweight work and retry later; do not bypass or rapidly poll. Asset inspection opens no game server and uses none of the reserved multiplayer ports.

## Source and export

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 4 --python-exit-code 1 \
  --python Art/Source/Objectives/Device01/Models/build_device01.py
```

The generator writes the editable blend, three original status-mask PNGs, a real FBX, measurements and four actual source previews. Append `-- --skip-render` to omit rendering. Append `-- --render-only` to load/verify the saved blend and render without changing source/FBX/masks. Append `-- --verify-only` for read-only saved-source/material/mask checks. Full regeneration overwrites its outputs; preserve manual edits first. Geometry, mask formulas and shader inputs are all editable source. See [source README](../../../Art/Source/Objectives/Device01/Models/README.md).

`appearance.json` is authoritative for Shell/Trim and the three status states. Hex colors are sRGB, decoded to linear using `v/12.92` below 0.04045 and `((v+0.055)/1.055)^2.4` otherwise. Mask PNGs are data, not color: equal RGB, opaque alpha, native `sRGB=false`, `TC_MASKS`, clamp addressing. The framebuffer image is not used as a texture. All mask pixels come from project-authored mathematical ring/line-segment formulas.

## Build and native authoring

```sh
VS_D01_ROOT="$PWD"
VS_D01_EDITOR="/Users/webproduktion3/UE_5.8/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
mkdir -p Saved/Logs
python3 Scripts/with_ue_slot.py -- python3 Scripts/ue.py \
  --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
python3 Scripts/with_ue_slot.py -- "$VS_D01_EDITOR" "$VS_D01_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_D01_ROOT/Art/Source/Objectives/Device01/import_device01.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_D01_ROOT/Saved/Logs/Device01_Import.log"
```

The Mac Editor build succeeded in 44.86 s (`Saved/Logs/Device01_Build.log`); it neither cooks nor packages. The importer refuses an existing or partial native batch by default. After inspecting a partial failure or deliberately regenerating source, add `-VSDeviceRefresh` for a scoped repeat. It creates/saves only Device01 assets using Unreal's own factories/APIs, preserves the fixed slot names, assigns Idle by default, creates Carry and removes all visual collision. It never edits maps, gameplay classes or project config.

Success: `VALSTRIKE_DEVICE01_NATIVE_SUCCESS: author_and_save`. Failure: `VALSTRIKE_DEVICE01_NATIVE_FAILED`. A zero editor exit code alone does not prove a Python script succeeded. Evidence is `unreal-import-report.json`.

## Fresh-process native reopen and captures

```sh
python3 Scripts/with_ue_slot.py -- "$VS_D01_EDITOR" "$VS_D01_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_D01_ROOT/Art/Source/Objectives/Device01/inspect_device01.py" \
  -VSDeviceInspectOnly -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_D01_ROOT/Saved/Logs/Device01_Inspection.log"
```

**Verified:** final native import, fresh-process reopening and all eight captures succeeded on 2026-11-03. Both native records agree with the frozen source; logs are `Saved/Logs/Device01_Import.log` and `Device01_Inspection.log`.

The capture process first loads the saved native batch without writing packages, checks exact equality with import-time mesh/material/source evidence, and writes `unreal-reopen-report.json`. It then creates an unsaved neutral studio and eight actual native captures: Idle/Armed/Defused under neutral and dim light, one carry composition and one player-eye ground view. Success markers are `VALSTRIKE_DEVICE01_NATIVE_SUCCESS: fresh_process_reopen` and `VALSTRIKE_DEVICE01_INSPECTION_SUCCESS: 8 native captures`. Check for `VALSTRIKE_DEVICE01_INSPECTION_FAILED` as well.

All frames are 1600×1000 with fixed exposure/bias 0. Neutral uses sun 3 / sky 1; dim uses sun 0.75 / sky 0.3. Bloom and motion blur are disabled. Variant framing/light settings are identical; meshes remain scale 1. Fresh pending filenames, PNG signatures/dimensions and SHA-256 prevent old captures from counting. Final images publish only when all eight complete. No map or material package is saved by capture. `unreal-inspection-report.json` records all transforms and hashes.

Studio camera looks from native+Y, corresponding to source−Y after FBX conversion. The asymmetrical checkmark verifies visible orientation. The Carry fixture places the socket at world (42,8,148)cm beneath a camera at (0,0,160), body yaw 90°/origin (42,8,140); this is a composition, not runtime attachment to a player. Player-eye camera is 160cm above the floor and 100cm away horizontally. These are editor images with no gameplay actor, hands, physics or state replication.

## Integration contract

Load mesh `/Game/Art/Objectives/Device01/Meshes/SM_VS_Device01`. Bounds are (-15,-9,0)..(15,9,10)cm; bottom-center pivot, +X forward / +Z up, scale 1. Runtime owns floor placement/collision; visual components must keep `NoCollision`. Exactly one socket `Carry` at (0,0,8), identity rotation/scale. To place Carry on a parent anchor, account for that 8 cm local offset instead of treating the bottom pivot as the hand anchor.

Material slots are literally and exactly ordered:

| Index | Slot | Default asset |
| --- | --- | --- |
| 0 | Shell | `/Game/Art/Objectives/Device01/Materials/M_VS_D01_Shell` |
| 1 | Trim | `/Game/Art/Objectives/Device01/Materials/M_VS_D01_Trim` |
| 2 | Status | `/Game/Art/Objectives/Device01/Materials/MI_VS_D01_StatusIdle` |

Change only slot 2 to `MI_VS_D01_StatusArmed` or `MI_VS_D01_StatusDefused` at the same Materials path. Those instances change both `GlyphMask` and `GlyphColor`, so status is not color-only. Parent `M_VS_D01_Status` mixes dark background and glyph; a constant 0.15 emissive contribution is confined to the glyph. No pulse/countdown/VFX behavior exists. Runtime selects these from its authoritative replicated state and supplies textual/interaction HUD cues; art selection does not establish game state.

All referenced materials and masks must be included by runtime hard/soft references for cooking; a string-only path in documentation does not guarantee packaging. No shared hook or gameplay implementation was added here. The runtime's optional mesh setting and primitive fallback remain the integration seam. Coordinator reviews the local commit and handles publication/merge.
