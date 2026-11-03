#include "VSEconomyRules.h"

#include <algorithm>
#include <initializer_list>

namespace ValStrike::Economy
{
namespace
{
// File-local names are unique: Unreal's unity build compiles this file with the rest of the module.
constexpr std::array<ItemInfo, ItemCount - 1> EconomyCatalog{{
    {Item::Pistol, Slot::Sidearm, "pistol"},
    {Item::Rifle, Slot::Primary, "rifle"},
}};

bool IsEconomyTeam(Team Value) { return Value == Team::One || Value == Team::Two; }
std::size_t EconomyTeamIndex(Team Value) { return Value == Team::One ? 0 : 1; }
Team EconomyOpponent(Team Value) { return Value == Team::One ? Team::Two : Team::One; }
bool InCreditRange(Credits Value) { return Value >= 0 && Value <= MaxConfigurableCredits; }
}

const ItemInfo* FindItem(Item Value)
{
    for (const ItemInfo& Info : EconomyCatalog)
    {
        if (Info.Id == Value) { return &Info; }
    }
    return nullptr;
}

const char* StatusName(Status Value)
{
    switch (Value)
    {
    case Status::Applied: return "applied";
    case Status::Duplicate: return "duplicate";
    case Status::InvalidTuning: return "invalid_tuning";
    case Status::InvalidEvent: return "invalid_event";
    case Status::UnknownPlayer: return "unknown_player";
    case Status::RosterFull: return "roster_full";
    case Status::StaleRound: return "stale_round";
    case Status::WrongPhase: return "wrong_phase";
    case Status::Replayed: return "replayed";
    case Status::RateLimited: return "rate_limited";
    case Status::UnknownItem: return "unknown_item";
    case Status::NotOnTeam: return "not_on_team";
    case Status::NotAlive: return "not_alive";
    case Status::NotBuyPhase: return "not_buy_phase";
    case Status::AlreadyOwned: return "already_owned";
    case Status::Unaffordable: return "unaffordable";
    case Status::NotRefundable: return "not_refundable";
    }
    return "unknown";
}

const Wallet* Snapshot::Find(Id PlayerId) const
{
    if (PlayerId == 0) { return nullptr; }
    for (const Wallet& Value : Wallets)
    {
        if (Value.PlayerId == PlayerId) { return &Value; }
    }
    return nullptr;
}

EconomyRules::EconomyRules(const Tuning& InValues) : Values(InValues)
{
    State.TuningValid = IsValidTuning(Values);
}

bool EconomyRules::IsValidTuning(const Tuning& Candidate)
{
    if (!InCreditRange(Candidate.CreditCap) || !InCreditRange(Candidate.StartingCredits) ||
        Candidate.StartingCredits > Candidate.CreditCap)
    {
        return false;
    }
    for (Credits Value : {Candidate.RoundWinReward, Candidate.RoundLossReward, Candidate.LossStreakStep,
                          Candidate.KillReward, Candidate.PlantReward})
    {
        if (!InCreditRange(Value)) { return false; }
    }
    if (Candidate.Prices[0] != 0) { return false; }
    for (std::size_t I = 1; I < ItemCount; ++I)
    {
        if (!InCreditRange(Candidate.Prices[I]) || Candidate.Prices[I] > Candidate.CreditCap) { return false; }
    }
    return (Candidate.StarterItems & ~CatalogItems) == 0 && Candidate.MaxRequestsPerRound > 0 &&
           Candidate.LossStreakMaxSteps <= 32;
}

Credits EconomyRules::Price(Item Value) const
{
    return FindItem(Value) ? Values.Prices[static_cast<std::size_t>(Value)] : 0;
}

Credits EconomyRules::LossReward(std::uint8_t ConsecutiveLosses) const
{
    if (ConsecutiveLosses == 0) { return 0; }
    const std::int64_t Steps = std::min<std::int64_t>(ConsecutiveLosses - 1, Values.LossStreakMaxSteps);
    const std::int64_t Amount = std::int64_t{Values.RoundLossReward} + Steps * Values.LossStreakStep;
    return static_cast<Credits>(std::min<std::int64_t>(Amount, Values.CreditCap));
}

Wallet* EconomyRules::Find(Id PlayerId)
{
    return const_cast<Wallet*>(State.Find(PlayerId));
}

void EconomyRules::Earn(Wallet& Target, Credits Amount)
{
    if (Amount <= 0 || Target.Balance >= Values.CreditCap) { return; }
    const std::int64_t Next = std::min<std::int64_t>(std::int64_t{Target.Balance} + Amount, Values.CreditCap);
    Target.RoundIncome += static_cast<Credits>(Next - Target.Balance);
    Target.Balance = static_cast<Credits>(Next);
}

void EconomyRules::ResetEconomy()
{
    for (Wallet& Value : State.Wallets)
    {
        if (Value.PlayerId == 0) { continue; }
        Value.Balance = Values.StartingCredits;
        Value.Owned = Values.StarterItems;
        Value.LastRoundIncome = 0;
    }
    State.LossStreak = {};
    ++State.Resets;
}

Status EconomyRules::Apply(const Event& Input)
{
    if (!State.TuningValid) { return Status::InvalidTuning; }
    switch (Input.Kind)
    {
    case EventKind::Join: return Join(Input);
    case EventKind::Leave:
    {
        Wallet* Target = Find(Input.PlayerId);
        if (!Target) { return Status::UnknownPlayer; }
        *Target = {};
        return Status::Applied;
    }
    case EventKind::BuyPhaseBegin: return BeginBuyPhase(Input);
    case EventKind::BuyPhaseEnd: return EndBuyPhase(Input);
    case EventKind::Plant: return Plant(Input);
    case EventKind::Kill: return Kill(Input);
    case EventKind::Death: return Death(Input);
    case EventKind::RoundResult: return RoundResult(Input);
    case EventKind::Purchase: return Request(Input, false);
    case EventKind::Refund: return Request(Input, true);
    }
    return Status::InvalidEvent;
}

Status EconomyRules::Join(const Event& Input)
{
    if (Input.PlayerId == 0 || !IsEconomyTeam(Input.TeamId)) { return Status::InvalidEvent; }
    if (Find(Input.PlayerId)) { return Status::Duplicate; }
    for (Wallet& Value : State.Wallets)
    {
        if (Value.PlayerId != 0) { continue; }
        Value = {};
        Value.PlayerId = Input.PlayerId;
        Value.TeamId = Input.TeamId;
        Value.Balance = Values.StartingCredits;
        Value.Owned = Values.StarterItems;
        return Status::Applied;
    }
    return Status::RosterFull;
}

Status EconomyRules::BeginBuyPhase(const Event& Input)
{
    if (Input.RoundId == 0 || !IsEconomyTeam(Input.TeamId)) { return Status::InvalidEvent; }
    if (Input.RoundId == State.RoundId) { return Status::Duplicate; }
    if (Input.RoundId < State.RoundId) { return Status::StaleRound; }

    // A new match starts with zero completed rounds (first match, rematch or a cleared
    // server). Roles swap only between halves, so a changed attacking team is the swap.
    const bool bNewMatch = State.RoundId == 0 || Input.CompletedRounds == 0 ||
                           Input.CompletedRounds < State.CompletedRounds;
    const bool bSideSwap = !bNewMatch && Input.TeamId != State.AttackingTeam;
    if (bNewMatch || bSideSwap)
    {
        ResetEconomy();
    }
    else
    {
        for (Wallet& Value : State.Wallets)
        {
            if (Value.PlayerId == 0) { continue; }
            // Survivors keep purchases into the next round; the fallen keep only starter items.
            Value.Owned = (Value.Died ? 0 : Value.Owned) | Values.StarterItems;
            // An aborted preparation has no result; keep showing the last completed round.
            if (State.ResultAwarded) { Value.LastRoundIncome = Value.RoundIncome; }
        }
    }
    for (Wallet& Value : State.Wallets)
    {
        Value.Refundable = 0;
        Value.RoundIncome = 0;
        Value.Died = false;
        Value.Killed = false;
        Value.Requests = 0;
    }
    State.RoundId = Input.RoundId;
    State.AttackingTeam = Input.TeamId;
    State.CompletedRounds = Input.CompletedRounds;
    State.CurrentPhase = Phase::Buy;
    State.PlantAwarded = false;
    State.ResultAwarded = false;
    return Status::Applied;
}

Status EconomyRules::EndBuyPhase(const Event& Input)
{
    if (Input.RoundId == 0 || Input.RoundId != State.RoundId) { return Status::StaleRound; }
    if (State.CurrentPhase != Phase::Buy) { return Status::Duplicate; }
    State.CurrentPhase = Phase::Live;
    for (Wallet& Value : State.Wallets) { Value.Refundable = 0; }
    return Status::Applied;
}

Status EconomyRules::Plant(const Event& Input)
{
    if (Input.RoundId == 0 || Input.RoundId != State.RoundId) { return Status::StaleRound; }
    if (State.PlantAwarded) { return Status::Duplicate; }
    if (State.CurrentPhase != Phase::Live) { return Status::WrongPhase; }
    State.PlantAwarded = true;
    for (Wallet& Value : State.Wallets)
    {
        if (Value.PlayerId != 0 && Value.TeamId == State.AttackingTeam) { Earn(Value, Values.PlantReward); }
    }
    return Status::Applied;
}

Status EconomyRules::Kill(const Event& Input)
{
    if (Input.RoundId == 0 || Input.RoundId != State.RoundId) { return Status::StaleRound; }
    if (State.CurrentPhase != Phase::Live) { return Status::WrongPhase; }
    if (Input.PlayerId == 0 || Input.OtherId == 0 || Input.PlayerId == Input.OtherId) { return Status::InvalidEvent; }
    Wallet* Killer = Find(Input.PlayerId);
    Wallet* Victim = Find(Input.OtherId);
    if (!Killer || !Victim) { return Status::UnknownPlayer; }
    if (Victim->Killed) { return Status::Duplicate; }
    Victim->Killed = true;
    Victim->Died = true;
    // Team kills are recorded as deaths but earn nothing.
    if (Killer->TeamId != Victim->TeamId) { Earn(*Killer, Values.KillReward); }
    return Status::Applied;
}

Status EconomyRules::Death(const Event& Input)
{
    if (Input.RoundId == 0 || Input.RoundId != State.RoundId) { return Status::StaleRound; }
    if (State.CurrentPhase != Phase::Live) { return Status::WrongPhase; }
    Wallet* Target = Find(Input.PlayerId);
    if (!Target) { return Status::UnknownPlayer; }
    if (Target->Died) { return Status::Duplicate; }
    Target->Died = true;
    return Status::Applied;
}

Status EconomyRules::RoundResult(const Event& Input)
{
    if (Input.RoundId == 0 || Input.RoundId != State.RoundId) { return Status::StaleRound; }
    if (State.ResultAwarded) { return Status::Duplicate; }
    if (State.CurrentPhase != Phase::Live) { return Status::WrongPhase; }
    if (!IsEconomyTeam(Input.TeamId)) { return Status::InvalidEvent; }
    const Team Winner = Input.TeamId;
    const Team Loser = EconomyOpponent(Winner);
    std::uint8_t& Streak = State.LossStreak[EconomyTeamIndex(Loser)];
    Streak = static_cast<std::uint8_t>(std::min(Streak + 1, 255));
    State.LossStreak[EconomyTeamIndex(Winner)] = 0;
    const Credits Loss = LossReward(Streak);
    for (Wallet& Value : State.Wallets)
    {
        if (Value.PlayerId == 0) { continue; }
        if (Value.TeamId == Winner) { Earn(Value, Values.RoundWinReward); }
        else if (Value.TeamId == Loser) { Earn(Value, Loss); }
    }
    State.ResultAwarded = true;
    State.CurrentPhase = Phase::Ended;
    return Status::Applied;
}

Status EconomyRules::Request(const Event& Input, bool bRefund)
{
    Wallet* Target = Find(Input.PlayerId);
    if (!Target) { return Status::UnknownPlayer; }
    if (Input.RequestId == 0) { return Status::InvalidEvent; }
    if (Input.RequestId <= Target->LastRequestId) { return Status::Replayed; }
    Target->LastRequestId = Input.RequestId;
    if (Target->Requests >= Values.MaxRequestsPerRound) { return Status::RateLimited; }
    ++Target->Requests;

    const ItemInfo* Info = FindItem(Input.ItemId);
    if (!Info) { return Status::UnknownItem; }
    if (!IsEconomyTeam(Input.TeamId) || Input.TeamId != Target->TeamId) { return Status::NotOnTeam; }
    if (!Input.Alive) { return Status::NotAlive; }
    if (State.CurrentPhase != Phase::Buy) { return Status::NotBuyPhase; }
    if (Input.RoundId != State.RoundId) { return Status::StaleRound; }

    const ItemMask Bit = ItemBit(Info->Id);
    const Credits Cost = Values.Prices[static_cast<std::size_t>(Info->Id)];
    if (bRefund)
    {
        if ((Target->Refundable & Bit) == 0) { return Status::NotRefundable; }
        Target->Refundable &= ~Bit;
        Target->Owned &= ~Bit;
        // Returns exactly the price paid. Buy phases carry no income, so this cannot pass the cap.
        Target->Balance = static_cast<Credits>(std::min<std::int64_t>(std::int64_t{Target->Balance} + Cost, Values.CreditCap));
        return Status::Applied;
    }
    if ((Target->Owned & Bit) != 0) { return Status::AlreadyOwned; }
    if (Target->Balance < Cost) { return Status::Unaffordable; }
    Target->Balance -= Cost;
    Target->Owned |= Bit;
    Target->Refundable |= Bit;
    return Status::Applied;
}
}
