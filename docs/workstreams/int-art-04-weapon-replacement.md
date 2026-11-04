# INT-ART-04 — R02 and P02 replace the rifle and pistol

## 2026-11-04 (Europe/Berlin) — Assignment acknowledged; candidate verified

**Ownership.** The existing sole native owner (the INT-ART-01/02 Claude session) took this task under the producer's [brief](../tasks/integration/INT-ART-04-weapon-replacement.md). Its order:
1. Publish INT-ART-02B unchanged as PR #116, tested candidate `c7386b6`, older base labelled.
2. Then this task.

**Branch.** `claude/int-art-04-weapon-replacement` from main `caf7551`.

**Dependency.**
- I inspected the KNIFE-01-M1 repair PR #114, head `adb7374`. It is a frozen single commit that removes exactly the duplicate declaration and RPC call.
- I combined it unchanged; it was not merged.
- I did not edit any gameplay files.

**Candidate `c2ba8522b22d3e46776c090c353d5356f4c5f6e2`**
- **Import:** R02 and P02 were imported with source SHA-256 checks, exact triangle counts and bounds, the original PBR recreated, recipe sockets, and a fresh-process reopen. R01, P01 and the character are byte-identical.
- **Grips:** measured on the native skeleton:
  - R02 on the ART-15 hold reproduces ART-17's fit exactly.
  - P02 sits on the #103 pistol hold at ART-23's (38, 3, 134).
- **Code:** presentation C++ only (`VSRifle/VSPistol/VSCharacterPresentationComponent`). `VSCharacter.cpp` is unchanged.
- **Build:** one Editor build, 0 warnings.

**Runs, once each**

| Run | Result |
| --- | --- |
| Rifle presentation | 37/39 |
| Shot effects | 73/73, tracers from the new muzzle |
| Weapons visual | 13/13 |
| Site01 match | 29/29 |
| Knife practice | 39 server / 36 of 37 driver / 36 of 37 observer; all 39 knife checks pass |

The only failures are two hardcoded old-asset-name checks. [Evidence and screenshots](../assets/weapon-replacement02/README.md).

**For the producer to route (not changed here):**
- `pistol_assets` in `Source/ValStrike/Combat/VSWeaponSmoke.cpp:376` should expect `SM_VS_Pistol02_*`. Owner: weapon smoke / GPT-04.
- `attached_assets` in `Source/ValStrike/Presentation/VSRiflePresentationTest.cpp:214` should expect `SM_VS_Rifle02_Body`. Owner: rifle presentation test.
- Optional: add the `Rifle02` and `Pistol02` folders to the evidence `asset_sha256` lists in `Scripts/test_weapons.py`, `test_rifle_presentation.py` and `test_shot_effects.py`.

**Contact and fit limits for the existing artists:**
- R02: stock at the cheek, no shoulder seat, fingers not retuned (ART-17).
- P02: support fingers near the guard (ART-23).
- First person has no arms.
- About 460K triangles per weapon, no LODs, not profiled.

**Not run:** movement, session or full regressions (by the brief), two-Mac LAN, packaged builds, four players.

**Stop.** Handoff frozen with one main-targeted PR. Full INT-ART-03 knife integration is the next separately admitted native batch. The user merges.
