# INT-ART-01 native integration evidence

2026-11-04 (Europe/Berlin). Runtime candidate `34e6e789fa58a28a3a53d22d589b46997e3a0aa6` (branch `claude/int-art-01-meshy-playtest`): main `4876acc` + #84/#85 merged as `7dfe3a3`, with ART-15 readiness `45deb70`/handoff `02f6bf2` and ART-16 readiness `757a56f`/handoff `587e6e7` as ancestors. UE 5.8.4-58902052, Xcode 26.1.1, macOS arm64 (this Mac). Every Unreal command ran under `Scripts/with_ue_slot.py`. Plan: [PREFLIGHT.md](PREFLIGHT.md). Playtest guide: [meshy-pilot.md](../../playtest/meshy-pilot.md).

## Native import and reopen

| Step | Result | Evidence |
| --- | --- | --- |
| Props W03/W04/W07/W10 | Imported, saved, reopened in a fresh process. Triangles match record (498/6000/3774/6000), 1 UV, no Nanite, no collision; base color sRGB, roughness/metallic linear masks, normals `TC_NormalMap` with green flip. W04/W10 bounds match the record; **W03/W07 native Y is negated** (FBX handedness), as predicted. | [import](props-import-report.json), [reopen](props-reopen-report.json) |
| Character | 170 cm, ±42.5 cm, one material, 3 clips, root motion off. Unreal adds the armature node `SKEL_UE01` as an extra root above `pelvis` (67 bones). Measured basis: native `hand_r` at hold frame 1 = source with Y negated, so the R01 grip is `hand_r` + (4.1008, −13.5549, −6.1893) cm, quat (−0.814922, 0.579475, 0.00887, 0.00564); rifle at (25.34, 16.51, 139.57) cm, yaw −8°, as ART-15 predicted. | [import](character-import-report.json), [reopen](character-reopen-report.json) |

Defects found and fixed during import (my scripts, not the art): the first character import did not save the FBX-created skeleton package (reopen caught it; batch deleted and reimported); UE 5.8 Python cannot name a `SkeletalMeshSocket`, so the grip offset lives in `VSCharacterPresentationComponent` instead of a socket; texture reopen sizes read the 32 px compile placeholder and are excluded from the comparison. The process exit code is 1 with two engine-startup `LogAutomationTest: Condition failed` lines that also appear in INT-03/INT-04 editor runs; success is the log marker plus report.

## Site01 placement

Nine NoCollision visuals placed exactly per ART-16's recipe envelopes; pivots solved from measured native bounds (W03 pivot Y +200; W07 yaw 180 so its native +Y front faces the −Y route with its back on the retained wall). The three replaced crates keep transform and collision and are hidden in game only. `VS_Floor`/`VS_GalleryPaving` use new map-local masked copies opening X [−100, 300], Y [750, 1150]. All 77 pre-existing actors' transforms, tags and collision are unchanged after save and reopen; crate collision traces stay inside the new 100 cm visuals. [Report](site01-placement-report.json), [reopen](site01-placement-reopen-report.json), [conversion and snapshot](site01-preexisting-snapshot.json).

Editor viewport captures (game view, fixed exposure; not runtime): [tiles standing](previews/tiles_standing.png), [crouched grazing](previews/tiles_crouched_grazing.png), [top-down](previews/tiles_top_down.png), [panel](previews/service_panel.png), [panel close](previews/service_panel_close.png), [roof crate from Gallery](previews/service_roof_from_gallery.png), [roof elevated](previews/service_roof_elevated.png), [north crate](previews/north_crate.png), [site stack](previews/site_crate_stack.png). Inspected: no mask bleed/void, panel facing correct, crate normals raised/recessed correctly, roof crate silhouette and shadow read.

Existing unmodified `inspect_site01.py` (run once via [a quit wrapper](../../../Art/Integration/MeshyPilot01/run_site01_inspection.py); outputs moved here, tracked originals restored): **486/486 pass, same check set as the baseline**, inventory 77 → 86. [Report and captures](site01-inspection/).

## Builds and actual server/client runs

| # | Command (all via `with_ue_slot.py`) | Result |
| --- | --- | --- |
| 1 | `ue.py build --target Editor --platform Mac` ×3 (initial; rifle bone offset; direct clip playback) | Succeeded, 0 warnings; final receipt `9860aca8…64b02` |
| 2 | `test_weapons.py --scenario visual` (graybox, server + 2 rendered clients) | 13/13 PASS. First run exposed the defect: [remote body in bind pose](runtime/defect_bindpose_before_fix.png) (Python blend space did not evaluate). After switching clips directly: [remote rifle hold](runtime/graybox_observer_remote_rifle.png), [remote pistol limitation](runtime/graybox_observer_remote_pistol.png), [driver first person, no self-occlusion](runtime/graybox_driver_first_person.png) |
| 3 | `test_weapons.py --scenario match` (**Site01**, match mode, server + 2 rendered clients) | 29/29 PASS: [remote character on Site01](runtime/site01_observer_remote_rifle.png), [elimination](runtime/site01_observer_elimination.png), [killer's view, dead body hidden](runtime/site01_driver_after_kill_body_hidden.png), [round reset](runtime/site01_observer_round_reset.png) |
| 4 | `test_movement.py` (server + 2 null-RHI clients) | 69/69 PASS (prototype body bounds unchanged) |
| 5 | Same movement-smoke roles, rendered, observer window captured (one-off capture, not a suite) | [walk sheet](runtime/graybox_observer_walk_sheet.png), [crops](runtime/graybox_observer_walk_crouch_crops.png): rifle hold while walking, alternating legs; crouch keeps standing pose |
| 6 | `test_rifle_presentation.py` (graybox) | 39/39 PASS |
| 7 | `run_session.py host --map site01 --mode practice --port 7861 --clients 2 --duration-seconds 25` on committed `34e6e78` | Server + 2 rendered clients connected; session completed |

Runs 2–6 used the working tree that became `34e6e78` (same source and build receipt; it still contained the unused blend-space asset, deleted in that commit). Run 7 used the commit itself.

## Hit-zone comparison (no damage change)

Standing: hit zones are a 50 × 40 cm body box to 144 cm plus a 16 cm head sphere to 176 cm; the 170 cm character's head sits at about 150–170 cm, a close match. Mismatches: in the rifle hold the forearms and rifle extend forward beyond the 25 cm body-box depth (shots there miss); crouched, the zones shrink to the 112 cm capsule while the visual keeps the standing pose. Recorded, not disguised.

## Not verified

Two-Mac LAN, packaged/cooked builds, Windows/Linux, four players, performance/LOD, human feel, close-up art quality, team readability.
