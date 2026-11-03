# R01 Kestrel authoring and Unreal inspection

This workflow produces one visual rifle body and a separate magazine for the first ValStrike gun asset. It uses **Blender 5.2.2 LTS**, **Unreal Engine 5.8.4**, and **Xcode 26.1.1**. It creates static visual assets and attachment sockets; firing, reload gameplay, recoil, damage, a skeletal rig, arms, and animation are separate work.

Historical ART-02 verification on 2026-11-03 (Europe/Berlin): the isolated Mac Editor build passed in **22.46 seconds**. Source/FBX checks, final native import, fresh-process reopening, socket and magazine-attachment checks, two native captures, and visual inspection passed for the original cream/teal finish. Gameplay and animation remain separate acceptance work.

**ART-03 superseding workflow:** the current Mac Editor build passed in
**22.29 seconds**. Source migration, real FBX material readback, saved Blender
reopening, final native material authoring and fresh-process native reopening
passed. Rifle mesh packages remain byte-identical. All ten combined native
captures completed and were visually inspected, including neutral/dim
first-person views. Current logs are `ART03_ReadableFinishImport.log`,
`ART03_Reopen.log`, and `ART03_FinalInspection.log` under ignored `Saved/Logs/`.
The original ART-02 reports in this rifle folder remain historical evidence;
use the new finish reports for current material values/source hashes. Dim
charcoal detail loss remains documented in the current manifest.

## Paths and prerequisites

The current asset branch is `codex/pistol-weapon-finish`. The original completed
rifle remains on `codex/rifle-asset` at `cac8151` as a comparison reference.
Run all current commands from the assigned isolated checkout:

```sh
cd /Users/webproduktion3/Documents/ValStrike/.asset-worktrees/pistol-weapon-finish

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

## ART-03 material-only source migration

The current authority is
[weapon_palette.json](../../../Art/Source/Weapons/Shared/weapon_palette.json).
It maps the existing Graphite, Teal, Ceramic and Accent material names to
Structure, Gunmetal, Steel and Accent roles. It defines the saved crimson
default plus ice-blue and copper/ember comparison variants. The source and
native scripts decode sRGB to linear; hex values are not linear constants.

```sh
/Applications/Blender.app/Contents/MacOS/Blender \
  --background --factory-startup --python-exit-code 1 \
  --python "$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/Models/update_finish.py" \
  -- --skip-render
```

This loads the existing saved rifle rather than rebuilding its geometry. It
updates its four material values and FBX exports, then checks actual imported
material names, linear Base Color, Roughness and Metallic. It saves and reopens
the `.blend`, compares geometry/UV/material-index/transform fingerprints and
reads back the saved shader and viewport values. Evidence is written to
`Models/mesh_validation.json` and `Models/finish_validation.json`; success is
`VALSTRIKE_RIFLE_FINISH_SOURCE_SUCCESS`. Omit `--skip-render` to also regenerate
both Blender previews. The render-only command further below also works after
the saved source has been updated.

FBX hashes change with embedded material data. ART-03 deliberately keeps the
original native rifle mesh packages byte-identical and updates their four
referenced native material assets. It does not reimport or remodel the rifle.

## ART-03 native update and fresh-process verification

The combined script
[import_weapon_finish.py](../../../Art/Source/Weapons/Pistol01/import_weapon_finish.py)
requires the existing ART-02 rifle assets. Its default mode applies the rifle
finish and imports a **previously absent** P01 batch:

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Pistol01/import_weapon_finish.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/WeaponFinishImport.log"
```

For a deliberate P01 source revision after a complete P01 batch exists, use
`-VSRefreshPistolMeshes`. It refreshes only the three pistol mesh assets and
the shared-finish material values; it never reimports the rifle mesh packages.
A partial P01 batch is refused. The flag is not needed just to inspect saved
assets:

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Pistol01/import_weapon_finish.py" \
  -VSRefreshPistolMeshes -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/WeaponFinishRefresh.log"
```

Then start a **separate fresh process** for verification. Do not combine the
refresh and inspect flags:

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Pistol01/import_weapon_finish.py" \
  -VSInspectOnly -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/WeaponFinishReopen.log"
```

Inspect mode reads the saved native assets without changing their packages.
It verifies current shader constants, palette hash, bounds, triangle counts,
material slots, UVs, collisions and sockets against the preceding update.
Rifle measurements must also match the original ART-02 contract, apart from
the expected changed FBX source hash; both rifle native mesh SHA-256 values
must match the frozen originals listed in [MANIFEST.md](MANIFEST.md).

The current report pairs are:

- Rifle: `docs/assets/rifle01/unreal-finish-report.json` and
  `unreal-finish-reopen-report.json`.
- Pistol: `docs/assets/pistol01/unreal-import-report.json` and
  `unreal-import-reopen-report.json`.

Each invocation must emit `VALSTRIKE_ART03_NATIVE_SUCCESS`; inspect logs for
`VALSTRIKE_ART03_NATIVE_FAILED` or Python errors. An editor exit code of zero
alone is insufficient. Freeze source exports and the palette before the final
author/reopen pair; changing either afterward invalidates that evidence.

## ART-03 actual native comparisons and first-person captures

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Pistol01/inspect_weapon_finishes.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/WeaponFinishInspection.log"
```

The script captures ten views of actual native meshes in an unsaved editor
scene: three matched studio finishes per weapon, plus neutral and dim
first-person views of the saved crimson default. Comparison variants use
transient material overrides; default captures use the saved native materials.
Neutral lights are sun 3.0 / sky 1.0; dim lights are sun 0.75 / sky 0.3 with
fixed exposure. Rifle first-person FOV stays 85 degrees with grip offset
`(32,16,-18)` cm. P01 uses `(34,12,-14)` cm. These are art compositions without
arms, HUD, animation or gameplay.

The combined report is `docs/assets/pistol01/unreal-inspection-report.json`.
For each weapon, PNGs under its `docs/assets/<weapon>/Previews/` folder use
suffixes `Unreal_Studio`, `Unreal_Studio_ice`, `Unreal_Studio_ember`,
`Unreal_FirstPerson` and `Unreal_FirstPerson_Dim`. Success requires
`VALSTRIKE_ART03_INSPECTION_SUCCESS`, ten fresh captures with recorded hashes,
and direct visual inspection. No native packages or maps are saved by this
capture script.

## Retained ART-02 clean-import workflow

The original commands below document the accepted baseline. The original
`import_rifle.py` normal mode is for a completely absent rifle batch only;
it refuses existing assets. Its old `-VSInspectOnly` equality compares current
FBX hashes to `unreal-import-report.json` and therefore is **not** the ART-03
finish verification path. The historical `unreal-import-report.json`,
`unreal-reopen-report.json` and rifle `unreal-inspection-report.json` describe
ART-02. Do not overwrite them as part of this finish revision. The old concept
and exact prompt are preserved; current source/preview files can have the same
filenames with revised material content.

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

The existing shared helper is used unchanged and passes `-WaitMutex`. ART-02's
22.46-second build and ART-03's 22.29-second build compiled the Editor target;
neither cooked/packaged a client or dedicated server. Do not run `generate-map`
in this asset workflow.

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

## ART-02 original import/reopen equality (historical)

```sh
"$VS_EDITOR" "$VS_RIFLE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_RIFLE_ROOT/Art/Source/Weapons/Rifle01/import_rifle.py" \
  -VSInspectOnly -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_RIFLE_ROOT/Saved/Logs/RifleReopen.log"
```

This loads existing native assets without reimporting, then writes `docs/assets/rifle01/unreal-reopen-report.json`. It checks Y-reflected bounds/pivots, source/native triangle and material-slot agreement, two UV channels, disabled Nanite, collision defaults/sections, and the exact socket locations, rotations, scales, and counts.

The fresh report's asset records must exactly match the import-time report, including current FBX SHA-256 values. If source files or imported assets changed, that equality must fail; do not treat old evidence as verification of a new export. Freeze final exports before the final import/reopen pair.

## ART-02 original native inspection (historical)

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
