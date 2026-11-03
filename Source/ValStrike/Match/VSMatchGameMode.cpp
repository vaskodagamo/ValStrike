#include "VSMatchGameMode.h"

#include "VSMatchGameState.h"
#include "VSMatchPlayerState.h"
#include "VSMatchPlayerController.h"
#include "VSMatchObjective.h"
#include "../VSCharacter.h"
#include "../Combat/VSHealthComponent.h"
#include "../Combat/VSRifleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/TriggerBox.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../ValStrike.h"
#include "Tests/VSMatchSmoke.h"

namespace R = ValStrike::Rules;

AVSMatchGameMode::AVSMatchGameMode()
{
    PlayerControllerClass = AVSMatchPlayerController::StaticClass();
    PlayerStateClass = AVSMatchPlayerState::StaticClass();
    GameStateClass = AVSMatchGameState::StaticClass();
    ObjectiveClass = AVSMatchObjective::StaticClass();
    bStartPlayersAsSpectators = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void AVSMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    FParse::Value(FCommandLine::Get(), TEXT("VSMatchMinimum="), MinimumPerTeam);
    R::Config Config;
    Config.MinimumPerTeam = static_cast<uint8>(FMath::Clamp(MinimumPerTeam, 1, 2));
    ResultDisplaySeconds = FMath::IsFinite(ResultDisplaySeconds) ? FMath::Max(0.1f, ResultDisplaySeconds) : 3.0f;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    FString Role;
    if (FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) && Role == TEXT("server"))
    {
        // Explicit development-only timing; never enabled by a client URL.
        Config.PreparationMs = 3000;
        Config.RoundMs = 20000;
        Config.PlantMs = 600;
        Config.DefuseMs = 700;
        Config.DetonationMs = 2500;
        ResultDisplaySeconds = 0.7f;
    }
#endif
    Rules = R::MatchRules(Config);
}

void AVSMatchGameMode::BeginPlay()
{
    Super::BeginPlay();
    bMapReady = ValidateMap();
    if (bMapReady)
    {
        Objective = GetWorld()->SpawnActor<AVSMatchObjective>(ObjectiveClass);
        bMapReady = Objective != nullptr;
        if (!bMapReady) { MapError = TEXT("Objective actor could not spawn"); }
    }
    if (!bMapReady) { UE_LOG(LogValStrike, Error, TEXT("VSMATCH map invalid: %s"), *MapError); }
    Publish();
    ValStrikeTests::StartMatchSmoke();
}

int64 AVSMatchGameMode::NowMs() const
{
    return FMath::Max<int64>(0, static_cast<int64>(FMath::FloorToDouble(GetWorld()->GetTimeSeconds() * 1000.0)));
}

bool AVSMatchGameMode::ValidateMap()
{
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        const bool A = It->ActorHasTag(TEXT("VS_AttackerSpawn"));
        const bool D = It->ActorHasTag(TEXT("VS_DefenderSpawn"));
        if (A && D) { MapError = TEXT("Spawn has competing role tags"); return false; }
        if (A) { AttackerStarts.Add(*It); }
        if (D) { DefenderStarts.Add(*It); }
    }
    for (TActorIterator<ATriggerBox> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("VS_Site_A")))
        {
            if (Site) { MapError = TEXT("Duplicate site A"); return false; }
            Site = *It;
        }
    }
    for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("VS_DeviceRecovery")))
        {
            if (Recovery) { MapError = TEXT("Duplicate recovery marker"); return false; }
            Recovery = *It;
        }
    }
    if (AttackerStarts.Num() != 2 || DefenderStarts.Num() != 2 || !Site || !Recovery)
    { MapError = TEXT("Site01 requires two spawns per role, site A TriggerBox and recovery TargetPoint"); return false; }
    const auto Sort = [](const APlayerStart& A, const APlayerStart& B) { return A.GetName() < B.GetName(); };
    AttackerStarts.Sort(Sort);
    DefenderStarts.Sort(Sort);
    const UBoxComponent* Box = Cast<UBoxComponent>(Site->GetCollisionComponent());
    if (!Box || Box->GetScaledBoxExtent().GetMin() <= 0)
    { MapError = TEXT("Site A has invalid bounds"); return false; }
    FHitResult Hit;
    const FVector Marker = Recovery->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSMatchMap), false, Recovery);
    Query.AddIgnoredActor(Site);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Marker + FVector(0,0,100), Marker - FVector(0,0,300), ECC_Visibility, Query)
        || Hit.ImpactNormal.Z < 0.9 || !Hit.GetActor())
    { MapError = TEXT("Recovery marker has no level floor"); return false; }
    ArenaFloor = Hit.GetActor();
    FloorBounds = ArenaFloor->GetComponentsBoundingBox(true);
    FloorHeight = Hit.ImpactPoint.Z;
    FVector Ground;
    if (!GroundAt(Marker, Ground)) { MapError = TEXT("Recovery marker has no standing clearance"); return false; }
    for (APlayerStart* Start : AttackerStarts)
    { if (!GroundAt(Start->GetActorLocation(), Ground)) { MapError = TEXT("Attacker spawn unsafe"); return false; } }
    for (APlayerStart* Start : DefenderStarts)
    { if (!GroundAt(Start->GetActorLocation(), Ground)) { MapError = TEXT("Defender spawn unsafe"); return false; } }
    return true;
}

bool AVSMatchGameMode::GroundAt(const FVector& Candidate, FVector& Ground) const
{
    if (!ArenaFloor || Candidate.ContainsNaN() || !FloorBounds.IsValid) { return false; }
    // The authored arena is flat. Only its actual recovery floor is accepted;
    // roofs/cover/unsupported holes are not reachable drop surfaces.
    if (Candidate.X < FloorBounds.Min.X + 100 || Candidate.X > FloorBounds.Max.X - 100 ||
        Candidate.Y < FloorBounds.Min.Y + 100 || Candidate.Y > FloorBounds.Max.Y - 100) { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSMatchPlacement), false);
    if (Objective) { Query.AddIgnoredActor(Objective); }
    if (Site) { Query.AddIgnoredActor(Site); }
    for (TActorIterator<APawn> It(GetWorld()); It; ++It) { Query.AddIgnoredActor(*It); }
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Candidate + FVector(0,0,100), Candidate - FVector(0,0,250), ECC_Visibility, Query)
        || Hit.GetActor() != ArenaFloor || Hit.ImpactNormal.Z < 0.9 || FMath::Abs(Hit.ImpactPoint.Z - FloorHeight) > 5)
    { return false; }
    Ground = Hit.ImpactPoint + FVector(0,0,1);
    return !GetWorld()->OverlapBlockingTestByChannel(Ground + FVector(0,0,90), FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(35, 88), Query);
}

FVector AVSMatchGameMode::GetSiteCenter() const { return Site ? Site->GetActorLocation() : FVector::ZeroVector; }
uint64 AVSMatchGameMode::ConnectionId(const AController* PC) const
{
    const AVSMatchPlayerState* PS = PC ? PC->GetPlayerState<AVSMatchPlayerState>() : nullptr;
    return PS ? PS->ConnectionId : 0;
}
AVSCharacter* AVSMatchGameMode::PawnFor(uint64 Id) const
{
    const FSession* Session = Sessions.Find(Id);
    return Session && Session->Controller.IsValid() ? Cast<AVSCharacter>(Session->Controller->GetPawn()) : nullptr;
}

void AVSMatchGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (UGameplayStatics::HasOption(Options, TEXT("SpectatorOnly"))) { ErrorMessage = TEXT("SpectatorOnly joins are not supported; late join spectating is automatic"); }
    if (Sessions.Num() >= 4) { ErrorMessage = TEXT("Match full (four players)"); }
}
void AVSMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    AVSMatchPlayerController* PC = Cast<AVSMatchPlayerController>(NewPlayer);
    AVSMatchPlayerState* PS = PC ? PC->GetPlayerState<AVSMatchPlayerState>() : nullptr;
    if (!PC || !PS) { return; }
    int32 One = 0, Two = 0;
    for (const auto& Pair : Sessions)
    {
        if (!Pair.Value.bLeaving) { (Pair.Value.Team == R::Team::One ? One : Two)++; }
    }
    if (One >= 2 && Two >= 2)
    {
        if (GameSession) { GameSession->KickPlayer(PC, FText::FromString(TEXT("Match full"))); }
        return;
    }
    PS->ConnectionId = ++NextConnection;
    PS->TeamId = static_cast<uint8>(One <= Two ? R::Team::One : R::Team::Two);
    FSession& Session = Sessions.Add(PS->ConnectionId);
    Session.Controller = PC;
    Session.Team = static_cast<R::Team>(PS->TeamId);
    R::Event Join;
    Join.Kind = R::EventKind::Join;
    Join.PlayerId = PS->ConnectionId;
    Join.TeamId = Session.Team;
    Queue(Join, NowMs());
}
void AVSMatchGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Preparation effects alone create combatants; late joins stay pawn-independent spectators.
    NewPlayer->ChangeState(NAME_Spectating);
    NewPlayer->ClientGotoState(NAME_Spectating);
    if (Recovery) { NewPlayer->ClientSetLocation(Recovery->GetActorLocation() + FVector(0,0,250), FRotator::ZeroRotator); }
}
void AVSMatchGameMode::Logout(AController* Exiting)
{
    const uint64 Id = ConnectionId(Exiting);
    if (FSession* Session = Sessions.Find(Id))
    {
        FlushThrough(NowMs() - 1);
        RememberSafeFloor(Id);
        Session = Sessions.Find(Id);
        if (Session) { Session->bLeaving = true; Session->bHeld = false; Session->HoldEnded = NowMs(); }
        R::Event Event;
        Event.Kind = R::EventKind::Disconnect;
        Event.PlayerId = Id;
        Queue(Event, NowMs());
    }
    Super::Logout(Exiting);
}

AActor* AVSMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const uint64 Id = ConnectionId(Player);
    const R::Player* P = Rules.Read().FindPlayer(Id);
    if (!P) { return Recovery ? static_cast<AActor*>(Recovery.Get()) : Super::ChoosePlayerStart_Implementation(Player); }
    int32 Index = 0;
    for (const R::Player& Other : Rules.Read().Players)
    { if (Other.PlayerId != 0 && Other.PlayerId < Id && Other.TeamId == P->TeamId) { ++Index; } }
    const auto& Starts = P->TeamId == Rules.Read().AttackingTeam ? AttackerStarts : DefenderStarts;
    return Starts.IsValidIndex(Index) ? Starts[Index].Get() : nullptr;
}

bool AVSMatchGameMode::LiveGate(const AVSCharacter* Pawn, bool bForCombat)
{
    if (!bMapReady || bApplying || !IsValid(Pawn)) { return false; }
    FlushThrough(NowMs() - 1);
    if (!IsValid(Pawn) || !Pawn->GetController() || Pawn->GetController()->GetPawn() != Pawn ||
        !Pawn->GetHealthComponent()->IsAlive()) { return false; }
    const uint64 Id = ConnectionId(Pawn->GetController());
    const R::Snapshot& S = Rules.Read();
    return S.CanCombat(Id) && S.PhaseDeadline > NowMs() &&
        (!bForCombat || S.CurrentInteraction.PlayerId != Id);
}
bool AVSMatchGameMode::AllowsCombat(const AVSCharacter* Pawn)
{
    AVSMatchGameMode* Mode = Pawn && Pawn->GetWorld() ? Pawn->GetWorld()->GetAuthGameMode<AVSMatchGameMode>() : nullptr;
    return !Mode || Mode->LiveGate(Pawn, true);
}
bool AVSMatchGameMode::AllowsDamage(const AVSCharacter* Pawn)
{
    AVSMatchGameMode* Mode = Pawn && Pawn->GetWorld() ? Pawn->GetWorld()->GetAuthGameMode<AVSMatchGameMode>() : nullptr;
    return !Mode || Mode->LiveGate(Pawn, false);
}
void AVSMatchGameMode::ReportDeath(AVSCharacter* Pawn)
{
    AVSMatchGameMode* Mode = Pawn && Pawn->GetWorld() ? Pawn->GetWorld()->GetAuthGameMode<AVSMatchGameMode>() : nullptr;
    if (!Mode || Mode->bApplying) { return; }
    const uint64 Id = Mode->ConnectionId(Pawn->GetController());
    Mode->CancelIntent(Id, false);
    Mode->Queue(Mode->MakeEvent(R::EventKind::Death, Id), Mode->NowMs());
}

R::Event AVSMatchGameMode::MakeEvent(R::EventKind Kind, uint64 Id)
{
    R::Event Event;
    Event.Kind = Kind;
    Event.PlayerId = Id;
    Event.RoundId = Rules.Read().RoundId;
    Event.DeviceRevision = Rules.Read().Objective.Revision;
    Event.InteractionToken = Rules.Read().CurrentInteraction.Token;
    Event.Site = R::ObjectiveSite;
    Event.WorldConfirmed = true;
    if (FSession* Session = Sessions.Find(Id)) { Event.RequestId = ++Session->DomainRequest; }
    return Event;
}
void AVSMatchGameMode::Queue(R::Event Event, int64 At)
{
    if (At <= Rules.Read().Now && bHasFrame) { At = Rules.Read().Now + 1; }
    const auto Category = [](R::EventKind Kind)
    {
        if (Kind == R::EventKind::Join || Kind == R::EventKind::Disconnect) { return 0; }
        if (Kind == R::EventKind::Death) { return 1; }
        if (Kind == R::EventKind::ConfirmPlacement || Kind == R::EventKind::RecoverDevice) { return 3; }
        if (Kind == R::EventKind::NextRound || Kind == R::EventKind::Rematch) { return 4; }
        return 2;
    };
    for (FPending& Existing : Pending)
    {
        if (Existing.Time == At && Category(Existing.Event.Kind) == Category(Event.Kind) &&
            (Category(Event.Kind) >= 3 || Existing.Event.PlayerId == Event.PlayerId))
        {
            // Release/death never leave a start queued in the same timestamp.
            const bool AlreadyCanceled = Existing.Event.Kind == R::EventKind::StopInteraction || Existing.Event.Kind == R::EventKind::Drop;
            const bool NewStart = Event.Kind == R::EventKind::StartPlant || Event.Kind == R::EventKind::StartDefuse;
            if (!AlreadyCanceled || !NewStart) { Existing.Event = Event; }
            return;
        }
    }
    Pending.Add({At, Event});
}

void AVSMatchGameMode::CancelIntent(uint64 Id, bool bQueueStop)
{
    FSession* Session = Sessions.Find(Id);
    if (!Session) { return; }
    const bool HadIntent = Session->bHeld;
    Session->bHeld = false;
    Session->HoldEnded = FMath::Min(Session->HoldEnded, NowMs());
    Session->LastCancelAt = NowMs();
    if (HadIntent && Session->Controller.IsValid()) { Session->Controller->ClientCancelObjective(); }
    if (bQueueStop)
    {
        if (Rules.Read().CurrentInteraction.PlayerId == Id)
        { Queue(MakeEvent(R::EventKind::StopInteraction, Id), NowMs()); }
        // A press still awaiting its first sealed frame must also be canceled.
        Pending.RemoveAll([Id, this](const FPending& E)
        { return E.Time >= NowMs() && E.Event.PlayerId == Id &&
            (E.Event.Kind == R::EventKind::StartPlant || E.Event.Kind == R::EventKind::StartDefuse); });
    }
}

bool AVSMatchGameMode::InSite(const AVSCharacter* Pawn) const
{
    const UBoxComponent* Box = Site ? Cast<UBoxComponent>(Site->GetCollisionComponent()) : nullptr;
    if (!Pawn || !Box || Pawn->GetCharacterMovement()->IsFalling()) { return false; }
    const FVector Position = Box->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
    const FVector Extent = Box->GetUnscaledBoxExtent();
    return FMath::Abs(Position.X) <= Extent.X - 35 && FMath::Abs(Position.Y) <= Extent.Y - 35 &&
        FMath::Abs(Position.Z) <= Extent.Z;
}
bool AVSMatchGameMode::NearDevice(const AVSCharacter* Pawn) const
{
    if (!Pawn || !Objective || FVector::DistSquared(Pawn->GetActorLocation(), Objective->GetActorLocation()) > FMath::Square(200.0))
    { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSMatchReach), false, Pawn);
    Query.AddIgnoredActor(Objective);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(),
        Objective->GetActorLocation() + FVector(0,0,8), ECC_Visibility, Query);
}
bool AVSMatchGameMode::InteractionEligible(uint64 Id, R::InteractionKind Kind) const
{
    const FSession* Session = Sessions.Find(Id);
    const AVSCharacter* Pawn = PawnFor(Id);
    if (!Session || Session->bLeaving || !Session->bHeld || !Pawn || !Pawn->GetHealthComponent()->IsAlive()) { return false; }
    const R::Player* P = Rules.Read().FindPlayer(Id);
    if (!P || !P->Participating || !P->Alive) { return false; }
    if (Kind == R::InteractionKind::Plant)
    {
        FVector Ground;
        return P->TeamId == Rules.Read().AttackingTeam && Rules.Read().Objective.Carrier == Id &&
            InSite(Pawn) && GroundAt(Pawn->GetActorLocation(), Ground);
    }
    return P->TeamId == Rules.Read().DefendingTeam() && NearDevice(Pawn);
}

void AVSMatchGameMode::HandleCommand(AVSMatchPlayerController* PC, EVSMatchCommand Command,
    uint32 RequestId, uint64 RoundId, uint64 DeviceRevision)
{
    if (!IsValid(PC) || !PC->HasAuthority()) { return; }
    FlushThrough(NowMs() - 1);
    const uint64 Id = ConnectionId(PC);
    FSession* Session = Sessions.Find(Id);
    if (!Session || Session->Controller.Get() != PC || Session->bLeaving) { ++RejectedCommands; return; }
    const uint32 Delta = RequestId - Session->LastClientRequest;
    if (RequestId == 0 || Delta == 0 || Delta > 1024) { ++RejectedCommands; return; }
    Session->LastClientRequest = RequestId;
    if (Command == EVSMatchCommand::Release) { CancelIntent(Id, true); return; }
    const R::Snapshot& S = Rules.Read();
    const R::Player* P = S.FindPlayer(Id);
    if (!bMapReady || RoundId == 0 || RoundId != S.RoundId || !P) { ++RejectedCommands; PC->ClientCancelObjective(); return; }
    if (Command == EVSMatchCommand::Rematch)
    {
        if (S.CurrentPhase == R::Phase::MatchComplete && P->Participating && !bRematchQueued)
        { bRematchQueued = true; Queue(MakeEvent(R::EventKind::Rematch), NowMs()); }
        else { ++RejectedCommands; }
        return;
    }
    AVSCharacter* Pawn = PawnFor(Id);
    if (!Pawn || !P->Alive || !P->Participating || !S.CanMove(Id) || S.PhaseDeadline <= NowMs() || DeviceRevision != S.Objective.Revision)
    { ++RejectedCommands; CancelIntent(Id, true); PC->ClientCancelObjective(); return; }
    if (Command == EVSMatchCommand::Drop)
    {
        CancelIntent(Id, true);
        Queue(MakeEvent(R::EventKind::Drop, Id), NowMs());
        return;
    }
    if (Command != EVSMatchCommand::Press || Session->bHeld || Session->LastCancelAt == NowMs())
    { ++RejectedCommands; PC->ClientCancelObjective(); return; }
    if (S.Objective.State == R::DeviceState::Dropped && P->TeamId == S.AttackingTeam && NearDevice(Pawn))
    {
        Queue(MakeEvent(R::EventKind::Retrieve, Id), NowMs());
        PC->ClientCancelObjective();
        return;
    }
    Session->bHeld = true;
    Session->HoldBegan = NowMs();
    Session->HoldEnded = MAX_int64;
    const R::InteractionKind Kind = S.CurrentPhase == R::Phase::Active ? R::InteractionKind::Plant : R::InteractionKind::Defuse;
    Session->IntentKind = Kind;
    if (!InteractionEligible(Id, Kind)) { ++RejectedCommands; CancelIntent(Id, true); return; }
    if (Kind == R::InteractionKind::Plant) { GroundAt(Pawn->GetActorLocation(), Session->PlantFloor); }
    Queue(MakeEvent(Kind == R::InteractionKind::Plant ? R::EventKind::StartPlant : R::EventKind::StartDefuse, Id), NowMs());
}

void AVSMatchGameMode::RememberSafeFloor(uint64 Id)
{
    FSession* Session = Sessions.Find(Id);
    const AVSCharacter* Pawn = PawnFor(Id);
    FVector Ground;
    if (!Session || !Pawn || Pawn->GetCharacterMovement()->IsFalling() || !GroundAt(Pawn->GetActorLocation(), Ground)) { return; }
    if (Session->bHasSafeFloor)
    {
        // Preserve a connected walkable trajectory from the validated role spawn.
        // A teleport across a solid wall cannot turn an isolated floor pocket into a safe drop.
        FCollisionQueryParams Query(SCENE_QUERY_STAT(VSMatchFloorPath), false, Pawn);
        for (TActorIterator<APawn> It(GetWorld()); It; ++It) { Query.AddIgnoredActor(*It); }
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit, Session->SafeFloor + FVector(0,0,90), Ground + FVector(0,0,90),
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34, 88), Query)) { return; }
    }
    Session->SafeFloor = Ground;
    Session->bHasSafeFloor = true;
}
void AVSMatchGameMode::AuditEligibility()
{
    for (const auto& Pair : Sessions) { RememberSafeFloor(Pair.Key); }
    // Include presses whose Start event has not yet reached a sealed timestamp.
    for (auto& Pair : Sessions)
    {
        FSession& Session = Pair.Value;
        if (!Session.bHeld) { continue; }
        if (!InteractionEligible(Pair.Key, Session.IntentKind)) { CancelIntent(Pair.Key, true); }
        else if (Session.IntentKind == R::InteractionKind::Plant)
        { GroundAt(PawnFor(Pair.Key)->GetActorLocation(), Session.PlantFloor); }
    }
}
void AVSMatchGameMode::OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
    if (!bApplying && bMapReady) { AuditEligibility(); }
}

void AVSMatchGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bMapReady) { return; }
    AuditEligibility();
    const int64 Now = NowMs();
    if (NextRoundAt != 0 && Now >= NextRoundAt && Rules.Read().CurrentPhase == R::Phase::Resolved)
    { NextRoundAt = 0; Queue(MakeEvent(R::EventKind::NextRound), Now); }
    // Keep the current millisecond open for subsequent damage/RPC/movement callbacks.
    FlushThrough(Now - 1);
    LastAuditAt = Now;
    if (Objective && Rules.Read().Objective.State == R::DeviceState::Carried)
    {
        if (const AVSCharacter* Carrier = PawnFor(Rules.Read().Objective.Carrier))
        { Objective->SetActorLocation(Carrier->GetActorLocation() + FVector(0,0,25)); }
    }
    if (Objective && Rules.Read().Objective.State == R::DeviceState::Dropped)
    {
        FVector Ground;
        if (!GroundAt(Objective->GetActorLocation(), Ground))
        { Queue(MakeEvent(R::EventKind::RecoverDevice), Now); }
    }
    Publish();
}

void AVSMatchGameMode::FlushThrough(int64 Through)
{
    if (!bMapReady || bApplying || Through < 0) { return; }
    while (true)
    {
        int64 Earliest = MAX_int64;
        for (const FPending& Item : Pending) { Earliest = FMath::Min(Earliest, Item.Time); }
        if (Earliest > Through) { break; }
        TArray<R::Event> Events;
        for (int32 I = Pending.Num() - 1; I >= 0; --I)
        {
            if (Pending[I].Time == Earliest) { Events.Add(Pending[I].Event); Pending.RemoveAt(I); }
        }
        ApplyFrame(Earliest, Events);
    }
    if (!bHasFrame || Through > Rules.Read().Now) { ApplyFrame(Through, {}); }
}
void AVSMatchGameMode::ApplyFrame(int64 At, const TArray<R::Event>& Events)
{
    if (bHasFrame && At <= Rules.Read().Now) { return; }
    R::Frame Frame;
    Frame.Now = At;
    Frame.Sequence = ++Sequence;
    const R::Interaction Current = Rules.Read().CurrentInteraction;
    // No retroactive eligibility certificate across an unobserved >100ms world gap.
    const FSession* Held = Sessions.Find(Current.PlayerId);
    if (Current.Kind != R::InteractionKind::None && NowMs() - LastAuditAt <= 100 && Held &&
        Held->HoldBegan <= Current.StartedAt && Held->HoldEnded >= At)
    { Frame.ContinuousInteractionToken = Current.Token; }
    for (R::Event Event : Events)
    {
        if (Event.Kind == R::EventKind::StartPlant || Event.Kind == R::EventKind::StartDefuse)
        {
            const FSession* Intent = Sessions.Find(Event.PlayerId);
            Event.WorldConfirmed = Event.WorldConfirmed && Intent && Intent->HoldBegan <= At && Intent->HoldEnded > At;
        }
        if (Frame.EventCount >= R::MaxEvents) { UE_LOG(LogValStrike, Error, TEXT("VSMATCH event capacity exceeded")); break; }
        Frame.Events[Frame.EventCount++] = Event;
    }
    TGuardValue<bool> Guard(bApplying, true);
    const R::StepResult Result = Rules.Apply(Frame);
    if (Result.Status != R::FrameStatus::Applied)
    { UE_LOG(LogValStrike, Error, TEXT("VSMATCH rejected sealed batch %d"), static_cast<int32>(Result.Status)); return; }
    bHasFrame = true;
    ApplyEffects(Result);
    Publish();
    for (auto It = Sessions.CreateIterator(); It; ++It)
    { if (It->Value.bLeaving && !Rules.Read().FindPlayer(It->Key)) { It.RemoveCurrent(); } }
}

void AVSMatchGameMode::ApplyEffects(const R::StepResult& Result)
{
    for (const R::Effect& Effect : Result.Effects)
    {
        if (Effect.EffectId <= LastEffect) { continue; }
        LastEffect = Effect.EffectId;
        UE_LOG(LogValStrike, Display, TEXT("VSMATCH effect=%llu kind=%d at=%lld round=%llu winner=%d reason=%d"),
            Effect.EffectId, static_cast<int32>(Effect.Kind), Effect.At, Effect.RoundId, static_cast<int32>(Effect.TeamId), static_cast<int32>(Effect.Reason));
        switch (Effect.Kind)
        {
        case R::EffectKind::PrepareRound:
            bRematchQueued = false; NextRoundAt = 0; ResetPawns(); break;
        case R::EffectKind::WaitingForRoster:
        case R::EffectKind::MatchCleared:
            NextRoundAt = 0; bRematchQueued = false; ClearPawns(); break;
        case R::EffectKind::RoundAwarded:
            NextRoundAt = Rules.Read().CurrentPhase == R::Phase::Resolved ? Effect.At + static_cast<int64>(ResultDisplaySeconds * 1000) : 0;
            for (const auto& Pair : Sessions) { CancelIntent(Pair.Key, false); }
            break;
        case R::EffectKind::DropPlacementRequested:
        case R::EffectKind::RecoveryPlacementRequested:
            PlaceDevice(Effect); break;
        case R::EffectKind::DevicePlanted:
        {
            FVector Ground;
            const FSession* Session = Sessions.Find(Effect.PlayerId);
            if (Objective && Session && GroundAt(Session->PlantFloor, Ground)) { Objective->SetActorLocation(Ground); }
            CancelIntent(Effect.PlayerId, false);
            break;
        }
        case R::EffectKind::DeviceDefused:
        case R::EffectKind::InteractionCanceled:
            CancelIntent(Effect.PlayerId, false); break;
        default: break;
        }
    }
}
void AVSMatchGameMode::ClearPawns()
{
    for (const auto& Pair : Sessions)
    {
        CancelIntent(Pair.Key, false);
        if (AVSMatchPlayerController* PC = Pair.Value.Controller.Get())
        {
            if (APawn* Old = PC->GetPawn()) { PC->UnPossess(); Old->Destroy(); }
            PC->ChangeState(NAME_Spectating); PC->ClientGotoState(NAME_Spectating);
        }
    }
    if (Objective) { Objective->SetActorHiddenInGame(true); }
}
void AVSMatchGameMode::ResetPawns()
{
    ClearPawns();
    for (const R::Player& P : Rules.Read().Players)
    {
        FSession* Session = Sessions.Find(P.PlayerId);
        if (!P.Participating || !Session || !Session->Controller.IsValid() || Session->bLeaving) { continue; }
        AVSMatchPlayerController* PC = Session->Controller.Get();
        AActor* Start = ChoosePlayerStart(PC);
        if (!Start) { continue; }
        PC->ChangeState(NAME_Playing); PC->ClientGotoState(NAME_Playing);
        PC->GetPlayerState<APlayerState>()->SetIsSpectator(false);
        RestartPlayerAtTransform(PC, Start->GetActorTransform());
        if (AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn()))
        {
            Pawn->SetMatchGates(false, false);
            Pawn->OnCharacterMovementUpdated.AddDynamic(this, &AVSMatchGameMode::OnMovementUpdated);
            FVector Ground;
            Session->bHasSafeFloor = GroundAt(Start->GetActorLocation(), Ground);
            Session->SafeFloor = Ground;
        }
        else { UE_LOG(LogValStrike, Error, TEXT("VSMATCH fresh pawn spawn failed for %llu"), P.PlayerId); }
    }
}
void AVSMatchGameMode::PlaceDevice(const R::Effect& Effect)
{
    const R::Snapshot& S = Rules.Read();
    if (!Objective || Effect.RoundId != S.RoundId || Effect.DeviceRevision != S.Objective.Revision ||
        (S.CurrentPhase != R::Phase::Preparation && S.CurrentPhase != R::Phase::Active)) { return; }
    FVector Ground;
    const FSession* Session = Sessions.Find(Effect.PlayerId);
    bool Placed = Effect.Kind == R::EffectKind::DropPlacementRequested && Session && Session->bHasSafeFloor &&
        GroundAt(Session->SafeFloor, Ground);
    if (!Placed && Recovery) { Placed = GroundAt(Recovery->GetActorLocation(), Ground); }
    if (!Placed) { UE_LOG(LogValStrike, Error, TEXT("VSMATCH placement failed; objective remains pending")); return; }
    Objective->SetActorLocation(Ground);
    Objective->SetActorHiddenInGame(false);
    Objective->ForceNetUpdate();
    Queue(MakeEvent(R::EventKind::ConfirmPlacement), FMath::Max(NowMs(), Effect.At + 1));
}

void AVSMatchGameMode::Publish()
{
    AVSMatchGameState* GS = GetGameState<AVSMatchGameState>();
    if (!GS) { return; }
    const R::Snapshot& S = Rules.Read();
    FVSMatchRepState& M = GS->Match;
    M.bReady = bMapReady;
    M.StatusText = MapError;
    M.Phase = static_cast<uint8>(S.CurrentPhase);
    M.AttackingTeam = static_cast<uint8>(S.AttackingTeam);
    M.Winner = static_cast<uint8>(S.RoundWinner);
    M.Result = static_cast<uint8>(S.Result);
    M.ScoreOne = S.Scores[0]; M.ScoreTwo = S.Scores[1]; M.CompletedRounds = S.CompletedRounds;
    M.RoundId = S.RoundId; M.DeviceRevision = S.Objective.Revision;
    M.ObjectiveState = static_cast<uint8>(S.Objective.State); M.CarrierId = S.Objective.Carrier;
    M.PhaseDeadlineSeconds = S.PhaseDeadline / 1000.0;
    M.InteractionToken = S.CurrentInteraction.Token; M.InteractionPlayerId = S.CurrentInteraction.PlayerId;
    M.InteractionStartedSeconds = S.CurrentInteraction.StartedAt / 1000.0;
    M.InteractionCompletesSeconds = S.CurrentInteraction.CompletesAt / 1000.0;
    GS->ForceNetUpdate();
    if (Objective)
    {
        Objective->SetObjectiveState(M.ObjectiveState);
        Objective->SetActorHiddenInGame(S.Objective.State == R::DeviceState::Unavailable || S.Objective.State == R::DeviceState::PlacementPending);
    }
    for (const auto& Pair : Sessions)
    {
        AVSMatchPlayerController* PC = Pair.Value.Controller.Get();
        if (!PC) { continue; }
        AVSMatchPlayerState* PS = PC->GetPlayerState<AVSMatchPlayerState>();
        const R::Player* P = S.FindPlayer(Pair.Key);
        if (!PS) { continue; }
        PS->bParticipating = P && P->Participating; PS->bAlive = P && P->Alive;
        PS->InteractionPrompt.Reset();
        AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn());
        const bool CanPlay = bMapReady && S.CanMove(Pair.Key) && S.PhaseDeadline > NowMs();
        if (Pawn) { Pawn->SetMatchGates(CanPlay, CanPlay && S.CurrentInteraction.PlayerId != Pair.Key); }
        if (CanPlay && Pawn)
        {
            FVector Ground;
            if (S.Objective.Carrier == Pair.Key && InSite(Pawn) && GroundAt(Pawn->GetActorLocation(), Ground)) { PS->InteractionPrompt = TEXT("Hold E to plant | G to drop"); }
            else if (S.Objective.Carrier == Pair.Key) { PS->InteractionPrompt = TEXT("Carry device to A | G to drop"); }
            else if (S.Objective.State == R::DeviceState::Dropped && P->TeamId == S.AttackingTeam && NearDevice(Pawn))
            { PS->InteractionPrompt = TEXT("E to retrieve device"); }
            else if (S.CurrentPhase == R::Phase::Planted && P->TeamId == S.DefendingTeam() && NearDevice(Pawn))
            { PS->InteractionPrompt = TEXT("Hold E to defuse"); }
        }
        PS->ForceNetUpdate();
    }
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool AVSMatchGameMode::DevelopmentScheduleInteraction(AVSMatchPlayerController* PC, int64 At, bool bDefuse)
{
    FString Role;
    if (!FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) || Role != TEXT("server") ||
        !PC || At <= Rules.Read().Now || At > NowMs()) { return false; }
    const uint64 Id = ConnectionId(PC);
    FSession* Session = Sessions.Find(Id);
    if (!Session || Session->bLeaving || Session->bHeld) { return false; }
    Session->bHeld = true;
    Session->HoldBegan = At; Session->HoldEnded = MAX_int64;
    Session->IntentKind = bDefuse ? R::InteractionKind::Defuse : R::InteractionKind::Plant;
    if (!InteractionEligible(Id, Session->IntentKind)) { CancelIntent(Id, false); return false; }
    if (!bDefuse) { GroundAt(PawnFor(Id)->GetActorLocation(), Session->PlantFloor); }
    Queue(MakeEvent(bDefuse ? R::EventKind::StartDefuse : R::EventKind::StartPlant, Id), At);
    return true;
}

void AVSMatchGameMode::DevelopmentSealAt(int64 Time)
{
    FString Role;
    if (FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) && Role == TEXT("server") && Time <= NowMs())
    { FlushThrough(Time); }
}
#endif
