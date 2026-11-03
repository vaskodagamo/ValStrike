#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VSDamageRuntimeSmoke.generated.h"

// Spawned only by the opt-in development harness (-VSDamageSmoke=server). It
// carries test stages to the clients and has no gameplay authority.
UCLASS(NotPlaceable, Transient)
class AVSDamageSmokeDirector : public AActor
{
    GENERATED_BODY()

public:
    AVSDamageSmokeDirector();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated) int32 Stage = 0;
    UPROPERTY(Replicated) double StageAt = 0.0; // Server world seconds.
};

namespace ValStrikeDamageTests
{
// Starts the harness once per process when -VSDamageSmoke=<server|driver|observer> is present.
void StartRuntimeSmoke();
}
