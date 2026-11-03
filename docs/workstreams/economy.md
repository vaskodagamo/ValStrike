# Economy workstream log

Append-only. Dates and times are Europe/Berlin.

## 2026-11-03 — Phase 1: round economy, server adapter and buy menu

**Request and scope.** The user asked for a round economy: credits per player, earned through the match and spent on weapons during the buy phase. This is a user-requested scope expansion. The MVP plan lists "weapon economy" as a later feature, and the backlog defers it as NEXT-03 ("Second weapon, economy and tactical utility"). This stream implements only the economy part of NEXT-03. The second weapon's gameplay belongs to the parallel Weapons stream; utility is not started. The integration session owns the central docs and decides how to record this in BACKLOG.md and MVP_PLAN.md.

Branch `claude/economy-77e1c3`, based on `claude/playtest-base` (`93b87c8`). Owned paths: `Source/ValStrike/Economy/**`, `Tests/Economy/`, `Scripts/test_economy.py`, this file. No other file changed.

### Design

**Rules model** (`Economy/VSEconomyRules.*`, plain C++17, no Unreal dependency). It is deterministic and idempotent. Every round event carries the match model's `RoundId`. A repeated or stale event returns `Duplicate`/`StaleRound` and never pays twice. Purchase requests carry a per-wallet, strictly increasing request id; replays are never evaluated again.

**One tunable table** (`ValStrike::Economy::Tuning`). All values are playtest proposals:

| Value | Proposal | Notes |
| --- | --- | --- |
| Starting credits | 800 | At match start, after the side swap, on rematch and for a new connection |
| Credit cap | 9000 | Applies to all income |
| Round win | 3000 | Every wallet on the winning team |
| Round loss | 1700 + 500 per further consecutive loss, at most 2 steps | 1700, 2200, 2700, 2700, … A win resets that team's streak; the swap resets both |
| Kill | 200 | Killer only, enemy kills only; team kills earn nothing; one credit per victim per round |
| Plant | 300 | **Every wallet on the attacking team**, once per round, including attackers who already fell. This is team-wide so the reward can be derived from the replicated objective state, without planter identity. |
| Rifle | 2700 | Primary slot |
| Pistol | 400 | Sidearm. It is in the default starter set, so it is free every round and cannot be bought or refunded. Remove it from `StarterItems` to make it a purchase. |
| Request budget | 24 per wallet per round | Bounds spam deterministically; purchases and refunds share it |

**Ownership rules.**
- Purchased items survive with the player: survivors keep them into the next round.
- A player who died keeps only the starter items at the next buy phase.
- A full reset (credits, items, loss streaks) happens when the match's attacking team changes (side swap after three rounds) and when a buy phase opens with zero completed rounds (first match, rematch, cleared server).
- An aborted preparation that restarts with the same roles does not reset.
- A refund returns the exact price, only during the same buy phase, only for items bought in it.

**Purchase validation**, in order: known wallet → fresh request id → round request budget → catalog item → requester on the wallet's team → alive on the server → buy phase open → request names the current round → not already owned → affordable.

**Server adapter.**
- `UVSEconomySubsystem` (`UTickableWorldSubsystem`) is active only when the world's GameState is `AVSMatchGameState` and only on the server. Practice mode and the existing graybox harnesses therefore see no wallets.
- `FGameModeEvents` PostLogin/Logout spawn and remove one `AVSEconomyWallet` per player. The wallet is an `AInfo` owned by that player's PlayerController, `bOnlyRelevantToOwner`. It carries the replicated view and the `ServerRequest` RPC. The player is registered once the match assigns connection id and team.
- Round events are derived read-only from `AVSMatchGameState::Match` and `AVSMatchPlayerState`:
  - Buy phase: preparation of a new `RoundId`. It closes when preparation ends or at its deadline, whichever comes first.
  - Plant: objective Planted/Defused/Detonated.
  - Deaths: participating player states with `bAlive` false.
  - Result: Resolved/MatchComplete with a winner.
  - Side swap and new match: the attacking team and completed rounds at preparation.
- Clients only send requests. The server checks:
  - the wallet belongs to the calling controller and still matches its match identity;
  - team and life state (player state flags plus pawn health);
  - the buy phase and deadline;
  - everything the model validates.

**Buy menu and credits display** (`Economy/UI/`, native Slate, no UMG or Blueprint assets).
- `UVSEconomyClientSubsystem` adds its own viewport widget (Z order 50, below the session menu's 100). It pushes its own input component binding **B**, so the HUD, controllers and menus are unchanged.
- Credits badge: top left, below the HUD brand, using the canvas HUD's logical 1280×720 scale. It shows "B BUY MENU" while buying is possible.
- Buy menu ("ARMORY"), centered, using the session menu's readable scale at 960×540. It shows credits, last-round income, the buy-phase countdown, one row per catalog item (price or FREE, then BUY/OWNED/REFUND) and the server's status for the latest request.
- Controls: keys 1/2 buy, or refund a purchase from this buy phase; mouse clicks the buttons; Tab/Enter navigate; B closes. The menu closes when the buy phase ends or the player can no longer buy.
- Before taking focus it follows the shared UI contract: cancel objective input, stop combat input, flush keys. Escape is never handled by the buy menu: it still reaches the controller and opens the session menu, and the buy menu yields without touching the session menu's input mode. B cannot reopen the buy menu over the session menu.

**Weapon grant interface** (`VSEconomyTypes.h`).
- `UVSEconomySubsystem::OnLoadoutChanged()` fires on the server with `(PlayerController, FVSEconomyLoadout, reason)`:
  - when ownership changes (purchase, refund);
  - for every registered player when a buy phase opens (`RoundStart`, or `Reset` at a swap or new match), when the match has already spawned fresh pawns;
  - on registration (`Join`).
- `GetLoadout(Controller)` is the pull variant for pawn possession.
- Until Weapons subscribes, every grant is only logged (`VSECON grant …`).

### Handoffs

1. **Kill attribution (Combat / Weapons owner, `Combat/VSRifleComponent.cpp` or `Combat/VSHealthComponent.*`).** `UVSHealthComponent::ApplyPointDamage(float)` has no instigator, and `OnLifeStateChanged` carries no killer, so the economy cannot attribute kills. The economy exposes the server-only `UVSEconomySubsystem::ReportKill(AController* Killer, AController* Victim)`.
   - Smallest change: in the authoritative hit path, after `ApplyPointDamage` returns more than zero and the target is no longer alive, call `ReportKill(ShooterController, TargetController)` synchronously.
   - The Weapons branch moves that path into the shared `UVSWeaponComponent` (rifle and pistol), so one call there covers both weapons.
   - Synchronously means before the match model seals the death, so the kill is credited before the round result.
   - The economy rejects reports for a live victim, repeats and team kills.
   - Alternatively Combat can add a killer-carrying delegate, and the economy will subscribe in Phase 2.
   - Until then only the development harness reports kills.
2. **Weapon grant (Weapons owner, Phase 2).** Weapons binds 1/2/Q in the character's Enhanced Input. While the buy menu is open, the menu has Slate focus and consumes 1/2 as buy keys, so they cannot switch weapons then. When it is closed, the economy binds only B.
   - Subscribe to `OnLoadoutChanged()` on the server, or pull `GetLoadout()` on possession.
   - Grant/remove rifle and pistol on that controller's pawn; `FVSEconomyLoadout::Owns(EVSEconomyItem::Rifle)`.
   - Default starter set: pistol only. A purchased rifle is kept by survivors and lost on death.
3. **No match change was needed.** Everything the economy needs except the killer is already in the public replicated match state.
4. **Server switches.** The networked test uses `-VSEconomySmoke=<role>`, `-VSEconomyOutput=` and `-VSEconomyScreenshots=` (no `-VSServer*` prefix) in the existing `match` mode on Site01. No new server mode is needed.
5. **CI.** `Tests/Economy/test_economy_runner.py` and `python3 Scripts/test_economy.py [--sanitize]` are engine-free and can join the CI workflow (owned by the server session).

### Scope addition: 30 s preparation (buy phase)

On 2026-11-03 the coordinating session relayed a user-approved change ("yes we need to change that"): lengthen the preparation phase, which is also the buy phase, so that buying is practical. This stream now also owns that value in `Source/ValStrike/Rules/`, `Tests/Rules/` and `Scripts/test_round_rules.py`.

- `ValStrike::Rules::Config::PreparationMs` changes from 8 000 to **30 000 ms**. It remains the single tunable value, and its comment gives the reason. This is a playtest proposal.
- Rules still keep players in spawn with combat closed during preparation. The default-timing rules test now checks the 30 s deadline and that movement and combat stay closed at 29.999 s.
- The match smoke (`Scripts/test_match.py`) pins its own 3 s preparation through `-VSMatchSmoke=server`, so it is unaffected. The Server session's `-VSMatchFastTimings` (1 s preparation, 3 s rounds) is too short for the buy-menu scenario. The economy network test therefore runs the real default and allows 480 s.
- `docs/MVP_PLAN.md` still says "8-second preparation"; the integration session owns that update. Other runtime scenarios that run match mode with default timings (for example a Weapons match scenario) now wait 30 s per preparation.

### Verification evidence (2026-11-03, loopback Development Editor, M3 / 16 GB, UE 5.8.4 CL 58902052, Xcode 26.1.1)

PR #18 was merged by the integration session at 22:53, before the runtime checks below had finished. They ran afterwards and are recorded here. Two fixes they prompted follow in a separate PR.

**Engine-free checks** on the merged tree (`main` 89f3c9c plus this branch, merge `5db0d79`):

| Check | Result |
| --- | --- |
| Economy rules (`Scripts/test_economy.py`) | 17/17 cases, 246 checks |
| Economy rules with ASan/UBSan (`--sanitize`) | 17/17 cases, 246 checks |
| Round rules with the 30 s default, normal and sanitized | 31/31 cases, 974 checks |
| Server contract | 16/16 cases, 141 checks |
| Economy runner contract (`Tests/Economy/test_economy_runner.py`) | 9/9 |
| `test_ue*`, `Tests/Match`, `Tests/Server`, `Tests/Playtest` | OK |
| `git diff --check` | clean |

**Builds.** A clean unity Editor build (one `Module.ValStrike.cpp` blob) passed at `5db0d79` (21 s). It passed again at `faa5ec8` and `26dc7bb`, on top of `main` 862e938 (Weapons #20 and Settings #19 merged). No unity name clashes.

**Server + two-client network test**: `python3 Scripts/test_economy.py --network`, Site01, `match` mode, default timings (30 s preparation), ports 7911.

- `Saved/Tests/Economy/network_20261103T220349Z_99d88c/result.json`, revision `5db0d79`, clean checkout, null RHI: **PASS, 34/34**, 237 s.
- Server-side checks, all on the server:
  - starting credits;
  - each round's opening, post-purchase and result balances against an independent ledger computed from the tuning table;
  - rejections: unaffordable rifle, starter pistol already owned, unknown item, replayed request id, out-of-phase request from a living player, request from a dead player;
  - 40 rifle RPCs in one client frame: 1 applied, 23 already owned, 16 rate-limited, charged once;
  - buy/refund/buy within one phase;
  - the fallen lose the purchased rifle, the survivor keeps it;
  - plant reward to the attacker who planted and then lost;
  - kill credit, with the repeat report refused;
  - loss streak 1700/2200/2700 after the swap;
  - credit cap: 10 400 uncapped becomes 9000;
  - full reset at the side swap (a held rifle was removed) and on rematch;
  - replayed round events ignored, 5 of 5, every round;
  - grant notifications for purchase, refund, round start and reset.
- Client-side checks:
  - each client sees exactly one wallet, its own;
  - B opens the menu with the cursor during the buy phase;
  - the menu closes, the cursor hides and look input returns when the round goes live;
  - Escape opens the session menu over the buy menu, and B cannot reopen it underneath;
  - the rematch reset is seen.
- The runner compared all 26 replicated checkpoints per player with the server ledger: no mismatches.
- Server balances per round (team One / team Two): 2800/4000, 3300/3000, 5000/3500, then after the swap 2500/4000, 4700/7200, 7400/9000; rematch 800/800. They match `Tests/Economy` `network_scenario_ledger` exactly.
- Real paths used:
  - the plant and the defuse are real held E inputs;
  - deaths go through `UVSHealthComponent::ApplyPointDamage`;
  - purchases use the wallet RPC;
  - menu keys use the controller input stack.
- Kill credit came from the harness through the server-only `ReportKill`. No combat code reported killers at this revision.

**Rendered run** (`--rendered`: alpha at 960×540, others null RHI).
- First attempt at `faa5ec8` on `main` 862e938: `network_20261103T221352Z_fdc134`, PASS 36/36. Inspecting the screenshots found the BUY/OWNED/REFUND labels clipped by a 104 px button.
- Fixed in `26dc7bb` (124 px button, smaller padding) and rerun: `Saved/Tests/Economy/network_20261103T223550Z_ceab96/result.json`, **PASS, 36/36**, 245 s, clean checkout.
- That includes `keyboard_purchase`: key "2" sent through Slate to the focused menu bought the rifle (request 6, applied, 100 CR left). It also includes Escape sent through Slate.
- Screenshots inspected (`…/screenshots/`):
  - `01-credits-hud`: badge under the brand with the "B BUY MENU" hint;
  - `02-buy-menu-round1`: 800 CR, pistol FREE/OWNED, rifle 2,700 CR shown as unaffordable with BUY disabled, status of the last probe;
  - `03-escape-over-buy`: the session menu on top, buy menu and badge hidden;
  - `04-buy-menu-purchased`: 100 CR, "LAST ROUND +2,000", rifle REFUND, the confirmation line;
  - `05-round-live`: menu closed, badge without hint, the plant prompt.
- Text is readable at 960×540. The orange "Multiple directional lights…" text in these captures is an engine development message about Site01 lighting, not economy UI. While open, the buy menu covers the round HUD's objective line; the phase header stays visible.

**Existing suites with the economy present**, at `26dc7bb` (`main` 862e938 + fixes), clean checkout:

| Suite | Result | Evidence |
| --- | --- | --- |
| Match | **43/43** | `Saved/Tests/Match/20261103T223955Z_0a51ab` (economy active: 80 `VSECON` server lines) |
| Movement | **45/45** | `Saved/Tests/Movement/20261103T224203Z_8c2558` |
| Combat | **55/55** | `Saved/Tests/Combat/20261103T224315Z_162a04` |

**Limits.**
- Loopback Development Editor only: no packaged build, LAN, second machine, latency or loss.
- Mouse clicks on the menu buttons were not injected. The buttons are SButtons and visible with the cursor shown, but only keyboard paths were driven.
- Kill credit is not reached by real fire yet (handoff 1). Weapons do not follow ownership yet (handoff 2).
- A reconnecting player gets a new identity and starts at 800 credits; there are no accounts.
- An out-of-catalog item value sent over the RPC is narrowed by enum serialization. `None` is still rejected as unknown; the server never trusts the value.

**Next action.** Phase 2, part A (approved by the coordinating session): subscribe to `UVSDamageSubsystem::OnKill()` in place of the `ReportKill` handoff. Part B (weapon grant) waits for the coordinator's go-ahead.

## 2026-11-04 — Phase 2, part A: kill rewards from the damage service

**Request.** On 2026-11-03 the coordinating session asked for this after damage Phase 1 (#22) merged: credit kills from `UVSDamageSubsystem::OnKill()` in place of the `ReportKill` handoff, editing only economy paths. Branch `claude/economy-kill-rewards`, PR #24.

**Change** (`e0d8f2d`).
- `UVSEconomySubsystem` gets the damage service with `Collection.InitializeDependency<UVSDamageSubsystem>()` and subscribes to `OnKill()` on the server. It unsubscribes in `Deinitialize`.
- For each `FVSDamageEvent` with `bKilled`, it uses `Instigator`, `VictimController` and `Victim` only during the broadcast.
- The kill is refused when:
  - the killer or victim has no wallet, or killer equals victim;
  - the victim's `UVSHealthComponent` is still alive;
  - the victim was already credited this round (model `Duplicate`);
  - the round is not live (model `WrongPhase`).
- A team kill is recorded as a death without a reward.
- The public `ReportKill` is removed; crediting is a private path.
- The kill reward is unchanged at 200. `IsHeadshot()` is not used, because the tuning table has no headshot bonus.
- No `Damage/`, `Combat/` or `VSCharacter.*` file changed.

**Kill path used for verification.** Real fire reaches `OnKill` only after Damage Phase 2.
- The harness kills through `ApplyPointDamage`, then publishes the attributed kill with `UVSDamageSubsystem::PublishDamage`, then publishes it again.
- In round 2 it also publishes a kill notification for a still-living player.
- These events also reach the scoreboard stats in that test world.

**Evidence.**
- Clean unity Editor build at `e0d8f2d`: PASS. The module now builds as three unity blobs.
- Engine-free checks at `e0d8f2d`:
  - economy rules 17/17 (246 checks), normal and sanitized;
  - round rules 31/31 (974 checks);
  - runner contract 9/9;
  - `git diff --check` clean.
- Network run `Saved/Tests/Economy/network_20261103T230713Z_1d5908` was **interrupted by SIGTERM after 109 s**: the user's new verification policy stopped this session's runtime runs. Its result is therefore FAIL by interruption and is **not** a pass.
- Before the stop, rounds 1–2 showed:
  - each `OnKill` kill credited exactly once;
  - the repeated notification refused (`duplicate`);
  - the living-victim notification refused before the model;
  - balances 2800/4000 and 3300/3000, matching the ledger.

**Verification policy change.** On 2026-11-04 the user said: "every session running tests and compiling this is horrible waisting time and tokens". Streams now run only engine-free tests. Integration runs one combined build and runtime pass per batch and returns failures. Runtime verification for part A is therefore pending in the integration batch:
- `python3 Scripts/with_ue_slot.py -- python3 Scripts/test_economy.py --network --engine <UE>`: expect PASS with 35 checks, including `server: kill_reward` and `server: kill_live_victim_refused`, and the per-round ledger 2800/4000, 3300/3000, 5000/3500, 2500/4000, 4700/7200, 7400/9000, then 800/800.
- `python3 Scripts/with_ue_slot.py -- python3 Scripts/test_match.py --engine <UE>`: expect 43/43.
- `python3 Scripts/with_ue_slot.py -- python3 Scripts/test_combat.py --engine <UE>`: expect 55/55.

**Next action.** Integration batch result for PR #24. Part B (weapon grant) waits for the coordinator's go-ahead.
