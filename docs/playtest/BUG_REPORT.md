# Mac playtest bug report

Copy this template for each reproducible issue. Mark unknown or untested fields explicitly. Include a failed join as a report even if gameplay never starts.

## What happened

- Short title:
- Impact: cannot start / cannot join / blocks round / gameplay defect / visual or input issue
- First observed: date, time and timezone (Europe/Berlin preferred)
- Frequency: attempts affected / total attempts
- Expected behavior:
- Actual behavior, including exact on-screen error:
- Smallest numbered reproduction steps:
  1. [Starting state]
  2. [Exact action/input]
  3. [Observed failure]
- Recovery or workaround attempted, and result:

## Same-source setup

- Coordinator-published full commit:
- Host actual `git rev-parse HEAD`:
- Client A actual commit:
- Client B actual commit:
- Dirty working-tree output, or explicitly clean:
- Preflight output/report from each Mac, including asset hashes and any failures:
- Build command, exit result and log from each Mac:
- Each Mac's `Saved/Playtest/editor-build.json` helper receipt and any missing/stale/mismatch error:
- Unreal version **and changelist**; engine installation path:
- Xcode version/build; selected developer directory; Metal version:
- Map: `graybox` / `site01`
- Mode: `practice` / `match` (also record the actual mode shown in logs/HUD)
- Evidence type: development Editor server/client / PIE / other, specify
- Session runner command and run/report directory:

## Machines and connection

| Role | Mac model/chip | RAM | macOS version/build | Resolution/settings | Connection |
| --- | --- | --- | --- | --- | --- |
| Server host | | | | headless or specify | Ethernet/Wi-Fi |
| Client A | | | | | Ethernet/Wi-Fi |
| Client B | | | | | Ethernet/Wi-Fi |

- Server host LAN IPv4 and port used by the remote Mac:
- Are both clients on this Mac, or on separate Macs? Specify:
- Same non-isolated LAN confirmed? Guest Wi-Fi/VPN/managed network present?
- macOS inbound-app / Local Network permission prompt and decision, if shown:
- Server was listening before join? Evidence:
- Join attempt time; client error time; server arrival/rejection evidence:
- RTT/loss/jitter: measured values and method, or **not measured**
- Synthetic network impairment: exact settings, or **none configured**

## Gameplay/input context, when applicable

- Player/team/attacker-or-defender/spectator state:
- Round number/phase, score, objective state and timer:
- Health, magazine/reserve ammo and reload state:
- Position/route/nearby landmark; screenshot or coordinates if available:
- Exact physical inputs: mouse direction/buttons, keys held/released, order:
- Escape/menu state, window focus and recent Alt-Tab/application switching:
- What the server, acting client and observer each showed:
- Did the issue repeat after a fresh server and rebuilt same-revision clients?

## Attach evidence

- Host server log and both client logs for the same attempt, with exact filenames.
- Runner report/commands and preflight output from each machine.
- Build-helper receipt from each checkout, with its source/engine identity and module/manifest hashes; retain it as evidence, never copy it into another checkout to bypass validation.
- Full error text and a short timestamped excerpt; retain the complete relevant log.
- Screenshot/video filenames, capture time and which client produced them.
- Crash report under `Saved/Crashes/` if the process produced one.
- Final process exit status or whether it was closed manually.

Review attachments for credentials or unrelated personal data before sharing. Share private LAN addresses only with the playtest coordinator. Do not label a local loopback result as LAN, an automated traversal as a human playtest, or an Editor process as a packaged build.

## Triage and retest — coordinator fills

- Backlog/issue ID and owner:
- Confirmed reproduction and root cause:
- Fix commit:
- Retest revision/machines/conditions:
- Retest result and evidence paths:
- Remaining limitations:
