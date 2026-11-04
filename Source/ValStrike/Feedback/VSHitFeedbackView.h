#pragma once

#include "CoreMinimal.h"
#include "VSHitFeedbackState.h"

/** Read-only per-frame snapshot of server-confirmed feedback for the local HUD. */
struct FVSHitFeedbackView
{
    struct FIndicator
    {
        float RelativeYawDeg = 0.0f; // 0 = straight ahead, positive = to the right.
        float Alpha = 0.0f;
    };

    VSHitFeedback::MarkerKind Marker = VSHitFeedback::MarkerKind::None;
    float MarkerAlpha = 0.0f;
    float MarkerDamage = 0.0f;
    TArray<FIndicator, TInlineAllocator<VSHitFeedback::MaxIndicators>> Indicators;
    bool bEliminated = false;
    FString EliminatedText; // "Eliminated by <name> · <weapon>[ · headshot]"
};
