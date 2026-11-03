#pragma once

#include "CoreMinimal.h"
#include "../VSCharacter.h"
#include "../VSGameMode.h"
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

UCLASS(NotBlueprintable, Transient)
class AVSShotEffectsTestCharacter : public AVSCharacter
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
};
