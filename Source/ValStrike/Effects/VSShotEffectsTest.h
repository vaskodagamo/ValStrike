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
 * Test clock. The engine's client estimate averages deltas and lagged ~1.6 s for
 * a long time after a client hitch (world time advances at most 0.4 s per frame).
 * Each replicated server time sample gives real_receipt - server_time = offset +
 * delay with delay >= 0; the smallest value in a 3 s window estimates the offset,
 * which is smooth, ignores late samples and follows server-clock drift.
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
    struct FClockSample { double Offset; double Received; };
    TArray<FClockSample> ClockSamples;
};

UCLASS(NotBlueprintable, Transient)
class AVSShotEffectsTestCharacter : public AVSCharacter
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
};
