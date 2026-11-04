#include "VSMatchPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "VSMatchGameMode.h"
#include "VSMatchGameState.h"
#include "VSMatchPlayerState.h"
#include "VSSpectateRules.h"
#include "../Combat/VSHealthComponent.h"
#include "../Rules/VSMatchRules.h"
#include "Tests/VSMatchSmoke.h"
#include "../ValStrike.h"
#include "../VSCharacter.h"

namespace
{
using RulesPhase = ValStrike::Rules::Phase;
using RulesTeam = ValStrike::Rules::Team;
using RulesObjective = ValStrike::Rules::DeviceState;
using RulesResult = ValStrike::Rules::MatchResult;

static_assert(static_cast<uint8>(EVSMatchHUDPhase::Waiting) == static_cast<uint8>(RulesPhase::Waiting));
static_assert(static_cast<uint8>(EVSMatchHUDPhase::Preparation) == static_cast<uint8>(RulesPhase::Preparation));
static_assert(static_cast<uint8>(EVSMatchHUDPhase::Active) == static_cast<uint8>(RulesPhase::Active));
static_assert(static_cast<uint8>(EVSMatchHUDPhase::Planted) == static_cast<uint8>(RulesPhase::Planted));
static_assert(static_cast<uint8>(EVSMatchHUDPhase::Resolved) == static_cast<uint8>(RulesPhase::Resolved));
static_assert(static_cast<uint8>(EVSMatchHUDPhase::MatchComplete) == static_cast<uint8>(RulesPhase::MatchComplete));
static_assert(static_cast<uint8>(EVSMatchHUDTeam::None) == static_cast<uint8>(RulesTeam::None));
static_assert(static_cast<uint8>(EVSMatchHUDTeam::One) == static_cast<uint8>(RulesTeam::One));
static_assert(static_cast<uint8>(EVSMatchHUDTeam::Two) == static_cast<uint8>(RulesTeam::Two));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::Unavailable) == static_cast<uint8>(RulesObjective::Unavailable));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::Carried) == static_cast<uint8>(RulesObjective::Carried));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::PlacementPending) == static_cast<uint8>(RulesObjective::PlacementPending));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::Dropped) == static_cast<uint8>(RulesObjective::Dropped));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::Planted) == static_cast<uint8>(RulesObjective::Planted));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::Defused) == static_cast<uint8>(RulesObjective::Defused));
static_assert(static_cast<uint8>(EVSMatchHUDObjective::Detonated) == static_cast<uint8>(RulesObjective::Detonated));

EVSMatchHUDTeam HUDTeam(uint8 TeamId)
{
    return TeamId <= static_cast<uint8>(EVSMatchHUDTeam::Two) ?
        static_cast<EVSMatchHUDTeam>(TeamId) : EVSMatchHUDTeam::None;
}

bool IsPlaying(uint8 Phase)
{
    return Phase == static_cast<uint8>(RulesPhase::Active) || Phase == static_cast<uint8>(RulesPhase::Planted);
}

// Keep the teammate view through the result display; preparation's fresh
// possession, roster loss and match completion all restore the normal view.
bool AllowsDeathSpectate(const FVSMatchRepState& Match)
{
    return Match.bReady && (IsPlaying(Match.Phase) || Match.Phase == static_cast<uint8>(RulesPhase::Resolved));
}

bool HasLivePawn(const AVSMatchPlayerState* Player)
{
    const AVSCharacter* Pawn = Player ? Player->GetPawn<AVSCharacter>() : nullptr;
    return IsValid(Pawn) && Pawn->GetHealthComponent() && Pawn->GetHealthComponent()->IsAlive();
}

ValStrike::Spectate::Viewer SpectateViewer(const AVSMatchPlayerState& Self)
{
    ValStrike::Spectate::Viewer Viewer;
    Viewer.PlayerId = Self.ConnectionId;
    Viewer.TeamId = Self.TeamId;
    return Viewer;
}

ValStrike::Spectate::Candidate SpectateCandidate(const AVSMatchPlayerState& Player)
{
    ValStrike::Spectate::Candidate Candidate;
    Candidate.PlayerId = Player.ConnectionId;
    Candidate.TeamId = Player.TeamId;
    Candidate.Participating = Player.bParticipating;
    // Replicated life state and the pawn can arrive in either order; require both.
    Candidate.Alive = Player.bAlive && HasLivePawn(&Player);
    return Candidate;
}

TArray<ValStrike::Spectate::Candidate> SpectateCandidates(const AVSMatchGameState& State)
{
    TArray<ValStrike::Spectate::Candidate> Candidates;
    for (const APlayerState* Entry : State.PlayerArray)
    {
        if (const AVSMatchPlayerState* Player = Cast<AVSMatchPlayerState>(Entry))
        {
            Candidates.Add(SpectateCandidate(*Player));
        }
    }
    return Candidates;
}

#if !UE_BUILD_SHIPPING
// One structured line per local view-target change, read by Scripts/test_match.py.
void LogSpectate(const UWorld* World, const AVSMatchPlayerState* Self, const TCHAR* Event, uint64 Target)
{
    const AVSMatchGameState* State = World ? World->GetGameState<AVSMatchGameState>() : nullptr;
    UE_LOG(LogValStrike, Display, TEXT("VSSPECTATE {\"event\":\"%s\",\"viewer\":%llu,\"target\":%llu,\"round\":%llu}"),
        Event, Self ? Self->ConnectionId : 0ull, Target, State ? State->Match.RoundId : 0ull);
}
#endif
}

void AVSMatchPlayerController::BeginPlay()
{
    Super::BeginPlay();
    ValStrikeTests::StartMatchSmoke();
}

void AVSMatchPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UpdateDeathSpectate();
}

bool AVSMatchPlayerController::NotifyServerReceivedClientData(APawn* InPawn, float TimeStamp)
{
    if (!Super::NotifyServerReceivedClientData(InPawn, TimeStamp)) { return false; }
    const AVSCharacter* Character = Cast<AVSCharacter>(InPawn);
    // This engine callback runs before executing a client movement packet.
    // Use the live deadline gate, rather than a possibly older replicated flag.
    return Character && AVSMatchGameMode::AllowsDamage(Character) && Character->CanMatchMove();
}

void AVSMatchPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent(); // Keeps the existing Escape/session menu binding.
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &AVSMatchPlayerController::ObjectivePressed);
    InputComponent->BindKey(EKeys::E, IE_Released, this, &AVSMatchPlayerController::ObjectiveReleased);
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &AVSMatchPlayerController::DropObjective);
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AVSMatchPlayerController::RequestMatchRematch);
}

bool AVSMatchPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (Params.Key == EKeys::E && Params.Event == IE_Pressed)
    {
        bFreshObjectivePress = !IsSessionMenuOpen();
    }
    // Raw key: the fire action lives on the pawn's Enhanced Input context.
    // The dead pawn's own fire attempt stays rejected by its health gate.
    if (Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Pressed && !IsSessionMenuOpen())
    {
        CycleSpectateTarget();
    }
    return Super::InputKey(Params);
}

bool AVSMatchPlayerController::SendLocalCommand(EVSMatchCommand Command)
{
    if (!IsLocalController() || LocalRequestId == MAX_uint32 || !GetWorld())
    {
        return false; // Never wrap the per-connection replay counter.
    }
    const AVSMatchGameState* State = GetWorld()->GetGameState<AVSMatchGameState>();
    if (!State)
    {
        return false;
    }
    ServerCommand(Command, ++LocalRequestId, State->Match.RoundId, State->Match.DeviceRevision);
    return true;
}

void AVSMatchPlayerController::ObjectivePressed()
{
    const bool bNewPress = bFreshObjectivePress;
    bFreshObjectivePress = false;
    if (!bNewPress || bObjectiveHeld || !IsLocalController() || IsSessionMenuOpen() || !GetWorld())
    {
        return;
    }
    const AVSMatchGameState* State = GetWorld()->GetGameState<AVSMatchGameState>();
    const AVSMatchPlayerState* Player = GetPlayerState<AVSMatchPlayerState>();
    if (!State || !State->Match.bReady || !IsPlaying(State->Match.Phase) ||
        !Player || !Player->bParticipating || !Player->bAlive)
    {
        return;
    }
    // Set intent before the RPC: a local authority may synchronously reject the
    // request and call ClientCancelObjective before SendLocalCommand returns.
    bObjectiveHeld = true;
    if (!SendLocalCommand(EVSMatchCommand::Press))
    {
        bObjectiveHeld = false;
    }
}

void AVSMatchPlayerController::ObjectiveReleased()
{
    CancelObjectiveInput();
}

void AVSMatchPlayerController::CancelObjectiveInput()
{
    const bool bWasHeld = bObjectiveHeld;
    bObjectiveHeld = false;
    bFreshObjectivePress = false;
    if (bWasHeld)
    {
        SendLocalCommand(EVSMatchCommand::Release);
    }
}

void AVSMatchPlayerController::DropObjective()
{
    const FVSMatchHUDData Data = GetMatchHUDData();
    const AVSMatchPlayerState* Player = GetPlayerState<AVSMatchPlayerState>();
    if (!IsLocalController() || IsSessionMenuOpen() || !Data.bLocalCarrier ||
        Data.Phase != EVSMatchHUDPhase::Active || !Player || !Player->bParticipating || !Player->bAlive)
    {
        return;
    }
    // Drop itself invalidates held interaction on the server. Avoid sending two
    // competing voluntary actions for one player in the same sealed batch.
    bObjectiveHeld = false;
    bFreshObjectivePress = false;
    SendLocalCommand(EVSMatchCommand::Drop);
}

void AVSMatchPlayerController::RequestMatchRematch()
{
    if (IsLocalController() && !IsSessionMenuOpen() && GetMatchHUDData().bCanRequestRematch)
    {
        SendLocalCommand(EVSMatchCommand::Rematch);
    }
}

void AVSMatchPlayerController::UpdateDeathSpectate()
{
    const AVSMatchGameState* State = IsLocalController() && GetWorld() ? GetWorld()->GetGameState<AVSMatchGameState>() : nullptr;
    const AVSMatchPlayerState* Self = GetPlayerState<AVSMatchPlayerState>();
    AVSCharacter* Own = Cast<AVSCharacter>(GetPawn());
    // Only a possessed, dead combatant pawn has a death view. Late joins and
    // ClearPawns spectators have no pawn and keep the engine spectator state.
    const bool bOwnDead = State && Self && Own && (!Self->bAlive ||
        (Own->GetHealthComponent() && !Own->GetHealthComponent()->IsAlive()));
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (!bOwnDead)
    {
        DeathPawn.Reset();
    }
    else if (DeathPawn.Get() != Own)
    {
        DeathPawn = Own;
        DeathSeconds = Now;
    }
    if (!ValStrike::Spectate::ShouldSpectate(State && AllowsDeathSpectate(State->Match),
        Self && Self->bParticipating, !bOwnDead, Now - DeathSeconds))
    {
        StopDeathSpectate();
        return;
    }
    const TArray<ValStrike::Spectate::Candidate> Candidates = SpectateCandidates(*State);
    const uint64 Target = ValStrike::Spectate::Keep(Candidates.GetData(), static_cast<std::size_t>(Candidates.Num()),
        SpectateViewer(*Self), SpectateTargetId);
    if (!WatchSpectateTarget(Target))
    {
        StopDeathSpectate(); // Nobody left alive: back to the death view.
    }
}

void AVSMatchPlayerController::CycleSpectateTarget()
{
    const AVSMatchGameState* State = GetWorld() ? GetWorld()->GetGameState<AVSMatchGameState>() : nullptr;
    const AVSMatchPlayerState* Self = GetPlayerState<AVSMatchPlayerState>();
    if (SpectateTargetId == 0 || !IsLocalController() || !State || !Self)
    {
        return;
    }
    const TArray<ValStrike::Spectate::Candidate> Candidates = SpectateCandidates(*State);
    WatchSpectateTarget(ValStrike::Spectate::Next(Candidates.GetData(), static_cast<std::size_t>(Candidates.Num()),
        SpectateViewer(*Self), SpectateTargetId));
}

bool AVSMatchPlayerController::WatchSpectateTarget(uint64 TargetId)
{
    const AVSMatchPlayerState* Target = FindSpectateTarget(TargetId);
    APawn* TargetPawn = Target ? Target->GetPawn() : nullptr;
    if (!TargetPawn)
    {
        return false;
    }
#if !UE_BUILD_SHIPPING
    if (SpectateTargetId != TargetId)
    {
        LogSpectate(GetWorld(), GetPlayerState<AVSMatchPlayerState>(), SpectateTargetId == 0 ? TEXT("start") : TEXT("cycle"), TargetId);
    }
#endif
    SpectateTargetId = TargetId;
    if (GetViewTarget() != TargetPawn)
    {
        SetViewTarget(TargetPawn);
    }
    return true;
}

void AVSMatchPlayerController::StopDeathSpectate()
{
    if (SpectateTargetId == 0)
    {
        return;
    }
    SpectateTargetId = 0;
#if !UE_BUILD_SHIPPING
    LogSpectate(GetWorld(), GetPlayerState<AVSMatchPlayerState>(), TEXT("stop"), 0);
#endif
    // A fresh possession already set its own view; this covers a still-held death pawn.
    if (APawn* Own = GetPawnOrSpectator())
    {
        SetViewTarget(Own);
    }
}

const AVSMatchPlayerState* AVSMatchPlayerController::FindSpectateTarget(uint64 TargetId) const
{
    const AVSMatchGameState* State = GetWorld() ? GetWorld()->GetGameState<AVSMatchGameState>() : nullptr;
    const AVSMatchPlayerState* Self = GetPlayerState<AVSMatchPlayerState>();
    if (TargetId == 0 || !State || !Self)
    {
        return nullptr;
    }
    for (const APlayerState* Entry : State->PlayerArray)
    {
        const AVSMatchPlayerState* Player = Cast<AVSMatchPlayerState>(Entry);
        if (Player && Player->ConnectionId == TargetId)
        {
            // Re-check the team rule at the point of use, never trusting a cached id alone.
            return ValStrike::Spectate::CanWatch(SpectateCandidate(*Player), SpectateViewer(*Self)) ? Player : nullptr;
        }
    }
    return nullptr;
}

void AVSMatchPlayerController::ServerCommand_Implementation(
    EVSMatchCommand Command, uint32 RequestId, uint64 RoundId, uint64 DeviceRevision)
{
    if (AVSMatchGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AVSMatchGameMode>() : nullptr)
    {
        Mode->HandleCommand(this, Command, RequestId, RoundId, DeviceRevision);
    }
}

void AVSMatchPlayerController::ClientCancelObjective_Implementation()
{
    // Cancellation can arrive after a newer local press. Release that intent too
    // so its eventual key-up cannot be swallowed by clearing bObjectiveHeld here.
    CancelObjectiveInput();
}

FVSMatchHUDData AVSMatchPlayerController::GetMatchHUDData() const
{
    FVSMatchHUDData Data;
    const AVSMatchGameState* State = GetWorld() ? GetWorld()->GetGameState<AVSMatchGameState>() : nullptr;
    if (!State)
    {
        return Data;
    }
    const FVSMatchRepState& Match = State->Match;
    // Keep an explicit setup/map error visible even while gameplay is unavailable.
    Data.bAvailable = Match.bReady || !Match.StatusText.IsEmpty();
    Data.StatusText = Match.StatusText;
    if (!Data.bAvailable)
    {
        return Data;
    }
    Data.Phase = Match.Phase <= static_cast<uint8>(EVSMatchHUDPhase::MatchComplete) ?
        static_cast<EVSMatchHUDPhase>(Match.Phase) : EVSMatchHUDPhase::Waiting;
    Data.AttackingTeam = HUDTeam(Match.AttackingTeam);
    Data.TeamOneScore = Match.ScoreOne;
    Data.TeamTwoScore = Match.ScoreTwo;
    Data.CompletedRounds = Match.CompletedRounds;
    Data.Winner = HUDTeam(Match.Winner);
    Data.ServerNowSeconds = State->GetServerWorldTimeSeconds();
    Data.PhaseDeadlineSeconds = Match.PhaseDeadlineSeconds;
    Data.bHasPhaseDeadline = Match.bReady &&
        (Data.Phase == EVSMatchHUDPhase::Preparation || IsPlaying(Match.Phase)) &&
        FMath::IsFinite(Match.PhaseDeadlineSeconds) && Match.PhaseDeadlineSeconds > 0.0;
    Data.Objective = Match.ObjectiveState <= static_cast<uint8>(EVSMatchHUDObjective::Detonated) ?
        static_cast<EVSMatchHUDObjective>(Match.ObjectiveState) : EVSMatchHUDObjective::Unavailable;

    if (Data.Phase == EVSMatchHUDPhase::MatchComplete)
    {
        Data.bDraw = Match.Result == static_cast<uint8>(RulesResult::Draw);
        Data.Winner = Match.Result == static_cast<uint8>(RulesResult::TeamOne) ? EVSMatchHUDTeam::One :
            (Match.Result == static_cast<uint8>(RulesResult::TeamTwo) ? EVSMatchHUDTeam::Two : EVSMatchHUDTeam::None);
    }

    const AVSMatchPlayerState* Player = GetPlayerState<AVSMatchPlayerState>();
    Data.bSpectating = !Player || !Player->bParticipating;
    if (!Player)
    {
        return Data;
    }
    Data.LocalTeam = HUDTeam(Player->TeamId);
    Data.bLocalCarrier = Match.bReady && Player->ConnectionId != 0 &&
        Match.CarrierId == Player->ConnectionId && Data.Objective == EVSMatchHUDObjective::Carried;
    Data.bCanRequestRematch = Match.bReady && Player->ConnectionId != 0 && Player->bParticipating &&
        Data.Phase == EVSMatchHUDPhase::MatchComplete;

    if (const AVSMatchPlayerState* Watched = FindSpectateTarget(SpectateTargetId))
    {
        Data.bSpectating = true;
        Data.SpectatingName = Watched->GetPlayerName();
        // The result panel owns StatusText while a round result is shown; a map error stays first.
        if (Data.StatusText.IsEmpty() && IsPlaying(Match.Phase))
        {
            Data.StatusText = TEXT("Spectating ") + Data.SpectatingName;
        }
    }

    if (Match.bReady && IsPlaying(Match.Phase) && Player->bParticipating && Player->bAlive)
    {
        Data.InteractionPrompt = IsSessionMenuOpen() ? FString{} : Player->InteractionPrompt;
        const double Duration = Match.InteractionCompletesSeconds - Match.InteractionStartedSeconds;
        Data.bInteractionActive = Player->ConnectionId != 0 && Match.InteractionToken != 0 &&
            Match.InteractionPlayerId == Player->ConnectionId && FMath::IsFinite(Duration) && Duration > 0.0 &&
            FMath::IsFinite(Match.InteractionStartedSeconds) && FMath::IsFinite(Data.ServerNowSeconds);
        if (Data.bInteractionActive)
        {
            Data.InteractionProgress = static_cast<float>(FMath::Clamp(
                (Data.ServerNowSeconds - Match.InteractionStartedSeconds) / Duration, 0.0, 1.0));
        }
    }
    return Data;
}
