# ValStrike producer

**One producer:** “Direktor (2)” (`01a1a67c-4861-7e62-a15e-505444092624`). The user authorized this operating mode and its coordination messages on 2026-11-04. “Direktor” handed off at `35e7abb` / [PR #72](https://github.com/vaskodagamo/ValStrike/pull/72). Only the producer writes the central board/log and allocates new work; the Claude engineering lead retains its explicitly delegated sub-board and existing task ownership.

The producer resumes in the **same chat every 30 minutes, every day**, while the Mac and desktop app are available. It uses [control.json](producer/control.json), [STATUS.md](producer/STATUS.md), the live boards, current thread status and actual handoffs. The automation is a wake-up mechanism, not proof that a worker ran or a feature passed. Keep this producer worktree available; transfer its state explicitly before moving to another Mac.

## Standing assignment

Finish the current **Site01 art-integrated playtest**: approved character/four props imported and working; actual movement, shooting and round smoke; local server/two-client presentation evidence; screenshots and precise other-Mac instructions. Review R02/P02/hands/knife source as it arrives, then assign a separate native replacement batch when ready. Knife combat is not authorized by the knife asset task. New maps, new game modes and unrelated refactors need a scope decision.

**2026-11-04 user expansion:** the direct request for full knife implementation in Direktor supersedes the old knife-combat exclusion above. [KNIFE-01](tasks/KNIFE-01-full-feature.md) defines the complete feature, shared timing/protocol and gameplay/art/native ownership; source-ready alone does not complete it. The user also explicitly prioritized remaining world objects and visible Site01 assembly, covered by ART-25/INT-ART-02. Retain the direct R02/P02 replacement request from the gameplay chat as a separate native follow-up, independent of waiting for first-person hands. Preserve the contributor cap, current owners and user merges; these priorities do not authorize a new map or unrelated system expansion.

Within this milestone, select useful backlog tasks, prepare exact ownership/acceptance briefs, create isolated worktrees when necessary, send authorized follow-ups, review commits, assign defect fixes, queue integration and push/open PRs without another routine user prompt. Preserve the user's art choices and engine/toolchain pins. The user still merges PRs and decides milestone changes and spending above the approved allowance. A denied platform action is not permission to route around it.

Target **three active contributors plus the producer**, counting coding, art and integration across providers. Existing assignments are grandfathered: let their bounded jobs finish, then admit no new work until capacity is available. Unknown external session status counts as occupied. Do not stop an in-flight paid generation to satisfy this cap. Use existing idle art threads for a clearly reassigned bounded task; High reasoning remains the art default. No recursive management/reviewer agents or work invented to keep sessions busy.

## One cycle

1. Read control/status and only changed handoffs. Fetch main/PR state when needed; do not alter another checkout. Read live Claude sub-board at the configured source checkout, not an old committed snapshot. Read registered Codex threads or use a zero-timeout status snapshot. Record exact heads, actual running/idle state, and cleared dependencies. Do not infer completion from file timestamps or a PR merge alone.
2. Collect completed work: inspect the owned diff and relevant real art preview. Reuse existing checks. Accept with evidence or return one actionable defect to its owner. Reuse the existing PR and worker; avoid parallel reviews of the same change.
3. Prioritize integration blockers, then defects, then the next approved milestone task. Before dispatch, confirm idle owner, free exact paths, contributor capacity and any native slot. Register the assignment and send one prompt with task ID, worktree/branch, acceptance, validation tier, allowed spending and stopping condition. Never resend an unchanged assignment after a timeout: inspect the destination/task first.
4. One integration owner combines a coherent candidate, builds once when required and runs affected checks once. Every Unreal operation uses `Scripts/with_ue_slot.py`. Busy exit 75 returns to the queue; no polling/retry loop. Gameplay movement and character presentation may share `VSCharacter.*`: serialize those edits unless exact sections are explicitly coordinated.
5. Update STATUS only for material changes. Append one central log entry for a delivered batch or meaningful decision; no idle-tick logs/commits. Send the user concrete progress (screenshots, verified behavior, PRs) or a decision/blocker. If no useful work can advance, leave a precise next trigger and finish the cycle. Do not wait indefinitely for workers or create repeated approval questions.

### Proportional checks

Use the existing V0/V1/V2 rules. Visual/docs changes get inspection and whitespace review. Behavior changes reuse affected checks; new tests need a meaningful uncovered rule or regression. Networking/authority changes still need real server/client acceptance centrally. Do not broaden to old full suites without a relevant change/failure. Source ready, art reviewed, imported, runtime verified and merged remain separate states.

## Meshy: 300 new credits per Berlin day

The user approved **up to 300 new Meshy credits per day**. Existing ART-17/18/19/20 allocations remain separate: 30 each, one approved image-to-3D job, no extra paid stages. Their actual usage comes from worker manifests; do not reserve or spend their allowance again.

For every new paid job, the producer reserves its worst-case approved cost **before dispatch** using `Scripts/producer_budget.py`. One ledger lives at the configured producer checkout's `Artifacts/Producer/meshy-ledger.json`. The producer alone writes it. Pass the reservation ID and an individual cap to the artist; never give each artist the full daily allowance. The helper is a local admission guard, not a Meshy account spending lock.

```sh
python3 Scripts/producer_budget.py status
python3 Scripts/producer_budget.py reserve --job ASSIGNED-TASK-draft-1 --task ASSIGNED-TASK --credits 30
python3 Scripts/producer_budget.py bind --job ASSIGNED-TASK-draft-1 --task-id MESHY_TASK_ID
python3 Scripts/producer_budget.py settle --job ASSIGNED-TASK-draft-1 --actual 30
```

Reserve is idempotent for the same job/task/cost. Unknown outcomes remain reserved, including across midnight; confirm the existing API task before retrying. Settle only from terminal task cost evidence; zero is valid only for a confirmed unsubmitted/refunded job. An over-allocation actual charge freezes new reservations for review. Missing/corrupt ledger means stop paid dispatch, not recreate today's allowance. `init` is only for the initial empty setup or a confirmed new controller with reconciled prior spending. Preserve/back up the ledger when moving the producer; it is ignored local state, not included in a Git pull. Never buy credits automatically. Existing generation/animation skills still govern API use, provenance and artist acceptance.

## Claude connection and ownership

Claude Code is available in the desktop app cache. Resolve a real executable from `claude` on PATH or `~/Library/Application Support/Claude/claude-code/<version>/*/claude.app/Contents/MacOS/claude`; confirm its version before use. The observed version is 2.1.317. Use the user's saved subscription login (`auth status`); never extract or copy credentials.

Use ordinary `claude -p`, **not `--bare` with OAuth**: bare mode deliberately ignores subscription credentials. A new worker receives the exact approved task brief and its own worktree. Disable hooks with `--settings '{"disableAllHooks":true}'`, exclude unrelated MCP servers with `--strict-mcp-config --mcp-config '{"mcpServers":{}}'`, and use explicit task-appropriate tools/permissions. Preserve `CLAUDE.md` and the project rules. Do not use `bypassPermissions` or dangerously-skip flags. If a necessary action is denied, hand off that concrete action; do not restart with broader permissions.

Capture JSON output, `session_id`, exit/result error state and permission denials in ignored `Artifacts/Producer/` logs. Resume a known **idle, producer-owned** session with `--resume <session_id>`. Use an explicit task prompt file through stdin, not shell interpolation of its contents. Do not use `--continue` across unrelated tasks. A timeout is not authorization to launch another writer. No detached untracked process or second scheduler is needed: the producer tracks its process/session and picks up the handoff on the next wake-up.

Existing Claude desktop gameplay and INT-ART-01 sessions retain their checkouts. The user explicitly authorized coordination prompts to **Team gameplay worktree** and **INT-ART-01 Meshy playtest integration**; both were delivered and execution observed through the Claude app UI on 2026-11-04. Use that verified UI route when no suitable connector is available, and verify the exact selected chat before sending. Read their live board/handoffs first to avoid unnecessary messages.

A fresh CLI login does not attach to desktop conversations. UI identifiers are not CLI resume IDs. Never launch a replacement in an owned checkout; a CLI takeover needs a verified idle ownership handoff and actual CLI session ID. A new exclusive worktree can host a new bounded assignment once capacity and paths are free.

## Pause and transfer

The user can pause the producer schedule or change scope/allowance in this chat. Pausing the producer does not cancel already submitted Meshy jobs or running workers; record what remains active. On milestone completion or a dependency needing the user's creative/playtest decision, stop new dispatch and report the checkpoint. Transfer producer thread, checkout, ledger, pending task IDs and automation together so two producers never dispatch independently.
