#pragma once

#include "CoreMinimal.h"
#include "VSWeaponComponent.h"
#include "VSRifleComponent.generated.h"

/** R01 automatic rifle: the original MVP weapon and the default equipped slot. */
UCLASS(ClassGroup = (Combat))
class VALSTRIKE_API UVSRifleComponent : public UVSWeaponComponent
{
    GENERATED_BODY()
public:
    UVSRifleComponent();
};
