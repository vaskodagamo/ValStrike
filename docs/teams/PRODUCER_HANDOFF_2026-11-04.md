# Producer checkpoint — Direktor to Direktor (2)

2026-11-04, Europe/Berlin. Human authorization was read in Direktor (2): “Yes, authorize these coordination messages,” explicitly covering checkpoint/producer transfer and routine approved art follow-ups. This handoff becomes effective when the outgoing coordinator publishes its final checkpoint. No cross-thread reply is needed; incoming producer reads the final result. No more central writes or worker starts from outgoing Direktor afterwards.

- Incoming producer/art lead: **Direktor (2)**, `01a1a67c-4861-7e62-a15e-505444092624`, `/Users/webproduktion3/.codex/worktrees/10a1/ValStrike`.
- Outgoing checkpoint: branch `codex/hands-knife-dispatch`, [PR #72](https://github.com/vaskodagamo/ValStrike/pull/72), final SHA in outgoing chat's final response. It includes the earlier weapon assignment records; PR #68 merged while this checkpoint was prepared. Current main inspected at `e29963e`.
- Central status is now your responsibility. Start from the published checkpoint/current main in your own checkout, preserving your unrelated local glove reference files. Do not modify the outgoing root checkout or any artist's checkout.
- Scheduling setup and verified Claude automation are owned by your session. The outgoing chat did not create or claim an automation or dispatch a Claude process. No daily credit cap was answered in the inspected user replies; only the explicit job allocations below are established here.

## Live bounded jobs (High reasoning, separate worktrees)

| Task / chat | Thread ID | Worktree / branch | Authorized Meshy spend |
| --- | --- | --- | --- |
| ART-17 — Art — Meshy R02 rifle | `01a1a6c3-9f0b-76b3-aabd-ccfed653040f` | `.parallel-worktrees/art17-rifle02` / `codex/art17-rifle02` | 30 credits, one textured standard Meshy 7.1 image task |
| ART-18 — Art — Meshy P02 pistol | `01a1a6c3-ae6d-7bc3-b164-ee3e417314ec` | `.parallel-worktrees/art18-pistol02` / `codex/art18-pistol02` | 30 credits, one same-type task |
| ART-19 — Art — Meshy first-person hands | `01a1a6cf-0f10-73d0-8c33-29cb863d7b33` | `.parallel-worktrees/art19-fparms02` / `codex/art19-fparms02` | 30 credits, one right-hand/forearm draft; pair/rig local |
| ART-20 — Art — Meshy K01 knife | `01a1a6cf-270a-7e70-a435-a966fd3a5822` | `.parallel-worktrees/art20-knife01` / `codex/art20-knife01` | 30 credits, one textured knife draft |

Root for relative paths is `/Users/webproduktion3/Documents/ValStrike`. Total **authorized** 120 credits, in two separate explicit 60-credit approvals. No paid retry, extra remesh/retexture/rig/motion/convert or purchase is covered. Actual consumed amounts are tracked in each worker's job manifest; outgoing producer has not audited completed consumption and must not label the full allowance spent. Workers submit once and resume task IDs. They own source/docs only; none has an Unreal slot. Do not interrupt/restart them simply to enforce a new concurrency cap—finish the existing bounded batch before starting more work.

ART-19's older rejected ART-05 predecessor is stopped and must stay preserved. The source for its local frozen glove-pair design reference is in ignored root `Artifacts/FPArms02Reference/`; the actual Meshy input must be one hand, not the pair contact layout. The worker owns new reference/source files. Knife **combat/equip logic is not implemented** by ART-20.

## Native integration: immediate outstanding action

Reviewed character pilot PR #65 and four-prop PR #63 are merged; INT-05 acceptance/release #64 is merged. ART-15/16 are frozen and stopped. The old FPArms01 was procedural, rejected and unimported. No new Meshy character/props/hands/knife native assets were present in main **or** existing Claude integration checkout at final audit.

Existing external Claude native owner: `.parallel-worktrees/int-art-01-meshy-playtest`, branch `claude/int-art-01-meshy-playtest`. Its preflight/importer is committed and its latest handoff still cites cleared source/slot gates. Do not create a competing integrator or overwrite its checkout. Resume it with the exact [cleared-gate handoff](../tasks/integration/INT-ART-01-RESUME.md). It can import/reopen, place/wire and verify the already-approved character/four props without waiting for the four new art jobs. The Y-handedness concern now has an explicit bounded native transform adjustment allowance; source/world-space layout contracts stay preserved.

Recheck the live Claude engineering board before engine work. INT-05 was released; new gameplay workers include GPT-12–15. GPT-15 owns the movement/jump/crouch portions of VSCharacter; native presentation edits must stay in their separate assigned areas or serialize any overlap with the gameplay lead. All heavy Unreal jobs use `Scripts/with_ue_slot.py`; busy 75 is stop/queue, never polling. One combined build/affected verification, not per-artist builds or a new test matrix.

R02/P02/FPArms02/K01 native import and knife gameplay need subsequent explicit integration ownership after source readiness; they are not silently added to INT-ART-01. User is waiting for a concrete Site01 playtest, screenshots and exact other-Mac instructions, not merely merged source folders. Preserve the source/import/runtime distinction and user-controlled PR merges.
