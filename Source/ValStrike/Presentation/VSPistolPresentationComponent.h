#pragma once

#include "CoreMinimal.h"
#include "VSWeaponPresentationComponent.h"
#include "VSPistolPresentationComponent.generated.h"

/** P02 pistol views (INT-ART-04): native ART-18 body/slide/magazine, slide kick per accepted shot; P01 stays for comparison. */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSPistolPresentationComponent : public UVSWeaponPresentationComponent
{
    GENERATED_BODY()
public:
    UVSPistolPresentationComponent();
};
