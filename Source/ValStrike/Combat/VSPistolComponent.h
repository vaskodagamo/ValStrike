#pragma once

#include "CoreMinimal.h"
#include "VSWeaponComponent.h"
#include "VSPistolComponent.generated.h"

/** P01 semi-automatic sidearm. Smaller magazine and lower damage than the rifle. */
UCLASS(ClassGroup = (Combat))
class VALSTRIKE_API UVSPistolComponent : public UVSWeaponComponent
{
    GENERATED_BODY()
public:
    UVSPistolComponent();
};
