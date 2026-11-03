# Site01 / Switchyard handoff

Updated: 2026-11-03 (Europe/Berlin). Branch: `codex/tactical-map`. Independent **PLAY-01 prototype**; full acceptance requires integrated rules and a human 2v2 playtest.

Switchyard is an original, flat industrial courtyard on a 52 × 42 m floor. The relay house separates the Gallery and Transformer approaches. An outer service flank trades a longer journey for access behind the defender north exit. Full-height screens shelter four spawn positions; an amber 8 × 10 m area marks the single site. Matte plaster, teal kit pieces, broad color areas and restrained trim preserve the requested stylized treatment.

## Review files

All paths below are relative to the assigned checkout, `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/tactical-map`.

| Item | Exact path |
| --- | --- |
| Native map | `Content/Maps/Site01/VS_Site01.umap` → `/Game/Maps/Site01/VS_Site01` |
| Map-local materials | `Content/Maps/Site01/Materials/M_Site01_Paving.uasset`, `M_Site01_Lane.uasset`, `M_Site01_Site.uasset` |
| Editable geometry, routes and viewpoints | `Art/Source/Maps/Site01/design.py` |
| Native authoring script | `Art/Source/Maps/Site01/build_site01.py` |
| Saved-actor and collision inspection | `Art/Source/Maps/Site01/inspect_site01.py` |
| Scripted PIE traversals | `Art/Source/Maps/Site01/traverse_site01.py` |
| Pure-Python layout checks | `Tests/Maps/Site01/test_layout.py` |
| Diagram generator | `Art/Source/Maps/Site01/draw_plan.py` |
| Version-1 integration metadata | [layout.json](layout.json) |
| Measured authoring diagram | [route-plan.svg](route-plan.svg) |
| Stream record | `docs/workstreams/tactical-map.md` |

The architecture and constant-color map materials are original for this task. Shared walls, pillars, cover, crates, vent and four materials come unchanged from environment-kit commit `8acb0d4`, under `Content/Art/Environment/`; their provenance is recorded in `docs/assets/MANIFEST.md`. Native engine cube geometry supplies simple architectural masses and presentation surfaces. No new external art or reference-map geometry was imported. Shared materials, source meshes, `VS_Graybox`, configuration and gameplay source remain outside this change.

## Authored integration contract

`layout.json` uses schema version 1 and centimeter coordinates. The four real PlayerStarts have these intended **Actor Tags**, not `PlayerStartTag` values:

| Actor label | Role Actor Tag | Location cm | Yaw |
| --- | --- | --- | ---: |
| `VS_ATK_1` | `VS_AttackerSpawn` | `[-2250, -250, 100]` | 90° |
| `VS_ATK_2` | `VS_AttackerSpawn` | `[-2250, 250, 100]` | −90° |
| `VS_DEF_1` | `VS_DefenderSpawn` | `[2200, -250, 100]` | −90° |
| `VS_DEF_2` | `VS_DefenderSpawn` | `[2200, 250, 100]` | 90° |

Each start has exactly one role tag. Roles are spawn-side assignments; they are not persistent team identities, and must swap after round three through the later adapter. The current `VSGameMode` ignores these tags and chooses an unobstructed start far from existing pawns. This map does not implement team-specific spawning, preparation barriers, planting or defusing.

`VS_Site_A` is a nonblocking TriggerBox tagged `VS_Site_A`, centered at `[1050, 0, 100]` with half-extents `[400, 500, 100]`. The decorative amber pad is only presentation; eligibility must use the exported bounds. `VS_DeviceRecovery` is a nonblocking TargetPoint with the same-named tag at `[1050, 0, 20]`. All authored routes end at its ground-plane location. The playable AABB is centered at `[0, 0, 300]`, half-extents `[2550, 2050, 300]`; it includes solid buildings and cover, so bounds membership alone cannot establish safe recovery. Integration must also validate floor support, collision and reachability.

## Distances, clearance and balance questions

These are **calculated authoring measurements**, derived from the route polylines in `design.py`. Estimates divide distance by a constant 450 cm/s; they omit acceleration, cornering, peeking, pauses and combat. They are not observed traversal times.

| Route, spawn to site center | Polyline distance | Estimated at 450 cm/s | Minimum centerline distance to an authored obstacle |
| --- | ---: | ---: | ---: |
| Gallery | 46.073 m | 10.238 s | 150.0 cm |
| Transformer | 55.302 m | 12.289 s | 115.1 cm |
| Service flank | 79.982 m | 17.774 s | 241.7 cm |
| Defender north | 23.701 m | 5.267 s | 223.6 cm |
| Defender south | 24.385 m | 5.419 s | 250.0 cm |

The narrowest sampled route clearance is on the Transformer route near `SouthHalfCover`: 115.1 cm from centerline to obstacle, leaving about 81.1 cm beyond the 34 cm capsule radius. The pure-Python test requires at least 44 cm throughout each segment: radius plus a 10 cm authoring margin. It evaluates full obstacle envelopes; native collision sweeps and actual movement remain separate evidence.

Selected tight architectural gaps are 240 cm between the east site stack and defender screen, 250 cm between west site cover and the relay house, and 250 cm between the north crate and service wall. The attacker screen–relay gap is 450 cm; the Gallery's relay–service separation is 550 cm before local cover; the outer service corridor is 550 cm deep. These are authored surface-to-surface dimensions, not a claim that every possible player path has been measured. Standing/crouched capsule heights are 176/112 cm, diameter 68 cm; the authored routes are open overhead and require neither crouch nor jumping. Kit half-cover is 120 cm tall, intended to obscure the 88 cm crouched eye while exposing the 152 cm standing eye.

The flank contains a **29 m uninterrupted straight segment** from `[-1000,1750]` to `[1900,1750]`. Its long exposure, holding advantage and usefulness for a two-person attacking team need human balance testing. Defender estimated access is substantially shorter than attacker access; that creates setup time but is not evidence of balanced first contact. C1/C2 in the diagram mark intended contact lanes, not observed encounters. Designed sightline segments are Gallery 18.5 m, Transformer exit 14.009 m, and service flank 29 m; native Visibility-ray results are recorded separately in editor-report.json. Spawn screens have no timed protection once players leave their pockets.

## Verification evidence

Final generation: `Saved/Logs/Site01Build3.log`, 77 saved/reopened actors. Final native inspection: `Saved/Logs/Site01Inspection3.log`, process exit 0, all 486 checks and eight captures passed. Inspection run `a8930f9acfff4e4b9e819b42d8b36bf8`; map SHA-256 `6798356d8d029112a0dabfa3a5fa2789eebeb79b114959836070801bb989f3aa`. Report source and image hashes were checked against the delivered files.

| Evidence | Result |
| --- | --- |
| Pure-Python tests | **7/7 passed**: contract, role counts, route footprint clearance, spawn/recovery support, opposing-spawn eye sightlines, connectivity and estimate labeling |
| Route diagram | Generated from current design and visually inspected; labels and calibrated scale checked |
| Editor build | **Passed**, 22.35 s via `Scripts/ue.py`, UE 5.8.4-58902052 / Xcode 26.1.1; Editor target only |
| Saved native map generation/reopen | **Passed**, 77 actors; `generation-report.json`; Map Check 0 errors / 0 warnings |
| Fresh-process metadata, Pawn sweeps, support and Visibility queries | **486/486 passed**, `editor-report.json`: four spawns, marker bounds/tags, both-direction standing/crouched route sweeps, floor probes, spawn occlusion, cover blocking, three clear sightlines |
| Native previews | **Eight captured** at 1600 × 1000; actual editor views with fixed exposure, no image generation or retouching |
| Scripted single-player PIE movement | **6/6 passed**, null rendering, process exit 0; `runtime-report.json`, `Saved/Logs/Site01Runtime7.log` |
| Human playtest, multiplayer, packaged client/server, cross-platform, performance | Unverified |

The native preview paths under `docs/maps/site01/previews/` are `01_attacker_exit.png`, `02_gallery_contact.png`, `03_transformer_contact.png`, `04_site_a.png`, `05_service_flank.png`, `06_defender_exit.png`, `07_overview.png`, and `08_transformer_exit.png`. Views 01–06 and 08 use actual Unreal editor cameras at standing-eye height; view 07 is a perspective overview. View 03 shows the protected Transformer approach; view 08 shows the exposed exit beyond the island. These are geometry/presentation captures, not player-controlled runtime evidence. Do not reuse earlier images as proof of a revised map.

Visual review of all eight final images found no obvious missing faces, clipping or z-fighting in the stills. This is sparse prototype architecture, not finished environment art. The service turn/rejoin needs clearer wayfinding; dark teal/trim value separation and player silhouettes need actual characters and human testing. The black area outside the overview is the empty scene background.

The traversal harness uses the real VSCharacter/CharacterMovement in **single-player PIE**, with scripted `AddMovementInput`, one initial test teleport per route, a 40 cm waypoint tolerance and a bounded timeout. Its observed times are world simulation seconds, kept distinct from the constant-speed estimates above. It does not test network replication, keyboard/mouse feel, human route choice or competitive balance.

Final movement run `b800475731ab40dbb738380444594470` completed in 80.03 wall seconds and exited cleanly. Its map/design/script hashes match the delivered files. Observed traversals (simulation time and actual sampled XY distance):

| Route / stance | Observed seconds | Observed distance |
| --- | ---: | ---: |
| north_main / standing | 10.198 s | 45.363 m |
| south_main / standing | 12.193 s | 54.293 m |
| north_flank / standing | 17.648 s | 78.829 m |
| defender_north / standing | 5.229 s | 23.002 m |
| defender_south / standing | 5.384 s | 23.693 m |
| north_main / crouched | 25.132 s | 45.164 m |

Paths are slightly shorter than the authored polylines because the harness accepts a waypoint within 40 cm and cuts corners. These are single-run measurements, not calibrated human timings. Standing/crouched capsule centers stayed at 90.15/58.15 cm respectively (the normal 2.15 cm floor gap); no route required jumping or left the flat floor. Full-map jump/climb/exploit coverage is unverified.

Earlier attempts exposed Python API/export mismatches and a final-report callback problem; an earlier rendered PIE attempt ended with a `GameViewport.IsUnique()` teardown assertion. The final null-rendered run saved its complete evidence and exited zero after separating PIE teardown from editor exit. Rendered PIE acceptance is still open. Engine startup also emits the previously known two automation-condition errors and absent Windows/Linux SDK diagnostics; they are not map-check failures.


## Reproduce in this checkout

Use Unreal **5.8.4** and selected Xcode **26.1.1**. Start fresh processes one at a time; retain their individual logs and close only processes launched for this task. The shared helper supplies the existing build mutex. No launcher or configuration changes are needed.

```sh
VS_SITE_ROOT="/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/tactical-map"
VS_ENGINE_ROOT="/Users/webproduktion3/UE_5.8/UE_5.8"
VS_SITE_EDITOR="$VS_ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
cd "$VS_SITE_ROOT"
mkdir -p "$VS_SITE_ROOT/Saved/Logs"

python3 Scripts/ue.py --engine "$VS_ENGINE_ROOT" build --target Editor --platform Mac \
  > "$VS_SITE_ROOT/Saved/Logs/Site01Compile.log" 2>&1

# Fresh generation only: the script refuses an existing Site01 map.
"$VS_SITE_EDITOR" "$VS_SITE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_SITE_ROOT/Art/Source/Maps/Site01/build_site01.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_SITE_ROOT/Saved/Logs/Site01Generate.log"

# Reopen the saved map, inspect native metadata/collision, capture real views.
"$VS_SITE_EDITOR" "$VS_SITE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_SITE_ROOT/Art/Source/Maps/Site01/inspect_site01.py" \
  -unattended -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_SITE_ROOT/Saved/Logs/Site01Inspect.log"

# Separate bounded single-player PIE traversal run.
"$VS_SITE_EDITOR" "$VS_SITE_ROOT/ValStrike.uproject" /Engine/Maps/Entry \
  "-ExecutePythonScript=$VS_SITE_ROOT/Art/Source/Maps/Site01/traverse_site01.py" \
  "-ini:EditorPerProjectUserSettings:[/Script/UnrealEd.LevelEditorPlaySettings]:PlayNetMode=PIE_Standalone" \
  -unattended -nosound -nullrhi -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_SITE_ROOT/Saved/Logs/Site01Traverse.log"

python3 Art/Source/Maps/Site01/draw_plan.py
python3 -m unittest discover -s Tests/Maps/Site01 -p 'test_*.py' -v
```

For a deliberate regeneration after reviewing an existing map, add `-VSSiteRebuild` to the generation invocation. It deletes/recreates the Site01 map through Unreal and discards manual map edits; map-local materials already present are reused. Normal review should reopen the saved map without regenerating it. Keep ordinary rendering for inspection. The scripted movement run uses null rendering; do not use it as rendered gameplay evidence. Editor exit zero alone is insufficient: require process exit zero, the corresponding `VALSTRIKE_SITE01_BUILD_SUCCESS`, `VALSTRIKE_SITE01_INSPECTION_SUCCESS` or `VALSTRIKE_SITE01_RUNTIME_SUCCESS` marker, complete reports, current hashes and no failure marker.

For a manual development view, select the new map explicitly:

```sh
"$VS_SITE_EDITOR" "$VS_SITE_ROOT/ValStrike.uproject" /Game/Maps/Site01/VS_Site01 \
  -game -windowed -ResX=1280 -ResY=720 -nosound -DisablePlugins=AndroidFileServer \
  "-abslog=$VS_SITE_ROOT/Saved/Logs/Site01Manual.log"
```

This uses the Editor executable, not a packaged client. Baseline spawn selection remains role-agnostic.

## Integration next steps

Review the committed native assets and final evidence together, then merge this branch through the integration chat. Preserve the environment-kit dependency and leave `VS_Graybox` as the regression arena. Wire the version-1 role/site/recovery metadata into the server adapter, validate authoritative eligibility/recovery, and verify role swapping. Run integrated server/client movement, cover/weapon traces, round resets and objective tests on Site01. Then run a human 2v2 session covering spawn exits, first-contact timing, the exposed flank, crossfires, readability and recovery behavior. Packaged, cross-machine/platform and performance acceptance remain distinct gates. PLAY-01 stays in progress until its human-playtest completion condition is met.
