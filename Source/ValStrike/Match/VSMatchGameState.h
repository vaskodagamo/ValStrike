#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "VSMatchGameState.generated.h"

// Presentation wire state. The authoritative rules model remains in GameMode.
// Enum fields use the corresponding ValStrike::Rules enum values.
USTRUCT()
struct VALSTRIKE_API FVSMatchRepState
{
    GENERATED_BODY()

    UPROPERTY() uint8 Phase = 0;
    UPROPERTY() uint8 AttackingTeam = 1;
    UPROPERTY() uint8 Winner = 0;
    UPROPERTY() uint8 Result = 0;
    UPROPERTY() uint8 ObjectiveState = 0;
    UPROPERTY() int32 ScoreOne = 0;
    UPROPERTY() int32 ScoreTwo = 0;
    UPROPERTY() int32 CompletedRounds = 0;
    UPROPERTY() uint64 RoundId = 0;
    UPROPERTY() uint64 DeviceRevision = 0;
    UPROPERTY() uint64 CarrierId = 0;
    UPROPERTY() uint64 InteractionToken = 0;
    UPROPERTY() uint64 InteractionPlayerId = 0;
    UPROPERTY() double PhaseDeadlineSeconds = 0.0;
    UPROPERTY() double InteractionStartedSeconds = 0.0;
    UPROPERTY() double InteractionCompletesSeconds = 0.0;
    UPROPERTY() bool bReady = false;
    UPROPERTY() FString StatusText;
};

UCLASS()
class VALSTRIKE_API AVSMatchGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated)
    FVSMatchRepState Match;
};
