#include "VSMatchStatsSubsystem.h"

#include "VSScoreboardState.h"
#include "../Combat/VSHealthComponent.h"
#include "../Damage/VSDamageSubsystem.h"
#include "../Match/VSMatchGameState.h"
#include "../Match/VSMatchPlayerState.h"
#include "../Rules/VSMatchRules.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

namespace
{
namespace VSStats = ValStrike::Stats;

uint8 VSMatchStatsTeamOf(const APlayerState* Player)
{
    const AVSMatchPlayerState* MatchPlayer = Cast<AVSMatchPlayerState>(Player);
    return MatchPlayer ? MatchPlayer->TeamId : 0;
}

EVSScoreboardLife VSMatchStatsLifeOf(const APlayerState* Player)
{
    const AVSCharacter* Pawn = Player ? Cast<AVSCharacter>(Player->GetPawn()) : nullptr;
    if (!Pawn || !Pawn->GetHealthComponent())
    {
        return EVSScoreboardLife::Spectating;
    }
    return Pawn->GetHealthComponent()->IsAlive() ? EVSScoreboardLife::Alive : EVSScoreboardLife::Dead;
}
}

UVSMatchStatsSubsystem* UVSMatchStatsSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UVSMatchStatsSubsystem>() : nullptr;
}

bool UVSMatchStatsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UVSMatchStatsSubsystem::IsServerWorld() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_Client;
}

void UVSMatchStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UVSDamageSubsystem* Damage = Collection.InitializeDependency<UVSDamageSubsystem>();
    if (Damage)
    {
        DamageHandle = Damage->OnDamage().AddUObject(this, &UVSMatchStatsSubsystem::HandleDamage);
    }
    PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UVSMatchStatsSubsystem::HandlePostLogin);
    LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UVSMatchStatsSubsystem::HandleLogout);
    if (UWorld* World = GetWorld())
    {
        SpawnHandle = World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateUObject(this, &UVSMatchStatsSubsystem::HandleActorSpawned));
    }
}

void UVSMatchStatsSubsystem::Deinitialize()
{
    FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
    FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
    if (UWorld* World = GetWorld())
    {
        World->RemoveOnActorSpawnedHandler(SpawnHandle);
        World->GetTimerManager().ClearTimer(SyncTimer);
        if (UVSDamageSubsystem* Damage = World->GetSubsystem<UVSDamageSubsystem>())
        {
            Damage->OnDamage().Remove(DamageHandle);
        }
    }
    for (const TWeakObjectPtr<AVSCharacter>& Pawn : BoundPawns)
    {
        if (Pawn.IsValid() && Pawn->GetHealthComponent())
        {
            Pawn->GetHealthComponent()->OnLifeStateChanged.RemoveAll(this);
        }
    }
    BoundPawns.Reset();
    Players.Reset();
    LifePawns.Reset();
    State = nullptr;
    Super::Deinitialize();
}

void UVSMatchStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (!IsServerWorld())
    {
        return;
    }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    State = InWorld.SpawnActor<AVSScoreboardState>(Params);
    if (!State)
    {
        UE_LOG(LogValStrike, Error, TEXT("VSSTATS scoreboard state actor could not spawn"));
        return;
    }
    for (FConstPlayerControllerIterator It = InWorld.GetPlayerControllerIterator(); It; ++It)
    {
        AddPlayer(It->Get());
    }
    for (TActorIterator<AVSCharacter> It(&InWorld); It; ++It)
    {
        BindPawn(*It);
    }
    InWorld.GetTimerManager().SetTimer(SyncTimer, this, &UVSMatchStatsSubsystem::Sync, 0.25f, true);
    Sync();
}

int32 UVSMatchStatsSubsystem::FindKey(const APlayerState* Player) const
{
    if (!Player) { return 0; }
    for (const TPair<int32, TWeakObjectPtr<APlayerState>>& Pair : Players)
    {
        if (Pair.Value.Get() == Player) { return Pair.Key; }
    }
    return 0;
}

void UVSMatchStatsSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
    if (GameMode && GameMode->GetWorld() == GetWorld() && IsServerWorld())
    {
        AddPlayer(NewPlayer);
        Publish();
    }
}

void UVSMatchStatsSubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
    if (!GameMode || GameMode->GetWorld() != GetWorld() || !Exiting)
    {
        return;
    }
    const int32 Key = FindKey(Exiting->PlayerState);
    if (Key != 0)
    {
        Players.Remove(Key);
        LifePawns.Remove(Key);
        Ledger.RemovePlayer(Key);
        Publish();
    }
}

void UVSMatchStatsSubsystem::AddPlayer(APlayerController* Controller)
{
    APlayerState* Player = Controller ? Controller->PlayerState.Get() : nullptr;
    if (!Player || FindKey(Player) != 0 || Player->IsABot())
    {
        return;
    }
    const int32 Key = ++NextKey;
    if (!Ledger.AddPlayer(Key, VSMatchStatsTeamOf(Player)))
    {
        UE_LOG(LogValStrike, Warning, TEXT("VSSTATS could not track %s"), *Player->GetPlayerName());
        return;
    }
    Players.Add(Key, Player);
}

void UVSMatchStatsSubsystem::HandleActorSpawned(AActor* Actor)
{
    if (IsServerWorld())
    {
        BindPawn(Cast<AVSCharacter>(Actor));
    }
}

void UVSMatchStatsSubsystem::BindPawn(AVSCharacter* Pawn)
{
    if (!IsValid(Pawn) || !Pawn->GetHealthComponent() || BoundPawns.Contains(Pawn))
    {
        return;
    }
    BoundPawns.Add(Pawn);
    Pawn->GetHealthComponent()->OnLifeStateChanged.AddUObject(
        this, &UVSMatchStatsSubsystem::HandleLifeStateChanged, TWeakObjectPtr<AVSCharacter>(Pawn));
}

void UVSMatchStatsSubsystem::SyncLife(int32 Key, APawn* Pawn)
{
    if (Key == 0 || !Pawn)
    {
        return;
    }
    TWeakObjectPtr<APawn>& Known = LifePawns.FindOrAdd(Key);
    if (Known.Get() != Pawn)
    {
        Known = Pawn;
        Ledger.BeginLife(Key);
    }
}

void UVSMatchStatsSubsystem::HandleLifeStateChanged(TWeakObjectPtr<AVSCharacter> WeakPawn)
{
    AVSCharacter* Pawn = WeakPawn.Get();
    if (!Pawn || !Pawn->GetHealthComponent() || !IsServerWorld())
    {
        return;
    }
    const int32 Key = FindKey(Pawn->GetPlayerState());
    if (Key == 0)
    {
        return;
    }
    SyncLife(Key, Pawn);
    const VSStats::PlayerLine* Line = Ledger.Find(Key);
    if (!Pawn->GetHealthComponent()->IsAlive())
    {
        // Counted here for every death, attributed or not; the kill hook only adds credit.
        if (Ledger.RecordDeath(Key))
        {
            UE_LOG(LogValStrike, Log, TEXT("VSSTATS death player=%s"), *Pawn->GetPlayerState()->GetPlayerName());
        }
    }
    else if (Line && !Line->Alive && LifePawns.FindRef(Key).Get() == Pawn)
    {
        // Same pawn revived (development reset): a new life for assist bookkeeping.
        Ledger.BeginLife(Key);
    }
    Publish();
}

void UVSMatchStatsSubsystem::HandleDamage(const FVSDamageEvent& Event)
{
    if (!IsServerWorld() || !Event.Victim)
    {
        return;
    }
    const int32 VictimKey = FindKey(Event.Victim->GetPlayerState());
    if (VictimKey == 0)
    {
        return;
    }
    const APlayerState* AttackerState = Event.Instigator ? Event.Instigator->PlayerState.Get()
        : (Event.InstigatorPawn ? Event.InstigatorPawn->GetPlayerState() : nullptr);
    SyncLife(VictimKey, Event.Victim);
    VSStats::HitReport Report;
    Report.Attacker = FindKey(AttackerState);
    Report.Victim = VictimKey;
    Report.AppliedDamage = Event.AppliedDamage;
    Report.Headshot = Event.IsHeadshot();
    Report.Killed = Event.bKilled;
    const VSStats::HitOutcome Outcome = Ledger.RecordHit(Report);
    if (Outcome == VSStats::HitOutcome::Kill)
    {
        UE_LOG(LogValStrike, Log, TEXT("VSSTATS kill killer=%s victim=%s headshot=%d"),
            AttackerState ? *AttackerState->GetPlayerName() : TEXT("?"),
            *Event.Victim->GetPlayerState()->GetPlayerName(), Report.Headshot ? 1 : 0);
    }
    Publish();
}

void UVSMatchStatsSubsystem::ObserveMatchReset()
{
    const AVSMatchGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AVSMatchGameState>() : nullptr;
    if (!GameState)
    {
        return;
    }
    const uint8 Phase = GameState->Match.Phase;
    const int32 Completed = GameState->Match.CompletedRounds;
    const uint8 Complete = static_cast<uint8>(ValStrike::Rules::Phase::MatchComplete);
    // A rematch leaves MatchComplete; a match cleared by the rules drops the round count.
    if ((LastPhase == Complete && Phase != Complete) || Completed < LastCompletedRounds)
    {
        Ledger.ResetStats();
        UE_LOG(LogValStrike, Log, TEXT("VSSTATS new match: counters reset"));
    }
    LastPhase = Phase;
    LastCompletedRounds = Completed;
}

void UVSMatchStatsSubsystem::Sync()
{
    for (const TPair<int32, TWeakObjectPtr<APlayerState>>& Pair : Players)
    {
        const APlayerState* Player = Pair.Value.Get();
        if (!Player) { continue; }
        Ledger.SetTeam(Pair.Key, VSMatchStatsTeamOf(Player));
        if (AVSCharacter* Pawn = Cast<AVSCharacter>(Player->GetPawn()))
        {
            BindPawn(Pawn);
            SyncLife(Pair.Key, Pawn);
        }
    }
    ObserveMatchReset();
    Publish();
}

void UVSMatchStatsSubsystem::Publish()
{
    if (!State)
    {
        return;
    }
    TArray<FVSScoreboardRow> Rows;
    for (const VSStats::PlayerKey Key : Ledger.ScoreboardOrder())
    {
        const VSStats::PlayerLine* Line = Ledger.Find(Key);
        APlayerState* Player = Players.FindRef(Key).Get();
        if (!Line || !Player) { continue; }
        FVSScoreboardRow& Row = Rows.AddDefaulted_GetRef();
        Row.PlayerState = Player;
        Row.PlayerKey = Key;
        Row.PlayerName = Player->GetPlayerName();
        Row.TeamId = Line->Team;
        Row.Life = VSMatchStatsLifeOf(Player);
        Row.Kills = Line->Kills;
        Row.Deaths = Line->Deaths;
        Row.Assists = Line->Assists;
        Row.HeadshotKills = Line->HeadshotKills;
        Row.DamageDealt = static_cast<int32>(FMath::Clamp(FMath::RoundToDouble(Line->DamageDealt), 0.0, 1.0e9));
    }
    State->ServerPublish(Rows);
}
