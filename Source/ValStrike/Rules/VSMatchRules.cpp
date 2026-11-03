#include "VSMatchRules.h"

#include <algorithm>
#include <cassert>

namespace ValStrike::Rules
{
namespace
{
bool IsTeam(Team Value) { return Value == Team::One || Value == Team::Two; }
bool IsPlaying(Phase Value) { return Value == Phase::Active || Value == Phase::Planted; }
bool IsAction(EventKind Value)
{
    return Value == EventKind::Drop || Value == EventKind::Retrieve || Value == EventKind::StartPlant ||
           Value == EventKind::StartDefuse || Value == EventKind::StopInteraction;
}
int Category(EventKind Value)
{
    switch (Value)
    {
    case EventKind::Join: case EventKind::Disconnect: return 0;
    case EventKind::Death: return 1;
    case EventKind::Drop: case EventKind::Retrieve: case EventKind::StartPlant:
    case EventKind::StartDefuse: case EventKind::StopInteraction: return 2;
    case EventKind::ConfirmPlacement: case EventKind::RecoverDevice: return 3;
    case EventKind::NextRound: case EventKind::Rematch: return 4;
    }
    return -1;
}
bool IsInvalidating(EventKind Value)
{
    return Value == EventKind::Drop || Value == EventKind::StopInteraction || Value == EventKind::RecoverDevice;
}
}

Team Snapshot::DefendingTeam() const { return AttackingTeam == Team::One ? Team::Two : Team::One; }
std::size_t Snapshot::RosterCount(Team Value) const
{
    return static_cast<std::size_t>(std::count_if(Players.begin(), Players.end(), [Value](const Player& P)
    { return P.PlayerId != 0 && P.TeamId == Value; }));
}
std::size_t Snapshot::ParticipantCount(Team Value) const
{
    return static_cast<std::size_t>(std::count_if(Players.begin(), Players.end(), [Value](const Player& P)
    { return P.PlayerId != 0 && P.TeamId == Value && P.Participating; }));
}
std::size_t Snapshot::LivingCount(Team Value) const
{
    return static_cast<std::size_t>(std::count_if(Players.begin(), Players.end(), [Value](const Player& P)
    { return P.PlayerId != 0 && P.TeamId == Value && P.Participating && P.Alive; }));
}
const Player* Snapshot::FindPlayer(Id PlayerId) const
{
    if (PlayerId == 0) { return nullptr; }
    for (const Player& P : Players) { if (P.PlayerId == PlayerId) { return &P; } }
    return nullptr;
}
bool Snapshot::CanMove(Id PlayerId) const
{
    const Player* P = FindPlayer(PlayerId);
    return ConfigurationValid && IsPlaying(CurrentPhase) && P && P->Participating && P->Alive;
}
bool Snapshot::CanCombat(Id PlayerId) const { return CanMove(PlayerId); }

MatchRules::MatchRules(const Config& InTuning) : Tuning(InTuning)
{
    State.ConfigurationValid = IsValidConfig(Tuning);
}
bool MatchRules::IsValidConfig(const Config& Value)
{
    const auto DurationValid = [](TimeMs Duration) { return Duration > 0 && Duration <= MaxDurationMs; };
    return (Value.MinimumPerTeam == 1 || Value.MinimumPerTeam == 2) &&
           DurationValid(Value.PreparationMs) && DurationValid(Value.RoundMs) && DurationValid(Value.PlantMs) &&
           DurationValid(Value.DefuseMs) && DurationValid(Value.DetonationMs);
}
Player* MatchRules::FindPlayer(Id PlayerId)
{
    if (PlayerId == 0) { return nullptr; }
    for (Player& P : State.Players) { if (P.PlayerId == PlayerId) { return &P; } }
    return nullptr;
}
bool MatchRules::HasMinimumRoster() const
{
    return State.RosterCount(Team::One) >= Tuning.MinimumPerTeam &&
           State.RosterCount(Team::Two) >= Tuning.MinimumPerTeam;
}
bool MatchRules::HasMinimumParticipants() const
{
    return State.ParticipantCount(Team::One) >= Tuning.MinimumPerTeam &&
           State.ParticipantCount(Team::Two) >= Tuning.MinimumPerTeam;
}
Id MatchRules::Emit(StepResult& Out, EffectKind Kind, TimeMs At, Id PlayerId, Team TeamId, WinReason Reason)
{
    // A frame has <=16 events; each emits <=2 effects. Timers and lifecycle add
    // <=12. No automatic loop starts another round. The reserved stride is 64.
    assert(Out.Effects.size() < 63);
    const Id EffectId = LastSequence * 64 + Out.Effects.size() + 1;
    Out.Effects.push_back({EffectId, At, Kind, State.RoundId, PlayerId, TeamId, Reason, State.Objective.Revision});
    return EffectId;
}
void MatchRules::CancelInteraction(StepResult& Out, TimeMs At)
{
    if (State.CurrentInteraction.Kind != InteractionKind::None)
    {
        Emit(Out, EffectKind::InteractionCanceled, At, State.CurrentInteraction.PlayerId);
        State.CurrentInteraction = {};
    }
}
void MatchRules::ClearMatch(StepResult& Out, TimeMs At)
{
    const auto Roster = State.Players;
    State = {};
    State.ConfigurationValid = true;
    State.Now = At;
    State.Players = Roster;
    for (Player& P : State.Players) { P.Participating = false; P.Alive = false; }
    Emit(Out, EffectKind::MatchCleared, At);
}
void MatchRules::WaitForRoster(StepResult& Out, TimeMs At)
{
    CancelInteraction(Out, At);
    State.CurrentPhase = Phase::Waiting;
    State.PhaseDeadline = 0;
    State.Objective = {};
    for (Player& P : State.Players) { P.Participating = false; P.Alive = false; }
    Emit(Out, EffectKind::WaitingForRoster, At);
}
void MatchRules::Prepare(StepResult& Out, TimeMs At)
{
    State.CurrentPhase = Phase::Preparation;
    State.AttackingTeam = State.CompletedRounds < 3 ? Team::One : Team::Two;
    State.RoundWinner = Team::None;
    State.Reason = WinReason::None;
    State.CurrentInteraction = {};
    State.PhaseDeadline = At + Tuning.PreparationMs;
    Id Carrier = 0;
    for (Player& P : State.Players)
    {
        P.Participating = P.PlayerId != 0;
        P.Alive = P.Participating;
        if (P.Participating && P.TeamId == State.AttackingTeam && (Carrier == 0 || P.PlayerId < Carrier))
        { Carrier = P.PlayerId; }
    }
    State.RoundId = Emit(Out, EffectKind::PrepareRound, At, Carrier, State.AttackingTeam);
    State.Objective = {DeviceState::Carried, State.RoundId, Carrier, 0};
    Out.Effects.back().RoundId = State.RoundId;
    Out.Effects.back().DeviceRevision = State.Objective.Revision;
}
void MatchRules::Resolve(StepResult& Out, TimeMs At, Team Winner, WinReason Reason)
{
    if (!IsPlaying(State.CurrentPhase)) { return; }
    CancelInteraction(Out, At);
    State.CurrentPhase = Phase::Resolved;
    State.PhaseDeadline = 0;
    State.RoundWinner = Winner;
    State.Reason = Reason;
    ++State.Scores[Winner == Team::One ? 0 : 1];
    ++State.CompletedRounds;
    Emit(Out, EffectKind::RoundAwarded, At, 0, Winner, Reason);
    if (State.CompletedRounds == 6)
    {
        State.CurrentPhase = Phase::MatchComplete;
        State.Result = State.Scores[0] == State.Scores[1] ? MatchResult::Draw :
            (State.Scores[0] > State.Scores[1] ? MatchResult::TeamOne : MatchResult::TeamTwo);
        Emit(Out, EffectKind::MatchCompleted, At, 0, Winner, Reason);
    }
}
void MatchRules::CheckElimination(StepResult& Out, TimeMs At)
{
    // A preplant mutual wipe favors defenders; after plant it favors attackers.
    if (State.CurrentPhase == Phase::Active && State.LivingCount(State.AttackingTeam) == 0)
    { Resolve(Out, At, State.DefendingTeam(), WinReason::Elimination); }
    else if (IsPlaying(State.CurrentPhase) && State.LivingCount(State.DefendingTeam()) == 0)
    { Resolve(Out, At, State.AttackingTeam, WinReason::Elimination); }
}
void MatchRules::DropDevice(StepResult& Out, TimeMs At, bool Recovery)
{
    const Id Carrier = State.Objective.Carrier;
    CancelInteraction(Out, At);
    State.Objective.State = DeviceState::PlacementPending;
    State.Objective.Carrier = 0;
    State.Objective.Site = 0;
    State.Objective.Revision = Emit(Out, Recovery ? EffectKind::RecoveryPlacementRequested :
        EffectKind::DropPlacementRequested, At, Carrier);
    Out.Effects.back().DeviceRevision = State.Objective.Revision;
}
void MatchRules::CompleteInteraction(StepResult& Out, TimeMs At)
{
    const Interaction Completed = State.CurrentInteraction;
    State.CurrentInteraction = {};
    State.Objective.Carrier = 0;
    if (Completed.Kind == InteractionKind::Plant)
    {
        State.CurrentPhase = Phase::Planted;
        State.PhaseDeadline = At + Tuning.DetonationMs;
        State.Objective.State = DeviceState::Planted;
        State.Objective.Site = ObjectiveSite;
        State.Objective.Revision = Emit(Out, EffectKind::DevicePlanted, At, Completed.PlayerId);
        Out.Effects.back().DeviceRevision = State.Objective.Revision;
    }
    else
    {
        State.Objective.State = DeviceState::Defused;
        State.Objective.Revision = Emit(Out, EffectKind::DeviceDefused, At, Completed.PlayerId);
        Out.Effects.back().DeviceRevision = State.Objective.Revision;
        Resolve(Out, At, State.DefendingTeam(), WinReason::Defuse);
    }
}
void MatchRules::HardDeadline(StepResult& Out, TimeMs At)
{
    if (State.PhaseDeadline == 0 || State.PhaseDeadline != At) { return; }
    switch (State.CurrentPhase)
    {
    case Phase::Preparation:
        State.CurrentPhase = Phase::Active;
        State.PhaseDeadline = At + Tuning.RoundMs;
        Emit(Out, EffectKind::RoundActivated, At);
        break;
    case Phase::Active:
        Resolve(Out, At, State.DefendingTeam(), WinReason::Timeout);
        break;
    case Phase::Planted:
        State.Objective.State = DeviceState::Detonated;
        State.Objective.Revision = Emit(Out, EffectKind::DeviceDetonated, At);
        Out.Effects.back().DeviceRevision = State.Objective.Revision;
        Resolve(Out, At, State.AttackingTeam, WinReason::Detonation);
        break;
    default: break;
    }
}
void MatchRules::AdvanceBefore(StepResult& Out, TimeMs Until)
{
    for (;;)
    {
        TimeMs Next = State.PhaseDeadline;
        const Interaction& Current = State.CurrentInteraction;
        if (Current.Kind != InteractionKind::None && (Next == 0 || Current.CompletesAt < Next))
        { Next = Current.CompletesAt; }
        if (Next == 0 || Next >= Until) { return; }
        // A hard deadline wins ties. A plant before the deadline replaces it.
        HardDeadline(Out, Next);
        if (State.CurrentInteraction.Kind != InteractionKind::None && State.CurrentInteraction.CompletesAt == Next)
        { CompleteInteraction(Out, Next); }
    }
}

EventStatus MatchRules::ApplyAction(const Event& Input, StepResult& Out)
{
    if (Input.RoundId == 0 || Input.RoundId != State.RoundId) { return EventStatus::StaleRound; }
    Player* P = FindPlayer(Input.PlayerId);
    if (IsAction(Input.Kind))
    {
        if (!P) { return EventStatus::UnknownPlayer; }
        if (Input.RequestId == 0 || Input.RequestId <= P->LastRequestId) { return EventStatus::ReplayedRequest; }
        P->LastRequestId = Input.RequestId; // Invalid requests cannot become valid when replayed later.
        if (!IsPlaying(State.CurrentPhase)) { return EventStatus::WrongPhase; }
        if (!P->Participating || !P->Alive) { return EventStatus::NotAlive; }
    }
    if (Input.Kind == EventKind::StopInteraction)
    {
        if (State.CurrentInteraction.PlayerId != Input.PlayerId || Input.InteractionToken == 0 ||
            State.CurrentInteraction.Token != Input.InteractionToken) { return EventStatus::WrongInteraction; }
        CancelInteraction(Out, State.Now);
        return EventStatus::Applied;
    }
    if (Input.Kind == EventKind::Drop)
    {
        if (State.CurrentPhase != Phase::Active || State.Objective.State != DeviceState::Carried ||
            State.Objective.Carrier != Input.PlayerId) { return EventStatus::WrongDevice; }
        DropDevice(Out, State.Now, false);
        return EventStatus::Applied;
    }
    if (Input.Kind == EventKind::ConfirmPlacement || Input.Kind == EventKind::RecoverDevice)
    {
        if (State.CurrentPhase != Phase::Preparation && State.CurrentPhase != Phase::Active)
        { return EventStatus::WrongPhase; }
    }
    if (Input.DeviceRevision == 0 || Input.DeviceRevision != State.Objective.Revision)
    { return EventStatus::WrongDevice; }
    if (!Input.WorldConfirmed)
    {
        if ((Input.Kind == EventKind::StartPlant || Input.Kind == EventKind::StartDefuse) &&
            State.CurrentInteraction.PlayerId == Input.PlayerId)
        { CancelInteraction(Out, State.Now); }
        return EventStatus::Ineligible;
    }
    if (Input.Kind == EventKind::ConfirmPlacement)
    {
        if (State.Objective.State != DeviceState::PlacementPending) { return EventStatus::WrongDevice; }
        State.Objective.State = DeviceState::Dropped;
        State.Objective.Revision = Emit(Out, EffectKind::DevicePlaced, State.Now);
        Out.Effects.back().DeviceRevision = State.Objective.Revision;
        return EventStatus::Applied;
    }
    if (Input.Kind == EventKind::RecoverDevice)
    {
        if (State.Objective.State != DeviceState::Carried && State.Objective.State != DeviceState::Dropped)
        { return EventStatus::WrongDevice; }
        DropDevice(Out, State.Now, true);
        return EventStatus::Applied;
    }
    if (Input.Kind == EventKind::Retrieve)
    {
        if (State.CurrentPhase != Phase::Active || P->TeamId != State.AttackingTeam ||
            State.Objective.State != DeviceState::Dropped) { return EventStatus::WrongDevice; }
        State.Objective.State = DeviceState::Carried;
        State.Objective.Carrier = Input.PlayerId;
        State.Objective.Revision = Emit(Out, EffectKind::DeviceRetrieved, State.Now, Input.PlayerId);
        Out.Effects.back().DeviceRevision = State.Objective.Revision;
        return EventStatus::Applied;
    }
    const bool Plant = Input.Kind == EventKind::StartPlant;
    if (Plant)
    {
        if (State.CurrentPhase != Phase::Active || P->TeamId != State.AttackingTeam ||
            State.Objective.State != DeviceState::Carried || State.Objective.Carrier != Input.PlayerId)
        { return EventStatus::WrongDevice; }
        if (Input.Site != ObjectiveSite)
        {
            if (State.CurrentInteraction.PlayerId == Input.PlayerId) { CancelInteraction(Out, State.Now); }
            return EventStatus::Ineligible;
        }
    }
    else if (State.CurrentPhase != Phase::Planted || P->TeamId != State.DefendingTeam() ||
             State.Objective.State != DeviceState::Planted) { return EventStatus::WrongDevice; }
    if (State.CurrentInteraction.Kind != InteractionKind::None)
    {
        return State.CurrentInteraction.PlayerId == Input.PlayerId ? EventStatus::Unchanged : EventStatus::Busy;
    }
    const Id Token = Emit(Out, EffectKind::InteractionStarted, State.Now, Input.PlayerId);
    State.CurrentInteraction = {Plant ? InteractionKind::Plant : InteractionKind::Defuse, Token, Input.PlayerId,
        State.Now, State.Now + (Plant ? Tuning.PlantMs : Tuning.DefuseMs)};
    return EventStatus::Applied;
}

StepResult MatchRules::Apply(const Frame& Input)
{
    StepResult Out;
    if (!State.ConfigurationValid) { Out.Status = FrameStatus::InvalidConfiguration; return Out; }
    if (Input.Now < 0 || Input.Now > MaxServerTimeMs || (HasFrame && Input.Now <= State.Now))
    { Out.Status = FrameStatus::InvalidTime; return Out; }
    if (Input.Sequence == 0 || Input.Sequence > MaxSequence || Input.Sequence <= LastSequence)
    { Out.Status = FrameStatus::ReplayedSequence; return Out; }
    if (Input.EventCount > MaxEvents) { Out.Status = FrameStatus::InvalidBatch; return Out; }
    // Reject ambiguous duplicate commands as an atomic batch, with no time/state changes.
    // Different categories (e.g. Death + Stop) are valid and use the priority below.
    for (std::size_t I = 0; I < Input.EventCount; ++I)
    {
        const int Group = Category(Input.Events[I].Kind);
        if (Group < 0) { Out.Status = FrameStatus::InvalidBatch; return Out; }
        for (std::size_t J = 0; J < I; ++J)
        {
            if (Category(Input.Events[J].Kind) == Group && (Group >= 3 ||
                Input.Events[I].PlayerId == Input.Events[J].PlayerId))
            { Out.Status = FrameStatus::InvalidBatch; return Out; }
        }
    }
    HasFrame = true;
    LastSequence = Input.Sequence;
    if (State.CurrentInteraction.Kind != InteractionKind::None &&
        Input.ContinuousInteractionToken != State.CurrentInteraction.Token)
    { CancelInteraction(Out, State.Now); }
    AdvanceBefore(Out, Input.Now);
    State.Now = Input.Now;

    // Canonical identity order also chooses the winner of a contested pickup/defuse start.
    std::array<std::size_t, MaxEvents> Order{};
    for (std::size_t I = 0; I < Input.EventCount; ++I) { Order[I] = I; }
    std::sort(Order.begin(), Order.begin() + Input.EventCount, [&Input](std::size_t A, std::size_t B)
    {
        if (Input.Events[A].PlayerId != Input.Events[B].PlayerId)
        { return Input.Events[A].PlayerId < Input.Events[B].PlayerId; }
        return static_cast<int>(Input.Events[A].Kind) < static_cast<int>(Input.Events[B].Kind);
    });

    const bool HadRoster = State.RosterCount(Team::One) + State.RosterCount(Team::Two) != 0;
    bool CarrierLost = false;
    // Departures and deaths are aggregated before any elimination decision.
    for (std::size_t K = 0; K < Input.EventCount; ++K)
    {
        const std::size_t I = Order[K];
        const Event& E = Input.Events[I];
        if (E.Kind != EventKind::Disconnect && E.Kind != EventKind::Death) { continue; }
        Player* P = FindPlayer(E.PlayerId);
        if (!P) { Out.Events[I] = EventStatus::UnknownPlayer; continue; }
        if (E.Kind == EventKind::Death)
        {
            if (E.RoundId == 0 || E.RoundId != State.RoundId) { Out.Events[I] = EventStatus::StaleRound; continue; }
            if (!IsPlaying(State.CurrentPhase)) { Out.Events[I] = EventStatus::WrongPhase; continue; }
            if (!P->Participating || !P->Alive) { Out.Events[I] = EventStatus::NotAlive; continue; }
            P->Alive = false;
        }
        else { *P = {}; }
        if (State.CurrentInteraction.PlayerId == E.PlayerId) { CancelInteraction(Out, Input.Now); }
        CarrierLost = CarrierLost || (State.Objective.State == DeviceState::Carried && State.Objective.Carrier == E.PlayerId);
        Out.Events[I] = EventStatus::Applied;
    }
    const bool Abandoned = HadRoster && State.RosterCount(Team::One) + State.RosterCount(Team::Two) == 0;
    if (Abandoned) { ClearMatch(Out, Input.Now); }
    else if (State.CurrentPhase == Phase::Preparation && !HasMinimumParticipants())
    { WaitForRoster(Out, Input.Now); }
    else if (CarrierLost && (State.CurrentPhase == Phase::Preparation || State.CurrentPhase == Phase::Active))
    { DropDevice(Out, Input.Now, false); }

    // Release/drop/recovery invalidation at Now defeats completion at Now.
    // Gameplay submitted at the preparation boundary still observes the locked
    // preparation gate. The resulting Active snapshot opens it for future input.
    for (std::size_t K = 0; K < Input.EventCount; ++K)
    {
        const std::size_t I = Order[K];
        if (IsInvalidating(Input.Events[I].Kind) ||
            (State.CurrentPhase == Phase::Preparation && IsAction(Input.Events[I].Kind)))
        { Out.Events[I] = ApplyAction(Input.Events[I], Out); }
    }
    HardDeadline(Out, Input.Now);
    CheckElimination(Out, Input.Now);

    for (std::size_t K = 0; K < Input.EventCount; ++K)
    {
        const std::size_t I = Order[K];
        const Event& E = Input.Events[I];
        if (E.Kind == EventKind::Join)
        {
            if (E.PlayerId == 0 || E.PlayerId <= LastJoinedId || FindPlayer(E.PlayerId))
            { Out.Events[I] = EventStatus::InvalidIdentity; }
            else if (!IsTeam(E.TeamId)) { Out.Events[I] = EventStatus::InvalidTeam; }
            else if (State.RosterCount(E.TeamId) >= 2) { Out.Events[I] = EventStatus::TeamFull; }
            else
            {
                for (Player& P : State.Players)
                {
                    if (P.PlayerId == 0) { P = {E.PlayerId, E.TeamId, false, false, 0}; break; }
                }
                LastJoinedId = E.PlayerId;
                Out.Events[I] = EventStatus::Applied;
            }
        }
        else if (Out.Events[I] == EventStatus::NotProcessed &&
                 (IsAction(E.Kind) || E.Kind == EventKind::ConfirmPlacement))
        { Out.Events[I] = ApplyAction(E, Out); }
    }
    if (State.CurrentInteraction.Kind != InteractionKind::None && State.CurrentInteraction.CompletesAt == Input.Now)
    { CompleteInteraction(Out, Input.Now); }

    // Controls are validated against the phase at this timestamp after outcomes.
    // RoundId prevents stale requests from continuing a different round.
    for (std::size_t I = 0; I < Input.EventCount; ++I)
    {
        const Event& E = Input.Events[I];
        if (E.Kind != EventKind::NextRound && E.Kind != EventKind::Rematch) { continue; }
        if (E.RoundId == 0 || E.RoundId != State.RoundId) { Out.Events[I] = EventStatus::StaleRound; }
        else if (E.Kind == EventKind::NextRound && State.CurrentPhase == Phase::Resolved)
        {
            if (HasMinimumRoster()) { Prepare(Out, Input.Now); }
            else { WaitForRoster(Out, Input.Now); }
            Out.Events[I] = EventStatus::Applied;
        }
        else if (E.Kind == EventKind::Rematch && State.CurrentPhase == Phase::MatchComplete)
        {
            ClearMatch(Out, Input.Now);
            Out.Events[I] = EventStatus::Applied;
        }
        else { Out.Events[I] = EventStatus::WrongPhase; }
    }
    if (State.CurrentPhase == Phase::Waiting && HasMinimumRoster()) { Prepare(Out, Input.Now); }
    return Out;
}
}
