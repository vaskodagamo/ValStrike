#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AVSPlayerController;
class SEditableTextBox;

class SVSSessionMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVSSessionMenu) {}
        SLATE_ARGUMENT(TWeakObjectPtr<AVSPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    TSharedRef<SWidget> InitialFocus() const;

private:
    bool IsConnecting() const;
    FReply Connect();
    TWeakObjectPtr<AVSPlayerController> Controller;
    TSharedPtr<SEditableTextBox> AddressInput;
    TArray<TSharedPtr<SWidget>> Navigation;
};
