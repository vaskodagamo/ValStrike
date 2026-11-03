#include "SVSEconomyHUD.h"

#include "VSEconomyClientSubsystem.h"
#include "../VSEconomyWallet.h"
#include "../../VSPlayerController.h"
#include "Engine/UserInterfaceSettings.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace VSEconomyHUDStyle
{
// Unique names: the unity build shares one translation unit with VSHUD.cpp and the menus.
const FLinearColor EconomyPanel(0.015f, 0.026f, 0.04f, 0.9f);
const FLinearColor EconomyCard(0.025f, 0.045f, 0.065f, 1.0f);
const FLinearColor EconomyInk(0.94f, 0.97f, 0.98f, 1.0f);
const FLinearColor EconomyMuted(0.66f, 0.76f, 0.80f, 1.0f);
const FLinearColor EconomyMint(0.30f, 0.95f, 0.79f, 1.0f);
const FLinearColor EconomyAlert(1.0f, 0.45f, 0.38f, 1.0f);
const FLinearColor EconomyRule(0.13f, 0.19f, 0.22f, 1.0f);

FSlateFontInfo EconomyFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle("Regular", Size); }

TSharedRef<STextBlock> EconomyText(const FText& Text, int32 Size, const FLinearColor& Color)
{
    return SNew(STextBlock).Text(Text).Font(EconomyFont(Size)).ColorAndOpacity(Color);
}

TSharedRef<SWidget> EconomyRuleLine()
{
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(EconomyRule)
        .Padding(0)[ SNew(SBox).HeightOverride(1) ];
}

FText EconomyItemName(EVSEconomyItem Item)
{
    switch (Item)
    {
    case EVSEconomyItem::Pistol: return NSLOCTEXT("ValStrikeEconomy", "Pistol", "Pistol");
    case EVSEconomyItem::Rifle: return NSLOCTEXT("ValStrikeEconomy", "Rifle", "Rifle");
    default: return NSLOCTEXT("ValStrikeEconomy", "UnknownItem", "Item");
    }
}

FText EconomyCredits(int32 Value)
{
    return FText::Format(NSLOCTEXT("ValStrikeEconomy", "CreditsValue", "{0} CR"), FText::AsNumber(Value));
}
}

void SVSEconomyHUD::Construct(const FArguments& Args)
{
    using namespace VSEconomyHUDStyle;
    Economy = Args._Economy;
    SetVisibility(EVisibility::SelfHitTestInvisible);
    ChildSlot
    [
        SNew(SOverlay)
        // Credits badge: aligned with the canvas HUD's logical 1280x720 layout, below its brand.
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
        [
            SNew(SDPIScaler).DPIScale_Lambda([this]() { return BadgeScale(); })
            [
                SNew(SBox).Padding(FMargin(24, 66, 0, 0))
                .Visibility_Lambda([this]() {
                    const UVSEconomyClientSubsystem* Owner = Economy.Get();
                    return Owner && LocalWallet() && !Owner->IsSessionMenuOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                })
                [
                    SNew(SBox).WidthOverride(150)
                    [
                        SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(EconomyPanel).Padding(FMargin(15, 8, 12, 9))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [ EconomyText(NSLOCTEXT("ValStrikeEconomy", "CreditsLabel", "CREDITS"), 10, EconomyMuted) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 0)
                            [
                                SNew(STextBlock).Font(EconomyFont(20)).ColorAndOpacity(EconomyInk)
                                .Text_Lambda([this]() { const AVSEconomyWallet* W = LocalWallet(); return EconomyCredits(W ? W->GetView().Credits : 0); })
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                            [
                                SNew(STextBlock).Font(EconomyFont(11)).ColorAndOpacity(EconomyMint)
                                .Text(NSLOCTEXT("ValStrikeEconomy", "BuyHint", "B  BUY MENU"))
                                .Visibility_Lambda([this]() {
                                    const UVSEconomyClientSubsystem* Owner = Economy.Get();
                                    return Owner && !Owner->IsBuyMenuOpen() && Owner->CanOpenBuyMenu() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
                                })
                            ]
                        ]
                    ]
                ]
            ]
        ]
        // Buy menu.
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SDPIScaler).DPIScale_Lambda([this]() { return MenuScale(); })
            [
                SNew(SBox).WidthOverride(470)
                .Visibility_Lambda([this]() {
                    const UVSEconomyClientSubsystem* Owner = Economy.Get();
                    return Owner && Owner->IsBuyMenuOpen() ? EVisibility::Visible : EVisibility::Collapsed;
                })
                [
                    SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(EconomyCard).Padding(FMargin(22, 18))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Bottom)
                            [ EconomyText(NSLOCTEXT("ValStrikeEconomy", "Title", "ARMORY"), 22, EconomyInk) ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
                            [
                                SNew(STextBlock).Font(EconomyFont(13)).ColorAndOpacity(EconomyMint)
                                .Text_Lambda([this]() {
                                    const UVSEconomyClientSubsystem* Owner = Economy.Get();
                                    const double Left = Owner ? Owner->GetBuySecondsRemaining() : -1.0;
                                    if (Left < 0.0) { return NSLOCTEXT("ValStrikeEconomy", "BuyPhase", "BUY PHASE"); }
                                    const int32 Seconds = FMath::CeilToInt(Left);
                                    return FText::Format(NSLOCTEXT("ValStrikeEconomy", "BuyPhaseTimer", "BUY PHASE  {0}:{1}"),
                                        FText::AsNumber(Seconds / 60), FText::FromString(FString::Printf(TEXT("%02d"), Seconds % 60)));
                                })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 12)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(0, 0, 12, 0)
                            [ EconomyText(NSLOCTEXT("ValStrikeEconomy", "MenuCredits", "CREDITS"), 12, EconomyMuted) ]
                            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Bottom)
                            [
                                SNew(STextBlock).Font(EconomyFont(24)).ColorAndOpacity(EconomyInk)
                                .Text_Lambda([this]() { const AVSEconomyWallet* W = LocalWallet(); return EconomyCredits(W ? W->GetView().Credits : 0); })
                            ]
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
                            [
                                SNew(STextBlock).Font(EconomyFont(12)).ColorAndOpacity(EconomyMuted)
                                .Text_Lambda([this]() {
                                    const AVSEconomyWallet* W = LocalWallet();
                                    return W && W->GetView().LastRoundIncome > 0
                                        ? FText::Format(NSLOCTEXT("ValStrikeEconomy", "LastRound", "LAST ROUND +{0}"), FText::AsNumber(W->GetView().LastRoundIncome))
                                        : FText::GetEmpty();
                                })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()[ EconomyRuleLine() ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ MakeRow(EVSEconomyItem::Pistol, NSLOCTEXT("ValStrikeEconomy", "KeyOne", "1"), EconomyItemName(EVSEconomyItem::Pistol),
                            NSLOCTEXT("ValStrikeEconomy", "Sidearm", "SIDEARM")) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [ MakeRow(EVSEconomyItem::Rifle, NSLOCTEXT("ValStrikeEconomy", "KeyTwo", "2"), EconomyItemName(EVSEconomyItem::Rifle),
                            NSLOCTEXT("ValStrikeEconomy", "Primary", "PRIMARY")) ]
                        + SVerticalBox::Slot().AutoHeight()[ EconomyRuleLine() ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                        [
                            SNew(SBox).MinDesiredHeight(36)
                            [
                                SNew(STextBlock).Font(EconomyFont(13)).AutoWrapText(true)
                                .Text_Lambda([this]() { return GetStatusText(); })
                                .ColorAndOpacity_Lambda([this]() {
                                    const AVSEconomyWallet* W = LocalWallet();
                                    const bool bApplied = W && W->GetView().LastStatus == static_cast<uint8>(ValStrike::Economy::Status::Applied);
                                    return FSlateColor(!W || W->GetView().LastRequestId == 0 ? EconomyMuted : bApplied ? EconomyMint : EconomyAlert);
                                })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                        [ EconomyText(NSLOCTEXT("ValStrikeEconomy", "Keys", "1-2  buy / refund     Click  buy     B  close     Esc  menu"), 12, EconomyMuted) ]
                    ]
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SVSEconomyHUD::MakeRow(EVSEconomyItem Item, const FText& Key, const FText& Name, const FText& SlotName)
{
    using namespace VSEconomyHUDStyle;
    return SNew(SBox).Padding(FMargin(0, 8))
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
        [
            SNew(SBox).WidthOverride(26).HeightOverride(26).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(EconomyRule)
                .HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(7, 2))
                [ EconomyText(Key, 13, EconomyMint) ]
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[ EconomyText(Name, 17, EconomyInk) ]
            + SVerticalBox::Slot().AutoHeight()[ EconomyText(SlotName, 10, EconomyMuted) ]
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 14, 0)
        [
            SNew(STextBlock).Font(EconomyFont(15))
            .Text_Lambda([this, Item]() {
                const AVSEconomyWallet* W = LocalWallet();
                if (!W) { return FText::GetEmpty(); }
                return W->IsStarter(Item) ? NSLOCTEXT("ValStrikeEconomy", "Starter", "FREE") : EconomyCredits(W->GetPrice(Item));
            })
            .ColorAndOpacity_Lambda([this, Item]() {
                const AVSEconomyWallet* W = LocalWallet();
                const bool bShort = W && !W->GetView().Owns(Item) && W->GetView().Credits < W->GetPrice(Item);
                return FSlateColor(bShort ? EconomyAlert : EconomyMuted);
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            // Wide enough for "REFUND"/"OWNED" at 960x540 without clipping.
            SNew(SBox).WidthOverride(124)
            [
                SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(4, 7))
                .ButtonColorAndOpacity_Lambda([this, Item]() {
                    const AVSEconomyWallet* W = LocalWallet();
                    return FSlateColor(W && W->GetView().CanRefund(Item) ? EconomyAlert : EconomyMint);
                })
                .IsEnabled_Lambda([this, Item]() {
                    const AVSEconomyWallet* W = LocalWallet();
                    if (!W) { return false; }
                    const FVSEconomyWalletView& View = W->GetView();
                    return View.CanRefund(Item) || (!View.Owns(Item) && View.Credits >= W->GetPrice(Item));
                })
                .OnClicked_Lambda([this, Item]() { return Activate(Item); })
                [
                    SNew(STextBlock).Font(EconomyFont(14)).Justification(ETextJustify::Center)
                    .Text_Lambda([this, Item]() {
                        const AVSEconomyWallet* W = LocalWallet();
                        if (W && W->GetView().CanRefund(Item)) { return NSLOCTEXT("ValStrikeEconomy", "Refund", "REFUND"); }
                        if (W && W->GetView().Owns(Item)) { return NSLOCTEXT("ValStrikeEconomy", "Owned", "OWNED"); }
                        return NSLOCTEXT("ValStrikeEconomy", "Buy", "BUY");
                    })
                ]
            ]
        ]
    ];
}

const AVSEconomyWallet* SVSEconomyHUD::LocalWallet() const
{
    const UVSEconomyClientSubsystem* Owner = Economy.Get();
    return Owner ? Owner->GetLocalWallet() : nullptr;
}

FReply SVSEconomyHUD::Activate(EVSEconomyItem Item)
{
    if (UVSEconomyClientSubsystem* Owner = Economy.Get()) { Owner->ActivateItem(Item); }
    return FReply::Handled();
}

FReply SVSEconomyHUD::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    UVSEconomyClientSubsystem* Owner = Economy.Get();
    if (!Owner || !Owner->IsBuyMenuOpen()) { return FReply::Unhandled(); }
    const FKey Key = Event.GetKey();
    if (Key == EKeys::B) { Owner->SetBuyMenuOpen(false); return FReply::Handled(); }
    if (Key == EKeys::One || Key == EKeys::NumPadOne) { return Activate(EVSEconomyItem::Pistol); }
    if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { return Activate(EVSEconomyItem::Rifle); }
    // Escape and everything else continue to the viewport and the controller's bindings.
    return FReply::Unhandled();
}

FText SVSEconomyHUD::GetStatusText() const
{
    using Status = ValStrike::Economy::Status;
    const AVSEconomyWallet* W = LocalWallet();
    if (!W || W->GetView().LastRequestId == 0)
    {
        return NSLOCTEXT("ValStrikeEconomy", "Intro", "Buy before the round starts. Survivors keep their weapons.");
    }
    const FVSEconomyWalletView& View = W->GetView();
    const FText Item = VSEconomyHUDStyle::EconomyItemName(static_cast<EVSEconomyItem>(View.LastItem));
    const bool bRefund = View.LastKind == static_cast<uint8>(EVSEconomyRequest::Refund);
    switch (static_cast<Status>(View.LastStatus))
    {
    case Status::Applied:
        return bRefund ? FText::Format(NSLOCTEXT("ValStrikeEconomy", "Refunded", "{0} refunded."), Item)
                       : FText::Format(NSLOCTEXT("ValStrikeEconomy", "Bought", "{0} bought. Refund it before the round starts if needed."), Item);
    case Status::Unaffordable:
        return FText::Format(NSLOCTEXT("ValStrikeEconomy", "Short", "Not enough credits for the {0}."), Item);
    case Status::AlreadyOwned:
        return FText::Format(NSLOCTEXT("ValStrikeEconomy", "AlreadyOwned", "You already have the {0}."), Item);
    case Status::NotRefundable:
        return NSLOCTEXT("ValStrikeEconomy", "NotRefundable", "Only items bought in this buy phase can be refunded.");
    case Status::NotBuyPhase:
    case Status::StaleRound:
        return NSLOCTEXT("ValStrikeEconomy", "Closed", "The buy phase is over.");
    case Status::NotAlive:
        return NSLOCTEXT("ValStrikeEconomy", "NotAlive", "You can only buy while alive.");
    case Status::NotOnTeam:
        return NSLOCTEXT("ValStrikeEconomy", "NotOnTeam", "Join a team to buy.");
    case Status::RateLimited:
        return NSLOCTEXT("ValStrikeEconomy", "RateLimited", "Too many requests this round.");
    case Status::UnknownItem:
        return NSLOCTEXT("ValStrikeEconomy", "NoSuchItem", "That item is not available.");
    default:
        return NSLOCTEXT("ValStrikeEconomy", "Ignored", "Request ignored by the server.");
    }
}

float SVSEconomyHUD::BadgeScale() const
{
    // Same logical layout and clamps as the canvas HUD, so the badge sits under its brand.
    int32 Width = 960, Height = 540;
    const UVSEconomyClientSubsystem* Owner = Economy.Get();
    if (const AVSPlayerController* Local = Owner ? Owner->GetLocalController() : nullptr) { Local->GetViewportSize(Width, Height); }
    const float EngineScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
    const float HUDScale = FMath::Clamp(FMath::Min(Width / 1280.0f, Height / 720.0f), 0.75f, 2.0f);
    return HUDScale / FMath::Max(EngineScale, 0.01f);
}

float SVSEconomyHUD::MenuScale() const
{
    // Same rule as the session menu: never below 1.0 at 960x540, so text stays readable.
    int32 Width = 960, Height = 540;
    const UVSEconomyClientSubsystem* Owner = Economy.Get();
    if (const AVSPlayerController* Local = Owner ? Owner->GetLocalController() : nullptr) { Local->GetViewportSize(Width, Height); }
    const float EngineScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
    const float ReadableScale = FMath::Clamp(FMath::Min(Width / 1280.0f, Height / 720.0f), 1.0f, 1.4f);
    return ReadableScale / FMath::Max(EngineScale, 0.01f);
}
