# ART-03 reproduction and native inspection

Run from `/Users/webproduktion3/Documents/ValStrike/.asset-worktrees/pistol-weapon-finish` on `codex/pistol-weapon-finish`. Pinned tools: Blender 5.2.2 LTS (`d13f752e3b9c`), Unreal 5.8.4 (`58902052`), Xcode 26.1.1 (`17B100`). Use only this checkout and fresh editor processes. Keep native asset authoring in Unreal Editor.

```sh
VS_ART_ROOT="$PWD"
VS_ART_EDITOR="/Users/webproduktion3/UE_5.8/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
mkdir -p Saved/Logs
python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac
```

The shared helper passes `-WaitMutex`. This worktree's build succeeded in 22.29 seconds; see ignored `Saved/Logs/ART03_EditorBuild.log`. It compiles the Mac Editor target, without cooking or packaging.

## Source authoring

```sh
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 4 --python-exit-code 1 \
  --python Art/Source/Weapons/Pistol01/Models/build_pistol01.py
/Applications/Blender.app/Contents/MacOS/Blender --background --factory-startup --threads 4 --python-exit-code 1 \
  --python Art/Source/Weapons/Rifle01/Models/update_finish.py
```

Pistol generation replaces its generated `.blend`, FBXs, validation JSON and source previews; preserve hand edits first. Rifle `update_finish.py` changes only materials in the existing saved model and exports, fingerprints all canonical/editable geometry before/after, saves and reopens, and asserts actual material constants. It does not remodel the rifle. Both commands support `-- --skip-render`. For later pistol palette-only updates use `build_pistol01.py -- --update-finish`; that path preserves/fingerprints saved geometry instead of rebuilding it. Pistol and original rifle `build_*` scripts support `-- --render-only` for rendering saved scenes without changing exports.

The shared palette is `Art/Source/Weapons/Shared/weapon_palette.json`. Hex values are sRGB, decoded to linear with `v/12.92` below 0.04045 and `((v+0.055)/1.055)^2.4` otherwise. Default Crimson is persisted in Blender, FBX and native materials. Ice/Ember are comparison variants and use transient Unreal materials, reproducible from the same JSON.

Source validation checks closed component shells, positive volumes, nondegenerate mesh/UV triangles, identity canonical transforms, two UV sets, real FBX round trips, material slot order and actual serialized shader values. Simple per-face UV tiles are not texture-density or baked-lighting acceptance.

## Native initial import or explicit refresh

The command below creates an absent pistol batch and revises the four existing rifle materials. It refuses a partial or existing pistol batch by default.

```sh
"$VS_ART_EDITOR" "$VS_ART_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_ART_ROOT/Art/Source/Weapons/Pistol01/import_weapon_finish.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_ART_ROOT/Saved/Logs/ART03_Import.log"
```

For a deliberately regenerated pistol source, add `-VSRefreshPistolMeshes`. That option requires the complete prior pistol batch, refreshes only its three meshes/materials, and preserves/rechecks sockets. It was used for the inspected trigger-guard correction. Rifle native meshes are never reimported: their SHA-256 hashes must match the original ART-02 binaries, and every native geometry/socket/collision record must match the original report. Native rifle source FBX material payloads changed, while native mesh geometry did not; the new finish report records that distinction.

Success marker: `VALSTRIKE_ART03_NATIVE_SUCCESS`. Failure marker: `VALSTRIKE_ART03_NATIVE_FAILED`. A zero Editor exit code alone is insufficient. Outputs are `unreal-import-report.json` here and `../rifle01/unreal-finish-report.json`. Original ART-02 reports remain historical provenance.

## Fresh-process reopening

```sh
"$VS_ART_EDITOR" "$VS_ART_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_ART_ROOT/Art/Source/Weapons/Pistol01/import_weapon_finish.py" \
  -VSInspectOnly -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_ART_ROOT/Saved/Logs/ART03_Reopen.log"
```

This does not save native packages. Geometry, slots, sockets, collision, UVs, source hashes, palette hash and actual native linear BaseColor/Roughness/Metallic must exactly match the authoring evidence. Emissive must be unconnected. Reports: `unreal-import-reopen-report.json` and `../rifle01/unreal-finish-reopen-report.json`.

## Actual native image comparisons

```sh
"$VS_ART_EDITOR" "$VS_ART_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_ART_ROOT/Art/Source/Weapons/Pistol01/inspect_weapon_finishes.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_ART_ROOT/Saved/Logs/ART03_Inspection.log"
```

Ordinary rendering is required. The script borrows the original rifle's transient studio setup, creates no saved maps/material variants, attaches the actual pistol Slide/Magazine to their native sockets, and produces ten 1600×1000 captures. Each weapon has identical studio camera/light settings across three material variants; pistol framing is closer than rifle framing, so comparison rows are not a common size scale. The default is also captured from a first-person composition under neutral (sun 3 / sky 1) and dim (sun 0.75 / sky 0.3) light, with fixed exposure/bias 0. All components use scale 1 and NoCollision.

All captures use fresh filenames, checked PNG signatures/dimensions/hashes, and publish only after the full sequence succeeds. Marker: `VALSTRIKE_ART03_INSPECTION_SUCCESS`; inspect `unreal-inspection-report.json` for cameras, lighting, material sources and attachment errors. `Previews/Weapon_Finish_Comparison.html` displays the six studio frames compactly; each source image remains unmodified.

These are native editor compositions, not gameplay captures. No arms, animation, runtime camera attachment, firing, reload mechanics, multiplayer, packaged build or performance acceptance is implied. No saved gameplay map or config is changed.
