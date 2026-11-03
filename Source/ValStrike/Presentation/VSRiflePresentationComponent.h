#pragma once

#include "CoreMinimal.h"
#include "VSWeaponPresentationComponent.h"
#include "VSRiflePresentationComponent.generated.h"

/** R01 rifle views: accepted body/magazine art, unchanged materials and sockets. */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSRiflePresentationComponent : public UVSWeaponPresentationComponent
{
    GENERATED_BODY()
public:
    UVSRiflePresentationComponent();
};
