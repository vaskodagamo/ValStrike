#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VSHitFeedbackSubsystem.generated.h"

class UVSDamageSubsystem;
struct FVSDamageEvent;

/**
 * Server-only router for confirmed-hit and damage-taken feedback.
 * Subscribes to UVSDamageSubsystem::OnDamage (kills included) and sends each
 * attributed event to the shooter's and the victim's owning client only,
 * through AVSPlayerController client RPCs. Other players receive nothing.
 */
UCLASS()
class VALSTRIKE_API UVSHitFeedbackSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    void HandleDamage(const FVSDamageEvent& Event);

    TWeakObjectPtr<UVSDamageSubsystem> DamageSource;
    FDelegateHandle DamageHandle;
};
