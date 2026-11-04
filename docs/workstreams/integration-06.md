# INT-06: verify main `09347c2` (GPT-12/13/14) plus PR #74 (GPT-15)

- Date: 2026-11-04 (Europe/Berlin), build at 14:56
- Task brief: `docs/tasks/gameplay/INT-06-batch.md` (on `claude/gameplay-board-int07`)
- Owner: the integration/QA session, assigned by the Claude gameplay lead
- Candidate SHA: `1b01540`, on the local disposable `claude/integration-06` (not pushed). It is `origin/main` `09347c2` plus `--no-ff` merge of `origin/claude/gpt-15-movement-feel` at `c52385e` (the tip matched the SHA the lead gave). There were no conflicts.
- Engine/toolchain/machine: UE 5.8.4 (CL 58902052), Xcode 26.1.1, Apple clang 17.0.0, Apple M4 (Mac16,10), macOS 15.6. This is the same setup as INT-01..05.
- Engine slot: the build ran under `python3 Scripts/with_ue_slot.py --` and did not return 75.

## Results

| # | suite | result | counts | evidence path |
| --- | --- | --- | --- | --- |
| 0 | `Scripts/ue.py build --target Editor --platform Mac` | **FAIL** | exit 6, `Result: Failed (OtherCompilationError)`; 1 unique error, reported once in each of `Module.ValStrike.1/2/3.cpp` | `Saved/INT-06/00.log` |
| 1–7 | movement (lan), movement (`typical`), match, damage, combat, weapons, session | not run | The brief says to stop on build failure | n/a |

## Defects

### INT-06-D1: GPT-15 header does not compile (`-Werror,-Wshadow`)

- Owner: **GPT-15 (movement feel, PR #74)**. Both lines are new in GPT-15 commit `35ff694`. `Source/ValStrike/VSMovementRules.h` does not exist on `origin/main`.
- Error:

```text
Source/ValStrike/VSMovementRules.h:103:80: error: declaration shadows a variable in namespace 'VSMovementRules' [-Werror,-Wshadow]
  103 | inline double JumpDistance(double HorizontalSpeed, double JumpVelocity, double Gravity)
Source/ValStrike/VSMovementRules.h:74:18: note: previous declaration is here
   74 | constexpr double Gravity = Cs2::Gravity * HeightScale;
```

- Cause: the parameter `Gravity` of `JumpDistance` shadows the namespace constant `VSMovementRules::Gravity`. Unreal compiles the module with shadowing as an error.
- Owner's lead: `JumpApex` and `Airtime` (lines 60 and 66) also take a `Gravity` parameter but come before line 74, so they compile. Renaming the parameter (or the constant) resolves it. Engine-free tests would not catch this, because they don't build with UE's `-Werror -Wshadow`.

### Main inputs (#69, #70, #71)

Before #74, these had never been built. All three unity files compiled with no other error, so GPT-12/13/14 source raised no compile diagnostics here. The link was never reached, and none of their runtime suites ran, so they are **not verified**.

## Limits

- Only the compile step ran. No link and no suite results.
- The other six INT-06 suites stay pending until GPT-15 provides a new tip.

## Completion handoff

- Commit/PR: this report only, on `claude/int-06-report` (docs-only PR). The disposable `claude/integration-06` stays local.
- Changed: `docs/workstreams/integration-06.md` (new)
- Checks actually run and result: build FAIL (INT-06-D1). Suites 1–7 were not run.
- Remaining defect/dependency: INT-06-D1, owner GPT-15
- Next action/owner: GPT-15 fixes the shadowing and pushes a new tip. The lead forwards the SHA. Integration then reruns INT-06 from step 0.
