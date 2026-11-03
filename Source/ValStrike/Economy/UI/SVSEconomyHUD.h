#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "../VSEconomyTypes.h"

class AVSEconomyWallet;
class UVSEconomyClientSubsystem;

/**
 * Credits badge (top left, below the HUD brand) and the centered buy menu. Reads only the
 * local wallet's replicated view; every action is a request the server may refuse.
 * Keys while open: 1/2 buy or refund, Tab/Enter or mouse for buttons, B closes. Escape is
 * never handled here, so it still reaches the session menu.
 */
class SVSEconomyHUD : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVSEconomyHUD) {}
        SLATE_ARGUMENT(TWeakObjectPtr<UVSEconomyClientSubsystem>, Economy)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

    /** Status line for the latest judged request; exposed for presentation tests. */
    FText GetStatusText() const;

private:
    const AVSEconomyWallet* LocalWallet() const;
    TSharedRef<SWidget> MakeRow(EVSEconomyItem Item, const FText& Key, const FText& Name, const FText& SlotName);
    FReply Activate(EVSEconomyItem Item);
    float BadgeScale() const;
    float MenuScale() const;

    TWeakObjectPtr<UVSEconomyClientSubsystem> Economy;
};
