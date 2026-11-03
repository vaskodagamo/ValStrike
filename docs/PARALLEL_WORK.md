# Parallel development ownership

Updated: 2026-11-03. The user requested parallel chats to accelerate the MVP. This document assigns independent deliverables; it does not waive milestone acceptance gates.

## Workstreams

| Chat / stream | Owns | First deliverable |
| --- | --- | --- |
| Plan FPS game MVP (integration) | Shared contracts, central docs/backlog/log, existing regression map, configuration, build/network verification, merge decisions | Combine reviewed changes and rerun movement/combat/UI checks |
| Add the first game assets (existing) | `Art/Source/`, `Content/Art/`, asset manifests and logs in its existing worktrees | Original stylized environment kit and rifle model |
| Implement rifle gameplay | `Source/ValStrike/Combat/`, `VSCharacter.h/.cpp`, `Tests/Combat/`, uniquely named combat smoke files/scripts, `docs/workstreams/combat.md` | Server-owned rifle, ammo/reload, traces, health/death and focused tests |
| Build HUD and session controls | `VSHUD.h/.cpp`, `Source/ValStrike/UI/`, new `VSPlayerController.h/.cpp`, `ValStrike.Build.cs` only for UI dependencies, `VSGameMode.cpp` only controller-class wiring, `Tests/UI/`, `docs/workstreams/ui.md` | Readable HUD and working join/menu controls |
| Create pistol and upgrade weapon colors | Pistol01 assets plus scoped Rifle01 material/color revisions in its own worktree | Original pistol and a cooler shared rifle/pistol finish; see WEAPON_ART_DIRECTION.md |
| Build the first tactical map | `Content/Maps/Site01/`, `Art/Source/Maps/Site01/`, `Tests/Maps/Site01/`, `docs/maps/site01/`, `docs/workstreams/tactical-map.md` | Original one-site 2v2 map using the finished environment kit; see MAP_BRIEF.md |
| Implement round and objective rules | `Source/ValStrike/Rules/`, `Tests/Rules/`, `Scripts/test_round_rules.py`, `docs/workstreams/round-rules.md` | Deterministic match/objective model and tests; see ROUND_RULES_BRIEF.md |

The current integration base is `codex/bootstrap-fps`. Each new stream uses its assigned Git worktree and branch. Never switch the integration checkout's branch or edit another stream's worktree. Central documentation is updated by integration; each stream maintains its own append-only log and handoff. The existing asset chat has its own scope and is already building the first rifle model; do not duplicate its art.

Update on 2026-11-03: the original rifle asset chat has completed `cac8151` and is idle. The user requested a new pistol chat and a stronger rifle/pistol palette. The new weapon-asset workstream owns that follow-up in a separate checkout; it reuses the completed rifle and does not rebuild its accepted geometry. Pistol art does not expand the single-rifle gameplay milestone.

The subsequent tactical-map and round-rules chats are independent foundations for PLAY-01 and RULE-01 through RULE-05. The map chat owns only the new Site01 map namespace; VS_Graybox and defaults remain integration-owned. The rules chat owns isolated C++ domain logic, not the active gameplay/UI actors. MAP_BRIEF.md defines version 1 of the authored spawn/site/recovery metadata. Integration will wire that metadata and the rules API into the server actors after review. Unit tests and a saved map do not complete the networked match milestone.

## Shared contract

`VSHUDData.h` is integration-owned. `AVSCharacter::GetHUDData() const` returns a plain `FVSPlayerHUDData` snapshot:

- `bHasHealth`, `Health`, `MaxHealth`, `bAlive` describe available health state.
- `bHasWeapon`, `MagazineAmmo`, `MagazineCapacity`, `ReserveAmmo`, `bReloading` describe available weapon state.
- At this baseline both availability flags are false. UI hides unavailable fields; missing health must not be displayed as death. No fake scores or timers.
- Combat populates the getter from authoritative replicated components. UI only reads this contract and does not include combat implementation headers or edit `VSCharacter`.
- UI calls `AVSCharacter::StopCombatInput()` before taking input focus, and flushes pressed keys. Combat implements this hook to release held fire through its normal validated path. It does not pause the server or cancel a reload by itself.

Propose contract changes in the stream handoff rather than silently creating a second interface or changing another stream's files. The contract is intentionally native C++; use code widgets or existing engine drawing without requiring shared Blueprint assets.

## Rifle gameplay scope

Implement GUN-01 and the independent GUN-02 foundation against the existing movement baseline. One configurable hitscan rifle, fixed loadout, fire cadence, magazine/reserve ammunition and reload; server-owned health and death. Bind fire/reload through existing Enhanced Input. Use engine replication and authoritative traces; clients never supply a final victim/damage result. Validate ownership, alive state, ammo, cadence, reload, duplicate shot requests and view plausibility. Current server positions are the initial hit-registration policy. Round-phase validation stays an explicit integration dependency until round rules exist.

Keep cosmetics minimal and use temporary engine geometry if needed. Do not wait for, import, or modify the rifle art branch. Preserve the movement harness and current movement behavior. Any reset/test controls must be explicit development-only code excluded from Shipping. Test successful firing/reloading/damage plus invalid requests and cover obstruction; distinguish unit tests, editor network evidence and packaged acceptance. Do not claim a complete GUN-04 gate or production anti-cheat.

## HUD and session scope

Implement the independent PLAY-03 UI foundation: crosshair, available health/ammo/reload/death state, Escape menu, private server address entry/connect, resume, and quit/disconnect behavior with clear failures. Keep it usable by keyboard and mouse. Validate addresses before travel; do not concatenate arbitrary console commands. Multiplayer menu opening must not pause the server. A local placeholder snapshot is unavailable data, not simulated gameplay.

The UI may add a custom player controller and wire only its class in the existing game mode constructor. Combat owns pawn input and state; UI owns menu focus. Avoid changing map, project descriptor, engine config or character source. If adding Slate/UMG, keep build dependencies scoped. A live UI smoke check must work against the baseline with unavailable combat data, then integration verifies real combat updates. Mouse sensitivity, round HUD, final sound and broader settings can remain documented follow-ups rather than invented functionality.

## Verification, resource use and handoff

- Use Unreal 5.8.4 at `/Users/webproduktion3/UE_5.8/UE_5.8` and Xcode 26.1.1. Each worktree has its own generated outputs; never share/copy live Binaries or Intermediate between them.
- Build through `Scripts/ue.py`, which passes `-WaitMutex`. Avoid simultaneous full-engine builds. Prefer bounded tests and close only processes started by the current stream. The machine has 24 GB RAM; do not keep several unnecessary editors running.
- The asset chat may be rendering/importing. Do not use global quit/kill commands or take over its interactive editor. Use explicit project paths and process-specific logs.
- GitHub source access last returned 404. Continue available editor work; do not repeatedly request account setup or describe a launcher-engine process as a packaged dedicated server.
- Keep Valorant-like stylized graphics, not CS2-like photorealism. Use original art and clear silhouettes.
- Commit and push only the stream's own branch. Do not merge into the integration branch or another stream. Hand back branch/commit, changed paths, exact verification evidence, remaining limitations, and integration instructions in the final message and stream log.
- Integration will merge one stream at a time, resolve shared-file conflicts, run the movement and combat regressions, then verify the HUD with real replicated data. A clean branch alone is not acceptance.

## Active chat references

- Implement rifle gameplay: chat `01a1a1f0-0648-7e91-aa70-f1452b983fe1`, branch `codex/rifle-gameplay`, checkout `.parallel-worktrees/rifle-gameplay`.
- Build HUD and session controls: chat `01a1a1f0-118e-7a92-837b-ef86f3c7aa60`, branch `codex/hud-session`, checkout `.parallel-worktrees/hud-session`.
- Existing Add the first game assets: chat `01a1a1d4-ad5c-76a3-8896-d9d65615be14`; its environment and rifle-art branches are separate from the two new gameplay/UI workstreams.

Both new chats started from shared-contract commit `b245939` on 2026-11-03. They are local project chats operating in the explicitly assigned Git worktrees; their app-level project directory may still show the main ValStrike folder.

- Create pistol and upgrade weapon colors: chat `01a1a1fa-ad9c-7c53-b206-86797861fb26`, branch `codex/pistol-weapon-finish`, checkout `.asset-worktrees/pistol-weapon-finish`. Started from the current integration branch with completed rifle commit `cac8151` merged into this task branch only.

- Build the first tactical map: chat `01a1a208-0727-72e3-8082-407a1fd80bc4`, branch `codex/tactical-map`, checkout `.parallel-worktrees/tactical-map`. Integration brief base `fc3e857` plus completed environment kit merged in task-local commit `b2647a8`.
- Implement round and objective rules: chat `01a1a208-8ed9-7651-8753-525b2b158f24`, branch `codex/round-rules`, checkout `.parallel-worktrees/round-rules`, starting from `fc3e857`.

The map and rules chats started on 2026-11-03 and acknowledged their assigned checkouts. They are local project chats; all edits must use those explicit checkout paths even if the app displays the parent project directory.
