# Environment 01 build, import, and inspection

These commands operate in the isolated asset worktree and use **Unreal Engine 5.8.4**, **Xcode 26.1.1**, and **Blender 5.2.2 LTS**. They do not generate or modify the gameplay map. Native `.uasset` files are created only by Unreal Editor.

Verified on 2026-11-03 (Europe/Berlin): Blender source/export checks, final Unreal import, fresh-process reopening, 30 editor collision queries, and native viewport inspection passed. The Mac Editor target built in 25.79 seconds. See the manifest and asset work log for measured results and remaining gameplay checks.

## Paths and tools

Run in a shell:

```sh
cd /Users/webproduktion3/Documents/ValStrike/.asset-worktrees/environment-kit

export UE_ROOT="/Users/webproduktion3/UE_5.8/UE_5.8"
VS_ASSET_ROOT="$PWD"
VS_EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
mkdir -p "$VS_ASSET_ROOT/Saved/Logs"
```

The scripts verify the exact Unreal patch and the project root. Use a fresh editor process, with no Play In Editor session or unsaved map. The `/Engine/Maps/Entry` argument avoids the gameplay startup map. `-DisablePlugins=AndroidFileServer` disables that plugin for each process, preventing its startup from appending settings to the worktree project configuration. No project configuration change is required. Each invocation keeps ordinary rendering and writes a separate log under ignored `Saved/Logs/`.

## Rebuild modeling sources and exports

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup \
  --python-exit-code 1 \
  --python "$VS_ASSET_ROOT/Art/Source/Environment/Models/build_environment_kit.py"
```

This regenerates the editable `.blend`, five FBX files, Blender preview, and `mesh_validation.json` in the asset-owned Models directory. It writes those generated files again; review source changes before regeneration. It does not edit native Unreal imports.

The script checks exact source bounds, identity transforms, closed manifold shells, nondegenerate faces, positive aggregate volume, and FBX round-trip dimensions, pivot bounds, UV-channel count, and collision-mesh count. It reports measurements instead of treating the preview as geometry evidence. UV layouts are basic isolated face tiles; no textures are required.

## Build the project Editor target

```sh
python3 Scripts/ue.py --engine "$UE_ROOT" build --target Editor --platform Mac
```

This uses the existing shared helper unchanged. It compiles the project Editor target; it does not cook or package a game. Do not run `generate-map` from this asset workflow.

## Import a fresh asset batch

```sh
"$VS_EDITOR" "$VS_ASSET_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_ASSET_ROOT/Art/Source/Environment/import_environment.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_ASSET_ROOT/Saved/Logs/AssetImport.log"
```

The importer creates four shared materials and five static meshes under `/Game/Art/Environment/`. It disables Interchange FBX for this process and uses the native legacy FBX importer with scene/unit conversion, uniform scale 1, imported normals, no generated textures, no automatic collision, and no Nanite. Existing UCX hulls supply the four solid pieces' collision. The vent has no collision, including triangle sections.

**The importer refuses to overwrite any existing batch asset.** A partial import must be inspected before a deliberate retry. It does not delete or silently replace existing assets; it never writes binary asset bytes directly. An existing complete batch should use the inspection-only command below.

Success produces `docs/assets/unreal-import-report.json` and the log marker `VALSTRIKE_ASSET_SUCCESS`. An Unreal process exiting with code zero alone is insufficient: check the marker, report, and absence of `VALSTRIKE_ASSET_FAILED`.

## Reopen saved assets in a fresh process

```sh
"$VS_EDITOR" "$VS_ASSET_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_ASSET_ROOT/Art/Source/Environment/import_environment.py" \
  -VSInspectOnly -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_ASSET_ROOT/Saved/Logs/AssetReopen.log"
```

This loads the saved assets without reimporting and produces `docs/assets/unreal-reopen-report.json`. It checks dimensions, base pivots, material assignment, UV channels, native collision settings, and disabled Nanite. It compares source triangle/material counts with the modeling report and requires the reopened measurements and current FBX SHA-256 hashes to equal the import-time report.

Blender front is −Y; Unreal's coordinate conversion reflects Y, making the native front +Y. Architecture remains based at a corner: minimum X / maximum Y / minimum Z. Props remain at bottom center. See [MANIFEST.md](MANIFEST.md) for exact dimensions and material values.

## Native viewport and collision-query inspection

```sh
"$VS_EDITOR" "$VS_ASSET_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_ASSET_ROOT/Art/Source/Environment/inspect_environment.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_ASSET_ROOT/Saved/Logs/AssetInspection.log"
```

Use ordinary rendering; do not add `-nullrhi` to this command. The script creates a fresh **unsaved** inspection world, places the five meshes at scale 1, uses one directional sun, captured-sky fill, and fixed exposure, runs editor visibility traces, and captures a 1600 × 1000 viewport PNG. No map package is saved. Preview floor, lights, and camera are temporary inspection aids.

Successful outputs:

- `docs/assets/Previews/VS_Environment01_Unreal.png`
- `docs/assets/unreal-inspection-report.json`
- Log marker `VALSTRIKE_INSPECTION_SUCCESS`

The screenshot must also be visually inspected for all five pieces, surface artifacts, material readability, and framing. The script uses a unique pending filename, so a stale PNG alone cannot satisfy a new capture. Check for `VALSTRIKE_INSPECTION_FAILED` and inspect logs if outputs are absent.

The script tests simple and complex editor visibility traces through each asset's center with its intended placement collision profile. Four solid assets also receive five front-face probes each (center and four corners inset 2 cm), checking the hit actor and impact depth within 0.2 cm of the intended envelope. These checks supplement the saved native collision settings; they do not establish every hull face, player movement behavior, sightline fairness, or runtime multiplayer collision.

## Review and integration limits

Keep `Content/Maps/VS_Graybox.umap`, bootstrap materials, gameplay source, configuration, and shared setup scripts outside this asset change. Do not mark `PLAY-02` complete from these imports. The first batch is ready for integration review only when final exports, saved native assets, fresh-process evidence, and native visual inspection agree.

Record remaining defects and the next action in the dedicated [asset work log](WORK_LOG.md). Source/export provenance and artifact classes are in [MANIFEST.md](MANIFEST.md). Main-workstream gameplay placement, movement, sightlines, runtime verification, packaged builds, and performance remain separate acceptance gates.
