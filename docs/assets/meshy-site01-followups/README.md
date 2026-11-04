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
