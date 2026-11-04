# Fire request rules workstream (GPT-11)

## 2026-11-04 — GUN-04 fire/reload request validation as a pure, tested rule set

Task: GPT-11 (GUN-04, rule-test part), branch `claude/gpt-11-fire-request-rules` from `origin/main` `ee25083` (source-identical to `82d7a4c` in the brief), worktree `.parallel-worktrees/gpt-11-fire-request-rules`. Validation tier V2 (server authority).

### Behavior

No intended behavior change. The ordered checks in `UVSWeaponComponent::ServerRequestFire_Implementation` and `ServerRequestReload_Implementation` moved into the engine-free `Combat/VSFireRequestRules.h`:

- `EvaluateFire(const FireInput&, CombatGate)` returns `{Outcome, bConsumesShotId}`. The order is unchanged: owner, sequence (zero, duplicate, unsigned delta over 1024), dead, holstered, equip, reload, empty, cadence (`Now + min(0.02, interval*0.2) < NextServerFireTime`), nonfinite (NaN or infinity, as `FVector::ContainsNaN`), origin (squared distance over 100² cm), direction (`|size² - 1| > 0.01` or dot with the base aim below `0.9396926`), then phase.
- `bConsumesShotId` is true once the sequence check passes, so the component still stores `LastRequestId = ShotId` when a later check rejects. Owner and sequence rejections do not consume the ID, as before.
- `EvaluateReload(const ReloadInput&, CombatGate)` keeps its own order: owner or death is dropped silently, then phase, holstered, equip, then a reload already running, a full magazine or an empty reserve are silent no-ops.
- `ReasonName()` returns the same `Reject` strings (`owner`, `sequence`, `dead`, `holstered`, `equip`, `reload`, `empty`, `cadence`, `nonfinite`, `origin`, `direction`, `phase`). Silent drops record nothing, as before.
- Signature differs from the brief's `Evaluate(const Input&)`. The phase gate is a callable, because `AVSMatchGameMode::AllowsCombat` runs `LiveGate`, which processes expired deadlines. Passing it as a precomputed bool would call it on every request, including rejected ones. The callable runs only when every earlier check passed, exactly as before. The test counts its calls.
- The component gathers the other inputs up front with engine types. All of them are side-effect-free getters: `GetPawnViewLocation`, `GetBaseAimRotation`, `IsAlive`, `GetReadiness` and world time. When `HasValidOwner()` fails, it reads nothing pawn-dependent. Vector math uses the same operand order as `FVector::DistSquared`, `SizeSquared` and `DotProduct`.

### Completion handoff

- Commit/PR: see the PR for `claude/gpt-11-fire-request-rules` (the user merges).
- Changed: `Source/ValStrike/Combat/VSFireRequestRules.h` (new), `Source/ValStrike/Combat/VSWeaponComponent.cpp` (the two request handlers plus two file-local helpers, `ToFireRulesVec` and `FireRulesReadiness`), `Tests/Weapons/fire_request_rules_test.cpp` (new), this file. `VSWeaponComponent.h` is unchanged.
- Checks actually run and their results:
  - `clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/fire_request_rules_test.cpp`: PASS, 67/67. It covers one case per fire and reload verdict, the reason names, the ordering of overlapping failures, gate laziness, and these boundaries: cadence exactly at the tolerance (both the 20 ms cap and the proportional 20%) and one ulp beyond it, origin at exactly 100 cm and one ulp beyond it, dot exactly `0.9396926` and one ulp below it, wrap-around `ShotId = 1` after `0xFFFFFFFF`, and deltas of 1024 and 1025.
  - The same command with `g++`: PASS, 67/67. **Note:** on this Mac, `g++` is Apple clang 17. No GNU GCC is installed, so a real GCC build is still unverified (CI or another host).
  - Existing Weapons tests with both commands and the same flags: `recoil_rules_test` 108/108, `weapon_loadout_rules_test` 54/54. `python3 Tests/Weapons/test_weapons_runner.py`: 9 tests OK.
  - Header-only syntax check under `-std=c++20 -Wshadow -Wconversion`: clean.
  - `git diff --check`: clean.
- Deferred checks (integration/QA owner): compile **verification pending**; runtime **verification pending**.
  1. Editor build of the combined candidate (the component change is not compiled here).
  2. `Scripts/test_weapons.py --scenario practice`: 74/74.
  3. `Scripts/test_combat.py`: 55/55.
  4. The rejection counters (`DevelopmentRejections`) match the baseline for every reason in the combat and weapons smoke runs, and `DevelopmentLastRequest` matches after the 100/100/99/101 sequence probe.
- Observations for the lead (not changed: behavior-preserving task, no real defect found):
  - The direction check applies the unit tolerance to the squared size, then compares the dot product of the **unnormalized** direction. A request with `|d|² = 1.01` can therefore be about 20.7° off the base aim, not 20°. The trace uses `GetSafeNormal()`, so the effect is only a slightly wider accepted cone. Also, `|0.99 - 1| > 0.01` in double, so the nominal 0.99 edge itself is rejected. The test uses 0.995/1.005 (accepted) and 0.985/1.015 (rejected).
  - The NaN check comes after the cadence check. A non-finite request inside the cadence window is counted as `cadence`, not `nonfinite`. Only the counter attribution changes, because no math runs on the values before the NaN check.
  - Reload requests from an unpossessed or dead owner are dropped without a rejection count, unlike fire requests. This is preserved.
  - The new test is not wired into `.github/workflows` CI (outside this task's paths). A one-line step like the other Weapons rule tests would cover it.
- Next action/owner: INT QA builds the candidate and runs the deferred checks. Defects return to the GPT-11 owner.
