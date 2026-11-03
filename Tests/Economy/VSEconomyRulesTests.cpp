#include "Economy/VSEconomyRules.h"

#include <cstdio>
#include <cstring>

using namespace ValStrike::Economy;

namespace
{
int Checks = 0;
int Failures = 0;
const char* CurrentTest = "";

bool Check(bool Condition, const char* Expression, int Line)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::fprintf(stderr, "FAIL %s:%d: %s\n", CurrentTest, Line, Expression);
    }
    return Condition;
}

#define CHECK(Expression) Check((Expression), #Expression, __LINE__)
#define REQUIRE(Expression) do { if (!CHECK(Expression)) { return; } } while (false)

constexpr Id PlayerA = 11; // Team One
constexpr Id PlayerB = 12; // Team One
constexpr Id PlayerC = 21; // Team Two
constexpr Id PlayerD = 22; // Team Two

// Drives the model the way the Unreal adapter does, with explicit round identities.
struct Fixture
{
    EconomyRules Model;
    Id Round = 0;
    Id NextRequest = 0;

    explicit Fixture(const Tuning& Values = Tuning{}) : Model(Values) {}

    const Snapshot& State() const { return Model.Read(); }
    const Wallet& W(Id PlayerId) const
    {
        static const Wallet Missing{};
        const Wallet* Found = State().Find(PlayerId);
        return Found ? *Found : Missing;
    }
    Credits Balance(Id PlayerId) const { return W(PlayerId).Balance; }
    bool Owns(Id PlayerId, Item Value) const { return (W(PlayerId).Owned & ItemBit(Value)) != 0; }

    Status Join(Id PlayerId, Team TeamId)
    {
        Event E;
        E.Kind = EventKind::Join;
        E.PlayerId = PlayerId;
        E.TeamId = TeamId;
        return Model.Apply(E);
    }
    void JoinFour()
    {
        Join(PlayerA, Team::One);
        Join(PlayerB, Team::One);
        Join(PlayerC, Team::Two);
        Join(PlayerD, Team::Two);
    }
    Status Begin(Team Attackers, std::uint8_t Completed, Id RoundId = 0)
    {
        Event E;
        E.Kind = EventKind::BuyPhaseBegin;
        E.RoundId = RoundId != 0 ? RoundId : Round + 64;
        E.TeamId = Attackers;
        E.CompletedRounds = Completed;
        const Status Result = Model.Apply(E);
        if (Result == Status::Applied) { Round = E.RoundId; }
        return Result;
    }
    Status RoundEvent(EventKind Kind, Id PlayerId = 0, Id OtherId = 0, Team TeamId = Team::None, Id RoundId = 0)
    {
        Event E;
        E.Kind = Kind;
        E.RoundId = RoundId != 0 ? RoundId : Round;
        E.PlayerId = PlayerId;
        E.OtherId = OtherId;
        E.TeamId = TeamId;
        return Model.Apply(E);
    }
    Status End() { return RoundEvent(EventKind::BuyPhaseEnd); }
    Status Result(Team Winner) { return RoundEvent(EventKind::RoundResult, 0, 0, Winner); }
    Status Plant() { return RoundEvent(EventKind::Plant); }
    Status Kill(Id Killer, Id Victim) { return RoundEvent(EventKind::Kill, Killer, Victim); }
    Status Death(Id PlayerId) { return RoundEvent(EventKind::Death, PlayerId); }
    Status Request(EventKind Kind, Id PlayerId, Item Value, Team TeamId, bool bAlive = true, Id RoundId = 0, Id RequestId = 0)
    {
        Event E;
        E.Kind = Kind;
        E.PlayerId = PlayerId;
        E.ItemId = Value;
        E.TeamId = TeamId;
        E.Alive = bAlive;
        E.RoundId = RoundId != 0 ? RoundId : Round;
        E.RequestId = RequestId != 0 ? RequestId : ++NextRequest;
        if (E.RequestId > NextRequest) { NextRequest = E.RequestId; }
        return Model.Apply(E);
    }
    Status Buy(Id PlayerId, Item Value, Team TeamId) { return Request(EventKind::Purchase, PlayerId, Value, TeamId); }
    Status Refund(Id PlayerId, Item Value, Team TeamId) { return Request(EventKind::Refund, PlayerId, Value, TeamId); }
    // One full round: buy phase, live, optional plant, result.
    void PlayRound(Team Attackers, std::uint8_t Completed, Team Winner, bool bPlant = false)
    {
        Begin(Attackers, Completed);
        End();
        if (bPlant) { Plant(); }
        Result(Winner);
    }
};

bool SameWallets(const Snapshot& Left, const Snapshot& Right)
{
    for (std::size_t I = 0; I < MaxWallets; ++I)
    {
        const Wallet& L = Left.Wallets[I];
        const Wallet& R = Right.Wallets[I];
        if (L.PlayerId != R.PlayerId || L.TeamId != R.TeamId || L.Balance != R.Balance || L.Owned != R.Owned ||
            L.Refundable != R.Refundable || L.RoundIncome != R.RoundIncome || L.LastRoundIncome != R.LastRoundIncome ||
            L.Died != R.Died || L.Killed != R.Killed || L.Requests != R.Requests || L.LastRequestId != R.LastRequestId)
        {
            return false;
        }
    }
    return Left.CurrentPhase == Right.CurrentPhase && Left.RoundId == Right.RoundId &&
           Left.AttackingTeam == Right.AttackingTeam && Left.LossStreak == Right.LossStreak &&
           Left.PlantAwarded == Right.PlantAwarded && Left.ResultAwarded == Right.ResultAwarded &&
           Left.Resets == Right.Resets && Left.CompletedRounds == Right.CompletedRounds;
}

void TuningIsValidatedAndInvalidTuningRejectsEverything()
{
    CHECK(EconomyRules::IsValidTuning(Tuning{}));
    Tuning Values;
    Values.StartingCredits = 9001;
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.KillReward = -1;
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.Prices[static_cast<std::size_t>(Item::Rifle)] = 9001;
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.Prices[0] = 1;
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.StarterItems = ItemBit(Item::None);
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.StarterItems = ItemMask{1} << 7; // Not in the catalog.
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.MaxRequestsPerRound = 0;
    CHECK(!EconomyRules::IsValidTuning(Values));
    Values = Tuning{};
    Values.CreditCap = MaxConfigurableCredits + 1;
    CHECK(!EconomyRules::IsValidTuning(Values));

    Values = Tuning{};
    Values.StartingCredits = -5;
    Fixture F(Values);
    CHECK(!F.State().TuningValid);
    CHECK(F.Join(PlayerA, Team::One) == Status::InvalidTuning);
    CHECK(F.Begin(Team::One, 0) == Status::InvalidTuning);
    CHECK(F.State().Find(PlayerA) == nullptr);
}

void CatalogAndDefaults()
{
    CHECK(FindItem(Item::None) == nullptr);
    CHECK(FindItem(static_cast<Item>(99)) == nullptr);
    REQUIRE(FindItem(Item::Rifle) != nullptr);
    CHECK(FindItem(Item::Rifle)->SlotId == Slot::Primary);
    CHECK(std::strcmp(FindItem(Item::Pistol)->Key, "pistol") == 0);
    EconomyRules Model;
    CHECK(Model.Price(Item::Rifle) == 2700);
    CHECK(Model.Price(Item::Pistol) == 400);
    CHECK(Model.Price(static_cast<Item>(42)) == 0);
    CHECK(std::strcmp(StatusName(Status::NotRefundable), "not_refundable") == 0);
    CHECK(std::strcmp(StatusName(static_cast<Status>(StatusCount)), "unknown") == 0);
}

void JoinLeaveAndRoster()
{
    Fixture F;
    CHECK(F.Join(PlayerA, Team::One) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 800);
    CHECK(F.Owns(PlayerA, Item::Pistol) && !F.Owns(PlayerA, Item::Rifle));
    CHECK(F.Join(PlayerA, Team::One) == Status::Duplicate);
    CHECK(F.Join(0, Team::One) == Status::InvalidEvent);
    CHECK(F.Join(99, Team::None) == Status::InvalidEvent);
    for (Id I = 0; I < MaxWallets - 1; ++I) { CHECK(F.Join(100 + I, I % 2 ? Team::One : Team::Two) == Status::Applied); }
    CHECK(F.Join(500, Team::Two) == Status::RosterFull);

    Event Leave;
    Leave.Kind = EventKind::Leave;
    Leave.PlayerId = PlayerA;
    CHECK(F.Model.Apply(Leave) == Status::Applied);
    CHECK(F.State().Find(PlayerA) == nullptr);
    CHECK(F.Model.Apply(Leave) == Status::UnknownPlayer);
    // A reconnect is a new server identity and starts with starting credits.
    CHECK(F.Join(500, Team::Two) == Status::Applied);
    CHECK(F.Balance(500) == 800);
}

void RoundWinAndLossRewards()
{
    Fixture F;
    F.JoinFour();
    F.PlayRound(Team::One, 0, Team::One);
    CHECK(F.Balance(PlayerA) == 3800 && F.Balance(PlayerB) == 3800);
    CHECK(F.Balance(PlayerC) == 2500 && F.Balance(PlayerD) == 2500);
    CHECK(F.W(PlayerA).RoundIncome == 3000 && F.W(PlayerC).RoundIncome == 1700);
    CHECK(F.State().LossStreak[0] == 0 && F.State().LossStreak[1] == 1);
    CHECK(F.State().CurrentPhase == Phase::Ended);
    // The income stays visible during the next buy phase.
    F.Begin(Team::One, 1);
    CHECK(F.W(PlayerA).LastRoundIncome == 3000 && F.W(PlayerA).RoundIncome == 0);
    CHECK(F.W(PlayerC).LastRoundIncome == 1700);
}

void LossStreakBonusIsCappedAndResetByAWin()
{
    Tuning Roomy; // Keep balances below the credit cap so every reward is visible.
    Roomy.CreditCap = 100000;
    Fixture F(Roomy);
    F.JoinFour();
    const Credits Expected[] = {1700, 2200, 2700, 2700, 2700};
    std::uint8_t Completed = 0;
    Credits Before = F.Balance(PlayerC);
    for (Credits Amount : Expected)
    {
        F.PlayRound(Team::One, Completed++, Team::One);
        CHECK(F.W(PlayerC).RoundIncome == Amount);
        CHECK(F.Balance(PlayerC) == Before + Amount);
        Before = F.Balance(PlayerC);
    }
    CHECK(F.State().LossStreak[1] == 5);
    CHECK(F.Model.LossReward(0) == 0 && F.Model.LossReward(1) == 1700 && F.Model.LossReward(255) == 2700);
    // A win resets that team's streak; the other team's streak starts counting.
    F.PlayRound(Team::One, Completed++, Team::Two);
    CHECK(F.State().LossStreak[1] == 0 && F.State().LossStreak[0] == 1);
    F.PlayRound(Team::One, Completed++, Team::One);
    CHECK(F.W(PlayerC).RoundIncome == 1700);
    CHECK(F.W(PlayerA).RoundIncome == 3000);

    // The cap is tunable: zero steps keeps the base loss reward.
    Tuning Flat;
    Flat.LossStreakMaxSteps = 0;
    Fixture G(Flat);
    G.JoinFour();
    G.PlayRound(Team::One, 0, Team::One);
    G.PlayRound(Team::One, 1, Team::One);
    CHECK(G.W(PlayerC).RoundIncome == 1700);
}

void KillRewards()
{
    Fixture F;
    F.JoinFour();
    F.Begin(Team::One, 0);
    CHECK(F.Kill(PlayerA, PlayerC) == Status::WrongPhase); // Buy phase is not live.
    F.End();
    CHECK(F.Kill(PlayerA, PlayerC) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 1000);
    CHECK(F.W(PlayerC).Died && F.W(PlayerC).Killed);
    // A second report for the same victim never pays again, even from someone else.
    CHECK(F.Kill(PlayerA, PlayerC) == Status::Duplicate);
    CHECK(F.Kill(PlayerB, PlayerC) == Status::Duplicate);
    CHECK(F.Balance(PlayerA) == 1000 && F.Balance(PlayerB) == 800);
    // A team kill is recorded as a death and earns nothing.
    CHECK(F.Kill(PlayerA, PlayerB) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 1000 && F.W(PlayerB).Died);
    CHECK(F.Kill(PlayerD, PlayerD) == Status::InvalidEvent);
    CHECK(F.Kill(PlayerD, 0) == Status::InvalidEvent);
    CHECK(F.Kill(999, PlayerD) == Status::UnknownPlayer);
    CHECK(F.RoundEvent(EventKind::Kill, PlayerD, PlayerA, Team::None, F.Round + 1) == Status::StaleRound);
    CHECK(F.Balance(PlayerD) == 800);
    // A death observed before the kill report still allows that one kill credit.
    CHECK(F.Death(PlayerA) == Status::Applied);
    CHECK(F.Kill(PlayerD, PlayerA) == Status::Applied);
    CHECK(F.Balance(PlayerD) == 1000);
    CHECK(F.Death(PlayerA) == Status::Duplicate);
    F.Result(Team::Two);
    CHECK(F.Kill(PlayerC, PlayerB) == Status::WrongPhase); // Round over.
    CHECK(F.Balance(PlayerC) == 800 + 3000);
}

void PlantRewardPaysEveryAttackerOnce()
{
    Fixture F;
    F.JoinFour();
    F.Begin(Team::Two, 3);
    CHECK(F.Plant() == Status::WrongPhase);
    F.End();
    F.Death(PlayerD); // A fallen attacker still shares the team reward.
    CHECK(F.Plant() == Status::Applied);
    CHECK(F.Balance(PlayerC) == 1100 && F.Balance(PlayerD) == 1100);
    CHECK(F.Balance(PlayerA) == 800 && F.Balance(PlayerB) == 800);
    CHECK(F.Plant() == Status::Duplicate);
    CHECK(F.RoundEvent(EventKind::Plant, 0, 0, Team::None, F.Round + 1) == Status::StaleRound);
    CHECK(F.Balance(PlayerC) == 1100);
    F.Result(Team::Two);
    CHECK(F.Plant() == Status::Duplicate);
    CHECK(F.Balance(PlayerC) == 1100 + 3000 && F.W(PlayerC).RoundIncome == 3300);
}

void CreditCapClampsAllIncome()
{
    Fixture F;
    F.JoinFour();
    for (std::uint8_t I = 0; I < 3; ++I) { F.PlayRound(Team::One, I, Team::One, true); }
    // 800 + 3 * (3000 + 300) = 10700, capped.
    CHECK(F.Balance(PlayerA) == 9000);
    CHECK(F.W(PlayerA).RoundIncome == 9000 - (800 + 2 * 3300));
    F.Begin(Team::One, 3, F.Round + 64); // Same attackers: no swap, no reset.
    F.End();
    CHECK(F.Kill(PlayerA, PlayerC) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 9000 && F.W(PlayerA).RoundIncome == 0);
    CHECK(F.Plant() == Status::Applied);
    CHECK(F.Balance(PlayerA) == 9000);

    Tuning Small;
    Small.CreditCap = 1000;
    Small.StartingCredits = 1000;
    Small.Prices[static_cast<std::size_t>(Item::Rifle)] = 1000;
    Fixture G(Small);
    G.JoinFour();
    G.Begin(Team::One, 0);
    CHECK(G.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(G.Refund(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(G.Balance(PlayerA) == 1000);
}

void SideSwapAndNewMatchResetEverything()
{
    Fixture F;
    F.JoinFour();
    F.PlayRound(Team::One, 0, Team::One, true);
    F.Begin(Team::One, 1);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    F.End();
    F.Result(Team::Two);
    F.PlayRound(Team::One, 2, Team::Two);
    CHECK(F.State().LossStreak[0] == 2);
    CHECK(F.Balance(PlayerA) != 800 && F.Owns(PlayerA, Item::Rifle));
    const std::uint32_t Resets = F.State().Resets;

    // Rounds completed = 3 and roles change: the side swap resets credits, items and streaks.
    CHECK(F.Begin(Team::Two, 3) == Status::Applied);
    CHECK(F.State().Resets == Resets + 1);
    for (Id P : {PlayerA, PlayerB, PlayerC, PlayerD})
    {
        CHECK(F.Balance(P) == 800);
        CHECK(F.Owns(P, Item::Pistol) && !F.Owns(P, Item::Rifle));
        CHECK(F.W(P).LastRoundIncome == 0 && F.W(P).Refundable == 0);
    }
    CHECK(F.State().LossStreak[0] == 0 && F.State().LossStreak[1] == 0);
    F.End();
    F.Result(Team::Two);
    // Same half, same attackers: no reset.
    F.Begin(Team::Two, 4);
    CHECK(F.State().Resets == Resets + 1 && F.Balance(PlayerC) == 3800);
    F.End();
    F.Result(Team::Two);
    F.PlayRound(Team::Two, 5, Team::Two);
    CHECK(F.Balance(PlayerC) == 9000);

    // A rematch begins at zero completed rounds with team One attacking again.
    CHECK(F.Begin(Team::One, 0) == Status::Applied);
    CHECK(F.State().Resets == Resets + 2);
    for (Id P : {PlayerA, PlayerB, PlayerC, PlayerD}) { CHECK(F.Balance(P) == 800 && !F.Owns(P, Item::Rifle)); }
    CHECK(F.State().LossStreak[0] == 0 && F.State().LossStreak[1] == 0);

    // A cleared server can also restart a match with the same attackers; zero completed rounds still resets.
    F.End();
    F.Result(Team::One);
    CHECK(F.Balance(PlayerA) == 3800);
    CHECK(F.Begin(Team::One, 0) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 800);
}

void AbortedPreparationKeepsStateAndRoundIncome()
{
    Fixture F;
    F.JoinFour();
    F.PlayRound(Team::One, 0, Team::One);
    F.Begin(Team::One, 1);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    // The match model returned to Waiting during preparation and prepared a new round id.
    const std::uint32_t Resets = F.State().Resets;
    CHECK(F.Begin(Team::One, 1) == Status::Applied);
    CHECK(F.State().Resets == Resets);
    CHECK(F.Owns(PlayerA, Item::Rifle) && F.Balance(PlayerA) == 1100);
    CHECK(F.W(PlayerA).Refundable == 0); // Refunds belong to one buy phase.
    CHECK(F.W(PlayerA).LastRoundIncome == 3000);
}

void PurchaseValidation()
{
    Fixture F;
    F.JoinFour();
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::NotBuyPhase); // Idle before any round.
    F.Begin(Team::One, 0);
    CHECK(F.Buy(999, Item::Rifle, Team::One) == Status::UnknownPlayer);
    CHECK(F.Buy(PlayerA, Item::None, Team::One) == Status::UnknownItem);
    CHECK(F.Buy(PlayerA, static_cast<Item>(77), Team::One) == Status::UnknownItem);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::Two) == Status::NotOnTeam);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::None) == Status::NotOnTeam);
    CHECK(F.Request(EventKind::Purchase, PlayerA, Item::Rifle, Team::One, false) == Status::NotAlive);
    CHECK(F.Request(EventKind::Purchase, PlayerA, Item::Rifle, Team::One, true, F.Round - 1) == Status::StaleRound);
    CHECK(F.Buy(PlayerA, Item::Pistol, Team::One) == Status::AlreadyOwned); // Starter sidearm.
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Unaffordable);
    CHECK(F.Balance(PlayerA) == 800 && !F.Owns(PlayerA, Item::Rifle));
    F.End();
    F.Result(Team::One);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::NotBuyPhase); // Round over.

    F.Begin(Team::One, 1);
    CHECK(F.Balance(PlayerA) == 3800);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 1100 && F.Owns(PlayerA, Item::Rifle));
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::AlreadyOwned);
    CHECK(F.Balance(PlayerA) == 1100);
    F.End();
    CHECK(F.Buy(PlayerC, Item::Rifle, Team::Two) == Status::NotBuyPhase);
    CHECK(F.Balance(PlayerC) == 2500);

    // With the pistol removed from the starter set it is an ordinary purchase.
    Tuning NoStarter;
    NoStarter.StarterItems = 0;
    Fixture G(NoStarter);
    G.JoinFour();
    CHECK(!G.Owns(PlayerA, Item::Pistol));
    G.Begin(Team::One, 0);
    CHECK(G.Buy(PlayerA, Item::Pistol, Team::One) == Status::Applied);
    CHECK(G.Balance(PlayerA) == 400 && G.Owns(PlayerA, Item::Pistol));
}

void RequestReplayAndSpam()
{
    Fixture F;
    F.JoinFour();
    F.PlayRound(Team::One, 0, Team::One);
    F.Begin(Team::One, 1);
    CHECK(F.Request(EventKind::Purchase, PlayerA, Item::Rifle, Team::One, true, 0, 10) == Status::Applied);
    const Id Last = F.W(PlayerA).LastRequestId;
    CHECK(Last == 10);
    // A replayed or reordered packet is never evaluated again.
    CHECK(F.Request(EventKind::Refund, PlayerA, Item::Rifle, Team::One, true, 0, Last) == Status::Replayed);
    CHECK(F.Request(EventKind::Refund, PlayerA, Item::Rifle, Team::One, true, 0, Last - 1) == Status::Replayed);
    Event Zero;
    Zero.Kind = EventKind::Purchase;
    Zero.PlayerId = PlayerA;
    Zero.RoundId = F.Round;
    Zero.ItemId = Item::Rifle;
    Zero.TeamId = Team::One;
    Zero.Alive = true;
    CHECK(F.Model.Apply(Zero) == Status::InvalidEvent);
    CHECK(F.Owns(PlayerA, Item::Rifle) && F.Balance(PlayerA) == 1100);

    // Spam: the round's request budget is shared by purchases and refunds; excess is refused unevaluated.
    int Applied = 0, Owned = 0, Limited = 0;
    for (int I = 0; I < 40; ++I)
    {
        const Status Result = F.Buy(PlayerA, Item::Rifle, Team::One);
        Applied += Result == Status::Applied;
        Owned += Result == Status::AlreadyOwned;
        Limited += Result == Status::RateLimited;
    }
    CHECK(Applied == 0 && Owned == 23 && Limited == 17);
    CHECK(F.Refund(PlayerA, Item::Rifle, Team::One) == Status::RateLimited);
    CHECK(F.Balance(PlayerA) == 1100 && F.Owns(PlayerA, Item::Rifle));
    // The budget renews with the next buy phase.
    F.End();
    F.Result(Team::One);
    F.Begin(Team::One, 2);
    CHECK(F.W(PlayerA).Requests == 0);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::AlreadyOwned);
}

void RefundsBelongToOneBuyPhase()
{
    Fixture F;
    F.JoinFour();
    F.PlayRound(Team::One, 0, Team::One);
    F.Begin(Team::One, 1);
    CHECK(F.Refund(PlayerA, Item::Pistol, Team::One) == Status::NotRefundable); // Starter.
    CHECK(F.Refund(PlayerA, Item::Rifle, Team::One) == Status::NotRefundable); // Not owned.
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(F.Refund(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 3800 && !F.Owns(PlayerA, Item::Rifle));
    CHECK(F.Refund(PlayerA, Item::Rifle, Team::One) == Status::NotRefundable);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(F.Balance(PlayerA) == 1100);
    CHECK(F.Request(EventKind::Refund, PlayerA, Item::Rifle, Team::One, false) == Status::NotAlive);
    F.End();
    CHECK(F.Refund(PlayerA, Item::Rifle, Team::One) == Status::NotBuyPhase);
    CHECK(F.W(PlayerA).Refundable == 0);
    F.Result(Team::One);
    // The next buy phase cannot refund a purchase from an earlier one.
    F.Begin(Team::One, 2);
    CHECK(F.Owns(PlayerA, Item::Rifle));
    CHECK(F.Refund(PlayerA, Item::Rifle, Team::One) == Status::NotRefundable);
    CHECK(F.Balance(PlayerA) == 1100 + 3000);
}

void PurchasesSurviveOnlyWithThePlayer()
{
    Fixture F;
    F.JoinFour();
    F.PlayRound(Team::One, 0, Team::One);
    F.Begin(Team::One, 1);
    CHECK(F.Buy(PlayerA, Item::Rifle, Team::One) == Status::Applied);
    CHECK(F.Buy(PlayerB, Item::Rifle, Team::One) == Status::Applied);
    F.End();
    CHECK(F.Kill(PlayerC, PlayerA) == Status::Applied);
    F.Result(Team::One);
    F.Begin(Team::One, 2);
    CHECK(!F.Owns(PlayerA, Item::Rifle) && F.Owns(PlayerA, Item::Pistol)); // Fell: starter only.
    CHECK(F.Owns(PlayerB, Item::Rifle)); // Survived: keeps the rifle.
    CHECK(!F.W(PlayerA).Died);
    F.End();
    CHECK(F.Death(PlayerB) == Status::Applied);
    F.Result(Team::Two);
    F.Begin(Team::One, 3);
    CHECK(!F.Owns(PlayerB, Item::Rifle) && F.Owns(PlayerB, Item::Pistol));
}

void DuplicateAndOutOfOrderRoundEvents()
{
    Fixture F;
    F.JoinFour();
    CHECK(F.Result(Team::One) == Status::StaleRound); // No round yet.
    CHECK(F.Begin(Team::None, 0) == Status::InvalidEvent);
    CHECK(F.Begin(Team::One, 0, 640) == Status::Applied);
    CHECK(F.Begin(Team::One, 0, 640) == Status::Duplicate);
    CHECK(F.Begin(Team::One, 0, 320) == Status::StaleRound);
    CHECK(F.Result(Team::One) == Status::WrongPhase); // The adapter must close the buy phase first.
    CHECK(F.Death(PlayerA) == Status::WrongPhase);
    CHECK(F.End() == Status::Applied);
    CHECK(F.End() == Status::Duplicate);
    CHECK(F.RoundEvent(EventKind::BuyPhaseEnd, 0, 0, Team::None, 641) == Status::StaleRound);
    CHECK(F.Result(Team::None) == Status::InvalidEvent);
    CHECK(F.Result(Team::Two) == Status::Applied);
    CHECK(F.Result(Team::Two) == Status::Duplicate);
    CHECK(F.Result(Team::One) == Status::Duplicate); // One award per round, whatever the claimed winner.
    CHECK(F.RoundEvent(EventKind::RoundResult, 0, 0, Team::Two, 641) == Status::StaleRound);
    CHECK(F.Balance(PlayerC) == 3800 && F.Balance(PlayerA) == 2500);
    CHECK(F.State().LossStreak[0] == 1);
    // Duplicating the next buy phase does not strip or pay twice.
    CHECK(F.Begin(Team::One, 1, 704) == Status::Applied);
    CHECK(F.Begin(Team::One, 1, 704) == Status::Duplicate);
    CHECK(F.Balance(PlayerC) == 3800 && F.W(PlayerC).LastRoundIncome == 3000);
    CHECK(F.RoundEvent(EventKind::Plant, 0, 0, Team::None, 640) == Status::StaleRound);
    CHECK(F.Model.Apply(Event{static_cast<EventKind>(99)}) == Status::InvalidEvent);
}

void SameEventsGiveTheSameState()
{
    const auto Script = [](Fixture& F)
    {
        F.JoinFour();
        F.Begin(Team::One, 0);
        F.Buy(PlayerA, Item::Rifle, Team::One);
        F.End();
        F.Plant();
        F.Kill(PlayerA, PlayerC);
        F.Death(PlayerB);
        F.Result(Team::One);
        F.Begin(Team::One, 1);
        F.Buy(PlayerA, Item::Rifle, Team::One);
        F.Refund(PlayerA, Item::Rifle, Team::One);
        F.Buy(PlayerC, Item::Rifle, Team::Two);
        F.End();
        F.Kill(PlayerC, PlayerA);
        F.Kill(PlayerC, PlayerA);
        F.Result(Team::Two);
        F.Result(Team::Two);
        F.Begin(Team::Two, 3);
    };
    Fixture First, Second;
    Script(First);
    Script(Second);
    CHECK(SameWallets(First.State(), Second.State()));
    CHECK(First.State().Resets == 2);
}

// The exact 1v1 scenario the networked smoke test drives (Scripts/test_economy.py --network,
// Source/ValStrike/Economy/Tests/VSEconomySmoke.cpp). Team One attacks in rounds 1-3.
void NetworkScenarioLedger()
{
    Fixture F;
    constexpr Id One = 1, Two = 2;
    F.Join(One, Team::One);
    F.Join(Two, Team::Two);
    // Round 1: rifles unaffordable; One plants, Two kills the planter and defuses.
    F.Begin(Team::One, 0);
    CHECK(F.Buy(One, Item::Rifle, Team::One) == Status::Unaffordable);
    CHECK(F.Buy(Two, Item::Rifle, Team::Two) == Status::Unaffordable);
    CHECK(F.Buy(One, Item::Pistol, Team::One) == Status::AlreadyOwned);
    F.End();
    CHECK(F.Plant() == Status::Applied);
    CHECK(F.Kill(Two, One) == Status::Applied);
    CHECK(F.Result(Team::Two) == Status::Applied);
    CHECK(F.Balance(One) == 2800 && F.Balance(Two) == 4000);
    // Round 2: both buy rifles; One kills Two and survives.
    F.Begin(Team::One, 1);
    CHECK(F.Buy(One, Item::Rifle, Team::One) == Status::Applied);
    CHECK(F.Buy(Two, Item::Rifle, Team::Two) == Status::Applied);
    CHECK(F.Balance(One) == 100 && F.Balance(Two) == 1300);
    F.End();
    F.Kill(One, Two);
    F.Result(Team::One);
    CHECK(F.Balance(One) == 3300 && F.Balance(Two) == 3000);
    // Round 3: the survivor keeps the rifle, the fallen lost it; Two buys, refunds, buys.
    F.Begin(Team::One, 2);
    CHECK(F.Owns(One, Item::Rifle) && !F.Owns(Two, Item::Rifle));
    CHECK(F.Buy(One, Item::Rifle, Team::One) == Status::AlreadyOwned);
    CHECK(F.Buy(Two, Item::Rifle, Team::Two) == Status::Applied);
    CHECK(F.Refund(Two, Item::Rifle, Team::Two) == Status::Applied);
    CHECK(F.Buy(Two, Item::Rifle, Team::Two) == Status::Applied);
    CHECK(F.Balance(Two) == 300);
    F.End();
    F.Kill(Two, One);
    F.Result(Team::Two);
    CHECK(F.Balance(One) == 5000 && F.Balance(Two) == 3500 && F.Owns(Two, Item::Rifle));
    // Rounds 4-6 after the swap: Two attacks, kills One and wins each round; One's loss
    // streak bonus grows and caps, Two reaches the credit cap.
    const Credits ExpectedOne[] = {2500, 4700, 7400};
    const Credits ExpectedTwo[] = {4000, 7200, 9000};
    for (std::uint8_t I = 0; I < 3; ++I)
    {
        F.Begin(Team::Two, static_cast<std::uint8_t>(3 + I));
        if (I == 0) { CHECK(F.Balance(One) == 800 && F.Balance(Two) == 800 && !F.Owns(Two, Item::Rifle)); }
        F.End();
        F.Kill(Two, One);
        F.Result(Team::Two);
        CHECK(F.Balance(One) == ExpectedOne[I]);
        CHECK(F.Balance(Two) == ExpectedTwo[I]);
    }
    // Rematch.
    F.Begin(Team::One, 0);
    CHECK(F.Balance(One) == 800 && F.Balance(Two) == 800);
}
}

int main()
{
    struct Case { const char* Name; void (*Run)(); };
    const Case Cases[] = {
        {"tuning", TuningIsValidatedAndInvalidTuningRejectsEverything},
        {"catalog", CatalogAndDefaults},
        {"join_leave", JoinLeaveAndRoster},
        {"round_rewards", RoundWinAndLossRewards},
        {"loss_streak_cap", LossStreakBonusIsCappedAndResetByAWin},
        {"kill_rewards", KillRewards},
        {"plant_reward", PlantRewardPaysEveryAttackerOnce},
        {"credit_cap", CreditCapClampsAllIncome},
        {"reset_swap_rematch", SideSwapAndNewMatchResetEverything},
        {"aborted_preparation", AbortedPreparationKeepsStateAndRoundIncome},
        {"purchase_validation", PurchaseValidation},
        {"replay_and_spam", RequestReplayAndSpam},
        {"refunds", RefundsBelongToOneBuyPhase},
        {"death_loses_purchases", PurchasesSurviveOnlyWithThePlayer},
        {"duplicate_round_events", DuplicateAndOutOfOrderRoundEvents},
        {"deterministic", SameEventsGiveTheSameState},
        {"network_scenario_ledger", NetworkScenarioLedger},
    };
    int FailedCases = 0;
    for (const Case& Item : Cases)
    {
        CurrentTest = Item.Name;
        const int Before = Failures;
        Item.Run();
        const bool Passed = Failures == Before;
        FailedCases += Passed ? 0 : 1;
        std::printf("%s %s\n", Passed ? "PASS" : "FAIL", Item.Name);
    }
    const int Total = static_cast<int>(sizeof(Cases) / sizeof(Cases[0]));
    std::printf("%d/%d cases passed, %d checks, %d failed checks\n", Total - FailedCases, Total, Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
