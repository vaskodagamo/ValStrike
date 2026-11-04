# INT-ART-03 evidence — K01 knife native integration

2026-11-04 (Europe/Berlin). **Runtime candidate `1b572d0783ba97337dd1cce2d376976ba1456980`**, branch `claude/int-art-03-knife`. Later commits on the branch add only scripts, evidence and documentation.

The candidate combines:
- **Base:** main `4997215`. The producer observed `fa637c5`; main has since gained one docs-only gameplay-board merge, #119.
- **Already on main:** ART-26-C1 #115 (`5b8b675`, source `25fc75b`), ART-27 #110 (`bca668a`), K01 #85 and KNIFE-01-M1 #114 (`adb7374`).
- **Combined here:** the frozen WPN-02-CHECKS correction `22b71bf` (PR #121, not yet merged).
- **This task:** the import commit and the presentation commit.

UE 5.8.4-58902052, Xcode 26.1.1, macOS arm64, one Mac. Every Unreal command ran through `Scripts/with_ue_slot.py`. Exit 75 occurred twice, each right after my own editor job ended. The first time I queued and did not retry. The second time the lock metadata showed my own finished job, so I sent the build once more.

## Inputs and native import ([script](../../../Art/Integration/KnifePilot01/import_knife.py))

| Item | Result | Evidence |
| --- | --- | --- |
| Allowlist | Every imported file's SHA-256 equals its own frozen record: K01 manifest, ART-26-C1 `c1-source-files.json`, FPArms03 `source-files.json`, ART-27 `artifact-manifest.json`. The old held #109 is not used. | [import report](knife-import-report.json) `source_sha256` |
| K01 `/Game/Art/Weapons/Knife01` | 70,640 triangles, **27.80 × 2.399 × 4.651 cm**. Grip at the origin; Tip socket at (21.517, −0.011, 1.318). No collision, no Nanite, one LOD. Original GLB graph: sRGB base colour, packed G roughness and B metallic, normal map green-flipped. | same |
| FPArms03 knife hands `/Game/Art/Characters/FPArms03` | Paired gloved forearms with the original PBR (two-sided, like the GLB) and their own skeleton. 40 bones: the 39 source bones plus the armature node, which the importer adds as root. Five clips, 60 fps, root motion off. Lengths are exactly Equip 0.40, Idle 1.00, Locomotion 0.80, Slash 0.55 and Stab 0.90 s. Impact sync markers at 0.15 and 0.30 s. | same |
| ART-27 clips `/Game/Art/Characters/UrbanEntry01/Animations/Knife` | Hold, Equip, Walk, Run, Slash and Stab on the **existing** UrbanEntry skeleton, with the same lengths and markers. All existing UrbanEntry, R01, P01, R02 and P02 files are byte-identical. | same |
| Fresh-process reopen | Identical to the import report. | [reopen report](knife-reopen-report.json) |

## Measured attachments and timing

- **First person:**
  - Source poses were read once with Blender, read-only ([script](../../../Art/Integration/KnifePilot01/read_source_poses.py), [poses](source-poses.json)).
  - Native `weapon_socket_r` equals the source with Y negated in position **and axes**, with errors ≤0.0001 cm and 1e-5.
  - The K01 offset is **identical on every recorded frame of all five clips** (spread 0): (−1.5, −6.88, −1.16) cm, quat (0, −0.948683, −0.316228, 0).
  - The arms sit at (+20, 0, −5) cm from the unchanged 90° first-person camera, from ART-26's source camera.
- **Third person:**
  - Native `hand_r` on Knife_Hold equals ART-27's matrix exactly.
  - The K01 offset on `hand_r` is (2.109, −12.1092, 0.6281) cm, quat (−0.565402, −0.404277, 0.316081, −0.645735).
- **Timing:**
  - Each clip starts on its gameplay cue, so the authored impact lands at the cue's impact time (0.15 / 0.30 s) with no retiming.
  - The in-place gait speeds were measured from the stance foot: walk 138 cm/s, run 287 cm/s. The body runs above 210 cm/s, with play rate = speed / clip speed, capped at 1.8.

## Code (presentation only; consumes the frozen `VSKnifePresentationEvents.h`)

**`UVSKnifePresentationComponent`** (new) binds the cue API:
- **Equip, Slash, Stab:** start the first-person clip and the third-person body action.
- **Holster, Cancel:** stop them.
- **Idle and locomotion:** follow speed.
- **Visibility:** the first-person arms and knife show only in the owner's own view, and the world knife only to others. Both hide when the knife isn't equipped or the pawn is dead.

**`UVSKnifeBodyAnimInstance`** is a native, graph-less animation instance. It plays a looping in-place base (hold, walk or run) and layers equip, slash and stab on `spine_01` and its children in local space, as ART-27's contract requires, so moving attacks keep the legs.

**Knife-only hooks:**
- `VSCharacterPresentationComponent`: knife clips and the knife mode.
- `VSCharacter`: creates and initializes the component.

**Untouched:** `VSKnifeComponent`, the loadout, HUD, audio and damage. The firearm views already hide on the knife slot (unchanged).

**Audio:** the gameplay audio subsystem's existing code-generated knife swing, wall and flesh cues are used. No art audio was added.

**Build:** one combined Editor build, **0 warnings**, receipt `8c00c24b…b5fdf`.

## Runs on `1b572d0` (once each)

| Check | Result | Run |
| --- | --- | --- |
| `test_weapons.py --scenario practice` (KNIFE-01 smoke, null-RHI) | **Server 39/39, driver 37/37, observer 37/37**, including `pistol_assets` with WPN-02 and all knife checks | `Saved/Tests/Weapons/20261104T181051Z_practice_45689d` |
| `test_rifle_presentation.py` | **39/39** | `Saved/Tests/RiflePresentation/20261104T181202Z_graybox_4a9f48` |
| Native cue binding (same practice run, `LogVSKnifePresentation`) | **Driver, own view:** Equip ×3, Slash ×3, Stab ×5, Impact ×4, Cancel ×4, Holster ×2, all local and first-person. **Observer, remote:** Equip ×3, Slash **×2**, Stab ×5, Impact ×4, Cancel ×4, Holster ×2, third-person. | [cue timeline](runtime/cue-timeline.json) |
| Rendered capture of the same practice roles (evidence only) | Same commands with windowed 1000×700 clients; both windows captured about 6 fps for 52 s. The capture's first attempt lost the window after the client replaced it and was rerun once. | [capture script](../../../Art/Integration/KnifePilot01/capture_rendered_practice.py) |

The rendered run covers the full first-person lifecycle, matched to the logged cues:

| Frames | What they show |
| --- | --- |
| 241–243 | Equip from below the view |
| 244 | Idle |
| 246–248 | Slash: windup, the 0.15 s impact on a player, follow-through |
| 254–255 | Out-of-range slash (miss) |
| 261–262 | Wall slash |
| 268–270 | Switch to the rifle: arms and knife hidden, R02 raises |
| 273–276 | Re-equip |
| 279 | Stab while the phase is closed: cancelled |
| 285–288 | Q to the rifle and back |
| 292–293 | Stab: player hit, then recovery |
| 297 | Driver dead: arms and knife hidden |

The HUD shows `KNIFE / LMB SLASH RMB STAB` with no ammo while the knife is out, and the rifle ammo returns after switching. See the [sequence sheet](runtime/first_person_runtime_sequence.png) and single frames: [equip](runtime/fp_equip.png), [idle](runtime/fp_idle.png), [slash impact](runtime/fp_slash_impact.png), [wall](runtime/fp_slash_wall.png), [stab impact](runtime/fp_stab_impact.png), [switch to rifle](runtime/fp_switch_to_rifle.png), [dead](runtime/fp_dead_hidden.png).

**Third person:** the practice smoke fixes the observer's camera at `ObserverView`, so the [observer window never sees the driver](runtime/observer_view_during_knife.png). Remote cue binding is runtime-verified through the log above. The rendered remote view is **not** runtime-verified.

The native assembly is shown instead in **editor** views: ART-27 poses on the native skeleton, with K01 at the measured runtime offset ([sheet](runtime/third_person_editor_sheet.png), [script](../../../Art/Integration/KnifePilot01/preview_third_person.py), [14 views and report](third-person/); the map was checked unchanged). They show:
- hold, equip, slash impact and follow-through, stab impact, walk and run;
- the knife seated in the right hand and the guard hand on its own side.

## Results

| Area | Status |
| --- | --- |
| First person: gloved K01 hold, equip, idle, locomotion, slash, stab, cancellation, switch and death visibility, firearm UI gating | **Runtime-verified** on real server and rendered clients |
| Practice authority and lifecycle (39/37/37) | **Pass** |
| Rifle presentation (39/39) | **Pass** |
| Remote cue binding and visibility logic | **Runtime-verified** by log |
| Remote rendered action | **Held:** needs a small smoke extension (below) |
| Movement attacks | Layered body implemented, not separately captured |

## Needs for owners (not changed here)

1. **Gameplay owner, weapons smoke:** the practice observer never looks at the driver.
   - **Exact extension:** in `Source/ValStrike/Combat/VSWeaponSmoke.cpp`'s observer branch (currently `PC->SetControlRotation(ObserverView)` at line 346), during `Time >= 13.0` set the observer's control rotation toward the driver's pawn, for example `(Driver->GetActorLocation() + FVector(0,0,40) - Target->GetPawnViewLocation()).Rotation()`.
   - Knife hit selection uses only the driver's view, so check counts are unaffected.
   - **Optional:** when `-VSWeaponCapture` is given, add `Capture()` calls at slash and stab impact for a rendered variant.
2. **Gameplay owner, `VSKnifeComponent`:** the remote observer receives 2 of 3 slash `Start` cues. In both runs the first slash after equip arrives as Impact only, so remote viewers miss that swing's animation.
   - The first-slash pattern is identical in both runs; stabs are 5/5.
   - Needs a look at how `OnRep_State` turns AttackCount changes into Start when the attack and its impact arrive in one update.
3. **Artists:**
   - **First person (ART-26):** the source framing makes K01 and the right forearm large, and at slash impact the forearm crosses the crosshair. The knife hand sits about 28 cm from the camera, per the recipe.
   - **Pilot weight:** the hand pair is 780,956 triangles with no LODs (not profiled). General deformation and the runtime budget remain gates.
   - **Third person (ART-27):** the glove limits ART-27 recorded remain. Crouch keeps the standing pose. First-person clip switches are direct, with no blending (single-node playback).

Not run: movement, session, map or shot-effects reruns (reused per the brief), two-Mac LAN, packaged builds, four players.
