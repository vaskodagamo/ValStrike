# Objective-device art workstream

Append-only ART-04 evidence; dates use Europe/Berlin. Coordinator owns central records, publication and gameplay integration.

## 2026-11-03 — Contract and implementation start

**Status:** ART-04 in progress. Assigned checkout `/Users/webproduktion3/Documents/ValStrike/.parallel-worktrees/objective-device`, branch `codex/objective-device`, base `a520ff20fba6bb9a94d0fc6ae2c59acff3d94606`. Prior pistol/rifle branch and checkout left intact.

**Scope:** One original compact D01 Relay device. Fixed bounds target (-15,-9,0) to (15,9,10) cm, +X forward/+Z up, bottom-center pivot, scale1; one `Carry` socket at (0,0,8), identity rotation/scale; ordered literal slots Shell, Trim, Status; equipped visual NoCollision; fewer than roughly4,000 triangles. All changes restricted to Device01 source/native/docs and this log.

**Design:** Chamfered slate equipment shell, cool-steel protective landmarks, small status surface. Idle ring/bar, armed warning symbol and defused check provide shape cues as well as restrained colors. Procedural glyph masks are original graphics, with no font, downloaded texture or external asset dependency. No animation, VFX system, mechanism or gameplay actor added.

**Workflow:** Source model/export and assertions in Blender 5.2.2; native assets authored only via UE5.8.4 Editor with Xcode26.1.1. Every Unreal build/import/render runs through `Scripts/with_ue_slot.py`; exit75 defers engine work while lightweight preparation continues. No network port is needed for asset-only editor work; reserved multiplayer ranges stay untouched.

**Next action:** Generate and inspect source, compile this checkout under the engine slot, import/verify native mesh and state materials, capture neutral/dim views, reopen in a fresh process and hand off a local commit.

## 2026-11-03 — Build and source checkpoint

**Build:** Pinned Mac Editor target succeeded in44.86s under the required `with_ue_slot.py` wrapper. Exact command: `python3 Scripts/with_ue_slot.py -- python3 Scripts/ue.py --engine /Users/webproduktion3/UE_5.8/UE_5.8 build --target Editor --platform Mac`. Log: `Saved/Logs/Device01_Build.log`. No generated outputs were copied; the slot was released on runner exit.

**Source checkpoint:** Initial geometry passed exact envelope, slots and FBX checks at910triangles. Only the deliberate two-triangle screen has open boundary edges. A native Blender reopen initially discarded inactive packed mask images; explicit fake-user retention fixed it and all three disk/packed masks now agree. Studio image QA identified a coplanar front trim/endcap face; correction is underway before native import. This checkpoint is not final asset acceptance.

**API review:** Read-only review of pinned engine source confirmed MaterialInstance vector/texture setter return values cannot be treated as success booleans in this release. The native authoring script verifies getter readback instead. Texture-node pin name `UVs` was confirmed via Unreal graph pin-name shortening. No engine launch was used for these API checks.

## 2026-11-03 — Source frozen; shared-slot deferrals

**Source verification complete:** Final geometry is910triangles/492source vertices; exact30×18×10cm bounds. The corrected front cap avoids coplanar overlap while retaining Xmax15. Final FBX SHA-256 `8a1be66441676bd397fbdeec32c4ed0a2ce6142753c91e2534c798e29072a3da`. All three mask hashes, saved pixel ranges/coverage and asymmetric checkmark reference values are recorded in `Models/mesh_validation.json`. `--verify-only` reopens and checks saved source without rewriting binaries. Four final Blender renders were visually inspected and their hashes checked; the full4-thread source/render run completed in114.8s.

**Slot compliance:** Three native-import attempts returned exit75 before starting an editor while another workstream held the shared slot (observed Site01 readability and Mac playtest runners). Continued importer/API checks, documentation and source preview review between attempts. No engine process was started outside the slot and no other stream's process was changed. Native asset acceptance remains pending.

**Independent API review applied:** Native verification now checks source bounds/slot contract, source appearance/mask hashes, actual connected material parameter nodes, mask sampler/UV0 links and getter values, as well as the expected fixed mesh/socket/collision contract. Unchanged shader parameters in disconnected nodes cannot satisfy these checks.

## 2026-11-03 — Native import, reopen and final image verification complete

**Status:** ART-04 asset completion condition met. One original device source/export/native batch, fixed envelope/pivot/slot/socket/collision contract, three status shapes/materials, provenance and inspected neutral/dim previews are complete. Runtime integration and coordinator publication remain separate.

**Correction to prior hash notice:** The `8a1be664…` value recorded at the source-freeze checkpoint was read prematurely while the asynchronous corrected generation run had not yet replaced its prior export. The final corrected FBX was written at18:03:43 Europe/Berlin and is `f93e72c5f2bacd794dd49f2b5d482400fd648e23c4ba8b58c8f93cccca182520`. Current source report SHA is `773ddc2c1474a547e66c41b206eb745271c825bf2b9ef358463505ea968753ef`. A later read-only Blender check compared all910 triangle positions and material assignments between final FBX and saved blend at0.0001cm precision and passed (`VALSTRIKE_D01_READONLY_FINAL_GEOMETRY_RECONCILED`). Shell front cap is X14.85; Trim retains overall X15. No binary was changed by that reconciliation. All native evidence uses the final f93e72c5… source.

**Native authoring:** Four busy slot attempts returned75 without launching this stream's editor; source/docs work continued, including a60-second wait before the successful retry. The successful import ran through `python3 Scripts/with_ue_slot.py -- <pinned UnrealEditor> <absolute assigned project> /Engine/Maps/Entry -ExecutePythonScript=<absolute Device01/import_device01.py> -unattended -nosound -DisablePlugins=AndroidFileServer -abslog=<assigned Saved/Logs/Device01_Import.log>`. It emitted `VALSTRIKE_DEVICE01_NATIVE_SUCCESS: author_and_save` and exited0. Native assets: one mesh, two shell/trim materials, one status parent, three status instances and three mask textures. Only Device01 native packages were authored.

**Fresh process and captures:** A second slot-wrapped fresh Editor ran `inspect_device01.py -VSDeviceInspectOnly`. It emitted `VALSTRIKE_DEVICE01_NATIVE_SUCCESS: fresh_process_reopen`, then `VALSTRIKE_DEVICE01_INSPECTION_SUCCESS: 8 native captures`, and exited0. Log: `Saved/Logs/Device01_Inspection.log`. No map or asset packages were saved by inspection. Slot released on process exit; no other process was closed or changed.

**Measured native contract:** Bounds(-15.000001,-9.000002,0)..(15.000001,9.000002,10)cm; dimensions30.000002×18.000004×10cm;910triangles/2,038 native render vertices; oneLOD, twoUVs, Nanite off. Slots exactly Shell,Trim,Status with Idle default. Exactly one Carry socket at(0,0,8), identity rotation/unit scale. Zero simple/convex hulls, all section collisions disabled, NoCollision default and inspected component. Source/native differences are only FBX float precision and UV/hard-normal vertex splitting.

**Materials:** Saved/reopened native getters and graph traversal verify sRGB-decoded colors, roughness/metallic, mask texture identity, linear-data compression/clamp, UV0 connection and glyph-only constant emission. The three shapes differ without relying on color. MaterialInstance getter values, connected graph evidence, mesh/source records and appearance hash exactly match import-time evidence. Source mask file hashes and saved Blender material/mask checks also pass.

**Image QA:** Eight actual native1600×1000 frames: three neutral states, three dim states, carry composition and ground/player-eye view. Fresh filenames, signature/dimension/hash checks prevent stale captures from satisfying success. Inspected clean silhouette, front trim without the old overlap artifact, separated rails/feet, clear status shapes and correct asymmetric-check orientation. At dim settings, recess detail fades but glyphs/trim remain identifiable. At the measured player-eye distance the warning triangle is visible but its inner exclamation mark is small; HUD/interaction cues remain necessary. Carry fixture has no hands and crops the low feet; runtime attachment is not claimed.

**Limits/diagnostics:** The existing two startup automation `Condition failed` messages persisted; no engine automation-suite success is claimed. No new asset-script errors occurred in the successful native runs. No source/config/map/shared-contract/weapon-art edits, gameplay actor, collision/placement behavior, replicated state selection, animation, VFX system, multiplayer, packaged build, second-Mac or performance verification is included. Source glyphs are mathematical originals; no external assets/fonts/purchases.

**Integration:** Use `/Game/Art/Objectives/Device01/Meshes/SM_VS_Device01`, scale1, bottom pivot, NoCollision. Carry anchor is8cm above that pivot. Runtime changes only slot2 to the appropriate `MI_VS_D01_StatusIdle/Armed/Defused` and retains authoritative state/placement/collision. Ensure all state instances/masks are referenced for cooking. The optional mesh setting/primitive fallback is the existing seam; no shared hook change is requested. Exact commands and paths are in `docs/assets/device01/IMPORT.md`.

**Next action:** Finish local scope/hash/LFS checks and commit only `codex/objective-device`; coordinator reviews, publishes and merges. Then runtime tests actual placement/attachment, replicated state selection and map-specific readability.

## 2026-11-03 — Local handoff accepted

**Final independent QA:** The remaining Studio Idle, Dim Idle and Dim Defused images were inspected with no blocking defect. Ring/bar and upright check-in-square remain distinct in dim light. Mild glyph-edge filtering softness is visible; runtime visibility and attachment remain separate acceptance work.

**Final checks:** All 3 Python files parse as AST; all 6 JSON files parse; all 26 local Markdown links resolve. Current source appearance, mesh report, FBX and all three masks match native import/reopen evidence. All 4 source and 8 native preview hashes match their reports; native PNG signatures/dimensions and matched neutral/dim rigs pass. Exactly 10 native packages exist. The staged batch contains only 41 files in the four assigned ownership paths. All 27 binary files have Git LFS pointers whose SHA-256 and sizes match the actual working-tree bytes. `git diff --cached --check` passes. Removed only this stream's ignored generated Python caches; no unrelated files changed.

**Handoff:** Asset deliverable complete; local branch `codex/objective-device` is ready for coordinator review and publication. No push, PR, merge, gameplay integration or shared documentation edit is part of this handoff. Use the fixed mesh/slot/socket contract in `docs/assets/device01/IMPORT.md`; the next integration checks are real runtime placement/carry alignment, authoritative replicated status selection, cook references and map-specific readability with HUD cues.
