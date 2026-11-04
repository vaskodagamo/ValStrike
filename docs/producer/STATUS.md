# Producer checkpoint

2026-11-04, Europe/Berlin. Owner: **Direktor (2)**. Read live sources before acting; this is a checkpoint, not a process monitor.

## Operating state

- Same-chat heartbeat **`valstrike-studio-producer` is ACTIVE**, every day/every 30 minutes. Creation, view and saved automation configuration confirmed the interval and target thread `01a1a67c-4861-7e62-a15e-505444092624`. Keep this worktree, the Mac and app available. No scheduled invocation has been observed yet; the first coordination cycle below ran interactively.
- Outgoing producer handoff: `35e7abb`, merged #72. Direktor has no independent dispatch/central-write authority. Its bounded ART-21 skill follow-up finished at `fbec588`, merged #80.
- New Meshy pool: **300 credits per Berlin day**, atomic local ledger initialized once. At this checkpoint: 0 newly committed/spent, 300 available. Prior ART-17/18/19/20 caps remain separate (30 each); actual earlier usage belongs to their manifests. Do not reset the ignored ledger or give every worker the whole daily pool.
- Capacity: grandfather the current art/gameplay/native assignments, then admit no new contributor work until fewer than three contributors are active. Unknown external activity counts as occupied. No new paid job or Unreal action was run by this producer setup.

## First coordination cycle

- Existing Codex art chats received the producer handoff and source-preservation direction. ART-17 R02 is correcting the original cleanup; #76 was already merged, so a new main-targeted correction PR was explicitly requested. ART-18 P02 is idle after its correction; #81 is merged. ART-19 hands/rig and ART-20 knife restoration remain active. Hands' R02 fit uses an earlier source and is explicitly provisional until the corrected rifle freezes. ART-15/16 remain frozen/merged.
- Reviewed ART-21's skill and linked preservation guide. Installed the same two files for Codex and Claude; byte equality with the merged repository source confirmed. SHA-256: SKILL.md `123c957a47c01f8050b5d25077074dfe4c21bd4a8063836d2fa0ea78ebce258e`; guide `b9cb7e3ff5b9da086a729dc50e43dcc64e1d2036cb17de8a4fd308369b9a225d`. No generation/auth/budget behavior changed. Source acceptance needs matched original/source/export views; reduced or reconstructed assets that look worse stay held.
- Claude subscription sign-in verified with CLI **2.1.317**. Tools-disabled smoke command returned `CLAUDE_RUNNER_OK`, exit 0, `is_error:false`, one turn. Normal mode was required; `--bare` ignored OAuth and failed before inference. No credentials copied and no competing CLI writer started.
- User-authorized coordination messages were visibly delivered to **Team gameplay worktree** and **INT-ART-01 Meshy playtest integration**; both began processing. The live gameplay board now acknowledges the sole producer and queues subsequent dispatch through it. Desktop UI IDs are recorded only as UI identities, not CLI resume IDs.
- Engineering: INT-06 rerun and D3 attribution are documented in open #82; board update #83 records GPF-04 queued through the producer. GPF-02 connection failure and GPF-03 spectate scenario were already assigned before handoff and retain their owners. Movement corrections under latency remain an open defect; do not flatten the result to “all verified.”
- Native integration: existing INT-ART-01 owner resumed preparation using the cleared ART-15/16 handoff. Heavy work remains conditional on recorded release of INT-06's final QA comparison and the slot wrapper. Merged #78 is preflight/importer preparation, not native/runtime completion. One owner imports character/four props; new weapons/hands/knife require a later explicit batch. Serialize any GPF-04 shared-character edits with native presentation work.

## Verification and next trigger

Four focused spending-guard tests passed: idempotency/settlement, concurrent cap enforcement, unresolved jobs across Berlin midnight, and missing/reset/overrun refusal. Command: `python3 -m unittest discover -s Tests -p 'test_producer_budget.py' -v`. No full suite, build, Meshy generation or game runtime test was run for TEAM-02. Review/diff/link checks cover the policy files.

Publication: implementation `a57cbda`, [PR #88](https://github.com/vaskodagamo/ValStrike/pull/88), merged by the user.

Next: inspect final R02/hands/knife handoffs and real comparisons as they arrive; reconcile the recorded QA slot release with INT-ART-01. Keep GPF-04 queued until contributor capacity and shared paths permit a bounded dispatch. Each native owner records the exact candidate and actual results; user retains merges and other-Mac playtest acceptance.

## Universal-hands priority update — 2026-11-04

Direct user request verified in Direktor. ART-22/FPArms03 is reserved for the same hands chat after its ART-19 frozen checkpoint, with a separate worktree and exact source paths. Checkpoint instruction delivered; full dispatch waits for safe completion. Reuse good glove geometry first; optional single 30-credit draft reserved as `ART-22-FPArms03-draft-1`, not yet submitted. Daily new pool now has 30 reserved / 270 available. No paid Meshy operation by producer.

K01 #85 and P02 #81 are merged/frozen. GPF-02/03 implementations are ready and their engine checks queued; the live gameplay board records INT-06 release. R02 correction, current hands and native integration are the continuing contributors. ART-22 replaces hands work after checkpoint; keep GPF-04 queued until capacity/shared paths clear.

ART-22 worktree prepared clean at `86549e74e5d2a83c4401a6ceb650e2c9c6d655cb` (main `159431d` plus brief). ART-19 checkpoint `afa582a` is committed and uploading; wait for publication/safe stop before dispatch. Producer follow-up branch: `codex/universal-hands-dispatch`; #88 is now merged.

INT-ART-01 received the explicit INT-06 release notice in its existing Claude chat and began reconciling the candidate for native work. Its one engine-slot job is now next; GPF-04 remains queued to protect the shared character source. No native result is claimed.

ART-22 dispatch completed: ART-19 owner became idle with clean stopped checkout, frozen source `afa582a`, handoff `45cf24d`, PR #90. New worktree finalized at `f3e55da` and the full ART-22 prompt delivered at explicit High reasoning to the same chat. Producer made no further artist-checkout edits after handover. Registry now identifies ART-22 as current and retains ART-19 history. Dispatch PR [#91](https://github.com/vaskodagamo/ValStrike/pull/91); actual generation/reuse decision is pending the artist, reservation remains unsubmitted.
