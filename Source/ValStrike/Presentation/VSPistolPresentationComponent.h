#pragma once

#include "CoreMinimal.h"
#include "VSWeaponPresentationComponent.h"
#include "VSPistolPresentationComponent.generated.h"

/** P01 pistol views: accepted body/slide/magazine art (read-only), slide kick per accepted shot. */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSPistolPresentationComponent : public UVSWeaponPresentationComponent
{
    GENERATED_BODY()
public:
    UVSPistolPresentationComponent();
};
