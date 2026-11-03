#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Engine-independent, server-owned round economy. Deterministic: the same event
// sequence always produces the same state, and repeated events never pay twice.
// Clients never form these events. The Unreal adapter derives round events from
// the authoritative match state and validates identity, team and life state
// before it forwards a purchase request.
namespace ValStrike::Economy
{
using Id = std::uint64_t;
using Credits = std::int32_t;
using ItemMask = std::uint32_t;

constexpr std::size_t MaxWallets = 10; // The 2v2 MVP uses four; leaves room for 5v5.
constexpr Credits MaxConfigurableCredits = 1'000'000;

enum class Team : std::uint8_t { None, One, Two }; // Same values as ValStrike::Rules::Team.

// Wire values. Append new items; never renumber or reuse a value.
enum class Item : std::uint8_t { None, Pistol, Rifle };
constexpr std::size_t ItemCount = 3; // Including None.
enum class Slot : std::uint8_t { None, Primary, Sidearm };

struct ItemInfo
{
    Item Id = Item::None;
    Slot SlotId = Slot::None;
    const char* Key = ""; // Stable lowercase identifier for logs and evidence.
};

// Static catalog identity. Prices live in Tuning so every tunable value is in one table.
const ItemInfo* FindItem(Item Value); // Null for None and unknown wire values.
constexpr ItemMask ItemBit(Item Value) { return ItemMask{1} << static_cast<unsigned>(Value); }
constexpr ItemMask CatalogItems = ItemBit(Item::Pistol) | ItemBit(Item::Rifle);

// The single tunable table. All values are playtest proposals; see docs/workstreams/economy.md.
struct Tuning
{
    Credits StartingCredits = 800;   // Every wallet at match start, after the side swap and on a fresh join.
    Credits CreditCap = 9000;        // No income raises a balance above this.
    Credits RoundWinReward = 3000;   // Every wallet on the winning team.
    Credits RoundLossReward = 1700;  // Every wallet on the losing team, first loss in a row.
    Credits LossStreakStep = 500;    // Added for each further consecutive loss...
    std::uint8_t LossStreakMaxSteps = 2; // ...at most this often: 1700, 2200, 2700, 2700, ...
    Credits KillReward = 200;        // The killer, for an enemy kill only.
    Credits PlantReward = 300;       // Every wallet on the attacking team, once per round.
    std::array<Credits, ItemCount> Prices{0, 400, 2700}; // Indexed by Item: None, Pistol, Rifle.
    // Granted free at every buy phase and never refundable. With the default the
    // pistol is the free sidearm; remove it here to make the pistol a purchase.
    ItemMask StarterItems = ItemBit(Item::Pistol);
    // Purchase/refund requests evaluated per wallet per round (buy phase to next buy phase).
    std::uint8_t MaxRequestsPerRound = 24;
};

struct Wallet
{
    Id PlayerId = 0; // Nonzero server connection identity; zero marks an empty slot.
    Team TeamId = Team::None;
    Credits Balance = 0;
    ItemMask Owned = 0;
    ItemMask Refundable = 0;      // Bought during the current buy phase.
    Credits RoundIncome = 0;      // Credited since the current round's buy phase opened.
    Credits LastRoundIncome = 0;  // The previous completed round's income, for presentation.
    bool Died = false;            // Died this round: keeps only starter items at the next buy phase.
    bool Killed = false;          // A kill on this victim was already credited this round.
    std::uint8_t Requests = 0;    // Requests evaluated since the current buy phase opened.
    Id LastRequestId = 0;         // Strictly increasing per wallet; older ids are replays.
};

enum class Phase : std::uint8_t { Idle, Buy, Live, Ended };

struct Snapshot
{
    bool TuningValid = false;
    Phase CurrentPhase = Phase::Idle;
    Id RoundId = 0; // The match model's round identity; strictly increases.
    Team AttackingTeam = Team::None;
    std::uint8_t CompletedRounds = 0; // As reported when this round's buy phase opened.
    std::array<std::uint8_t, 2> LossStreak{}; // Consecutive losses; index 0 is Team One.
    bool PlantAwarded = false;
    bool ResultAwarded = false;
    std::uint32_t Resets = 0; // Full resets so far (new match or side swap).
    std::array<Wallet, MaxWallets> Wallets{};

    const Wallet* Find(Id PlayerId) const;
};

enum class EventKind : std::uint8_t
{
    Join, Leave, BuyPhaseBegin, BuyPhaseEnd, Plant, Kill, Death, RoundResult, Purchase, Refund
};

struct Event
{
    EventKind Kind = EventKind::Join;
    Id RoundId = 0;       // Every event except Join/Leave. Requests carry the client's round.
    Id PlayerId = 0;      // Join/Leave/Death/Purchase/Refund subject; Kill: the killer.
    Id OtherId = 0;       // Kill: the victim.
    Team TeamId = Team::None; // Join: team. BuyPhaseBegin: attackers. RoundResult: winner.
                              // Purchase/Refund: the requester's current server team.
    std::uint8_t CompletedRounds = 0; // BuyPhaseBegin: rounds completed in this match (0 = new match).
    Item ItemId = Item::None;         // Purchase/Refund.
    Id RequestId = 0;                 // Purchase/Refund.
    bool Alive = false;               // Purchase/Refund: server-observed life state, never a client claim.
};

enum class Status : std::uint8_t
{
    Applied, Duplicate, InvalidTuning, InvalidEvent, UnknownPlayer, RosterFull, StaleRound, WrongPhase,
    Replayed, RateLimited, UnknownItem, NotOnTeam, NotAlive, NotBuyPhase, AlreadyOwned, Unaffordable,
    NotRefundable
};
constexpr std::size_t StatusCount = 17;
const char* StatusName(Status Value);

class EconomyRules
{
public:
    explicit EconomyRules(const Tuning& Values = Tuning{});
    static bool IsValidTuning(const Tuning& Candidate);

    const Snapshot& Read() const { return State; }
    const Tuning& GetTuning() const { return Values; }
    Credits Price(Item Value) const; // Zero for unknown items.
    Credits LossReward(std::uint8_t ConsecutiveLosses) const; // Zero for no loss.

    Status Apply(const Event& Input);

private:
    Tuning Values;
    Snapshot State;

    Wallet* Find(Id PlayerId);
    void Earn(Wallet& Target, Credits Amount);
    void ResetEconomy();
    Status Join(const Event& Input);
    Status BeginBuyPhase(const Event& Input);
    Status EndBuyPhase(const Event& Input);
    Status Plant(const Event& Input);
    Status Kill(const Event& Input);
    Status Death(const Event& Input);
    Status RoundResult(const Event& Input);
    Status Request(const Event& Input, bool bRefund);
};
}
