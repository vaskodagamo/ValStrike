# INT-ART-05 — Knife acceptance gaps and ART-28 rifle fit

## 2026-11-04 (Europe/Berlin) — Candidate verified; three gates held

**Ownership.**
- **Branch:** `claude/int-art-05-knife-rifle-acceptance` from main **`561ea4d`** (as observed), under the producer's [brief](../tasks/integration/INT-ART-05-knife-acceptance-rifle-fit.md).
- **#123 branch:** preserved.
- **Process correction acknowledged:** exit 75 stops that engine action, is reported, and waits for a producer release; no lock inspection or self-retry. No 75 occurred in this batch.

**Dependency.** I combined KNIFE-01-C1's head `a96e188` (PR #127) unchanged. The gameplay lead's board marks it Frozen; no GitHub review is recorded and CI was pending. I did not edit any gameplay, smoke or test file.

**Candidate `8c70673eea6fd44b446e22418de5deac11e59aab`.**
- **ART-28:** Hold, Walk and Run imported and reopened. All 66 native bones equal the source at the 13 reviewed frames (0.00014 cm). The `hand_r` offset (5.1772, −12.7049, −3.0491) is constant and as predicted. Clips and offset are applied together; walk and run are cadence-matched (138 / 287 cm/s).
- **Knife framing:** the arm offset moves from (20, 0, −5) to (28, 3, −10), chosen by measured pixel coverage. Crosshair coverage at slash impact drops from 32% to 18%. Scale, clips and timing are unchanged.
- **Build:** one build, 0 warnings.
- **Runs:**

| Run | Result |
| --- | --- |
| Practice | 39 / 37 / **36 of 37** |
| Rifle presentation | 39/39 |
| Weapons visual | 13/13 |
| Rendered practice capture (audio on) | Evidence only |
| Rendered movement capture | Evidence only |

- [Evidence](../assets/knife-rifle-acceptance/README.md).

**Closed.**
- The first live remote slash now presents (3/3), in Start→Impact order.
- The remote observer visibly sees the attacker's equip, first slash, switch, Q and stab.
- The rifle stock is seated and the head is clear (same-camera before/after).
- First-person target visibility through the slash is better.

**Held, for the producer to route.**
1. **Gameplay owner:** C1's same-update cancelled-Start suppression conflicts with its own `knife_presentation_cues` requirement of 5 stabs. The observer sees 1, because 4 stabs are cancelled within about 17 ms. One of the two must change.
2. **Gameplay owner:** no smoke attacks while moving. The exact extension is to hold forward around the driver's 22.6 s stab (`VSWeaponSmoke.cpp:626`, step 313).
3. **Environment/user:** this Mac's default output ("External Headphones") fails UE audio initialization, so no client audio was captured. Rerun the `--audio` capture once an output device UE opens is selected. The recorder is a no-microphone ScreenCaptureKit app-audio tool.

**Remaining limits.**
- Rifle crouch/airborne keep the standing pose, and there is no rifle reload animation.
- Moving-rifle views were unreadable at the captured distance; walk and run are verified at the bone level.
- Not run: two-Mac, packaged, four players, performance.

**Stop.** Handoff frozen with one main-targeted PR. The user merges.
