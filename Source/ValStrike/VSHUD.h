#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VSHUD.generated.h"

UCLASS()
class VALSTRIKE_API AVSHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
