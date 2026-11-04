# Producer checkpoint

2026-11-04, Europe/Berlin. Owner: **Direktor (2)**. Read live sources before acting; this is a checkpoint, not a process monitor.

## Latest checkpoint — 16:18 scheduled cycle

This section supersedes the earlier states retained below.

- **Delivered native batch:** #93 merged, final head `247518542c7212e8391288ca316a5ea4e9dc031a`, runtime candidate `34e6e789fa58a28a3a53d22d589b46997e3a0aa6`. Character/four props imported, saved and reopened; Site01 486/486, visual weapons 13/13, Site01 match 29/29 with server + two rendered clients, movement 69/69 and rifle presentation 39/39. Producer read the actual owner report; no producer rerun. `git diff` confirms no Source/Content/Config/Scripts/Tests difference between final head and merged main `a1c2371`. [Evidence](../assets/meshy-pilot-integration/README.md), [other-Mac instructions](../playtest/meshy-pilot.md).
- **Native limits:** remote pistol still floats; crouched visual remains standing, gait/directional coverage is limited, body hides on death. Two-Mac LAN, packaged, four-player, performance and human feel are pending. Native owner visibly finished and stopped its assignment; current user checkout assistance is not a new integration job.
- **Gameplay resumed:** full GPF-04 prompt delivered through the authorized existing Claude lead. Lead acknowledged dispatch to the existing GPT-15 owner; live board records active and checkout is on `claude/gpf-04-steady-corrections` from `a1c2371`. Engine-free movement correction only, preserving native presentation. Old queue ordering superseded: combine GPF-04 acceptance with GPF-02/03 in one future candidate; no automatic QA launch.
- **Art review:** ART-22 #92 merged, source `bcd377d`, frozen handoff `b179599`, owner idle/stopped. Producer inspected matched original/source/export glove close views: preservation passes. Actual reverse pistol image confirms buried support fingers/thumb, so native hand integration remains held. [ART-24](../tasks/art/ART-24-fparms03-pistol-contact.md) goes back to that same owner at High: local P02 pose/weight repair, zero credits, same isolated checkout/new branch, no native work. Owner acknowledged the new branch at `9dcd04f` and began a pose-only correction first.
- **Third-person pistol source:** ART-23 merged and frozen at `9232dad` (source `887de0a`), [#95](https://github.com/vaskodagamo/ValStrike/pull/95), owner idle. Producer reviewed actual P01 original/candidate/FBX views and 5 m composition: accepted for stationary third-person pilot, with angular glove curls and small contact gaps documented. This does not claim the game is fixed or approve P02 native replacement.
- **Capacity/budget:** R02, P02, K01 and props chats confirmed idle; ART-23 frozen. GPF-04 and ART-24 are the two admitted implementation contributors. Ledger checked: 30 new credits spent, 270 available; no new paid job, test or Unreal run in this cycle. Earlier task caps remain separate.

Producer checkout fast-forwarded to user-merged main `9dcd04f`, including #94–96, before publication. ART-23 is merged source, still awaiting native application.

**Next trigger:** frozen GPF-04 handoff plus reviewed/merged ART-23 #95. Prepare one native/QA assignment to apply the pistol animation and matching offset together, preserve gameplay authority/hit zones, then build once and run affected movement (LAN/typical), session, reachable-spectate match and pistol presentation checks. Use the slot wrapper; no contributor builds. Keep first-person hands out until ART-24 contact review passes. User retains merges and other-Mac playtest acceptance.

## Prior checkpoints (historical)

## Operating state

- Same-chat heartbeat **`valstrike-studio-producer` is ACTIVE**, every day/every 30 minutes. Creation, view and saved automation configuration confirmed the interval and target thread `01a1a67c-4861-7e62-a15e-505444092624`. Keep this worktree, the Mac and app available. First scheduled invocation observed 2026-11-04 at 15:48 Europe/Berlin; earlier cycles ran interactively.
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

## Scheduled cycle — 2026-11-04, 15:48 Europe/Berlin

- R02 correction #89 (`f209b664`) is published and owner idle. Producer inspected the matched untouched/source/actual-FBX images: receiver/rail surface quality and material response are retained; local grip fitting is intentional. Surface-preservation gate passes; character contact, optimization and native acceptance remain pending. No rerender or extra test.
- ART-22 job `01a1a702-abc1-76dc-97f3-b5ad96d100e1` is SUCCEEDED with 30 actual credits in the worker manifest. Central reservation bound and settled: 30 spent / 270 available. Owner is actively authoring the compatible rig and all three frozen weapon studies; no additional paid allowance.
- Read-only Claude UI confirmed native character/four props imported and saved/reopened, Site01 placement contracts checked, and visual server/two-client checks run. Owner corrected the observed bind-pose/import defects; Site01 match acceptance continues. No duplicate native dispatch. The actual observer pistol image shows an unfitted floating pistol, so source-only ART-23 is reserved for the idle former ART-15 owner, using a new worktree and zero credits. Existing crouch/gait limits stay open.
- Existing GPF-02/03 are merged; their new acceptance remains queued. No GPF-04 dispatch while native character integration owns the overlapping source. No idle worker is launched just to fill capacity.

ART-23 prepared clean at `fd7621c` and full bounded assignment delivered at High to the existing idle character artist. New source writer owns only PistolPilot and its task docs; ART-15 remains frozen. Producer will wait for a frozen source handoff before assigning any native pistol change.
