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

Next: publish TEAM-02 for user merge; inspect final R02/hands/knife handoffs and real comparisons as they arrive; reconcile the recorded QA slot release with INT-ART-01. Keep GPF-04 queued until contributor capacity and shared paths permit a bounded dispatch. Each native owner records the exact candidate and actual results; user retains merges and other-Mac playtest acceptance.
