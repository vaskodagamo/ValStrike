#pragma once

#include "CoreMinimal.h"
#include "../VSGameMode.h"
#include "../VSCharacter.h"
#include "VSRiflePresentationTest.generated.h"

/** Isolated URL-selected development fixture. Never selected by normal maps. */
UCLASS(NotBlueprintable, Transient)
class AVSRiflePresentationTestGameMode : public AVSGameMode
{
    GENERATED_BODY()
public:
    AVSRiflePresentationTestGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
};

UCLASS(NotBlueprintable, Transient)
class AVSRiflePresentationTestCharacter : public AVSCharacter
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
};
