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
namespace VSFeed = ValStrike::KillFeed;

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
    for (const TPair<int32, TWeakObjectPtr<AController>>& Pair : Controllers)
    {
        if (Pair.Value.IsValid()) { Pair.Value->GetOnNewPawnNotifier().RemoveAll(this); }
    }
    Controllers.Reset();
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
        Exiting->GetOnNewPawnNotifier().RemoveAll(this);
        Controllers.Remove(Key);
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
    Controllers.Add(Key, Controller);
    Controller->GetOnNewPawnNotifier().AddUObject(this, &UVSMatchStatsSubsystem::HandleNewPawn, Key);
}

void UVSMatchStatsSubsystem::HandleNewPawn(APawn* NewPawn, int32 Key)
{
    if (!IsServerWorld() || !Players.Contains(Key))
    {
        return;
    }
    if (AVSCharacter* Pawn = Cast<AVSCharacter>(NewPawn))
    {
        BindPawn(Pawn);
        SyncLife(Key, Pawn);
    }
    Publish();
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
    AppendKillFeed(Event, AttackerState, Report.Attacker, VictimKey, Outcome);
    Publish();
}

void UVSMatchStatsSubsystem::AppendKillFeed(const FVSDamageEvent& Event, const APlayerState* Attacker,
    int32 AttackerKey, int32 VictimKey, VSStats::HitOutcome Outcome)
{
    VSFeed::Kind FeedKind = VSFeed::Kind::World;
    // Only an outcome that resolved this life's death: exactly one entry per death.
    if (!VSFeed::KindFromOutcome(Outcome, FeedKind))
    {
        return;
    }
    // A kill landing before the 0.25 s poll saw the new round must not be cleared by it.
    ObserveFeedRound();
    const APlayerState* Victim = Event.Victim ? Event.Victim->GetPlayerState() : nullptr;
    FVSKillFeedEntry Entry;
    Entry.KillerKey = FeedKind == VSFeed::Kind::World ? 0 : AttackerKey;
    Entry.VictimKey = VictimKey;
    // An untracked source (a bot) keeps its name; no source at all leaves it empty.
    Entry.KillerName = Attacker ? Attacker->GetPlayerName() : FString();
    Entry.VictimName = Victim ? Victim->GetPlayerName() : FString();
    Entry.KillerTeam = VSMatchStatsTeamOf(Attacker);
    Entry.VictimTeam = VSMatchStatsTeamOf(Victim);
    Entry.Kind = static_cast<uint8>(FeedKind);
    Entry.Weapon = static_cast<uint8>(Event.Weapon == ValStrike::Damage::WeaponClass::Pistol ? VSFeed::Weapon::Pistol
        : Event.Weapon == ValStrike::Damage::WeaponClass::Rifle ? VSFeed::Weapon::Rifle : VSFeed::Weapon::Unknown);
    Entry.bHeadshot = Event.IsHeadshot();
    const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
    const VSFeed::Seq Sequence = KillFeed.Append(Now, Entry);
    UE_LOG(LogValStrike, Log, TEXT("VSFEED seq=%u kind=%u killer=%s victim=%s weapon=%u headshot=%d"),
        Sequence, static_cast<uint32>(Entry.Kind), *Entry.KillerName, *Entry.VictimName,
        static_cast<uint32>(Entry.Weapon), Entry.bHeadshot ? 1 : 0);
    PublishKillFeed();
}

bool UVSMatchStatsSubsystem::ObserveFeedRound()
{
    const AVSMatchGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AVSMatchGameState>() : nullptr;
    if (!GameState || GameState->Match.RoundId == LastFeedRoundId)
    {
        return false;
    }
    LastFeedRoundId = GameState->Match.RoundId;
    if (KillFeed.Clear())
    {
        UE_LOG(LogValStrike, Log, TEXT("VSFEED round %llu: feed cleared"), static_cast<unsigned long long>(LastFeedRoundId));
        PublishKillFeed();
    }
    return true;
}

void UVSMatchStatsSubsystem::PublishKillFeed()
{
    if (!State)
    {
        return;
    }
    TArray<FVSKillFeedEntry> Lines;
    Lines.Reserve(static_cast<int32>(KillFeed.Items().size()));
    for (const VSFeed::Ring<FVSKillFeedEntry>::Item& Item : KillFeed.Items())
    {
        FVSKillFeedEntry& Line = Lines.Add_GetRef(Item.Data);
        Line.Sequence = static_cast<int32>(Item.Number);
        Line.ServerTimeSeconds = Item.TimeSeconds;
    }
    State->ServerPublishKillFeed(Lines);
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
    ObserveFeedRound();
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
