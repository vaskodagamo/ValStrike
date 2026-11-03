#pragma once

#include "CoreMinimal.h"
#include "../../VSMatchHUDData.h"

/** Formatted presentation only: no pawn, local clock, rule evaluation or input. */
struct FVSRoundHUDPresentation
{
    bool bAvailable = false;
    FString TeamOneScore;
    FString TeamTwoScore;
    FString TeamOneRole;
    FString TeamTwoRole;
    FString LocalRole;
    FString PhaseLabel;
    FString RoundLabel;

    bool bShowTimer = false;
    FString TimerLabel;
    FString TimerText;
    FString ObjectiveLabel;
    FString InteractionPrompt;
    FString StatusText;
    bool bShowInteractionProgress = false;
    float InteractionProgress = 0.0f;
    bool bSpectating = false;

    bool bShowResult = false;
    FString ResultHeadline;
    bool bLocalWinner = false;
    bool bDraw = false;
    FString RematchHint;
};

namespace VSRoundHUDPresentation
{
    FVSRoundHUDPresentation Build(const FVSMatchHUDData& Data);
}
