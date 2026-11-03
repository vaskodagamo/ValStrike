#include "SVSSessionMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "Engine/UserInterfaceSettings.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "VSSessionState.h"
#include "../VSPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor MenuInk(0.025f, 0.045f, 0.065f, 1.0f);
const FLinearColor Mint(0.30f, 0.95f, 0.76f, 1.0f);
const FLinearColor MenuMuted(0.68f, 0.76f, 0.80f, 1.0f);

TSharedRef<STextBlock> Label(const FText& Text, int32 Size, const FLinearColor& Color = FLinearColor::White)
{
    return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).ColorAndOpacity(Color);
}
}

void SVSSessionMenu::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    TSharedPtr<SButton> ConnectButton, CancelButton, ResumeButton, DisconnectButton, QuitButton;
    const UVSSessionState* State = Controller.IsValid() ? Controller->GetSessionState() : nullptr;
    ChildSlot
    [
        // The engine's default UI curve halves Slate at 540p. Keep desktop
        // controls readable without changing shared project DPI configuration.
        SNew(SDPIScaler).DPIScale_Lambda([this]() {
            int32 Width = 960, Height = 540;
            if (Controller.IsValid()) Controller->GetViewportSize(Width, Height);
            const float EngineScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
            const float ReadableScale = FMath::Clamp(FMath::Min(Width / 1280.0f, Height / 720.0f), 1.0f, 1.4f);
            return ReadableScale / FMath::Max(EngineScale, 0.01f);
        })
        [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.008f, 0.015f, 0.025f, 0.88f))
        .Padding(FMargin(24, 16))
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(520)
            [
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(MenuInk).Padding(22)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [ Label(NSLOCTEXT("ValStrike", "MenuBrand", "V A L S T R I K E"), 13, Mint) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 4)
                        [ Label(NSLOCTEXT("ValStrike", "SessionTitle", "SESSION"), 28) ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)
                        [
                            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(MenuMuted)
                            .Text_Lambda([this]() {
                                if (Controller.IsValid() && Controller->GetNetMode() == NM_Client)
                                    return NSLOCTEXT("ValStrike", "OnlineMenu", "Connected session • The match continues while this menu is open.");
                                if (Controller.IsValid() && Controller->GetNetMode() == NM_ListenServer)
                                    return NSLOCTEXT("ValStrike", "HostMenu", "Hosting • The match continues while this menu is open.");
                                return NSLOCTEXT("ValStrike", "LocalMenu", "Local arena • Join a private server by address.");
                            }).AutoWrapText(true)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
                        [ Label(NSLOCTEXT("ValStrike", "AddressLabel", "SERVER ADDRESS"), 12, MenuMuted) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SAssignNew(AddressInput, SEditableTextBox)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 17)).Padding(FMargin(10, 7))
                            .Text(State ? FText::FromString(State->Address) : FText::GetEmpty())
                            .HintText(NSLOCTEXT("ValStrike", "AddressHint", "192.168.1.20:7777"))
                            .IsEnabled_Lambda([this]() { return !IsConnecting(); })
                            .OnTextChanged_Lambda([this](const FText& Text) {
                                if (Controller.IsValid())
                                    if (UVSSessionState* Session = Controller->GetSessionState()) Session->Address = Text.ToString();
                            })
                            .OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit) {
                                if (Commit == ETextCommit::OnEnter)
                                {
                                    Connect();
                                }
                            })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 6)
                        [ Label(NSLOCTEXT("ValStrike", "AddressHelp", "IPv4 or hostname. Default port: 7777."), 12, MenuMuted) ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SBox).MinDesiredHeight(52)
                            [
                                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).AutoWrapText(true)
                                .ColorAndOpacity_Lambda([this]() {
                                    const UVSSessionState* Session = Controller.IsValid() ? Controller->GetSessionState() : nullptr;
                                    return Session && Session->bError ? FLinearColor(1.0f, 0.55f, 0.45f) : Mint;
                                })
                                .Text_Lambda([this]() {
                                    const UVSSessionState* Session = Controller.IsValid() ? Controller->GetSessionState() : nullptr;
                                    return Session ? Session->Status : FText::GetEmpty();
                                })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
                            [
                                SAssignNew(ConnectButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                                .ButtonColorAndOpacity(Mint)
                                .IsEnabled_Lambda([this]() { return !IsConnecting(); })
                                .OnClicked(this, &SVSSessionMenu::Connect)
                                [ Label(NSLOCTEXT("ValStrike", "Connect", "Connect"), 15) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SAssignNew(CancelButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                                .IsEnabled_Lambda([this]() { return IsConnecting(); })
                                .OnClicked_Lambda([this]() { if (Controller.IsValid()) Controller->CancelConnection(); return FReply::Handled(); })
                                [ Label(NSLOCTEXT("ValStrike", "Cancel", "Cancel connection"), 15) ]
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 9, 0, 0)
                        [
                            SAssignNew(ResumeButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                            .IsEnabled_Lambda([this]() { return !IsConnecting(); })
                            .OnClicked_Lambda([this]() { if (Controller.IsValid()) Controller->SetSessionMenuOpen(false); return FReply::Handled(); })
                            [ Label(NSLOCTEXT("ValStrike", "Resume", "Resume  ·  Esc"), 15) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 9, 0, 0)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
                            [
                                SAssignNew(DisconnectButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                                .IsEnabled_Lambda([this]() { return !IsConnecting() && Controller.IsValid() && Controller->CanDisconnect(); })
                                .OnClicked_Lambda([this]() { if (Controller.IsValid()) Controller->DisconnectFromServer(); return FReply::Handled(); })
                                [
                                    SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                                    .Text_Lambda([this]() {
                                        return Controller.IsValid() && Controller->GetNetMode() == NM_ListenServer
                                            ? NSLOCTEXT("ValStrike", "EndHost", "End hosted session") : NSLOCTEXT("ValStrike", "Disconnect", "Disconnect");
                                    })
                                ]
                            ]
                            + SHorizontalBox::Slot().FillWidth(1)
                            [
                                SAssignNew(QuitButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                                .OnClicked_Lambda([this]() { if (Controller.IsValid()) Controller->QuitSession(); return FReply::Handled(); })
                                [ Label(NSLOCTEXT("ValStrike", "Quit", "Quit game"), 15) ]
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
                        [ Label(NSLOCTEXT("ValStrike", "MenuKeys", "Tab / Shift+Tab: navigate     Enter: select     Esc: resume"), 12, MenuMuted) ]
                    ]
                ]
            ]
        ]
        ]
    ];
    Navigation = {AddressInput, ConnectButton, CancelButton, ResumeButton, DisconnectButton, QuitButton};
}

bool SVSSessionMenu::IsConnecting() const
{
    const UVSSessionState* State = Controller.IsValid() ? Controller->GetSessionState() : nullptr;
    return State && State->bConnecting;
}

TSharedRef<SWidget> SVSSessionMenu::InitialFocus() const
{
    return Navigation[IsConnecting() ? 2 : 3].ToSharedRef();
}

FReply SVSSessionMenu::Connect()
{
    if (Controller.IsValid() && AddressInput && !IsConnecting())
    {
        Controller->ConnectToServer(AddressInput->GetText().ToString());
        if (IsConnecting())
        {
            // Slate caches enabled attributes. Refresh Cancel before assigning
            // focus, for both button clicks and text-commit delegates.
            Navigation[2]->UpdateAllAttributes();
            FSlateApplication::Get().SetKeyboardFocus(Navigation[2], EFocusCause::SetDirectly);
        }
    }
    return FReply::Handled();
}

FReply SVSSessionMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape)
    {
        if (!Event.IsRepeat() && Controller.IsValid())
        {
            if (IsConnecting()) Controller->CancelConnection();
            else Controller->SetSessionMenuOpen(false);
        }
        return FReply::Handled();
    }
    if (Event.GetKey() == EKeys::Tab)
    {
        int32 Focused = INDEX_NONE;
        for (int32 Index = 0; Index < Navigation.Num(); ++Index)
            if (Navigation[Index]->HasKeyboardFocus() || Navigation[Index]->HasFocusedDescendants()) Focused = Index;
        const int32 Direction = Event.IsShiftDown() ? -1 : 1;
        for (int32 Step = 1; Step <= Navigation.Num(); ++Step)
        {
            const int32 Next = (Focused + Direction * Step + Navigation.Num() * 2) % Navigation.Num();
            if (Navigation[Next]->IsEnabled())
                return FReply::Handled().SetUserFocus(Navigation[Next].ToSharedRef(), EFocusCause::Navigation);
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
}
