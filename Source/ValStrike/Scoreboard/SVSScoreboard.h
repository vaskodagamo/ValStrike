#pragma once

#include "CoreMinimal.h"
#include "VSScoreboardModel.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SVerticalBox;

/**
 * Native Slate scoreboard overlay. Purely presentational: never focusable and
 * hit-test invisible, so it cannot take keyboard or mouse focus from the game
 * viewport or the Escape menu.
 */
class SVSScoreboard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVSScoreboard) {}
        SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    // Rebuilds rows only when a displayed value changed.
    void SetModel(const FVSScoreboardModel& Model);
    const FString& GetShownSignature() const { return Signature; }

    virtual bool SupportsKeyboardFocus() const override { return false; }

private:
    TSharedRef<SWidget> MakeTeam(const FVSScoreboardTeam& Team) const;
    TSharedRef<SWidget> MakeLine(const FVSScoreboardLine& Line, bool bHeader) const;

    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<SVerticalBox> Body;
    FString Signature;
};
