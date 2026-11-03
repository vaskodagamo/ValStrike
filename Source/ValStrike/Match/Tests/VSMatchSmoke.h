#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VSMatchSmoke.generated.h"

// Spawned only by the opt-in development harness; contains no gameplay authority.
UCLASS()
class AVSMatchSmokeState : public AActor
{
    GENERATED_BODY()
public:
    AVSMatchSmokeState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated) int32 MatchIndex = 0;
    UPROPERTY(Replicated) int32 Stage = 0;
    UPROPERTY(Replicated) double StageAt = 0;
    UPROPERTY(Replicated) uint64 RoundId = 0;
};
namespace ValStrikeTests { void StartMatchSmoke(); }
