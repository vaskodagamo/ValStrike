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
    /** Adds the dedicated server's lifecycle and admission check (SRV-03) after the engine's checks. */
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
};
