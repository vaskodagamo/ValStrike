#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VSGameMode.generated.h"

/** Spawn policy for the movement prototype; round rules will follow separately. */
UCLASS()
class VALSTRIKE_API AVSGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AVSGameMode();
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
