#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VSStatsLedger.h"
#include "VSMatchStatsSubsystem.generated.h"

class AController;
class AGameModeBase;
class APawn;
class APlayerController;
class APlayerState;
class AVSCharacter;
class AVSScoreboardState;
struct FVSDamageEvent;

/**
 * Server-side kill/death/assist/damage tracking for every logged-in player.
 * - Players come from FGameModeEvents post-login/logout.
 * - Deaths come from each pawn's UVSHealthComponent::OnLifeStateChanged, so every
 *   death counts even when no shooter is known.
 * - Kills, headshot kills, assists and damage dealt come from
 *   UVSDamageSubsystem::OnDamage (shooter attribution).
 * - A new match (rematch or a match cleared by the rules) resets the counters.
 * Results are mirrored into a replicated AVSScoreboardState for clients.
 */
UCLASS()
class VALSTRIKE_API UVSMatchStatsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    static UVSMatchStatsSubsystem* Get(const UObject* WorldContext);

    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    AVSScoreboardState* GetState() const { return State; }
    const ValStrike::Stats::Ledger& GetLedger() const { return Ledger; }
    int32 FindKey(const APlayerState* Player) const;

private:
    bool IsServerWorld() const;
    void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
    void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
    void HandleActorSpawned(AActor* Actor);
    void HandleLifeStateChanged(TWeakObjectPtr<AVSCharacter> Pawn);
    // Server possession notifier: a fresh round pawn shows as alive immediately.
    void HandleNewPawn(APawn* NewPawn, int32 Key);
    void HandleDamage(const FVSDamageEvent& Event);
    void AddPlayer(APlayerController* Controller);
    void BindPawn(AVSCharacter* Pawn);
    // Starts a new ledger life when this player's pawn changed (fresh round pawn).
    void SyncLife(int32 Key, APawn* Pawn);
    void ObserveMatchReset();
    void Sync();
    void Publish();

    ValStrike::Stats::Ledger Ledger;

    UPROPERTY(Transient)
    TObjectPtr<AVSScoreboardState> State;

    TMap<int32, TWeakObjectPtr<APlayerState>> Players;
    TMap<int32, TWeakObjectPtr<AController>> Controllers;
    TMap<int32, TWeakObjectPtr<APawn>> LifePawns;
    TSet<TWeakObjectPtr<AVSCharacter>> BoundPawns;
    int32 NextKey = 0;
    uint8 LastPhase = 0;
    int32 LastCompletedRounds = 0;
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;
    FDelegateHandle SpawnHandle;
    FDelegateHandle DamageHandle;
    FTimerHandle SyncTimer;
};
