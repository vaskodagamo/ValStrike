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
