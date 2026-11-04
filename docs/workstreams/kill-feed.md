# Kill feed workstream (GPT-14)

Append-only. Dates and times are Europe/Berlin. Task brief: `docs/tasks/gameplay/GPT-14-kill-feed.md` (on `claude/gameplay-board-int06`). Backlog PLAY-03.

## 2026-11-04: Implementation ready, verification pending

- Branch `claude/gpt-14-kill-feed`, base `b342ca9`, implementation commit `f28eb7c`. Validation tier V2 (replicated state).
- Owner: Claude session "GPT-14 kill feed". Next owner: the gameplay lead (review), then integration QA.

### Change

- **Model** (new, engine-free): `Source/ValStrike/Scoreboard/VSKillFeedModel.h`.
  - `Ring<Payload>` keeps the newest 5 entries and assigns a monotonic `Seq`. `Clear()` empties the ring but the sequence keeps counting.
  - `KindFromOutcome` maps ledger `HitOutcome` values to a feed kind. Kill, TeamKill, SelfKill and Unattributed (shown as `World`) produce an entry. Damage and Ignored, which includes a repeated kill report, produce none.
  - `Select` picks the rows to draw from replicated `{seq, server time}` stamps: newest first, one row per sequence, unexpired after 5 s (fading over the last 0.5 s), at most 5 rows.
  - Also has text labels and the local-player highlight test.
- **Server**: `Scoreboard/VSMatchStatsSubsystem.*`.
  - `HandleDamage` calls `AppendKillFeed` with the ledger's outcome. The ledger's `KillResolved` flag allows one resolving outcome per life, so each death gets exactly one entry.
  - Each entry holds: player keys, name snapshots, teams, weapon (rifle/pistol), headshot, kind, and `GetServerWorldTimeSeconds()`.
  - A change of `AVSMatchGameState::Match.RoundId` clears the feed. This is checked in the 0.25 s `Sync` poll and again just before each append, so a kill early in a new round is not cleared by a late poll.
  - Logs `VSFEED seq=… kind=… killer=… victim=…` and `VSFEED round …: feed cleared`.
- **Replication**: `Scoreboard/VSScoreboardState.*`.
  - Adds the `FVSKillFeedEntry` USTRUCT and the replicated `TArray<FVSKillFeedEntry> KillFeed` (at most 5 entries) on the always-relevant scoreboard actor.
  - `ServerPublishKillFeed` is authority-only, compares sequences, and calls `ForceNetUpdate` on change. There is no RepNotify: the HUD reads the feed every frame.
- **HUD**: `VSHUD.cpp/.h`, new `DrawKillFeed` block.
  - Drawn top right, under the ESC MENU box. It is drawn before the pawn check, so dead players and spectators see it. Not drawn while the session menu is open.
  - Row layout is `Killer  [TEAMKILL] RIFLE|PISTOL [HS]  Victim`. Self kills show `SELF  Victim`. A death with no source shows `DIED  Victim`.
  - Names use the existing team palette (team 1 Accent, team 2 TeamTwo, unassigned Ink). The middle text is Muted, Headshot yellow for headshots, Warning for team kills. Long names are cut off with "...".
  - Rows involving the local player (matched by scoreboard player key) get a brighter panel and an Ink outline.
  - Expiry uses the replicated server clock, so late joiners don't replay old lines.
  - The reticle, hit-marker and damage-indicator blocks are unchanged.
- No new assets. **Icon slot for art:** the weapon/`HS` text in the middle column can later be replaced by an icon per `ValStrike::KillFeed::Weapon` plus a headshot icon.
- No read-only file was touched: `VSStatsLedger.*`, `VSScoreboardModel.*`, `SVSScoreboard.*`, `Damage/**`, `Match/**`, `Feedback/**`, `Combat/**`.

### Checks actually run

- `Tests/Scoreboard/VSKillFeedModelTests.cpp` (with `VSStatsLedger.cpp`), `-std=c++17 -Wall -Wextra -Werror -pedantic`: **58 checks, 0 failures** with `clang++` (Apple clang 17.0.0).
  - Covers: append and sequence, the cap of 5, reset with the sequence continuing, newest-first selection and the row cap, sequence dedupe for repeated or reordered deliveries, expiry and fade, late-joiner skip, server clock ahead of the client, bad input, the outcome-to-kind mapping, one entry per death through the real ledger (death counted first, kill reported 3 times), labels and highlight.
- The same command with `g++` also passed 58/0. On this Mac `g++` is Apple clang 17, not GNU GCC (no GCC is installed), so **a real GCC run is still pending** (CI or another machine).
- `git diff --check`: clean.
- No Unreal build or run, as the brief requires.

### Integration-owned checks (deferred)

1. **Editor build** of the combined candidate (standard unity configuration, through `Scripts/with_ue_slot.py`). This is the first compile of `DrawKillFeed`, `FVSKillFeedEntry` and the template ring inside the subsystem.
2. **`Scripts/test_damage.py`: 46/46**. No regression. The new test is not added to that runner, so the count is unchanged.
3. **`Scripts/test_match.py`**: no regression. Look for `VSFEED` lines in the server log for each kill, and `feed cleared` at each new round.
4. **`Tests/UI/test_session.py --rendered`: 72/72**. If a kill happens on camera, check a screenshot: the feed is top right, does not overlap the match header or the ESC MENU box, and the local row is highlighted. Otherwise a human playtest covers it.
5. **Network scenarios**, server plus 2 clients:
   - An enemy kill, a team kill and a self kill each give one line on both clients.
   - A headshot shows `HS`.
   - Lines disappear after about 5 s.
   - A client that joins mid-round sees no stale lines.
   - A new round empties the feed.
   - A dedicated server draws nothing.

### Risks and known limits

- A death that never produces a `FVSDamageEvent` gets no feed entry. Such a death is counted only by `OnLifeStateChanged`, for example a development reset or any future non-weapon damage path. Today all lethal damage goes through `UVSDamageSubsystem`, so this does not happen. A future world-damage source should report through the damage subsystem or call into the feed.
- Bots are not tracked by the ledger. A bot victim gets no entry. A bot killer shows as a kill with its name, and kind `World` on the wire.
- Clients compute expiry from `GetServerWorldTimeSeconds()`, which can drift a little after connection hitches. A line can then show slightly shorter or longer than 5 s. The model clamps negative ages to 0.
- The layout was not rendered, so the widths and positions at the 960×540 minimum need the screenshot check above.
