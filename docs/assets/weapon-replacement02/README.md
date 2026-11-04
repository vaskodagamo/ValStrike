# INT-ART-04 evidence — R02 rifle and P02 pistol in the game

2026-11-04 (Europe/Berlin). Runtime candidate **`c2ba8522b22d3e46776c090c353d5356f4c5f6e2`**, branch `claude/int-art-04-weapon-replacement`:
- **Base:** main `caf7551` (with #108–#112).
- **Combined:** the frozen KNIFE-01-M1 repair `adb7374` (PR #114, not yet merged).
- **This task:** import and presentation commits.
- **Environment:** UE 5.8.4-58902052, Xcode 26.1.1, macOS arm64, one Mac. Every Unreal command ran through `Scripts/with_ue_slot.py`, and none returned 75.

## Native import (`Art/Integration/WeaponReplacement02/import_weapons.py`)

| Item | Result | Evidence |
| --- | --- | --- |
| Sources | R02 #89: 2 FBXs and the 3 exact GLB maps. P02 #81: 3 FBXs and 4 original maps. All SHA-256 values equal their own frozen records. Source art is unchanged. The importer's extracted `.fbm` folders were deleted, untracked. | [import report](weapons-import-report.json) `source_sha256` |
| Meshes | Each part is a separate static mesh. Scale 1, no reduction, collision removed (NoCollision), Nanite off, one LOD, one UV channel, authored normals imported. Triangles equal the source exactly: R02 448,486 + 11,860; P02 211,980 + 241,968 + 7,464. | same |
| Size and axes | Native bounds equal the source local bounds with Y negated (FBX import), within 0.01 cm. Assembled R02 **75.000 × 7.251 × 25.455 cm**, P02 **21.000 × 3.352 × 13.833 cm**. +X is the muzzle, +Z up. | same |
| Materials | The original graphs are recreated. Base colour is sRGB with no multiplier. R02's packed ORM uses G for roughness and B for metallic, with no clamps. P02 uses its separate roughness/metallic maps directly. Normal strength is 1 with the green channel flipped (OpenGL source). Data maps are linear masks. The seam/cut interior slots get the recorded flat colours. | same |
| Sockets | **R02:** Grip (0,0,0), Muzzle (52,0,7.05), SupportHand (29,0,2.4), Magazine (15,0,−2.5). The magazine is insertion-local and attaches at identity. **P02:** Grip (0,0,0), Muzzle (15.5,0,5). Body, slide and magazine share the Grip datum, so the attachment sockets Slide and Magazine sit at (0,0,0). The recipe markers (0,0,3.9) and (−2.5,0,−1) are recorded and **not applied twice**. This keeps the base `VSWeaponPresentationComponent` unchanged. | same |
| Reopen | A fresh process gives an identical report. R01, P01 and the UrbanEntry assets are byte-identical before and after. | [reopen report](weapons-reopen-report.json) |
| Import warnings | "No smoothing groups" (the authored normals are imported). P02 Body and Magazine have "some nearly zero tangents/binormals". No shading artefact is visible in the runtime views below. | `Saved/IntArt04/import.log` |

## Grips, measured on the native skeleton

| Weapon / pose | Result |
| --- | --- |
| R02 on the ART-15 rifle hold | ART-15's hold keeps the source frame's hand orientation (difference 1e-6) and moves both wrists together. So ART-17's hand-relative fit applies unchanged: R02 sits at component (26.45, 18.56, 139.08) cm with no rotation. Left wrist, left middle finger, right wrist and right middle finger land in R02 space exactly where ART-17 measured them (≤0.001 cm). `hand_r` offset is (2.3021, −15.0518, −5.7134) cm, quat (−0.772515, 0.63491, 0.009241, 0.005007). R01's offset is kept in a code comment for comparison. |
| P02 on the #103 ART-23 pistol hold | Component (38, 3, 134), no rotation, as ART-23 predicted. `hand_r` offset is (4.2820, −12.9586, −2.2868) cm, the same rotation as P01. Both hands are at ART-23's weapon-space references (≤0.001 cm). |
| First person | The camera anchor and FOV 90 are unchanged. **R02** uses ART-17's own composition (30, 12, −19), 5 cm nearer and more central than R01's (35, 17, −20), because R02 is 11 cm shorter. The existing (−3, −2, 0) cant is kept. **P02** keeps P01's accepted (34, 12, −14) / (−2, −3, 0), since it shares P01's datum convention and P02 is 21 cm against P01's 24 cm. The first-person view has no arms or hands; that is the separate held hands task. |

The size is unchanged: no scaling of the weapon or the character.

## Runtime runs on `c2ba852` (one build, Editor 0 warnings, receipt `06108543…2e5190c`)

| Check | Result | Run |
| --- | --- | --- |
| `test_rifle_presentation.py` (rendered owner and observer) | **37/39.** The only failures are owner and observer `attached_assets`, a hardcoded `SM_VS_Rifle01_Body` name; see the adjustment below. Flash, single accepted-shot pulse, reload pose and completion, aim unchanged, no collision, visibility, death cleanup and late attach without replay all pass. | `Saved/Tests/RiflePresentation/20261104T173158Z_graybox_ea833c` |
| `test_shot_effects.py` | **73/73.** Every tracer starts at the new Muzzle socket (`muzzle_error=1.80`, within the check). | `Saved/Tests/ShotEffects/20261104T173328Z_e21c29` |
| `test_weapons.py --scenario visual` | **13/13**, with first-person and observer captures for rifle, pistol equip, pistol, pistol fire, pistol reload and rifle after switch. | `Saved/Tests/Weapons/20261104T173511Z_visual_44bac5` |
| `test_weapons.py --scenario match` (Site01, server + 2 rendered clients) | **29/29** | `Saved/Tests/Weapons/20261104T173630Z_match_c43802` |
| `test_weapons.py --scenario practice` (KNIFE-01 phase B smoke) | Server **39/39**, driver **36/37**, observer **36/37**: the expected 39/37/37 checks all ran. All 39 knife checks pass on the repaired build, including `knife_reject_replay`. The only failure is driver and observer `pistol_assets`, a hardcoded `SM_VS_Pistol01_*` name. | `Saved/Tests/Weapons/20261104T173743Z_practice_3d64ed` |

Movement, session and full regressions were not rerun. Phase A gameplay and phase B map evidence are reused unchanged.

### Checks that need their owners (not changed here)

1. **`pistol_assets`:** [VSWeaponSmoke.cpp:376](../../../Source/ValStrike/Combat/VSWeaponSmoke.cpp) compares the first-person meshes with `SM_VS_Pistol01_Body/_Slide/_Magazine`.
   - **Required change:** compare with `SM_VS_Pistol02_Body/_Slide/_Magazine`. The socket names it checks (Slide, Magazine, Muzzle) are unchanged and pass.
   - **Owner:** weapon smoke (GPT-04, KNIFE-01).
2. **`attached_assets`:** [VSRiflePresentationTest.cpp:214](../../../Source/ValStrike/Presentation/VSRiflePresentationTest.cpp) compares with `SM_VS_Rifle01_Body`.
   - **Required change:** `SM_VS_Rifle02_Body`. The Grip, Muzzle and SupportHand sockets and the Magazine attachment are unchanged and pass.
   - **Owner:** rifle presentation test.
3. **Evidence metadata only (non-failing):** `asset_sha256` lists `Content/Art/Weapons/Rifle01` and `Pistol01` in `Scripts/test_weapons.py:254`, `test_rifle_presentation.py:191` and `test_shot_effects.py:243`. Adding `Rifle02` and `Pistol02` would record the active assets.

## Screenshots (actual runtime captures)

The "before" images are R01/P01 runs on this Mac from earlier today with the same test cameras:
- Rifle presentation `145933Z`.
- Visual `144844Z`, which predates #103, so its remote pistol floats.
- Site01 match `160808Z`, which has the #103 pose.

**Comparison sheets**
- [First person: rifle idle, fire, reload, switch back](runtime/first_person_rifle_before_after.png)
- [First person: pistol idle, equip, fire, reload](runtime/first_person_pistol_before_after.png)
- [Remote: rifle hold, fire, reload, Site01; pistol Site01, fire, reload, switch](runtime/remote_before_after.png)

**Single after frames**
- First person: [rifle idle](runtime/fp_rifle_idle.png), [rifle fire](runtime/fp_rifle_fire.png), [rifle reload](runtime/fp_rifle_reload.png), [rifle after switch](runtime/fp_rifle_after_switch.png), [pistol idle](runtime/fp_pistol_idle.png), [pistol switch](runtime/fp_pistol_switch.png), [pistol fire](runtime/fp_pistol_fire.png), [pistol reload](runtime/fp_pistol_reload.png).
- Remote: [Site01 rifle](runtime/site01_remote_rifle.png), [Site01 pistol](runtime/site01_remote_pistol.png), [rifle fire](runtime/remote_rifle_fire.png), [rifle reload](runtime/remote_rifle_reload.png), [pistol fire](runtime/remote_pistol_fire.png), [pistol reload](runtime/remote_pistol_reload.png), [pistol switch](runtime/remote_pistol_switch.png).

**What the images show**
- The original Meshy PBR detail survives in both views.
- Flashes come from the new muzzles.
- The P02 slide kicks back on fire, and both magazines drop during reload.
- The first-person view shows only for the owner and the world view only for observers.
- The pistol equip frame is lowered out of view, as before.

The Site01 frames still show the orange directional-light message. That fix is in INT-ART-02B PR #116, which this branch's base does not include.

## Limits and notes for the artists

- **R02 remote:** the stock reaches the cheek and there is no shoulder seat. Right-hand fingers are not retuned for R02's grip, and the support wrist is about 12.8 cm off the centreline (R01's was 8.2). ART-17's measured fit is preserved, not hidden. These are ART-17's recorded open items for art and animation.
- **P02 remote:** ART-23's recorded limits remain: support fingers near the lower guard, and close-up glove contact. The pose is still stationary, so the body glides while moving.
- **First person:** floating weapons with no arms; the hands are the separately held knife and arms task. The R02 composition is larger on screen than a smaller rifle might suggest because it sits nearer. Art may tune it later.
- **Pilot weight:** about 460K triangles per weapon, 4K base colour and normal maps, Nanite off, no LODs. Rendering showed no hitch or artefact in these runs, but nothing was profiled; there is no performance target or acceptance.
- **Not run:** two-Mac LAN, packaged builds, four players, crouch or movement views of the new weapons.
