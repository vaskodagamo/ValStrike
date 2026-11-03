#pragma once

#include "CoreMinimal.h"
#include "../../VSMatchHUDData.h"

class AVSPlayerController;

/** Explicit local presentation fixtures. Normal play never overrides the provider. */
namespace VSRoundHUDPreview
{
    // Call before menu/pawn early returns so screenshots and cleanup can advance.
    // A true result requires a prominent on-screen PRESENTATION TEST label.
    bool Tick(AVSPlayerController* Controller, FVSMatchHUDData& InOutData);
    FString GetFixtureName();
}
