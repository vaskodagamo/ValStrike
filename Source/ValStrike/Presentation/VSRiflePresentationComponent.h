#pragma once

#include "CoreMinimal.h"
#include "VSWeaponPresentationComponent.h"
#include "VSRiflePresentationComponent.generated.h"

/** R02 rifle views (INT-ART-04): native ART-17 body/magazine, recipe sockets; R01 assets stay for comparison. */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSRiflePresentationComponent : public UVSWeaponPresentationComponent
{
    GENERATED_BODY()
public:
    UVSRiflePresentationComponent();
};
