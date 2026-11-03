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
 * Kill credit needs the killer, which combat does not expose yet: see ReportKill().
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

    /**
     * Server-only kill credit, for the combat owner to call synchronously when lethal damage
     * is applied (before returning from the damage path). The victim's pawn must already be
     * dead. One credit per victim per round; team kills earn nothing. Returns true if a kill
     * was recorded.
     */
    bool ReportKill(AController* Killer, AController* Victim);

    /** Called by AVSEconomyWallet::ServerRequest on the server. */
    void HandleRequest(AVSEconomyWallet* Wallet, EVSEconomyRequest Kind, EVSEconomyItem Item, uint64 RoundId, uint32 RequestId);

    /** Weapon grant interface; see VSEconomyTypes.h. */
    FVSEconomyLoadout GetLoadout(const AController* Controller) const;
    FVSEconomyLoadoutChanged& OnLoadoutChanged() { return LoadoutChanged; }

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    uint32 DevelopmentStatusCount(uint64 PlayerId, ValStrike::Economy::Status Status) const;
    // Re-sends the current round's already-applied round events; returns how many were ignored.
    int32 DevelopmentReplayRoundEvents();
#endif

private:
    void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
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
    bool bObserving = false;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TMap<uint64, TArray<uint32>> StatusCounts;
#endif
};
