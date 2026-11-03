#pragma once

#include "CoreMinimal.h"
#include "../VSCharacter.h"
#include "../VSGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "VSShotEffectsTest.generated.h"

/** Isolated URL-selected development fixture for shot effects. Never selected by normal maps. */
UCLASS(NotBlueprintable, Transient)
class AVSShotEffectsTestGameMode : public AVSGameMode
{
    GENERATED_BODY()
public:
    AVSShotEffectsTestGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};

/**
 * Test clock: the engine's client estimate averages time deltas and lags for a
 * long time after a hitch (world time advances at most 0.4 s per frame). This
 * extrapolates the latest replicated server time with real elapsed time.
 */
UCLASS(NotBlueprintable, Transient)
class AVSShotEffectsTestGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    double EstimateServerSeconds() const;
protected:
    virtual void OnRep_ReplicatedWorldTimeSecondsDouble() override;
private:
    double LastServerSeconds = -1.0;
    double LastReceiptRealSeconds = 0.0;
};

UCLASS(NotBlueprintable, Transient)
class AVSShotEffectsTestCharacter : public AVSCharacter
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
};
