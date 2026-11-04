# Weapon asset checks (WPN-02-CHECKS)

- **Brief:** producer `docs/tasks/gameplay/WPN-02-CHECKS.md`.
- **Owner:** existing GPT-04 session. Branch `claude/wpn02-asset-checks` from `origin/main` `4997215` (admission `fa637c5`).
- **Status:** implementation ready; verification pending in INT-ART-03's combined native build.

## Why

INT-ART-04 (#118) replaced the active rifle and pistol with R02/P02. Its runtime on `c2ba852` failed only on two hardcoded mesh names:
- `attached_assets`: rifle presentation 37/39.
- `pistol_assets`: weapons practice clients 36/37.

Every socket, attachment and knife check passed (INT-ART-04 handoff, "Runtime runs on c2ba852").

## Change (constants and lists only)

The names were taken from `git ls-tree -r --name-only origin/main Content/Art/Weapons`.

| File | Change |
| --- | --- |
| `Source/ValStrike/Combat/VSWeaponSmoke.cpp` `pistol_assets` | Expects `SM_VS_Pistol02_Body`, `SM_VS_Pistol02_Slide` and `SM_VS_Pistol02_Magazine`. The Slide, Magazine and Muzzle socket checks are unchanged. |
| `Source/ValStrike/Presentation/VSRiflePresentationTest.cpp` `attached_assets` | Expects `SM_VS_Rifle02_Body`. The flash material, sockets and magazine attachment checks are unchanged. |
| `Scripts/test_weapons.py` `asset_sha256` | Adds `Content/Art/Weapons/Rifle02` and `Pistol02`. |
| `Scripts/test_rifle_presentation.py` `asset_sha256` | Adds `Content/Art/Weapons/Rifle02`. |
| `Scripts/test_shot_effects.py` `asset_sha256` | Adds `Content/Art/Weapons/Rifle02`. This test is rifle-only, like its existing `Rifle01` entry. |

Notes:
- The historical `Rifle01`/`Pistol01` folders are still in the repository and stay in the hash lists as evidence metadata.
- The mesh-name checks accept only the new names. An old asset would fail them, so an active-asset regression stays visible.
- Check counts, thresholds and timing are unchanged.

## Checks

- **Run:** inspected the constant- and list-only diff against the imported paths; `git diff --check` is clean. No old mesh name remains in `Source/`, `Scripts/` or `Tests/`.
- **Deferred to INT-ART-03:** the combined build, then:
  - `test_rifle_presentation.py`: 39/39 expected;
  - `test_weapons.py --scenario practice`: 39/37/37 expected.
