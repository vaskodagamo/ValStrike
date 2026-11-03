#include "Rules/VSMatchRules.h"

#include <cstdio>
#include <initializer_list>
#include <set>

using namespace ValStrike::Rules;

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

Config FastConfig(std::uint8_t Minimum = 1)
{
    Config Result;
    Result.MinimumPerTeam = Minimum;
    Result.PreparationMs = 8;
    Result.RoundMs = 90;
    Result.PlantMs = 3;
    Result.DefuseMs = 5;
    Result.DetonationMs = 30;
    return Result;
}

Event Join(Id PlayerId, Team TeamId)
{
    Event Result;
    Result.Kind = EventKind::Join;
    Result.PlayerId = PlayerId;
    Result.TeamId = TeamId;
    return Result;
}

std::size_t Count(const StepResult& Result, EffectKind Kind)
{
    std::size_t Total = 0;
    for (const Effect& Value : Result.Effects)
    {
        if (Value.Kind == Kind) { ++Total; }
    }
    return Total;
}

TimeMs EffectTime(const StepResult& Result, EffectKind Kind)
{
    for (const Effect& Value : Result.Effects)
    {
        if (Value.Kind == Kind) { return Value.At; }
    }
    return -1;
}

struct Fixture
{
    Config Tuning;
    MatchRules Rules;
    Id Sequence = 0;

    explicit Fixture(const Config& ConfigValue = FastConfig()) : Tuning(ConfigValue), Rules(ConfigValue) {}

    const Snapshot& State() const { return Rules.Read(); }

    Event Make(EventKind Kind, Id PlayerId = 0, Id RequestId = 0) const
    {
        Event Result;
        Result.Kind = Kind;
        Result.PlayerId = PlayerId;
        Result.RoundId = State().RoundId;
        Result.RequestId = RequestId;
        Result.DeviceRevision = State().Objective.Revision;
        Result.InteractionToken = State().CurrentInteraction.Token;
        Result.WorldConfirmed = true;
        Result.Site = ObjectiveSite;
        return Result;
    }

    Frame MakeFrame(TimeMs Now, std::initializer_list<Event> Events = {}, bool Certified = false)
    {
        Frame Input;
        Input.Now = Now;
        Input.Sequence = ++Sequence;
        Input.ContinuousInteractionToken = Certified ? State().CurrentInteraction.Token : 0;
        for (const Event& Value : Events) { Input.Events[Input.EventCount++] = Value; }
        return Input;
    }

    StepResult Step(TimeMs Now, std::initializer_list<Event> Events = {}, bool Certified = false)
    {
        return Rules.Apply(MakeFrame(Now, Events, Certified));
    }

    StepResult Pair(TimeMs Now = 0)
    {
        return Step(Now, {Join(1, Team::One), Join(2, Team::Two)});
    }

    StepResult Four(TimeMs Now = 0)
    {
        // Deliberately shuffled input: admission/carrier choice must be canonical.
        return Step(Now, {Join(4, Team::Two), Join(2, Team::Two), Join(3, Team::One), Join(1, Team::One)});
    }

    StepResult Activate() { return Step(State().PhaseDeadline); }

    StepResult BeginPlant(TimeMs Now = 9, Id PlayerId = 1, Id RequestId = 1)
    {
        return Step(Now, {Make(EventKind::StartPlant, PlayerId, RequestId)});
    }

    StepResult Plant(TimeMs Now = 9)
    {
        BeginPlant(Now);
        return Step(Now + Tuning.PlantMs, {}, true);
    }
};

bool SameSnapshot(const Snapshot& A, const Snapshot& B)
{
    bool Same = A.ConfigurationValid == B.ConfigurationValid && A.Now == B.Now &&
        A.CurrentPhase == B.CurrentPhase && A.RoundId == B.RoundId && A.AttackingTeam == B.AttackingTeam &&
        A.Scores == B.Scores && A.CompletedRounds == B.CompletedRounds && A.RoundWinner == B.RoundWinner &&
        A.Reason == B.Reason && A.Result == B.Result && A.PhaseDeadline == B.PhaseDeadline &&
        A.Objective.State == B.Objective.State && A.Objective.Revision == B.Objective.Revision &&
        A.Objective.Carrier == B.Objective.Carrier && A.Objective.Site == B.Objective.Site &&
        A.CurrentInteraction.Kind == B.CurrentInteraction.Kind && A.CurrentInteraction.Token == B.CurrentInteraction.Token &&
        A.CurrentInteraction.PlayerId == B.CurrentInteraction.PlayerId &&
        A.CurrentInteraction.StartedAt == B.CurrentInteraction.StartedAt &&
        A.CurrentInteraction.CompletesAt == B.CurrentInteraction.CompletesAt;
    for (std::size_t I = 0; I < MaxPlayers; ++I)
    {
        const Player& X = A.Players[I];
        const Player& Y = B.Players[I];
        Same = Same && X.PlayerId == Y.PlayerId && X.TeamId == Y.TeamId &&
            X.Participating == Y.Participating && X.Alive == Y.Alive && X.LastRequestId == Y.LastRequestId;
    }
    return Same;
}

void DefaultTimingAndPhaseGates()
{
    Fixture F(Config{});
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    CHECK(!F.State().CanMove(1));
    CHECK(!F.State().CanCombat(1));
    const StepResult Joined = F.Pair();
    REQUIRE(Joined.Status == FrameStatus::Applied);
    CHECK(Count(Joined, EffectKind::PrepareRound) == 1);
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().PhaseDeadline == 30'000);
    CHECK(F.State().RosterCount(Team::One) == 1);
    CHECK(F.State().ParticipantCount(Team::Two) == 1);
    CHECK(F.State().LivingCount(Team::One) == 1);
    CHECK(F.State().Objective.State == DeviceState::Carried);
    CHECK(F.State().Objective.Carrier == 1);
    CHECK(!F.State().CanMove(1));
    CHECK(!F.State().CanCombat(2));
    F.Step(29'999);
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    // The longer buy phase still keeps everyone in spawn with combat closed until the deadline.
    CHECK(!F.State().CanMove(1) && !F.State().CanCombat(1) && !F.State().CanCombat(2));
    const StepResult Active = F.Step(30'000);
    CHECK(F.State().CurrentPhase == Phase::Active);
    CHECK(F.State().PhaseDeadline == 120'000);
    CHECK(F.State().CanMove(1));
    CHECK(F.State().CanCombat(2));
    CHECK(!F.State().CanCombat(99));
    CHECK(Count(Active, EffectKind::RoundActivated) == 1);
}

void InvalidConfigurationAndBoundedTime()
{
    const Config Good = FastConfig();
    CHECK(MatchRules::IsValidConfig(Good));
    for (int Case = 0; Case < 9; ++Case)
    {
        Config Bad = Good;
        switch (Case)
        {
        case 0: Bad.MinimumPerTeam = 0; break;
        case 1: Bad.MinimumPerTeam = 3; break;
        case 2: Bad.PreparationMs = 0; break;
        case 3: Bad.RoundMs = -1; break;
        case 4: Bad.PlantMs = 0; break;
        case 5: Bad.DefuseMs = -1; break;
        case 6: Bad.DetonationMs = 0; break;
        case 7: Bad.RoundMs = MaxDurationMs + 1; break;
        default: Bad.PreparationMs = INT64_MAX; break;
        }
        Fixture Invalid(Bad);
        CHECK(!MatchRules::IsValidConfig(Bad));
        CHECK(!Invalid.State().ConfigurationValid);
        const StepResult Result = Invalid.Pair();
        CHECK(Result.Status == FrameStatus::InvalidConfiguration);
        CHECK(Result.Effects.empty());
        CHECK(Invalid.State().CurrentPhase == Phase::Waiting);
    }
    Fixture F;
    CHECK(F.Step(-1).Status == FrameStatus::InvalidTime);
    CHECK(F.Step(MaxServerTimeMs + 1).Status == FrameStatus::InvalidTime);
    CHECK(F.Pair(MaxServerTimeMs - 100).Status == FrameStatus::Applied);
    CHECK(F.State().PhaseDeadline == MaxServerTimeMs - 92);
    CHECK(F.Step(MaxServerTimeMs).Status == FrameStatus::Applied);
    CHECK(F.State().CurrentPhase == Phase::Resolved);
    CHECK(F.State().Reason == WinReason::Timeout);
    CHECK(F.State().CompletedRounds == 1);

    Config Maximum = FastConfig();
    Maximum.PreparationMs = MaxDurationMs;
    Maximum.RoundMs = MaxDurationMs;
    Maximum.PlantMs = MaxDurationMs;
    Maximum.DefuseMs = MaxDurationMs;
    Maximum.DetonationMs = MaxDurationMs;
    CHECK(MatchRules::IsValidConfig(Maximum));
    Fixture NearLimit(Maximum);
    CHECK(NearLimit.Pair(MaxServerTimeMs).Status == FrameStatus::Applied);
    CHECK(NearLimit.State().PhaseDeadline > MaxServerTimeMs);
    CHECK(NearLimit.State().PhaseDeadline > 0);

    Fixture LastSequence;
    Frame Last = LastSequence.MakeFrame(0, {Join(1, Team::One), Join(2, Team::Two)});
    Last.Sequence = MaxSequence;
    const StepResult LastResult = LastSequence.Rules.Apply(Last);
    CHECK(LastResult.Status == FrameStatus::Applied);
    REQUIRE(!LastResult.Effects.empty());
    CHECK(LastResult.Effects.front().EffectId > MaxSequence);
    CHECK(LastResult.Effects.back().EffectId < UINT64_MAX);
}

void MonotonicFramesAndBatchValidation()
{
    Fixture F;
    const Frame Initial = F.MakeFrame(0, {Join(1, Team::One), Join(2, Team::Two)});
    REQUIRE(F.Rules.Apply(Initial).Status == FrameStatus::Applied);
    const Snapshot Before = F.State();
    const StepResult Replay = F.Rules.Apply(Initial);
    CHECK(Replay.Status != FrameStatus::Applied);
    CHECK(Replay.Effects.empty());
    CHECK(SameSnapshot(Before, F.State()));
    CHECK(F.Step(0).Status == FrameStatus::InvalidTime);
    CHECK(F.Step(-1).Status == FrameStatus::InvalidTime);
    Frame BadSequence = F.MakeFrame(1);
    BadSequence.Sequence = 0;
    CHECK(F.Rules.Apply(BadSequence).Status == FrameStatus::ReplayedSequence);
    BadSequence.Sequence = MaxSequence + 1;
    CHECK(F.Rules.Apply(BadSequence).Status == FrameStatus::ReplayedSequence);
    BadSequence.Sequence = Initial.Sequence;
    CHECK(F.Rules.Apply(BadSequence).Status == FrameStatus::ReplayedSequence);
    CHECK(SameSnapshot(Before, F.State()));

    Frame Bad = F.MakeFrame(1);
    Bad.EventCount = MaxEvents + 1;
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    Bad = F.MakeFrame(1, {F.Make(EventKind::Disconnect, 1), Join(1, Team::One)});
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    Bad = F.MakeFrame(1, {F.Make(EventKind::Death, 1), F.Make(EventKind::Death, 1)});
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    Bad = F.MakeFrame(1, {F.Make(EventKind::StartPlant, 1, 1), F.Make(EventKind::Drop, 1, 2)});
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    Bad = F.MakeFrame(1, {F.Make(EventKind::ConfirmPlacement), F.Make(EventKind::RecoverDevice)});
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    Bad = F.MakeFrame(1, {F.Make(EventKind::NextRound), F.Make(EventKind::Rematch)});
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    Bad = F.MakeFrame(1, {F.Make(static_cast<EventKind>(255))});
    CHECK(F.Rules.Apply(Bad).Status == FrameStatus::InvalidBatch);
    CHECK(SameSnapshot(Before, F.State()));
    CHECK(F.Step(1).Status == FrameStatus::Applied);
}

void TeamLimitsAndConnectionIdentities()
{
    Fixture F(FastConfig(2));
    const StepResult Initial = F.Step(0, {Join(3, Team::One), Join(1, Team::One), Join(2, Team::One)});
    CHECK(Initial.Events[0] == EventStatus::TeamFull);
    CHECK(F.State().RosterCount(Team::One) == 2);
    CHECK(F.State().FindPlayer(1) != nullptr);
    CHECK(F.State().FindPlayer(2) != nullptr);
    CHECK(F.State().FindPlayer(3) == nullptr);
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    const StepResult Invalid = F.Step(1, {Join(0, Team::Two), Join(4, Team::None)});
    CHECK(Invalid.Events[0] == EventStatus::InvalidIdentity);
    CHECK(Invalid.Events[1] == EventStatus::InvalidTeam);
    CHECK(F.Step(2, {Join(1, Team::Two)}).Events[0] == EventStatus::InvalidIdentity);
    F.Step(3, {Join(5, Team::Two), Join(6, Team::Two)});
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().RosterCount(Team::Two) == 2);
    CHECK(F.State().ParticipantCount(Team::One) == 2);
    CHECK(F.State().Objective.Carrier == 1);
    F.Step(4, {F.Make(EventKind::Disconnect, 1)});
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    const StepResult Rejoin = F.Step(5, {Join(1, Team::One)});
    CHECK(Rejoin.Events[0] == EventStatus::InvalidIdentity);
    CHECK(F.State().FindPlayer(1) == nullptr);
    F.Step(6, {Join(7, Team::One)});
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().FindPlayer(7) != nullptr);
}

void LateJoinsAreSpectatorsUntilNextPreparation()
{
    Fixture F;
    F.Pair();
    F.Step(1, {Join(3, Team::One)});
    REQUIRE(F.State().FindPlayer(3) != nullptr);
    CHECK(!F.State().FindPlayer(3)->Participating);
    CHECK(!F.State().FindPlayer(3)->Alive);
    CHECK(F.State().RosterCount(Team::One) == 2);
    CHECK(F.State().LivingCount(Team::One) == 1);
    F.Activate();
    F.Step(9, {Join(4, Team::Two)});
    CHECK(!F.State().FindPlayer(4)->Participating);
    CHECK(!F.State().CanCombat(3));
    CHECK(!F.State().CanMove(4));
    const StepResult Action = F.Step(10, {F.Make(EventKind::StartPlant, 3, 1)});
    CHECK(Action.Events[0] != EventStatus::Applied);
    F.Step(11, {F.Make(EventKind::Death, 1)});
    CHECK(F.State().CurrentPhase == Phase::Resolved);
    CHECK(F.State().RoundWinner == Team::Two);
    CHECK(F.State().LivingCount(Team::One) == 0);
    const Id PriorRound = F.State().RoundId;
    F.Step(12, {F.Make(EventKind::NextRound)});
    CHECK(F.State().RoundId > PriorRound);
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().ParticipantCount(Team::One) == 2);
    CHECK(F.State().ParticipantCount(Team::Two) == 2);
    CHECK(F.State().FindPlayer(3)->Alive);
    CHECK(F.State().FindPlayer(4)->Alive);
}

void PreparationDisconnectAndRestart()
{
    Fixture F(FastConfig(2));
    F.Four();
    const Id OldRound = F.State().RoundId;
    const StepResult Lost = F.Step(1, {F.Make(EventKind::Disconnect, 3)});
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    CHECK(F.State().CompletedRounds == 0);
    CHECK(Count(Lost, EffectKind::RoundAwarded) == 0);
    CHECK(Count(Lost, EffectKind::WaitingForRoster) == 1);
    CHECK(F.State().Objective.State == DeviceState::Unavailable);
    CHECK(F.State().PhaseDeadline == 0);
    const StepResult Resumed = F.Step(2, {Join(5, Team::One)});
    CHECK(Count(Resumed, EffectKind::PrepareRound) == 1);
    CHECK(F.State().RoundId > OldRound);
    CHECK(F.State().PhaseDeadline == 10);
    CHECK(F.State().LivingCount(Team::One) == 2);
    CHECK(F.State().Scores[0] == 0 && F.State().Scores[1] == 0);

    Fixture SameBatch(FastConfig(2));
    SameBatch.Four();
    const Id Before = SameBatch.State().RoundId;
    SameBatch.Step(1, {SameBatch.Make(EventKind::Disconnect, 3), Join(5, Team::One)});
    CHECK(SameBatch.State().CurrentPhase == Phase::Preparation);
    CHECK(SameBatch.State().RoundId > Before);
    CHECK(SameBatch.State().FindPlayer(5)->Participating);
    CHECK(SameBatch.State().PhaseDeadline == 9);
}

void PreparationExpiryKeepsEndpointGameplayGated()
{
    Fixture F;
    F.Four();
    const StepResult Boundary = F.Step(8, {
        F.Make(EventKind::StartPlant, 1, 1),
        F.Make(EventKind::StartDefuse, 2, 1),
        F.Make(EventKind::Retrieve, 3, 1),
        F.Make(EventKind::Drop, 4, 1),
        F.Make(EventKind::Death, 2)
    });
    CHECK(Boundary.Status == FrameStatus::Applied);
    for (std::size_t I = 0; I < 5; ++I) { CHECK(Boundary.Events[I] == EventStatus::WrongPhase); }
    CHECK(F.State().CurrentPhase == Phase::Active);
    CHECK(F.State().LivingCount(Team::One) == 2);
    CHECK(F.State().LivingCount(Team::Two) == 2);
    CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
    CHECK(F.State().Objective.State == DeviceState::Carried);
    CHECK(F.State().CanCombat(1));
    CHECK(F.State().CanMove(2));
    CHECK(Count(Boundary, EffectKind::RoundActivated) == 1);
    const StepResult After = F.Step(9, {F.Make(EventKind::StartPlant, 1, 2), F.Make(EventKind::Death, 4)});
    CHECK(After.Events[0] == EventStatus::Applied);
    CHECK(After.Events[1] == EventStatus::Applied);
    CHECK(F.State().CurrentInteraction.Kind == InteractionKind::Plant);
    CHECK(F.State().LivingCount(Team::Two) == 1);

    Fixture Stop;
    Stop.Pair();
    const StepResult StopBoundary = Stop.Step(8, {Stop.Make(EventKind::StopInteraction, 1, 1)});
    CHECK(StopBoundary.Events[0] == EventStatus::WrongPhase);
    CHECK(Stop.State().CurrentPhase == Phase::Active);

    Fixture World;
    World.Pair();
    const StepResult Recovery = World.Step(8, {World.Make(EventKind::RecoverDevice)});
    CHECK(Recovery.Events[0] == EventStatus::Applied);
    CHECK(World.State().CurrentPhase == Phase::Active);
    CHECK(World.State().Objective.State == DeviceState::PlacementPending);
    CHECK(Count(Recovery, EffectKind::RecoveryPlacementRequested) == 1);
    World.Step(9, {World.Make(EventKind::ConfirmPlacement)});
    CHECK(World.State().Objective.State == DeviceState::Dropped);
}

void EliminationWinsAndSingleAward()
{
    for (const Team VictimTeam : {Team::One, Team::Two})
    {
        Fixture F;
        F.Pair();
        F.Activate();
        const Id Victim = VictimTeam == Team::One ? 1 : 2;
        const Team Winner = VictimTeam == Team::One ? Team::Two : Team::One;
        const StepResult Killed = F.Step(9, {F.Make(EventKind::Death, Victim)});
        CHECK(F.State().CurrentPhase == Phase::Resolved);
        CHECK(F.State().RoundWinner == Winner);
        CHECK(F.State().Reason == WinReason::Elimination);
        CHECK(F.State().CompletedRounds == 1);
        CHECK(Count(Killed, EffectKind::RoundAwarded) == 1);
        CHECK(!F.State().CanCombat(1));
        CHECK(!F.State().CanMove(2));
        CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
        const StepResult Repeated = F.Step(200, {F.Make(EventKind::Death, Victim), F.Make(EventKind::StartPlant, 1, 1)});
        CHECK(Count(Repeated, EffectKind::RoundAwarded) == 0);
        CHECK(F.State().CompletedRounds == 1);
        CHECK(F.State().Scores[0] + F.State().Scores[1] == 1);
    }
}

void TimeoutAndDeadlineOrdering()
{
    Fixture F;
    F.Pair();
    F.Activate();
    F.Step(97);
    CHECK(F.State().CurrentPhase == Phase::Active);
    const StepResult Result = F.Step(98, {F.Make(EventKind::Death, 2)});
    CHECK(F.State().CurrentPhase == Phase::Resolved);
    CHECK(F.State().RoundWinner == Team::Two);
    CHECK(F.State().Reason == WinReason::Timeout);
    CHECK(EffectTime(Result, EffectKind::RoundAwarded) == 98);
    CHECK(Count(Result, EffectKind::RoundAwarded) == 1);
}

void PlantBoundaries()
{
    for (const TimeMs Start : {TimeMs{94}, TimeMs{95}, TimeMs{96}})
    {
        Fixture F;
        F.Pair();
        F.Activate();
        F.BeginPlant(Start);
        REQUIRE(F.State().CurrentInteraction.Kind == InteractionKind::Plant);
        const StepResult Result = F.Step(Start + 3, {}, true);
        if (Start == 94)
        {
            CHECK(F.State().CurrentPhase == Phase::Planted);
            CHECK(F.State().Objective.State == DeviceState::Planted);
            CHECK(F.State().Objective.Carrier == 0);
            CHECK(F.State().Objective.Site == ObjectiveSite);
            CHECK(F.State().PhaseDeadline == 127);
            CHECK(Count(Result, EffectKind::DevicePlanted) == 1);
        }
        else
        {
            CHECK(F.State().CurrentPhase == Phase::Resolved);
            CHECK(F.State().RoundWinner == Team::Two);
            CHECK(F.State().Reason == WinReason::Timeout);
            CHECK(Count(Result, EffectKind::DevicePlanted) == 0);
            CHECK(EffectTime(Result, EffectKind::RoundAwarded) == 98);
        }
        CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
    }
}

void DefuseBoundariesAndDetonation()
{
    for (const TimeMs Start : {TimeMs{36}, TimeMs{37}, TimeMs{38}})
    {
        Fixture F;
        F.Pair();
        F.Activate();
        F.Plant(); // planted at 12, detonates at 42.
        REQUIRE(F.State().CurrentPhase == Phase::Planted);
        F.Step(Start, {F.Make(EventKind::StartDefuse, 2, 1)});
        const StepResult Result = F.Step(Start + 5, {}, true);
        CHECK(F.State().CurrentPhase == Phase::Resolved);
        CHECK(F.State().CompletedRounds == 1);
        CHECK(Count(Result, EffectKind::RoundAwarded) == 1);
        if (Start == 36)
        {
            CHECK(F.State().Reason == WinReason::Defuse);
            CHECK(F.State().RoundWinner == Team::Two);
            CHECK(F.State().Objective.State == DeviceState::Defused);
            CHECK(Count(Result, EffectKind::DeviceDefused) == 1);
            CHECK(EffectTime(Result, EffectKind::RoundAwarded) == 41);
        }
        else
        {
            CHECK(F.State().Reason == WinReason::Detonation);
            CHECK(F.State().RoundWinner == Team::One);
            CHECK(F.State().Objective.State == DeviceState::Detonated);
            CHECK(Count(Result, EffectKind::DeviceDefused) == 0);
            CHECK(Count(Result, EffectKind::DeviceDetonated) == 1);
            CHECK(EffectTime(Result, EffectKind::RoundAwarded) == 42);
        }
    }
    Fixture F;
    F.Pair();
    F.Activate();
    F.Plant();
    F.Step(41);
    CHECK(F.State().CurrentPhase == Phase::Planted);
    F.Step(42);
    CHECK(F.State().Reason == WinReason::Detonation);
}

void InteractionEligibilityAndRequests()
{
    Fixture F;
    F.Pair();
    const StepResult Early = F.Step(1, {F.Make(EventKind::StartPlant, 1, 1)});
    CHECK(Early.Events[0] == EventStatus::WrongPhase);
    F.Activate();
    Event WrongSite = F.Make(EventKind::StartPlant, 1, 2);
    WrongSite.Site = 'B';
    CHECK(F.Step(9, {WrongSite}).Events[0] == EventStatus::Ineligible);
    Event Corrected = F.Make(EventKind::StartPlant, 1, 2);
    CHECK(F.Step(10, {Corrected}).Events[0] == EventStatus::ReplayedRequest);
    Event NoWorld = F.Make(EventKind::StartPlant, 1, 3);
    NoWorld.WorldConfirmed = false;
    CHECK(F.Step(11, {NoWorld}).Events[0] == EventStatus::Ineligible);
    Event WrongRevision = F.Make(EventKind::StartPlant, 1, 4);
    ++WrongRevision.DeviceRevision;
    CHECK(F.Step(12, {WrongRevision}).Events[0] == EventStatus::WrongDevice);
    CHECK(F.Step(13, {F.Make(EventKind::StartPlant, 2, 1)}).Events[0] != EventStatus::Applied);
    CHECK(F.Step(14, {F.Make(EventKind::StartPlant, 99, 1)}).Events[0] == EventStatus::UnknownPlayer);
    CHECK(F.Step(15, {F.Make(EventKind::StartPlant, 1, 0)}).Events[0] == EventStatus::ReplayedRequest);
    F.BeginPlant(16, 1, 5);
    REQUIRE(F.State().CurrentInteraction.Kind == InteractionKind::Plant);
    const Id Token = F.State().CurrentInteraction.Token;
    const StepResult Duplicate = F.Step(17, {F.Make(EventKind::StartPlant, 1, 5)}, true);
    CHECK(Duplicate.Events[0] == EventStatus::ReplayedRequest);
    CHECK(F.State().CurrentInteraction.Token == Token);
    CHECK(F.State().CurrentInteraction.StartedAt == 16);
    const StepResult Repeated = F.Step(18, {F.Make(EventKind::StartPlant, 1, 6)}, true);
    CHECK(Repeated.Events[0] == EventStatus::Unchanged);
    CHECK(F.State().CurrentInteraction.Token == Token);
    CHECK(F.State().CurrentInteraction.CompletesAt == 19);
    F.Step(19, {}, true);
    CHECK(F.State().CurrentPhase == Phase::Planted);
    CHECK(F.Step(20, {F.Make(EventKind::StartDefuse, 1, 7)}).Events[0] != EventStatus::Applied);
    Event InvalidDefuse = F.Make(EventKind::StartDefuse, 2, 2);
    InvalidDefuse.WorldConfirmed = false;
    CHECK(F.Step(21, {InvalidDefuse}).Events[0] == EventStatus::Ineligible);
}

void MissingCertificationCancelsRatherThanRetainingProgress()
{
    Fixture F;
    F.Pair();
    F.Activate();
    F.BeginPlant();
    const Id OldToken = F.State().CurrentInteraction.Token;
    const StepResult Missing = F.Step(11);
    CHECK(Count(Missing, EffectKind::InteractionCanceled) == 1);
    CHECK(EffectTime(Missing, EffectKind::InteractionCanceled) == 9);
    CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
    F.BeginPlant(12, 1, 2);
    CHECK(F.State().CurrentInteraction.Token != OldToken);
    CHECK(F.State().CurrentInteraction.StartedAt == 12);
    CHECK(F.State().CurrentInteraction.CompletesAt == 15);
    F.Step(14, {}, true);
    CHECK(F.State().CurrentPhase == Phase::Active);
    F.Step(15, {}, true);
    CHECK(F.State().CurrentPhase == Phase::Planted);

    Fixture Gap;
    Gap.Pair();
    Gap.Activate();
    Gap.BeginPlant();
    const StepResult Late = Gap.Step(200);
    CHECK(Count(Late, EffectKind::DevicePlanted) == 0);
    CHECK(Gap.State().Reason == WinReason::Timeout);
    CHECK(EffectTime(Late, EffectKind::InteractionCanceled) == 9);
    CHECK(EffectTime(Late, EffectKind::RoundAwarded) == 98);

    Fixture WrongToken;
    WrongToken.Pair();
    WrongToken.Activate();
    WrongToken.BeginPlant();
    Frame FrameValue = WrongToken.MakeFrame(12);
    FrameValue.ContinuousInteractionToken = WrongToken.State().CurrentInteraction.Token + 1;
    WrongToken.Rules.Apply(FrameValue);
    CHECK(WrongToken.State().CurrentInteraction.Kind == InteractionKind::None);
    CHECK(WrongToken.State().CurrentPhase == Phase::Active);
}

void RefreshedIneligibilityCancelsExistingProgress()
{
    for (const bool Defusing : {false, true})
    {
        for (const bool AtCompletion : {false, true})
        {
            Fixture F;
            F.Pair();
            F.Activate();
            if (Defusing)
            {
                F.Plant();
                F.Step(13, {F.Make(EventKind::StartDefuse, 2, 1)});
            }
            else { F.BeginPlant(); }
            const TimeMs CompleteAt = F.State().CurrentInteraction.CompletesAt;
            const TimeMs InvalidAt = AtCompletion ? CompleteAt : CompleteAt - 1;
            const EventKind Kind = Defusing ? EventKind::StartDefuse : EventKind::StartPlant;
            const Id Actor = Defusing ? 2 : 1;
            Event Refreshed = F.Make(Kind, Actor, 2);
            Refreshed.WorldConfirmed = false;
            // Even a conflicting continuity claim must not retain progress once the
            // same adapter frame explicitly states that this interaction is invalid.
            const StepResult Invalidated = F.Step(InvalidAt, {Refreshed}, true);
            CHECK(Invalidated.Events[0] == EventStatus::Ineligible);
            CHECK(Count(Invalidated, EffectKind::InteractionCanceled) == 1);
            CHECK(Count(Invalidated, Defusing ? EffectKind::DeviceDefused : EffectKind::DevicePlanted) == 0);
            CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
            CHECK(F.State().CurrentPhase == (Defusing ? Phase::Planted : Phase::Active));
            F.Step(InvalidAt + 1, {F.Make(Kind, Actor, 3)});
            CHECK(F.State().CurrentInteraction.StartedAt == InvalidAt + 1);
            CHECK(F.State().CurrentInteraction.CompletesAt == InvalidAt + 1 + (Defusing ? 5 : 3));
        }
    }
    Fixture SiteLoss;
    SiteLoss.Pair();
    SiteLoss.Activate();
    SiteLoss.BeginPlant();
    Event Outside = SiteLoss.Make(EventKind::StartPlant, 1, 2);
    Outside.Site = 0;
    const StepResult Result = SiteLoss.Step(12, {Outside}, true);
    CHECK(Result.Events[0] == EventStatus::Ineligible);
    CHECK(SiteLoss.State().CurrentInteraction.Kind == InteractionKind::None);
    CHECK(Count(Result, EffectKind::DevicePlanted) == 0);
}

void StopAndInvalidationAtCompletion()
{
    for (const TimeMs StopAt : {TimeMs{11}, TimeMs{12}})
    {
        Fixture F;
        F.Pair();
        F.Activate();
        F.BeginPlant();
        const StepResult Stopped = F.Step(StopAt, {F.Make(EventKind::StopInteraction, 1, 2)}, true);
        CHECK(F.State().CurrentPhase == Phase::Active);
        CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
        CHECK(Count(Stopped, EffectKind::DevicePlanted) == 0);
        CHECK(Count(Stopped, EffectKind::InteractionCanceled) == 1);
        F.BeginPlant(StopAt + 1, 1, 3);
        CHECK(F.State().CurrentInteraction.CompletesAt == StopAt + 4);
    }
    for (const EventKind Kind : {EventKind::Death, EventKind::Disconnect, EventKind::Drop, EventKind::RecoverDevice})
    {
        Fixture F;
        F.Four();
        F.Activate();
        F.BeginPlant();
        Event Invalidating = F.Make(Kind, Kind == EventKind::RecoverDevice ? 0 : 1, Kind == EventKind::Drop ? 2 : 0);
        const StepResult Result = F.Step(12, {Invalidating}, true);
        CHECK(Count(Result, EffectKind::DevicePlanted) == 0);
        CHECK(F.State().CurrentPhase == Phase::Active);
        CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
        CHECK(F.State().Objective.State == DeviceState::PlacementPending);
    }
    Fixture RangeLoss;
    RangeLoss.Pair();
    RangeLoss.Activate();
    RangeLoss.BeginPlant();
    // Uncertified range/held interval cancels before processing elapsed time.
    RangeLoss.Step(12);
    CHECK(RangeLoss.State().CurrentPhase == Phase::Active);
    CHECK(RangeLoss.State().CurrentInteraction.Kind == InteractionKind::None);
}

void DefuseInterruptionAndDeathAtCompletion()
{
    for (const EventKind Kind : {EventKind::StopInteraction, EventKind::Death, EventKind::Disconnect})
    {
        Fixture F;
        F.Four();
        F.Activate();
        F.Plant();
        F.Step(13, {F.Make(EventKind::StartDefuse, 2, 1)});
        const StepResult Interrupted = F.Step(18, {F.Make(Kind, 2, Kind == EventKind::StopInteraction ? 2 : 0)}, true);
        CHECK(F.State().CurrentPhase == Phase::Planted);
        CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
        CHECK(Count(Interrupted, EffectKind::DeviceDefused) == 0);
        F.Step(19, {F.Make(EventKind::StartDefuse, 4, 1)});
        CHECK(F.State().CurrentInteraction.StartedAt == 19);
        CHECK(F.State().CurrentInteraction.CompletesAt == 24);
        F.Step(24, {}, true);
        CHECK(F.State().Reason == WinReason::Defuse);
    }
    Fixture LastDefender;
    LastDefender.Pair();
    LastDefender.Activate();
    LastDefender.Plant();
    LastDefender.Step(13, {LastDefender.Make(EventKind::StartDefuse, 2, 1)});
    LastDefender.Step(18, {LastDefender.Make(EventKind::Death, 2)}, true);
    CHECK(LastDefender.State().Reason == WinReason::Elimination);
    CHECK(LastDefender.State().RoundWinner == Team::One);
}

void WrongStopAndRejectedFrameDoNotCorruptProgress()
{
    Fixture F;
    F.Pair();
    F.Activate();
    F.BeginPlant();
    const Id Token = F.State().CurrentInteraction.Token;
    Event Wrong = F.Make(EventKind::StopInteraction, 1, 2);
    ++Wrong.InteractionToken;
    CHECK(F.Step(10, {Wrong}, true).Events[0] == EventStatus::WrongInteraction);
    CHECK(F.State().CurrentInteraction.Token == Token);
    const Snapshot Before = F.State();
    Frame Invalid = F.MakeFrame(11, {F.Make(EventKind::Drop, 1, 3), F.Make(EventKind::StopInteraction, 1, 4)});
    const StepResult Rejected = F.Rules.Apply(Invalid);
    CHECK(Rejected.Status == FrameStatus::InvalidBatch);
    CHECK(Rejected.Effects.empty());
    CHECK(SameSnapshot(Before, F.State()));
    F.Step(12, {}, true);
    CHECK(F.State().CurrentPhase == Phase::Planted);
}

void DropPlacementAndRetrieveValidation()
{
    Fixture F;
    F.Four();
    F.Activate();
    const Id CarriedRevision = F.State().Objective.Revision;
    const StepResult Dropped = F.Step(9, {F.Make(EventKind::Drop, 1, 1)});
    CHECK(Dropped.Events[0] == EventStatus::Applied);
    CHECK(Count(Dropped, EffectKind::DropPlacementRequested) == 1);
    CHECK(F.State().Objective.State == DeviceState::PlacementPending);
    CHECK(F.State().Objective.Carrier == 0);
    CHECK(F.State().Objective.Revision > CarriedRevision);
    CHECK(F.Step(10, {F.Make(EventKind::Retrieve, 3, 1)}).Events[0] == EventStatus::WrongDevice);
    Event StalePlacement = F.Make(EventKind::ConfirmPlacement);
    StalePlacement.DeviceRevision = CarriedRevision;
    CHECK(F.Step(11, {StalePlacement}).Events[0] == EventStatus::WrongDevice);
    Event Unconfirmed = F.Make(EventKind::ConfirmPlacement);
    Unconfirmed.WorldConfirmed = false;
    CHECK(F.Step(12, {Unconfirmed}).Events[0] == EventStatus::Ineligible);
    const StepResult Placed = F.Step(13, {F.Make(EventKind::ConfirmPlacement)});
    CHECK(Placed.Events[0] == EventStatus::Applied);
    CHECK(F.State().Objective.State == DeviceState::Dropped);
    CHECK(Count(Placed, EffectKind::DevicePlaced) == 1);
    CHECK(F.Step(14, {F.Make(EventKind::Retrieve, 2, 1)}).Events[0] != EventStatus::Applied);
    Event TooFar = F.Make(EventKind::Retrieve, 3, 2);
    TooFar.WorldConfirmed = false;
    CHECK(F.Step(15, {TooFar}).Events[0] == EventStatus::Ineligible);
    Event StalePickup = F.Make(EventKind::Retrieve, 3, 3);
    StalePickup.DeviceRevision = CarriedRevision;
    CHECK(F.Step(16, {StalePickup}).Events[0] == EventStatus::WrongDevice);
    const StepResult Pickup = F.Step(17, {F.Make(EventKind::Retrieve, 3, 4)});
    CHECK(Pickup.Events[0] == EventStatus::Applied);
    CHECK(F.State().Objective.State == DeviceState::Carried);
    CHECK(F.State().Objective.Carrier == 3);
    CHECK(Count(Pickup, EffectKind::DeviceRetrieved) == 1);
    CHECK(F.Step(18, {F.Make(EventKind::Drop, 1, 2)}).Events[0] == EventStatus::WrongDevice);
}

void CarrierDeathDisconnectAndRecovery()
{
    for (const EventKind Kind : {EventKind::Death, EventKind::Disconnect})
    {
        Fixture F;
        F.Four();
        F.Activate();
        const StepResult Lost = F.Step(9, {F.Make(Kind, 1)});
        CHECK(F.State().CurrentPhase == Phase::Active);
        CHECK(F.State().LivingCount(Team::One) == 1);
        CHECK(F.State().Objective.State == DeviceState::PlacementPending);
        CHECK(Count(Lost, EffectKind::DropPlacementRequested) == 1);
        F.Step(10, {F.Make(EventKind::ConfirmPlacement)});
        F.Step(11, {F.Make(EventKind::Retrieve, 3, 1)});
        CHECK(F.State().Objective.Carrier == 3);
    }
    Fixture F;
    F.Four();
    F.Activate();
    const StepResult Recovered = F.Step(9, {F.Make(EventKind::RecoverDevice)});
    CHECK(Count(Recovered, EffectKind::RecoveryPlacementRequested) == 1);
    CHECK(F.State().Objective.State == DeviceState::PlacementPending);
    const StepResult Duplicate = F.Step(10, {F.Make(EventKind::RecoverDevice)});
    CHECK(Count(Duplicate, EffectKind::RecoveryPlacementRequested) == 0);
    F.Step(11, {F.Make(EventKind::ConfirmPlacement)});
    const Id DroppedRevision = F.State().Objective.Revision;
    const StepResult Again = F.Step(12, {F.Make(EventKind::RecoverDevice)});
    CHECK(Count(Again, EffectKind::RecoveryPlacementRequested) == 1);
    CHECK(F.State().Objective.Revision > DroppedRevision);
    F.Step(13, {F.Make(EventKind::ConfirmPlacement)});
    F.Step(14, {F.Make(EventKind::Retrieve, 3, 1)});
    F.BeginPlant(15, 3, 2);
    F.Step(18, {}, true);
    const StepResult PlantedRecovery = F.Step(19, {F.Make(EventKind::RecoverDevice)});
    CHECK(Count(PlantedRecovery, EffectKind::RecoveryPlacementRequested) == 0);
    CHECK(F.State().Objective.State == DeviceState::Planted);
}

void PostPlantAttackerEliminationDoesNotWinForDefenders()
{
    for (const EventKind Kind : {EventKind::Death, EventKind::Disconnect})
    {
        Fixture F;
        F.Pair();
        F.Activate();
        F.Plant();
        const StepResult Lost = F.Step(13, {F.Make(Kind, 1)});
        CHECK(F.State().LivingCount(Team::One) == 0);
        CHECK(F.State().CurrentPhase == Phase::Planted);
        CHECK(Count(Lost, EffectKind::RoundAwarded) == 0);
        F.Step(42);
        CHECK(F.State().RoundWinner == Team::One);
        CHECK(F.State().Reason == WinReason::Detonation);
    }
    Fixture Defuse;
    Defuse.Pair();
    Defuse.Activate();
    Defuse.Plant();
    Defuse.Step(13, {Defuse.Make(EventKind::Death, 1), Defuse.Make(EventKind::StartDefuse, 2, 1)});
    Defuse.Step(18, {}, true);
    CHECK(Defuse.State().RoundWinner == Team::Two);
    CHECK(Defuse.State().Reason == WinReason::Defuse);
}

void SimultaneousDeathsAreOrderIndependent()
{
    for (const bool Planted : {false, true})
    {
        Fixture A;
        Fixture B;
        A.Pair(); B.Pair();
        A.Activate(); B.Activate();
        if (Planted) { A.Plant(); B.Plant(); }
        const TimeMs Now = Planted ? 13 : 9;
        const StepResult First = A.Step(Now, {A.Make(EventKind::Death, 1), A.Make(EventKind::Death, 2)});
        const StepResult Second = B.Step(Now, {B.Make(EventKind::Death, 2), B.Make(EventKind::Death, 1)});
        CHECK(SameSnapshot(A.State(), B.State()));
        CHECK(A.State().RoundWinner == (Planted ? Team::One : Team::Two));
        CHECK(A.State().Reason == WinReason::Elimination);
        REQUIRE(First.Effects.size() == Second.Effects.size());
        for (std::size_t I = 0; I < First.Effects.size(); ++I)
        {
            CHECK(First.Effects[I].EffectId == Second.Effects[I].EffectId);
            CHECK(First.Effects[I].Kind == Second.Effects[I].Kind);
            CHECK(First.Effects[I].PlayerId == Second.Effects[I].PlayerId);
        }
    }
}

void CompetingPickupsAndDefusesAreCanonical()
{
    Fixture A;
    Fixture B;
    A.Four(); B.Four();
    A.Activate(); B.Activate();
    A.Step(9, {A.Make(EventKind::Drop, 1, 1)});
    B.Step(9, {B.Make(EventKind::Drop, 1, 1)});
    A.Step(10, {A.Make(EventKind::ConfirmPlacement)});
    B.Step(10, {B.Make(EventKind::ConfirmPlacement)});
    A.Step(11, {A.Make(EventKind::Retrieve, 3, 1), A.Make(EventKind::Retrieve, 1, 2)});
    B.Step(11, {B.Make(EventKind::Retrieve, 1, 2), B.Make(EventKind::Retrieve, 3, 1)});
    CHECK(SameSnapshot(A.State(), B.State()));
    CHECK(A.State().Objective.Carrier == 1);
    A.BeginPlant(12, 1, 3); B.BeginPlant(12, 1, 3);
    A.Step(15, {}, true); B.Step(15, {}, true);
    A.Step(16, {A.Make(EventKind::StartDefuse, 4, 1), A.Make(EventKind::StartDefuse, 2, 1)});
    B.Step(16, {B.Make(EventKind::StartDefuse, 2, 1), B.Make(EventKind::StartDefuse, 4, 1)});
    CHECK(SameSnapshot(A.State(), B.State()));
    CHECK(A.State().CurrentInteraction.PlayerId == 2);
    CHECK(A.State().CurrentInteraction.Kind == InteractionKind::Defuse);
}

void UnderfilledWaitingPreservesScoreAndResumes()
{
    Fixture F;
    F.Pair();
    F.Activate();
    const StepResult Disconnected = F.Step(9, {F.Make(EventKind::Disconnect, 1)});
    CHECK(F.State().CurrentPhase == Phase::Resolved);
    CHECK(F.State().RoundWinner == Team::Two);
    CHECK(F.State().Scores[1] == 1);
    CHECK(Count(Disconnected, EffectKind::RoundAwarded) == 1);
    const StepResult Wait = F.Step(10, {F.Make(EventKind::NextRound)});
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    CHECK(Count(Wait, EffectKind::WaitingForRoster) == 1);
    F.Step(1'000);
    CHECK(F.State().Scores[1] == 1);
    CHECK(F.State().CompletedRounds == 1);
    CHECK(F.State().PhaseDeadline == 0);
    CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
    F.Step(1'001, {Join(3, Team::One)});
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().CompletedRounds == 1);
    CHECK(F.State().Scores[1] == 1);
    CHECK(F.State().Objective.Carrier == 3);
    CHECK(F.State().FindPlayer(3)->Participating);
    CHECK(F.State().FindPlayer(2)->Alive);
}

void MinimumTwoDoesNotPauseAnOngoingRound()
{
    Fixture F(FastConfig(2));
    F.Four();
    F.Activate();
    F.Step(9, {F.Make(EventKind::Disconnect, 3)});
    CHECK(F.State().RosterCount(Team::One) == 1);
    CHECK(F.State().LivingCount(Team::One) == 1);
    CHECK(F.State().CurrentPhase == Phase::Active);
    F.Step(10, {F.Make(EventKind::Death, 1)});
    REQUIRE(F.State().CurrentPhase == Phase::Resolved);
    F.Step(11, {F.Make(EventKind::NextRound)});
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    CHECK(F.State().CompletedRounds == 1);
    CHECK(F.State().Scores[1] == 1);
    F.Step(12, {Join(5, Team::One)});
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().LivingCount(Team::One) == 2);
    CHECK(F.State().LivingCount(Team::Two) == 2);
    CHECK(F.State().Scores[1] == 1);
}

void EmptyServerClearsWithoutAnotherAward()
{
    for (const TimeMs At : {TimeMs{9}, TimeMs{98}})
    {
        Fixture F;
        F.Pair();
        F.Activate();
        const StepResult Empty = F.Step(At, {F.Make(EventKind::Disconnect, 2), F.Make(EventKind::Disconnect, 1)});
        CHECK(F.State().CurrentPhase == Phase::Waiting);
        CHECK(F.State().Scores[0] == 0 && F.State().Scores[1] == 0);
        CHECK(F.State().CompletedRounds == 0);
        CHECK(F.State().RosterCount(Team::One) == 0);
        CHECK(F.State().RosterCount(Team::Two) == 0);
        CHECK(F.State().Objective.State == DeviceState::Unavailable);
        CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
        CHECK(Count(Empty, EffectKind::MatchCleared) == 1);
        CHECK(Count(Empty, EffectKind::RoundAwarded) == 0);
        CHECK(F.Step(At + 1).Effects.empty());
        CHECK(F.Step(At + 2, {Join(1, Team::One)}).Events[0] == EventStatus::InvalidIdentity);
        F.Step(At + 3, {Join(3, Team::One), Join(4, Team::Two)});
        CHECK(F.State().CurrentPhase == Phase::Preparation);
        CHECK(F.State().Scores[0] == 0 && F.State().Scores[1] == 0);
    }
    Fixture Scored;
    Scored.Pair();
    Scored.Activate();
    Scored.Step(9, {Scored.Make(EventKind::Death, 2)});
    REQUIRE(Scored.State().Scores[0] == 1);
    const StepResult Cleared = Scored.Step(10, {Scored.Make(EventKind::Disconnect, 1), Scored.Make(EventKind::Disconnect, 2)});
    CHECK(Scored.State().CompletedRounds == 0);
    CHECK(Scored.State().Scores[0] == 0);
    CHECK(Count(Cleared, EffectKind::RoundAwarded) == 0);
}

void CrossedDeadlinesMatchSmallSteps()
{
    Fixture Large;
    Fixture Small;
    Large.Pair(); Small.Pair();
    const StepResult Catchup = Large.Step(200);
    Small.Step(8); Small.Step(98); Small.Step(200);
    CHECK(SameSnapshot(Large.State(), Small.State()));
    CHECK(EffectTime(Catchup, EffectKind::RoundActivated) == 8);
    CHECK(EffectTime(Catchup, EffectKind::RoundAwarded) == 98);

    Fixture PlantLarge;
    Fixture PlantSmall;
    PlantLarge.Pair(); PlantSmall.Pair();
    PlantLarge.Activate(); PlantSmall.Activate();
    PlantLarge.BeginPlant(94); PlantSmall.BeginPlant(94);
    const StepResult PlantedCatchup = PlantLarge.Step(200, {}, true);
    PlantSmall.Step(97, {}, true); PlantSmall.Step(98); PlantSmall.Step(127); PlantSmall.Step(200);
    // IDs intentionally encode frame sequence, so compare observable rule outcome and logical times.
    CHECK(PlantLarge.State().CurrentPhase == PlantSmall.State().CurrentPhase);
    CHECK(PlantLarge.State().Scores == PlantSmall.State().Scores);
    CHECK(PlantLarge.State().Reason == PlantSmall.State().Reason);
    CHECK(PlantLarge.State().Objective.State == PlantSmall.State().Objective.State);
    CHECK(PlantLarge.State().Reason == WinReason::Detonation);
    CHECK(EffectTime(PlantedCatchup, EffectKind::DevicePlanted) == 97);
    CHECK(EffectTime(PlantedCatchup, EffectKind::DeviceDetonated) == 127);
    CHECK(EffectTime(PlantedCatchup, EffectKind::RoundAwarded) == 127);

    Fixture DefuseLarge;
    Fixture DefuseSmall;
    DefuseLarge.Pair(); DefuseSmall.Pair();
    DefuseLarge.Activate(); DefuseSmall.Activate();
    DefuseLarge.Plant(); DefuseSmall.Plant();
    DefuseLarge.Step(13, {DefuseLarge.Make(EventKind::StartDefuse, 2, 1)});
    DefuseSmall.Step(13, {DefuseSmall.Make(EventKind::StartDefuse, 2, 1)});
    const StepResult DefusedCatchup = DefuseLarge.Step(200, {}, true);
    DefuseSmall.Step(18, {}, true); DefuseSmall.Step(200);
    CHECK(DefuseLarge.State().Reason == WinReason::Defuse);
    CHECK(DefuseLarge.State().Scores == DefuseSmall.State().Scores);
    CHECK(EffectTime(DefusedCatchup, EffectKind::RoundAwarded) == 18);
    CHECK(Count(DefusedCatchup, EffectKind::DeviceDetonated) == 0);
}

void EarlierCompletionPrecedesLaterEndpointInvalidation()
{
    Fixture F;
    F.Four();
    F.Activate();
    F.BeginPlant();
    const StepResult Result = F.Step(15, {F.Make(EventKind::Death, 1)}, true);
    CHECK(F.State().CurrentPhase == Phase::Planted);
    CHECK(F.State().Objective.State == DeviceState::Planted);
    CHECK(EffectTime(Result, EffectKind::DevicePlanted) == 12);
    CHECK(Count(Result, EffectKind::DropPlacementRequested) == 0);
    CHECK(F.State().LivingCount(Team::One) == 1);
}

void ControlsObserveOutcomesInTheSameBatch()
{
    Fixture F;
    F.Pair();
    const StepResult Invalid = F.Step(1, {F.Make(EventKind::Rematch)});
    CHECK(Invalid.Events[0] == EventStatus::WrongPhase);
    F.Activate();
    const Id OldRound = F.State().RoundId;
    const StepResult Combined = F.Step(9, {F.Make(EventKind::NextRound), F.Make(EventKind::Death, 2)});
    CHECK(Combined.Events[0] == EventStatus::Applied);
    CHECK(F.State().CurrentPhase == Phase::Preparation);
    CHECK(F.State().RoundId > OldRound);
    CHECK(F.State().CompletedRounds == 1);
    CHECK(F.State().Scores[0] == 1);
    CHECK(Count(Combined, EffectKind::RoundAwarded) == 1);
    CHECK(Count(Combined, EffectKind::PrepareRound) == 1);
    const Event StaleControl = [&F, OldRound]()
    {
        Event Value = F.Make(EventKind::NextRound);
        Value.RoundId = OldRound;
        return Value;
    }();
    CHECK(F.Step(10, {StaleControl}).Events[0] == EventStatus::StaleRound);
}

void StaleRoundCommandsAndRequestIdsSurviveReset()
{
    Fixture F;
    F.Pair();
    F.Activate();
    F.BeginPlant();
    const Id OldRound = F.State().RoundId;
    Event StaleAction = F.Make(EventKind::StartPlant, 1, 99);
    Event StaleDeath = F.Make(EventKind::Death, 1);
    F.Step(10, {F.Make(EventKind::Death, 2)}, true);
    F.Step(11, {F.Make(EventKind::NextRound)});
    REQUIRE(F.State().RoundId > OldRound);
    const StepResult Stale = F.Step(12, {StaleAction, StaleDeath});
    CHECK(Stale.Events[0] == EventStatus::StaleRound);
    CHECK(Stale.Events[1] == EventStatus::StaleRound);
    CHECK(F.State().FindPlayer(1)->Alive);
    CHECK(F.State().FindPlayer(1)->LastRequestId == 1);
    F.Activate();
    CHECK(F.Step(20, {F.Make(EventKind::StartPlant, 1, 1)}).Events[0] == EventStatus::ReplayedRequest);
    F.BeginPlant(21, 1, 2);
    CHECK(F.State().CurrentInteraction.Kind == InteractionKind::Plant);
}

void SixRoundDrawRoleSwapRematchAndTwelveRoundReset()
{
    Fixture F;
    std::set<Id> EffectIds;
    auto Remember = [&EffectIds](const StepResult& Result)
    {
        CHECK(Result.Status == FrameStatus::Applied);
        for (const Effect& Value : Result.Effects)
        {
            CHECK(Value.EffectId != 0);
            CHECK(EffectIds.insert(Value.EffectId).second);
        }
    };
    Remember(F.Pair());
    Id LastRoundId = 0;
    Id LastDeviceRevision = 0;
    Id RequestId = 0;
    for (int Match = 0; Match < 2; ++Match)
    {
        for (int Round = 0; Round < 6; ++Round)
        {
            REQUIRE(F.State().CurrentPhase == Phase::Preparation);
            CHECK(F.State().RoundId > LastRoundId);
            CHECK(F.State().Objective.Revision > LastDeviceRevision);
            LastRoundId = F.State().RoundId;
            LastDeviceRevision = F.State().Objective.Revision;
            CHECK(F.State().CompletedRounds == Round);
            CHECK(F.State().AttackingTeam == (Round < 3 ? Team::One : Team::Two));
            CHECK(F.State().DefendingTeam() == (Round < 3 ? Team::Two : Team::One));
            CHECK(F.State().FindPlayer(1)->TeamId == Team::One);
            CHECK(F.State().FindPlayer(2)->TeamId == Team::Two);
            CHECK(F.State().FindPlayer(1)->Alive && F.State().FindPlayer(2)->Alive);
            CHECK(F.State().CurrentInteraction.Kind == InteractionKind::None);
            CHECK(F.State().RoundWinner == Team::None);
            CHECK(F.State().Reason == WinReason::None);
            CHECK(F.State().Result == MatchResult::None);
            CHECK(F.State().Objective.State == DeviceState::Carried);
            CHECK(F.State().Objective.Site == 0);
            CHECK(F.State().Objective.Carrier == (Round < 3 ? 1 : 2));
            Remember(F.Activate());
            // A denied objective request is still consumed, including across rematch.
            Event Denied = F.Make(EventKind::StartPlant, 1, ++RequestId);
            Denied.WorldConfirmed = false;
            Remember(F.Step(F.State().Now + 1, {Denied}));
            CHECK(F.State().FindPlayer(1)->LastRequestId == RequestId);
            // First match: persistent teams split 3–3. Second match: team one wins 6–0.
            const Id Victim = Match == 0 ? (Round % 2 == 0 ? 2 : 1) : 2;
            const StepResult Resolved = F.Step(F.State().Now + 1, {F.Make(EventKind::Death, Victim)});
            Remember(Resolved);
            CHECK(Count(Resolved, EffectKind::RoundAwarded) == 1);
            CHECK(F.State().CompletedRounds == Round + 1);
            CHECK(F.State().Scores[0] + F.State().Scores[1] == Round + 1);
            if (Round < 5)
            {
                CHECK(F.State().CurrentPhase == Phase::Resolved);
                CHECK(Count(Resolved, EffectKind::MatchCompleted) == 0);
                Remember(F.Step(F.State().Now + 1, {F.Make(EventKind::NextRound)}));
            }
            else
            {
                CHECK(F.State().CurrentPhase == Phase::MatchComplete);
                CHECK(Count(Resolved, EffectKind::MatchCompleted) == 1);
            }
        }
        CHECK(F.State().Result == (Match == 0 ? MatchResult::Draw : MatchResult::TeamOne));
        CHECK(F.State().Scores[0] == (Match == 0 ? 3 : 6));
        CHECK(F.State().Scores[1] == (Match == 0 ? 3 : 0));
        CHECK(!F.State().CanMove(1) && !F.State().CanCombat(2));
        const StepResult WrongControl = F.Step(F.State().Now + 1, {F.Make(EventKind::NextRound)});
        CHECK(WrongControl.Events[0] == EventStatus::WrongPhase);
        CHECK(WrongControl.Effects.empty());
        if (Match == 0)
        {
            Remember(F.Step(F.State().Now + 1, {F.Make(EventKind::Rematch)}));
            CHECK(F.State().CompletedRounds == 0);
            CHECK(F.State().Scores[0] == 0 && F.State().Scores[1] == 0);
            CHECK(F.State().AttackingTeam == Team::One);
            CHECK(F.State().FindPlayer(1)->LastRequestId == RequestId);
            const StepResult OldRequest = F.Step(F.State().Now + 1, {F.Make(EventKind::StartPlant, 1, RequestId)});
            CHECK(OldRequest.Events[0] == EventStatus::ReplayedRequest);
        }
    }
    const StepResult Cleared = F.Step(F.State().Now + 1, {F.Make(EventKind::Disconnect, 1), F.Make(EventKind::Disconnect, 2)});
    Remember(Cleared);
    CHECK(F.State().Result == MatchResult::None);
    Remember(F.Step(F.State().Now + 1, {Join(3, Team::One), Join(4, Team::Two)}));
    CHECK(F.State().RoundId > LastRoundId);
    CHECK(F.State().Objective.Revision > LastDeviceRevision);
}

void RematchWithMissingRosterWaits()
{
    Fixture F;
    F.Pair();
    for (int Round = 0; Round < 6; ++Round)
    {
        F.Activate();
        F.Step(F.State().Now + 1, {F.Make(EventKind::Death, 1)});
        if (Round < 5) { F.Step(F.State().Now + 1, {F.Make(EventKind::NextRound)}); }
    }
    CHECK(F.State().Result == MatchResult::TeamTwo);
    F.Step(F.State().Now + 1, {F.Make(EventKind::Disconnect, 1)});
    CHECK(F.State().CurrentPhase == Phase::MatchComplete);
    const StepResult Rematched = F.Step(F.State().Now + 1, {F.Make(EventKind::Rematch)});
    CHECK(Rematched.Events[0] == EventStatus::Applied);
    CHECK(F.State().CurrentPhase == Phase::Waiting);
    CHECK(F.State().CompletedRounds == 0);
    CHECK(F.State().Scores[0] == 0 && F.State().Scores[1] == 0);
    CHECK(F.State().AttackingTeam == Team::One);
    F.Step(F.State().Now + 1, {Join(3, Team::One)});
    CHECK(F.State().CurrentPhase == Phase::Preparation);
}

struct TestCase
{
    const char* Name;
    void (*Run)();
};
}

int main()
{
    const TestCase Tests[] = {
        {"default timing and phase gates", DefaultTimingAndPhaseGates},
        {"invalid configuration and bounded time", InvalidConfigurationAndBoundedTime},
        {"monotonic frames and batch validation", MonotonicFramesAndBatchValidation},
        {"team limits and connection identities", TeamLimitsAndConnectionIdentities},
        {"late joins are spectators", LateJoinsAreSpectatorsUntilNextPreparation},
        {"preparation disconnect and restart", PreparationDisconnectAndRestart},
        {"preparation expiry keeps gameplay gated", PreparationExpiryKeepsEndpointGameplayGated},
        {"elimination wins and single award", EliminationWinsAndSingleAward},
        {"timeout and deadline ordering", TimeoutAndDeadlineOrdering},
        {"plant deadline boundaries", PlantBoundaries},
        {"defuse boundaries and detonation", DefuseBoundariesAndDetonation},
        {"interaction eligibility and requests", InteractionEligibilityAndRequests},
        {"missing certification resets progress", MissingCertificationCancelsRatherThanRetainingProgress},
        {"refreshed ineligibility resets progress", RefreshedIneligibilityCancelsExistingProgress},
        {"stop and invalidation at completion", StopAndInvalidationAtCompletion},
        {"defuse interruption and death", DefuseInterruptionAndDeathAtCompletion},
        {"wrong stop and rejected frame preserve progress", WrongStopAndRejectedFrameDoNotCorruptProgress},
        {"drop placement and retrieve validation", DropPlacementAndRetrieveValidation},
        {"carrier loss and recovery", CarrierDeathDisconnectAndRecovery},
        {"postplant attacker elimination", PostPlantAttackerEliminationDoesNotWinForDefenders},
        {"simultaneous deaths are deterministic", SimultaneousDeathsAreOrderIndependent},
        {"competing interactions are canonical", CompetingPickupsAndDefusesAreCanonical},
        {"underfilled waiting preserves score", UnderfilledWaitingPreservesScoreAndResumes},
        {"minimum roster does not pause ongoing round", MinimumTwoDoesNotPauseAnOngoingRound},
        {"empty server reset", EmptyServerClearsWithoutAnotherAward},
        {"crossed deadlines are chronological", CrossedDeadlinesMatchSmallSteps},
        {"earlier completion before endpoint invalidation", EarlierCompletionPrecedesLaterEndpointInvalidation},
        {"controls observe same-batch outcomes", ControlsObserveOutcomesInTheSameBatch},
        {"stale rounds and request replay after reset", StaleRoundCommandsAndRequestIdsSurviveReset},
        {"six round draw win rematch and twelve rounds", SixRoundDrawRoleSwapRematchAndTwelveRoundReset},
        {"rematch waits for missing roster", RematchWithMissingRosterWaits},
    };
    int Passed = 0;
    for (const TestCase& Test : Tests)
    {
        CurrentTest = Test.Name;
        const int Before = Failures;
        Test.Run();
        if (Failures == Before)
        {
            ++Passed;
            std::printf("PASS %s\n", Test.Name);
        }
    }
    const int Total = static_cast<int>(sizeof(Tests) / sizeof(Tests[0]));
    std::printf("Round rules: %d/%d cases passed, %d checks, %d failures\n", Passed, Total, Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
