#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Feedback/VSCrosshairRules.h"
#include "VSHUD.generated.h"

class AVSCharacter;
struct FVSHitFeedbackView;
struct FVSMatchHUDData;
struct FVSRoundHUDPresentation;

UCLASS()
class VALSTRIKE_API AVSHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawLabel(const FString& Text, float X, float Y, float Height, const FLinearColor& Color,
        bool bCentered = false);
    float LabelWidth(const FString& Text, float Height) const;
    void DrawBoundedLabel(const FString& Text, float X, float Y, float Height, float Width,
        const FLinearColor& Color, bool bCentered = false);
    void DrawWrappedLabel(const FString& Text, float X, float Y, float Height, float Width,
        const FLinearColor& Color, bool bCentered = false);
    void DrawMatchHUD(const FVSMatchHUDData& Data, const FVSRoundHUDPresentation& View, float Scale);
    // Server-confirmed shooter marker around the reticle.
    void DrawHitMarker(const FVSHitFeedbackView& Feedback, float CenterX, float CenterY, float Scale);
    // Victim's directional arcs pointing at where the damage came from.
    void DrawDamageIndicators(const FVSHitFeedbackView& Feedback, float CenterX, float CenterY, float Scale);
    // Reticle inner gap: the equipped weapon's predicted spread cone, smoothed;
    // exactly StaticGap without a ready weapon or a showable spread.
    float ReticleGap(const AVSCharacter& Character, float StaticGap, float Scale);

    VSCrosshairRules::State CrosshairGap;
    double CrosshairGapTime = 0.0;
};
