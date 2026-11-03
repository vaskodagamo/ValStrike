#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "VSMatchPlayerState.generated.h"

UCLASS()
class VALSTRIKE_API AVSMatchPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated) uint64 ConnectionId = 0;
    UPROPERTY(Replicated) uint8 TeamId = 0;
    UPROPERTY(Replicated) bool bParticipating = false;
    UPROPERTY(Replicated) bool bAlive = false;

    // Eligibility text is private to this connection, rather than a global hint.
    UPROPERTY(Replicated) FString InteractionPrompt;
};
