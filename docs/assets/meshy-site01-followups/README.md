# INT-ART-02 evidence

## Phase A — stationary pistol pose and combined QA (2026-11-04, Europe/Berlin)

Runtime candidate **`61fc9721aaf93bbb6ab4ddb841c8982e53bdb1fb`**, branch `claude/int-art-02-site01-followups`, = main `5407272` (already containing GPF-02 #84, GPF-03 #86, GPF-04 #97 at exactly `100245d7`, ART-23 #95 with source `887de0a` / handoff `9232dad`) + this phase. UE 5.8.4-58902052, Xcode 26.1.1, macOS arm64, one Mac. Every Unreal command ran through `Scripts/with_ue_slot.py`; none returned 75.

### Native

| Step | Result | Evidence |
| --- | --- | --- |
| Import `A_UE01_Pilot_PistolHold` onto the existing UrbanEntry skeleton | 1.0 s constant clip, root motion off. Existing mesh, skeleton and three clips byte-identical before/after. Fresh-process reopen identical. | [import](pistol-import-report.json), [reopen](pistol-reopen-report.json) |
| P01 offset, measured | Native `hand_r` at frame 1 = source with Y negated → P01 component (38, 3, 137) cm, identity rotation, exactly ART-23's prediction. `hand_r` offset (4.2582, −12.9002, −5.2861) cm, quat (−0.70704, 0.707095, 0.009685, 0.004084). | same reports, `p01_grip_derivation` |

### Code (presentation only)

`VSCharacterPresentationComponent` plays the pistol hold whenever the pistol is equipped (same slot source as the weapon views), otherwise the rifle hold/walk as before. `VSCharacter` attaches the remote P01 to `hand_r` with the measured offset **only when that pose loaded**, so pose and attachment activate together; otherwise the old offset remains. No movement, input, replication, health, camera, first-person or hit-zone change. Editor build: 0 warnings, receipt `55543a13…e5b529`.

### Runs on the candidate

| Check | Result | Run |
| --- | --- | --- |
| `test_movement.py` (lan) | 69/69 | `Saved/Tests/Movement/20261104T154554Z_0ecaae` |
| `test_movement.py --net-profile typical` | 69/69; emulation valid (PktLag 40 / var 10 / loss 1 in all three processes). Server corrections 26: **all 26 are zero-velocity setup teleports**, each within 0.11 s of a scripted placement (initial, boundary, stairs, cover, feel); **0 in motion**. INT-06 had 10 in motion. One valid run. | `…/20261104T154719Z_efc084` |
| `Tests/UI/test_session.py --rendered` ×3 | 72/72, 72/72, 72/72 (no repeat of the INT-06 run-1 teardown failure) | `Saved/Tests/UI/20261104T154917Z_ea2c3b`, `…155008Z_3c3091`, `…155101Z_416575` |
| `test_match.py` | 46/46, including the three GPT-13 spectate checks now **reached** with the fourth teammate client | `Saved/Tests/Match/20261104T155159Z_095c83` |
| `test_weapons.py --scenario match` (**Site01**, server + 2 rendered clients) | 29/29; remote pistol switching/visibility | `Saved/Tests/Weapons/20261104T155426Z_match_5b8b68` |

Screenshots: [remote pistol hold on Site01](runtime/site01_observer_remote_pistol_hold.png), [before/after crop (INT-ART-01 floating pistol vs now)](runtime/pistol_before_after_crop.png), [remote rifle unchanged](runtime/site01_observer_remote_rifle_hold.png), [driver first-person pistol unchanged](runtime/site01_driver_first_person_pistol.png). INT-ART-01 map/props/rifle evidence is reused unchanged: [meshy-pilot-integration](../meshy-pilot-integration/README.md).

### Limits

Stationary pose only: with the pistol equipped the body keeps the pistol hold while moving (glides). Close-up glove curl/grip contact limits from ART-23 remain. Crouch, jump, reload, fire and death animation still absent; crouch hit-zone mismatch and hold-pose forearm/rifle extent from INT-ART-01 unchanged. D3: one clean `typical` run, not a statistical proof; GPF-04 owner/lead decides closure. Not run: two-Mac LAN, packaged, four players.

## Phase B — north service facade section in Site01 (2026-11-04, Europe/Berlin)

Runtime candidate **`c7386b6c054ca2ad4bc58ee8aec29a6020a37d4e`** (branch `claude/int-art-02b-site01-courtyard`) = phase A head `39eb01a` + ART-25 frozen handoff `0c70b2f` (source `31b74bd`, readiness `3089486`) + this phase. Map/asset-only: no C++ change after phase A, so the phase A Editor build (receipt `55543a13…`) remains valid and was not rebuilt.

| Step | Result | Evidence |
| --- | --- | --- |
| Import W01 straight wall, W08 pedestrian doorway, W09 parapet | Allowlist + 33 hashes from the ART-25 record. Native bounds equal ART-25's `expected_native_bounds_cm` (Y negated, front +Y); triangles 4231/4388/4390, 1 UV, no Nanite, no collision; base color sRGB, roughness/metallic linear masks, normals `TC_NormalMap` + green flip. Fresh-process reopen identical. | [import](courtyard-import-report.json), [reopen](courtyard-reopen-report.json) |
| Place ART-25's six visuals on the north face of NorthService | Wall–blind recess–wall strip at Y 1510 (X 400→1045.7) and three roof parapets on the roof band, at ART-25's pivots with yaw 0; every world AABB matches the recipe within 0.1 cm. All NoCollision, no nav influence. Existing north facade max Y = 1500 (no relief through the skin). | [placement](courtyard-placement-report.json), [fresh reopen](courtyard-placement-reopen-report.json) |
| Contracts | All 86 pre-existing actors' transforms, tags, collision and visibility unchanged after save + fresh reopen (lights compared field by field). Pawn traces from the north route at X 450/600/722.8/845/1000 still stop on `VS_ServicePanel_1_2/1_3` at Y = 1500; route visibility stops at the retained wall. | same |
| Directional-light warning | Site01 has exactly two directional lights: `VS_Sun` (3.0, shadows, atmosphere sun) and `VS_StylizedFill` (1.5, no shadows), both forward-shading priority 0. On a tie the engine picks the brighter light (`LightGridInjection.cpp`), i.e. already `VS_Sun`. Set **only** `VS_Sun` `ForwardShadingPriority = 1`: same light selected, no color/intensity/visibility change, warning removed. Note for the map owner: the fill also has `atmosphere_sun_light` on; left unchanged (out of this bounded fix). | before/after `directional_lights_*` in the placement report; [phase A client view with warning](runtime/site01_observer_remote_pistol_hold.png) vs [phase B, no warning](runtime/site01_after_phaseB_no_light_warning.png) |
| Existing `inspect_site01.py` (once, via the INT-ART-01 quit wrapper; tracked originals restored) | **486/486, identical check set**; inventory 77 → 92 | [report and captures](site01-inspection/) |
| `test_match.py` (Site01, rounds/kills/spectate, 4 clients) | 46/46 | `Saved/Tests/Match/20261104T160548Z_451631` |
| `test_weapons.py --scenario match` (Site01, server + 2 rendered clients, shoot/eliminate/round reset) | 29/29 | `Saved/Tests/Weapons/20261104T160808Z_match_7de394` |

Before/after Unreal editor views (game view, fixed exposure; not runtime), eye level on the actual north service route: [approach](courtyard-previews/before_approach_service_route.png) → [after](courtyard-previews/after_approach_service_route.png), [facing the facade](courtyard-previews/after_route_facing_facade.png), [crouched](courtyard-previews/after_route_crouched.png), [oblique west](courtyard-previews/after_oblique_from_west.png), [oblique east](courtyard-previews/after_oblique_from_east.png), [west joint close](courtyard-previews/after_close_west_joint.png), [blind recess close](courtyard-previews/after_close_blind_recess.png); diagnostics: [overhead](courtyard-previews/after_overhead_diagnostic.png), [roof](courtyard-previews/after_roof_diagnostic.png). Comparison sheets: [eye level](courtyard-previews/compare_a.png), [close/overhead/roof](courtyard-previews/compare_b.png).

Inspected: the recess is visibly backed by the retained tan wall (closed, not an opening); west/east joints show the expected 2–5 cm reveal with solid backing behind, no void or flicker in these views; bolt holes and panel seams read recessed (green flip correct); parapets sit on the roof band as one 602 cm run with visible small joints. The grey concrete section deliberately contrasts with the remaining tan facade: it is a first arted section, not the whole building.

Limits: one facade section only (W02/W05/W06, garage, corner, roof pieces later). No actual player walk-up/close-wall clipping capture (editor cameras only; runtime suites exercise Site01 rounds, not this corridor). Not run: two-Mac LAN, packaged, four players.
