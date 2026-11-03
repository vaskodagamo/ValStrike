#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VSEconomyRules.h"
#include "VSEconomyTypes.h"
#include "VSEconomySubsystem.generated.h"

class AController;
class AGameModeBase;
class APlayerController;
class AVSEconomyWallet;
class AVSMatchGameState;
class AVSMatchPlayerState;
class APawn;
class UVSDamageSubsystem;
struct FVSDamageEvent;

/**
 * Server-authoritative round economy adapter. Active only in match mode (the world's
 * GameState is AVSMatchGameState) and never on a network client.
 *
 * Round events are derived from the match's replicated state, read-only:
 *  - buy phase = Preparation of a new RoundId, closed by Active or the preparation deadline
 *  - plant     = objective Planted/Defused/Detonated during that round
 *  - deaths    = participating AVSMatchPlayerState with bAlive false
 *  - result    = Resolved/MatchComplete with a winner for that round
 *  - side swap / new match = the attacking team changes / zero completed rounds at preparation
 *  - kills    = UVSDamageSubsystem::OnKill() (attributed lethal hits), validated again here
 */
UCLASS()
class VALSTRIKE_API UVSEconomySubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    UVSEconomySubsystem();

    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    bool IsEconomyActive() const;
    const ValStrike::Economy::EconomyRules& GetModel() const { return Model; }
    AVSEconomyWallet* FindWallet(const AController* Controller) const;

    /** Called by AVSEconomyWallet::ServerRequest on the server. */
    void HandleRequest(AVSEconomyWallet* Wallet, EVSEconomyRequest Kind, EVSEconomyItem Item, uint64 RoundId, uint32 RequestId);

    /** Weapon grant interface; see VSEconomyTypes.h. */
    FVSEconomyLoadout GetLoadout(const AController* Controller) const;
    FVSEconomyLoadoutChanged& OnLoadoutChanged() { return LoadoutChanged; }

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    uint32 DevelopmentStatusCount(uint64 PlayerId, ValStrike::Economy::Status Status) const;
    // Kill notifications seen from the damage subsystem: credited, and refused before the model
    // (no killer/victim wallet, victim still alive) or by the model (repeat, wrong phase, ...).
    uint32 DevelopmentKillsCredited() const { return KillsCredited; }
    uint32 DevelopmentKillsRefused() const { return KillsRefused; }
    // Re-sends the current round's already-applied round events; returns how many were ignored.
    int32 DevelopmentReplayRoundEvents();
#endif

private:
    void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
    void HandleKill(const FVSDamageEvent& Event);
    // One credit per victim per round; team kills earn nothing; the victim must be dead.
    bool CreditKill(AController* Killer, AController* Victim, const APawn* VictimPawn);
    void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
    const AVSMatchGameState* MatchState() const;
    AVSEconomyWallet* SpawnWallet(APlayerController* Controller);
    void RemoveWallet(AVSEconomyWallet* Wallet);
    void SyncWallets();
    void ObserveMatch();
    void PublishWallets();
    bool IsAliveOnServer(const APlayerController* Controller) const;
    int64 ServerNowMs() const;
    ValStrike::Economy::Status ApplyEvent(const ValStrike::Economy::Event& Input, EVSEconomyGrantReason Reason);
    void BroadcastLoadout(AVSEconomyWallet* Wallet, EVSEconomyGrantReason Reason);

    ValStrike::Economy::EconomyRules Model;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AVSEconomyWallet>> Wallets;

    FVSEconomyLoadoutChanged LoadoutChanged;
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;
    FDelegateHandle KillHandle;
    TWeakObjectPtr<UVSDamageSubsystem> DamageSource;
    bool bObserving = false;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TMap<uint64, TArray<uint32>> StatusCounts;
    uint32 KillsCredited = 0;
    uint32 KillsRefused = 0;
#endif
};
