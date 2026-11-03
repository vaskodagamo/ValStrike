#include "VSEconomySubsystem.h"

#include "VSEconomyWallet.h"
#include "Tests/VSEconomySmoke.h"
#include "../Combat/VSHealthComponent.h"
#include "../Damage/VSDamageEvents.h"
#include "../Damage/VSDamageSubsystem.h"
#include "../Match/VSMatchGameState.h"
#include "../Match/VSMatchPlayerState.h"
#include "../Rules/VSMatchRules.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

namespace
{
// Unique file-local names: the unity build shares one translation unit across the module.
namespace VSEconomyServer
{
using EconomyStatus = ValStrike::Economy::Status;
using EconomyEvent = ValStrike::Economy::Event;
using EconomyKind = ValStrike::Economy::EventKind;
using EconomyTeam = ValStrike::Economy::Team;
using EconomyPhase = ValStrike::Economy::Phase;
using MatchPhase = ValStrike::Rules::Phase;
using MatchDevice = ValStrike::Rules::DeviceState;

static_assert(static_cast<uint8>(EconomyTeam::One) == static_cast<uint8>(ValStrike::Rules::Team::One));
static_assert(static_cast<uint8>(EconomyTeam::Two) == static_cast<uint8>(ValStrike::Rules::Team::Two));

bool IsPlayingTeam(uint8 TeamId)
{
    return TeamId == static_cast<uint8>(EconomyTeam::One) || TeamId == static_cast<uint8>(EconomyTeam::Two);
}

FString StatusText(EconomyStatus Value)
{
    // Copy: the conversion buffer only lives for the full expression.
    return FString(UTF8_TO_TCHAR(ValStrike::Economy::StatusName(Value)));
}
}
}

UVSEconomySubsystem::UVSEconomySubsystem() = default;

bool UVSEconomySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVSEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UVSEconomySubsystem::HandlePostLogin);
    LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &UVSEconomySubsystem::HandleLogout);
    // Attributed kills come from the damage service; it is initialized first and outlives us.
    if (UVSDamageSubsystem* Damage = Collection.InitializeDependency<UVSDamageSubsystem>())
    {
        DamageSource = Damage;
        KillHandle = Damage->OnKill().AddUObject(this, &UVSEconomySubsystem::HandleKill);
    }
    ValStrikeEconomyTests::StartEconomySmoke();
}

void UVSEconomySubsystem::Deinitialize()
{
    FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
    FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
    if (UVSDamageSubsystem* Damage = DamageSource.Get()) { Damage->OnKill().Remove(KillHandle); }
    DamageSource.Reset();
    Wallets.Reset();
    Super::Deinitialize();
}

TStatId UVSEconomySubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVSEconomySubsystem, STATGROUP_Tickables);
}

const AVSMatchGameState* UVSEconomySubsystem::MatchState() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_Client ? World->GetGameState<AVSMatchGameState>() : nullptr;
}

bool UVSEconomySubsystem::IsEconomyActive() const
{
    return MatchState() != nullptr && Model.Read().TuningValid;
}

int64 UVSEconomySubsystem::ServerNowMs() const
{
    // Same clock and rounding as the match game mode's sealed model time.
    const UWorld* World = GetWorld();
    return World ? FMath::Max<int64>(0, static_cast<int64>(FMath::FloorToDouble(World->GetTimeSeconds() * 1000.0))) : 0;
}

AVSEconomyWallet* UVSEconomySubsystem::FindWallet(const AController* Controller) const
{
    if (!Controller) { return nullptr; }
    for (AVSEconomyWallet* Wallet : Wallets)
    {
        if (IsValid(Wallet) && Wallet->GetOwner() == Controller) { return Wallet; }
    }
    return nullptr;
}

FVSEconomyLoadout UVSEconomySubsystem::GetLoadout(const AController* Controller) const
{
    FVSEconomyLoadout Loadout;
    const AVSEconomyWallet* Wallet = FindWallet(Controller);
    const ValStrike::Economy::Wallet* Entry = Wallet ? Model.Read().Find(Wallet->GetView().PlayerId) : nullptr;
    Loadout.OwnedMask = Entry ? Entry->Owned : 0;
    return Loadout;
}

bool UVSEconomySubsystem::IsAliveOnServer(const APlayerController* Controller) const
{
    const AVSMatchPlayerState* PS = Controller ? Controller->GetPlayerState<AVSMatchPlayerState>() : nullptr;
    const AVSCharacter* Pawn = Controller ? Cast<AVSCharacter>(Controller->GetPawn()) : nullptr;
    return PS && PS->bParticipating && PS->bAlive && Pawn && Pawn->GetHealthComponent() &&
        Pawn->GetHealthComponent()->IsAlive();
}

void UVSEconomySubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
    // The match game mode assigns connection id and team after this event; SyncWallets registers then.
    if (GameMode && GameMode->GetWorld() == GetWorld() && NewPlayer && IsEconomyActive() && !FindWallet(NewPlayer))
    {
        SpawnWallet(NewPlayer);
    }
}

void UVSEconomySubsystem::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
    if (GameMode && GameMode->GetWorld() == GetWorld())
    {
        RemoveWallet(FindWallet(Exiting));
    }
}

AVSEconomyWallet* UVSEconomySubsystem::SpawnWallet(APlayerController* Controller)
{
    UWorld* World = GetWorld();
    if (!World || !Controller) { return nullptr; }
    FActorSpawnParameters Params;
    Params.Owner = Controller;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.ObjectFlags |= RF_Transient;
    AVSEconomyWallet* Wallet = World->SpawnActor<AVSEconomyWallet>(Params);
    if (!Wallet) { return nullptr; }
    TArray<int32> Prices;
    for (int32 Index = 0; Index < static_cast<int32>(ValStrike::Economy::ItemCount); ++Index)
    {
        Prices.Add(Model.Price(static_cast<ValStrike::Economy::Item>(Index)));
    }
    Wallet->ServerSetCatalog(Prices, Model.GetTuning().StarterItems);
    Wallets.Add(Wallet);
    return Wallet;
}

void UVSEconomySubsystem::RemoveWallet(AVSEconomyWallet* Wallet)
{
    if (!Wallet) { return; }
    const uint64 PlayerId = Wallet->GetView().PlayerId;
    if (PlayerId != 0)
    {
        VSEconomyServer::EconomyEvent Leave;
        Leave.Kind = VSEconomyServer::EconomyKind::Leave;
        Leave.PlayerId = PlayerId;
        const VSEconomyServer::EconomyStatus Result = Model.Apply(Leave);
        UE_LOG(LogValStrike, Display, TEXT("VSECON leave player=%llu status=%s"), PlayerId, *VSEconomyServer::StatusText(Result));
    }
    Wallets.Remove(Wallet);
    Wallet->Destroy();
}

void UVSEconomySubsystem::SyncWallets()
{
    UWorld* World = GetWorld();
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();
        if (Controller && !FindWallet(Controller)) { SpawnWallet(Controller); }
    }
    for (int32 Index = Wallets.Num() - 1; Index >= 0; --Index)
    {
        AVSEconomyWallet* Wallet = Wallets[Index];
        APlayerController* Controller = Wallet ? Cast<APlayerController>(Wallet->GetOwner()) : nullptr;
        if (!IsValid(Wallet) || !IsValid(Controller))
        {
            if (IsValid(Wallet)) { RemoveWallet(Wallet); }
            else { Wallets.RemoveAt(Index); }
            continue;
        }
        if (Wallet->GetView().PlayerId != 0) { continue; }
        const AVSMatchPlayerState* PS = Controller->GetPlayerState<AVSMatchPlayerState>();
        if (!PS || PS->ConnectionId == 0 || !VSEconomyServer::IsPlayingTeam(PS->TeamId)) { continue; }
        VSEconomyServer::EconomyEvent Join;
        Join.Kind = VSEconomyServer::EconomyKind::Join;
        Join.PlayerId = PS->ConnectionId;
        Join.TeamId = static_cast<VSEconomyServer::EconomyTeam>(PS->TeamId);
        const VSEconomyServer::EconomyStatus Result = Model.Apply(Join);
        UE_LOG(LogValStrike, Display, TEXT("VSECON join player=%llu team=%d status=%s"), PS->ConnectionId, PS->TeamId,
            *VSEconomyServer::StatusText(Result));
        if (Result == VSEconomyServer::EconomyStatus::Applied)
        {
            FVSEconomyWalletView View = Wallet->GetView();
            View.PlayerId = PS->ConnectionId;
            Wallet->ServerPublish(View);
            BroadcastLoadout(Wallet, EVSEconomyGrantReason::Join);
        }
    }
}

void UVSEconomySubsystem::ObserveMatch()
{
    const AVSMatchGameState* GS = MatchState();
    if (!GS || !GS->Match.bReady || bObserving) { return; }
    TGuardValue<bool> Guard(bObserving, true);
    const FVSMatchRepState& Match = GS->Match;
    const VSEconomyServer::MatchPhase Phase = static_cast<VSEconomyServer::MatchPhase>(Match.Phase);
    const ValStrike::Economy::Snapshot& S = Model.Read();
    const auto RoundEvent = [&S](VSEconomyServer::EconomyKind Kind)
    {
        VSEconomyServer::EconomyEvent Value;
        Value.Kind = Kind;
        Value.RoundId = S.RoundId;
        return Value;
    };

    // A new round's preparation opens its buy phase (fresh pawns exist by now).
    if (Phase == VSEconomyServer::MatchPhase::Preparation && Match.RoundId > S.RoundId &&
        VSEconomyServer::IsPlayingTeam(Match.AttackingTeam))
    {
        VSEconomyServer::EconomyEvent Begin;
        Begin.Kind = VSEconomyServer::EconomyKind::BuyPhaseBegin;
        Begin.RoundId = Match.RoundId;
        Begin.TeamId = static_cast<VSEconomyServer::EconomyTeam>(Match.AttackingTeam);
        Begin.CompletedRounds = static_cast<uint8>(FMath::Clamp(Match.CompletedRounds, 0, 255));
        const uint32 ResetsBefore = S.Resets;
        const VSEconomyServer::EconomyStatus Result = Model.Apply(Begin);
        const bool bReset = S.Resets != ResetsBefore;
        UE_LOG(LogValStrike, Display, TEXT("VSECON buy_open round=%llu attackers=%d completed=%d reset=%d status=%s"),
            Match.RoundId, Match.AttackingTeam, Match.CompletedRounds, bReset ? 1 : 0, *VSEconomyServer::StatusText(Result));
        if (Result == VSEconomyServer::EconomyStatus::Applied)
        {
            for (AVSEconomyWallet* Wallet : Wallets)
            {
                if (IsValid(Wallet) && Wallet->GetView().PlayerId != 0)
                {
                    BroadcastLoadout(Wallet, bReset ? EVSEconomyGrantReason::Reset : EVSEconomyGrantReason::RoundStart);
                }
            }
        }
    }
    if (S.RoundId == 0 || Match.RoundId != S.RoundId) { return; }

    // The buy phase ends with preparation. Also close at the deadline itself: the match model
    // seals that time in its own tick, which can run after a request in the same frame.
    const int64 DeadlineMs = FMath::RoundToInt64(Match.PhaseDeadlineSeconds * 1000.0);
    if (S.CurrentPhase == VSEconomyServer::EconomyPhase::Buy &&
        (Phase != VSEconomyServer::MatchPhase::Preparation || ServerNowMs() >= DeadlineMs))
    {
        const VSEconomyServer::EconomyStatus Result = Model.Apply(RoundEvent(VSEconomyServer::EconomyKind::BuyPhaseEnd));
        UE_LOG(LogValStrike, Display, TEXT("VSECON buy_closed round=%llu status=%s"), S.RoundId, *VSEconomyServer::StatusText(Result));
    }
    const bool bPlayed = Phase == VSEconomyServer::MatchPhase::Active || Phase == VSEconomyServer::MatchPhase::Planted ||
        Phase == VSEconomyServer::MatchPhase::Resolved || Phase == VSEconomyServer::MatchPhase::MatchComplete;
    if (S.CurrentPhase != VSEconomyServer::EconomyPhase::Live || !bPlayed) { return; }

    for (AVSEconomyWallet* Wallet : Wallets)
    {
        const APlayerController* Controller = IsValid(Wallet) ? Cast<APlayerController>(Wallet->GetOwner()) : nullptr;
        const AVSMatchPlayerState* PS = Controller ? Controller->GetPlayerState<AVSMatchPlayerState>() : nullptr;
        const ValStrike::Economy::Wallet* Entry = Wallet ? S.Find(Wallet->GetView().PlayerId) : nullptr;
        if (PS && Entry && PS->bParticipating && !PS->bAlive && !Entry->Died)
        {
            VSEconomyServer::EconomyEvent Fallen = RoundEvent(VSEconomyServer::EconomyKind::Death);
            Fallen.PlayerId = Entry->PlayerId;
            const VSEconomyServer::EconomyStatus Result = Model.Apply(Fallen);
            UE_LOG(LogValStrike, Display, TEXT("VSECON death player=%llu round=%llu status=%s"), Fallen.PlayerId, S.RoundId,
                *VSEconomyServer::StatusText(Result));
        }
    }
    const VSEconomyServer::MatchDevice Device = static_cast<VSEconomyServer::MatchDevice>(Match.ObjectiveState);
    if (!S.PlantAwarded && (Device == VSEconomyServer::MatchDevice::Planted || Device == VSEconomyServer::MatchDevice::Defused ||
        Device == VSEconomyServer::MatchDevice::Detonated))
    {
        const VSEconomyServer::EconomyStatus Result = Model.Apply(RoundEvent(VSEconomyServer::EconomyKind::Plant));
        UE_LOG(LogValStrike, Display, TEXT("VSECON plant round=%llu attackers=%d status=%s"), S.RoundId,
            static_cast<int32>(S.AttackingTeam), *VSEconomyServer::StatusText(Result));
    }
    if ((Phase == VSEconomyServer::MatchPhase::Resolved || Phase == VSEconomyServer::MatchPhase::MatchComplete) &&
        VSEconomyServer::IsPlayingTeam(Match.Winner) && !S.ResultAwarded)
    {
        VSEconomyServer::EconomyEvent Result = RoundEvent(VSEconomyServer::EconomyKind::RoundResult);
        Result.TeamId = static_cast<VSEconomyServer::EconomyTeam>(Match.Winner);
        const VSEconomyServer::EconomyStatus Status = Model.Apply(Result);
        FString Balances;
        for (const ValStrike::Economy::Wallet& Entry : S.Wallets)
        {
            if (Entry.PlayerId != 0)
            {
                Balances += FString::Printf(TEXT(" %llu:%d(+%d)"), Entry.PlayerId, Entry.Balance, Entry.RoundIncome);
            }
        }
        UE_LOG(LogValStrike, Display, TEXT("VSECON result round=%llu winner=%d streaks=%d/%d status=%s balances:%s"),
            S.RoundId, Match.Winner, S.LossStreak[0], S.LossStreak[1], *VSEconomyServer::StatusText(Status), *Balances);
    }
}

void UVSEconomySubsystem::PublishWallets()
{
    const AVSMatchGameState* GS = MatchState();
    const ValStrike::Economy::Snapshot& S = Model.Read();
    for (AVSEconomyWallet* Wallet : Wallets)
    {
        if (!IsValid(Wallet)) { continue; }
        FVSEconomyWalletView View = Wallet->GetView();
        const ValStrike::Economy::Wallet* Entry = S.Find(View.PlayerId);
        if (!Entry) { continue; }
        View.TeamId = static_cast<uint8>(Entry->TeamId);
        View.Credits = Entry->Balance;
        View.OwnedMask = Entry->Owned;
        View.RefundableMask = Entry->Refundable;
        View.RoundIncome = Entry->RoundIncome;
        View.LastRoundIncome = Entry->LastRoundIncome;
        View.RoundId = S.RoundId;
        if (S.ResultAwarded) { View.ResultRoundId = S.RoundId; }
        View.bBuyOpen = S.CurrentPhase == VSEconomyServer::EconomyPhase::Buy && GS && GS->Match.RoundId == S.RoundId &&
            IsAliveOnServer(Cast<APlayerController>(Wallet->GetOwner()));
        View.BuyDeadlineSeconds = View.bBuyOpen ? GS->Match.PhaseDeadlineSeconds : 0.0;
        Wallet->ServerPublish(View);
    }
}

void UVSEconomySubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!IsEconomyActive()) { return; }
    SyncWallets();
    ObserveMatch();
    PublishWallets();
}

void UVSEconomySubsystem::HandleRequest(AVSEconomyWallet* Wallet, EVSEconomyRequest Kind, EVSEconomyItem Item,
    uint64 RoundId, uint32 RequestId)
{
    if (!IsEconomyActive() || !IsValid(Wallet) || !Wallets.Contains(Wallet) || !Wallet->HasAuthority()) { return; }
    // Bring the model up to the current match state (for example a just-expired preparation).
    ObserveMatch();
    APlayerController* Controller = Cast<APlayerController>(Wallet->GetOwner());
    const AVSMatchPlayerState* PS = Controller ? Controller->GetPlayerState<AVSMatchPlayerState>() : nullptr;
    const bool bRefund = Kind == EVSEconomyRequest::Refund;
    VSEconomyServer::EconomyEvent Request;
    Request.Kind = bRefund ? VSEconomyServer::EconomyKind::Refund : VSEconomyServer::EconomyKind::Purchase;
    Request.PlayerId = Wallet->GetView().PlayerId;
    Request.ItemId = static_cast<ValStrike::Economy::Item>(static_cast<uint8>(Item));
    Request.RoundId = RoundId;
    Request.RequestId = RequestId;
    Request.TeamId = PS ? static_cast<VSEconomyServer::EconomyTeam>(PS->TeamId) : VSEconomyServer::EconomyTeam::None;
    Request.Alive = IsAliveOnServer(Controller);
    // The wallet's identity must still be the controller's match identity.
    const VSEconomyServer::EconomyStatus Result = !PS || Request.PlayerId == 0 || PS->ConnectionId != Request.PlayerId
        ? VSEconomyServer::EconomyStatus::UnknownPlayer : Model.Apply(Request);

    FVSEconomyWalletView View = Wallet->GetView();
    View.LastRequestId = RequestId;
    View.LastKind = static_cast<uint8>(Kind);
    View.LastItem = static_cast<uint8>(Item);
    View.LastStatus = static_cast<uint8>(Result);
    Wallet->ServerPublish(View);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TArray<uint32>& Counts = StatusCounts.FindOrAdd(Request.PlayerId);
    Counts.SetNumZeroed(static_cast<int32>(ValStrike::Economy::StatusCount));
    ++Counts[static_cast<int32>(Result)];
#endif
    const ValStrike::Economy::Wallet* Entry = Model.Read().Find(Request.PlayerId);
    UE_LOG(LogValStrike, Display, TEXT("VSECON request player=%llu kind=%s item=%u round=%llu id=%u alive=%d status=%s credits=%d"),
        Request.PlayerId, bRefund ? TEXT("refund") : TEXT("purchase"), static_cast<uint32>(Item), RoundId, RequestId,
        Request.Alive ? 1 : 0, *VSEconomyServer::StatusText(Result), Entry ? Entry->Balance : -1);
    if (Result == VSEconomyServer::EconomyStatus::Applied)
    {
        BroadcastLoadout(Wallet, bRefund ? EVSEconomyGrantReason::Refund : EVSEconomyGrantReason::Purchase);
    }
    PublishWallets();
}

void UVSEconomySubsystem::HandleKill(const FVSDamageEvent& Event)
{
    // Server-only broadcast; its pointers are valid only during this call.
    if (!Event.bKilled || !IsEconomyActive()) { return; }
    const bool bCredited = CreditKill(Event.Instigator, Event.VictimController, Event.Victim);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++(bCredited ? KillsCredited : KillsRefused);
#else
    (void)bCredited;
#endif
}

bool UVSEconomySubsystem::CreditKill(AController* Killer, AController* Victim, const APawn* VictimPawn)
{
    if (!Killer || !Victim || Killer == Victim) { return false; }
    const AVSEconomyWallet* KillerWallet = FindWallet(Killer);
    const AVSEconomyWallet* VictimWallet = FindWallet(Victim);
    const AVSCharacter* VictimCharacter = Cast<AVSCharacter>(VictimPawn ? VictimPawn : Victim->GetPawn());
    // Never credit a report about a player who is not actually dead on the server.
    if (!KillerWallet || !VictimWallet || !VictimCharacter || !VictimCharacter->GetHealthComponent() ||
        VictimCharacter->GetHealthComponent()->IsAlive())
    {
        UE_LOG(LogValStrike, Display, TEXT("VSECON kill refused: killer=%s victim=%s (no wallet or victim alive)"),
            *GetNameSafe(Killer), *GetNameSafe(Victim));
        return false;
    }
    ObserveMatch();
    VSEconomyServer::EconomyEvent Kill;
    Kill.Kind = VSEconomyServer::EconomyKind::Kill;
    Kill.RoundId = Model.Read().RoundId;
    Kill.PlayerId = KillerWallet->GetView().PlayerId;
    Kill.OtherId = VictimWallet->GetView().PlayerId;
    const VSEconomyServer::EconomyStatus Result = Model.Apply(Kill);
    UE_LOG(LogValStrike, Display, TEXT("VSECON kill killer=%llu victim=%llu round=%llu status=%s"),
        Kill.PlayerId, Kill.OtherId, Kill.RoundId, *VSEconomyServer::StatusText(Result));
    PublishWallets();
    return Result == VSEconomyServer::EconomyStatus::Applied;
}

void UVSEconomySubsystem::BroadcastLoadout(AVSEconomyWallet* Wallet, EVSEconomyGrantReason Reason)
{
    APlayerController* Controller = Wallet ? Cast<APlayerController>(Wallet->GetOwner()) : nullptr;
    if (!Controller) { return; }
    const FVSEconomyLoadout Loadout = GetLoadout(Controller);
    UE_LOG(LogValStrike, Display, TEXT("VSECON grant player=%llu reason=%s pistol=%d rifle=%d subscribers=%d"),
        Wallet->GetView().PlayerId, VSEconomyGrantReasonName(Reason), Loadout.Owns(EVSEconomyItem::Pistol) ? 1 : 0,
        Loadout.Owns(EVSEconomyItem::Rifle) ? 1 : 0, LoadoutChanged.IsBound() ? 1 : 0);
    LoadoutChanged.Broadcast(Controller, Loadout, Reason);
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
uint32 UVSEconomySubsystem::DevelopmentStatusCount(uint64 PlayerId, ValStrike::Economy::Status Status) const
{
    const TArray<uint32>* Counts = StatusCounts.Find(PlayerId);
    const int32 Index = static_cast<int32>(Status);
    return Counts && Counts->IsValidIndex(Index) ? (*Counts)[Index] : 0;
}

int32 UVSEconomySubsystem::DevelopmentReplayRoundEvents()
{
    const ValStrike::Economy::Snapshot Before = Model.Read();
    if (Before.RoundId == 0) { return 0; }
    TArray<VSEconomyServer::EconomyEvent> Replays;
    VSEconomyServer::EconomyEvent Value;
    Value.RoundId = Before.RoundId;
    Value.Kind = VSEconomyServer::EconomyKind::BuyPhaseBegin;
    Value.TeamId = Before.AttackingTeam;
    Value.CompletedRounds = Before.CompletedRounds;
    Replays.Add(Value);
    Value.TeamId = VSEconomyServer::EconomyTeam::None;
    Value.Kind = VSEconomyServer::EconomyKind::BuyPhaseEnd;
    Replays.Add(Value);
    Value.Kind = VSEconomyServer::EconomyKind::Plant;
    Replays.Add(Value);
    Value.Kind = VSEconomyServer::EconomyKind::RoundResult;
    Value.TeamId = VSEconomyServer::EconomyTeam::One;
    Replays.Add(Value);
    Value.TeamId = VSEconomyServer::EconomyTeam::Two;
    Replays.Add(Value);
    int32 Ignored = 0;
    for (const VSEconomyServer::EconomyEvent& Replay : Replays)
    {
        Ignored += Model.Apply(Replay) != VSEconomyServer::EconomyStatus::Applied ? 1 : 0;
    }
    for (std::size_t Index = 0; Index < ValStrike::Economy::MaxWallets; ++Index)
    {
        if (Before.Wallets[Index].Balance != Model.Read().Wallets[Index].Balance) { return -1; }
    }
    return Ignored;
}
#endif
