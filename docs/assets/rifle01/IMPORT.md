# R01 Kestrel authoring and Unreal inspection

This workflow produces one visual rifle body and a separate magazine for the first ValStrike gun asset. It uses **Blender 5.2.2 LTS**, **Unreal Engine 5.8.4**, and **Xcode 26.1.1**. It creates static visual assets and attachment sockets; firing, reload gameplay, recoil, damage, a skeletal rig, arms, and animation are separate work.

Verified on 2026-11-03 (Europe/Berlin): the isolated Mac Editor build passed in **22.46 seconds**. Source/FBX checks, final native import, fresh-process reopening, socket and magazine-attachment checks, two native captures, and visual inspection passed. Gameplay and animation remain separate acceptance work.

## Paths and prerequisites

The asset branch is `codex/rifle-asset`, based on `d076442`. Run from its isolated worktree:

```sh
cd /Users/webproduktion3/Documents/ValStrike/.asset-worktrees/rifle-asset

export UE_ROOT="/Users/webproduktion3/UE_5.8/UE_5.8"
VS_RIFLE_ROOT="$PWD"
VS_EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
mkdir -p "$VS_RIFLE_ROOT/Saved/Logs"

xcode-select -p
xcodebuild -version
/Applications/Blender.app/Contents/MacOS/Blender --version
```

The selected developer directory is `/Applications/Xcode.app/Contents/Developer`; `xcodebuild -version` must report Xcode `26.1.1`. The engine is pinned to `5.8.4-58902052`; do not silently substitute another patch. The source script is [build_rifle01.py](../../../Art/Source/Weapons/Rifle01/Models/build_rifle01.py).

Use fresh editor processes with no Play In Editor session or unsaved map. `/Engine/Maps/Entry` avoids the gameplay startup map. `-DisablePlugins=AndroidFileServer` disables that plugin per process so it does not append project configuration. Each command keeps ordinary rendering, disables audio, and writes a separate log under ignored `Saved/Logs/`.

## Build or regenerate source models

```sh
/Applications/Blender.app/Contents/MacOS/Blender \
  --background --factory-startup --python-exit-code 1 \
  --python "$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/Models/build_rifle01.py"
```

The authoring script regenerates these files under `Art/Source/Weapons/Rifle01/Models/`:

- `R01_Kestrel.blend`: editable canonical meshes plus a separate preview composition.
- `Exports/SM_VS_Rifle01_Body.fbx` and `Exports/SM_VS_Rifle01_Magazine.fbx`: real exported meshes, with no collision geometry.
- `mesh_validation.json`: source and FBX round-trip measurements, material slots, palette, socket locations, and assembly metadata.
- `Previews/R01_Kestrel_Actual_Meshes.png` and `Previews/R01_Kestrel_FirstPerson_Composition.png`: Blender renders of actual geometry.

Regeneration writes the generated modeling/export files again. Review source changes first; it does not update existing native Unreal assets. A successful full run emits `VALSTRIKE_R01_MESH_SUCCESS`; `--python-exit-code 1` ensures Python failures fail the process.

To render only from the saved `.blend`, without rewriting the model, exports, or source-validation report:

```sh
/Applications/Blender.app/Contents/MacOS/Blender \
  --background --factory-startup --python-exit-code 1 \
  --python "$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/Models/build_rifle01.py" \
  -- --render-only
```

That path emits `VALSTRIKE_R01_PREVIEWS_SUCCESS`. It still rewrites the two preview PNGs. A Blender first-person composition is an art review camera; it is not an in-game weapon implementation.

## Build the Mac Editor target

```sh
python3 Scripts/ue.py --engine "$UE_ROOT" build --target Editor --platform Mac
```

The existing shared helper is used unchanged. The verified 22.46-second build compiled the Editor target; it did not cook/package a client or dedicated server. Do not run `generate-map` in this asset workflow.

## Import a fresh native rifle batch

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/import_rifle.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/RifleImport.log"
```

[import_rifle.py](../../../Art/Source/Weapons/Rifle01/import_rifle.py) reads `Models/mesh_validation.json`, validates the two expected exports and source socket locations, creates four palette materials, and imports:

| Asset | Native package |
| --- | --- |
| Rifle body | `/Game/Art/Weapons/Rifle01/Meshes/SM_VS_Rifle01_Body` |
| Separate magazine | `/Game/Art/Weapons/Rifle01/Meshes/SM_VS_Rifle01_Magazine` |
| Four shared rifle materials | `/Game/Art/Weapons/Rifle01/Materials/` |

The importer uses the native legacy FBX path with centimeter scene/unit conversion and imported normals. It creates no textures or animations, disables Nanite, and removes simple/convex collision. Both meshes have `NoCollision` defaults and all triangle sections have collision disabled. They are visual attachment meshes, not authoritative shot or hit geometry.

The importer creates four named sockets on the body:

| Socket | Location in cm (X, Y, Z) | Rotation | Scale |
| --- | --- | --- | --- |
| `Grip` | (0, 0, 0) | (0, 0, 0) | (1, 1, 1) |
| `Muzzle` | (56, 0, 8.2) | (0, 0, 0) | (1, 1, 1) |
| `SupportHand` | (31, 0, 2.4) | (0, 0, 0) | (1, 1, 1) |
| `Magazine` | (14, 0, 2) | (0, 0, 0) | (1, 1, 1) |

Coordinates use **+X forward, +Z up, centimeters**. FBX-to-Unreal conversion reflects source Y; all socket Y values are zero. The body root is the Grip attachment at local zero. The magazine root is its insertion anchor at local zero; assemble it at the body's Magazine socket, with zero rotation and unit scale. These are presentation attachment locations, not a verified animation rig.

**Existing native assets are never overwritten.** The importer refuses a complete or partial existing rifle batch. Inspect partial output before any deliberate retry; do not silently delete or replace assets. All `.uasset` files must be authored by Unreal Editor.

Success writes `docs/assets/rifle01/unreal-import-report.json` and logs `VALSTRIKE_RIFLE_IMPORT_SUCCESS`. Check for `VALSTRIKE_RIFLE_IMPORT_FAILED`; a zero editor exit code alone does not prove that a Python import succeeded.

## Reopen saved assets in a fresh process

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/import_rifle.py" \
  -VSInspectOnly -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/RifleReopen.log"
```

This loads existing native assets without reimporting, then writes `docs/assets/rifle01/unreal-reopen-report.json`. It checks Y-reflected bounds/pivots, source/native triangle and material-slot agreement, two UV channels, disabled Nanite, collision defaults/sections, and the exact socket locations, rotations, scales, and counts.

The fresh report's asset records must exactly match the import-time report, including current FBX SHA-256 values. If source files or imported assets changed, that equality must fail; do not treat old evidence as verification of a new export. Freeze final exports before the final import/reopen pair.

## Native studio and first-person composition inspection

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/inspect_rifle.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/RifleInspection.log"
```

The native inspection script was executed successfully. Use ordinary rendering, without `-nullrhi`. It creates two native editor compositions—studio and first-person—with the saved body and actual magazine actor attached at the verified Magazine socket. It checks unit scale, attachment error below 0.01 cm, Muzzle +X orientation, required sockets, and equipped-component `NoCollision` profiles. No map or asset package is saved and no gameplay source or project configuration is changed.

Successful outputs:

- `docs/assets/rifle01/Previews/VS_Rifle01_Unreal_Studio.png` (1600 × 1000).
- `docs/assets/rifle01/Previews/VS_Rifle01_Unreal_FirstPerson.png` (1600 × 1000).
- `docs/assets/rifle01/unreal-inspection-report.json`.
- Log marker `VALSTRIKE_RIFLE_INSPECTION_SUCCESS`.

Check for `VALSTRIKE_RIFLE_INSPECTION_FAILED`. The script uses unique pending captures, checks PNG signatures and resolution, and replaces final evidence only after both captures succeed. The report records image hashes. Its first-person camera is an optical composition with horizontal FOV 85°, looking +X, and Grip offset (32, 16, −18) cm; it includes no arms or HUD.

Visually inspect the final images for complete silhouettes, material readability, visible surface artifacts, correctly seated magazine, open trigger guard, sight visibility, and first-person framing. A first-person editor composition does not verify camera attachment during play, weapon sway, animation, recoil, reload, firing, or collision.

## Review and integration limits

The asset work stays under `Art/Source/Weapons/Rifle01/`, `Content/Art/Weapons/Rifle01/`, and `docs/assets/rifle01/`. Gameplay source, configuration, shared scripts, bootstrap materials, and maps belong to the gameplay workstream.

See [MANIFEST.md](MANIFEST.md) for task status, final measurements, provenance, previews, and remaining defects. This visual increment does not complete `GUN-01` or any networked-gunplay milestone. Runtime attachment, animation, server weapon validation, damage, audio, packaged builds, multiplayer, and performance remain unverified.
