# INT-ART-05 evidence — knife acceptance gaps and ART-28 rifle fit

2026-11-04 (Europe/Berlin). **Runtime candidate `8c70673eea6fd44b446e22418de5deac11e59aab`**, branch `claude/int-art-05-knife-rifle-acceptance`. Later commits on the branch add only evidence and documentation.

The candidate combines:
- **Base:** main **`561ea4d`** (as observed), which contains #123 and ART-28 #124.
- **This task:** the ART-28 import and the presentation commits.
- **Combined dependency:** the frozen **KNIFE-01-C1 head `a96e188`** (PR #127). The gameplay lead's board (PR #125) marks it Frozen. No GitHub review is recorded, and CI was pending.

UE 5.8.4-58902052, Xcode 26.1.1, macOS arm64, one Mac. Every Unreal command ran through `Scripts/with_ue_slot.py`, and **none returned 75 in this batch**.

## ART-28 rifle fit ([import script](../../../Art/Integration/Rifle02CharacterFit/import_rifle_fit.py))

| Item | Result | Evidence |
| --- | --- | --- |
| Import | `A_UE01_R02_Hold` (1.000 s), `Walk` (1.0333 s) and `Run` (0.6333 s) on the existing UrbanEntry skeleton, root motion off. SHA-256 values equal ART-28's manifest. All existing UrbanEntry, R02, K01 and FPArms03 files are byte-identical. Fresh-process reopen is identical. | [import](rifle02-fit-import-report.json), [reopen](rifle02-fit-reopen-report.json) |
| Native verification | Source poses were read once with Blender, read-only ([script](../../../Art/Integration/Rifle02CharacterFit/read_source_poses.py), [poses](rifle02-source-poses.json)). **All 66 bones** at the 13 frames ART-28 reviewed (Hold 1/31/61, Walk 1/16/32/47/63, Run 1/10/20/30/39) equal the native skeleton under the measured Y-negation basis: at most **0.00014 cm** in position and **0.00015** in axes. The only native-only bone is the armature root `SKEL_UE01`. Because native bones, rifle placement and geometry all match, ART-28's sampled stock, face and hand surface contacts hold natively unchanged: stock contact below 0.0005 cm, up to 0.30 cm clothing overlap, at least 16.27 cm head clearance, 0.22 / 0.07 cm hand depth. | same, `fit` |
| `hand_r` offset | (5.1772, −12.7049, −3.0491) cm, same rotation as before. It is constant over all 13 frames (spread 0.0001) and equals ART-28's predicted `C·S·C`. It is applied **together with** the three clips; the old pose/offset pair is retired. | same |
| Locomotion | Stance-foot speed: walk 138 cm/s, run 287 cm/s. The body runs above 210 cm/s, with play rate = speed / clip speed, capped at 1.8. | same, `gait_stance_speed_cm_s` |

**Runtime, same test camera** ([before/after](runtime/rifle_fit_before_after.png); weapons visual 13/13, `Saved/Tests/Weapons/20261104T191109Z_visual_369929`):
- **Before:** the INT-ART-04 stock sat at the cheek.
- **After:** the head is upright and clear, the stock is seated at the shoulder, and R02 is held in both hands. This holds for the hold and after switching back from the pistol.
- The rifle→pistol transition is unchanged. The pistol keeps ART-23's pose.
- The first-person driver view also shows the remote target holding R02 in the new pose ([sheet](runtime/first_person_framing_A_vs_D.png)).

## First-person knife framing ([tool](../../../Art/Integration/KnifePilot01/preview_first_person.py), [editor comparison](fp-framing/framing_comparison.png), [pixel coverage](fp-framing/coverage.json))

Same-scale arms, the same clips and timings, and the unchanged 90° camera. Only the arm offset from the camera changed. Candidate **D (+28, +3, −10)** was chosen over the ART-26 source framing A (+20, 0, −5):

| Pose | Screen coverage, A | Screen coverage, D | Crosshair box, A | Crosshair box, D |
| --- | --- | --- | --- | --- |
| Idle | 13% | 10% | 4% | 3% |
| Slash impact | 15% | 11% | 32% | 18% |
| Slash follow-through | 24% | 15% | 41% | 26% |
| Stab impact | 10% | 8% | 6% | 3% |

Both hands and K01 stay whole, and a brief blade crossing at impact remains.

In the runtime comparison ([A vs D, same smoke camera and timeline](runtime/first_person_framing_A_vs_D.png)), the target's face stays visible through the slash, and the hands and knife read clearly at idle. Caveats:
- C1 also turned the observer toward the driver, so the target's posture differs between the two columns.
- In the editor views, the target character I placed did not render, so those coverage numbers measure the crosshair region only.

## Runs on `8c70673` (one build, 0 warnings, receipt `12569127…027d41`)

| Check | Result | Run |
| --- | --- | --- |
| `test_weapons.py --scenario practice` | Server **39/39**, driver **37/37**, observer **36/37**. The only failure is C1's tightened `knife_presentation_cues` on the observer: slash 3 ✓, **stab 1 of 5 required**. All knife authority and lifecycle checks pass. | `Saved/Tests/Weapons/20261104T185908Z_practice_a20edc` |
| `test_rifle_presentation.py` | **39/39** | `Saved/Tests/RiflePresentation/20261104T190034Z_graybox_37fa96` |
| `test_weapons.py --scenario visual` (rifle and transition views) | **13/13** | `…/20261104T191109Z_visual_369929` |
| Rendered capture of the practice roles with audio on (evidence only) | 592 frames | [record](runtime/rendered-capture.json) |
| Rendered capture of the movement roles (evidence only) | 979 frames; the players are mostly far apart, so the rifle is not readable while moving | `Saved/IntArt05/rendered_movement/20261104T200852` |

### Cue order ([timeline](runtime/cue-timeline.json))

**Observer, both runs:** every slash shows Start then Impact: player hit, miss, wall. **The first live slash now presents (3/3)**, which fixes INT-ART-03's 2/3. The stab hit shows Start then Impact.

**Driver:** all local Starts, then Impact or Cancel, as before.

### Remote rendered action ([sheet](runtime/remote_knife_sequence.png))

With C1, the observer now faces the attacking driver:

| Frames | What the observer sees |
| --- | --- |
| 228 | Rifle in the ART-28 hold |
| 230–233 | Knife equip and hold |
| **235** | **The first live slash, visibly swung** |
| 236 | Its impact |
| 251 | The wall slash |
| 257–259 | Switch to R02 |
| 264 | Re-equip |
| 274 | Q to the rifle |
| 283–285 | Stab that kills the observer |

The view is backlit and dark but readable.

## Held gates (precise)

1. **C1 rule versus assertion (gameplay owner).**
   - C1 deliberately suppresses a remote Start that the same replication update already cancels.
   - In practice, 4 of the 5 stabs are cancelled within about 17 ms (switch 05.699→05.717, phase 07.500→07.517, reset 08.606→08.622, death 11.307→11.341). Their Start and Cancel replicate together, so the observer presents 1 stab.
   - C1's own assertion requires 5. Either the rule or the assertion must change; that's the gameplay owner's decision. I did not edit their files.
2. **Moving knife attacks: not demonstrated.** Every practice attack is stationary, and no existing smoke attacks while moving. The layered body (attack on `spine_01` over walk/run) is implemented but not shown rendered.
   - **Exact smoke extension for the gameplay owner:** in `VSWeaponSmoke.cpp`, for the driver's 22.6 s stab (step 313), hold forward movement for about 1 s around the press. The authority checks don't depend on the driver being stationary there; the gameplay owner should confirm.
3. **Audible cues: not established on this Mac.**
   - The audio-on rendered run started both clients with audio, but the UE client failed to initialize the macOS default output, "External Headphones": `LogAudioMixerAudioUnit: Warning: Error querying Sample Rate`, then "Using Audio Hardware Device Unknown".
   - My app-output recorder ([script](../../../Art/Integration/KnifePilot01/record_app_audio.swift); ScreenCaptureKit, no microphone) received **0 audio buffers** from the client, and the observer's recorder timed out.
   - A test of the recorder with QuickTime captured buffers, but the test sound itself never played, so it was inconclusive.
   - **Needed:** a run with an output device UE initializes, for example the Mac's own speakers selected by the user. Changing system settings is not mine. With that, one rerun of the same capture command with `--audio` produces the recording.
4. **Rifle crouch, airborne and reload** were not captured readably: crouch keeps the standing pose, and there is no rifle reload animation. Walk and run are verified at the bone level only.

Also not run: two-Mac LAN, packaged builds, four players, performance or LOD.
