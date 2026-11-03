#include "SVSSessionMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "Engine/UserInterfaceSettings.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "VSSessionState.h"
#include "VSUserSettings.h"
#include "../VSPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor MenuInk(0.025f, 0.045f, 0.065f, 1.0f);
const FLinearColor Mint(0.30f, 0.95f, 0.76f, 1.0f);
const FLinearColor MenuMuted(0.68f, 0.76f, 0.80f, 1.0f);
const FLinearColor MenuError(1.0f, 0.55f, 0.45f, 1.0f);

TSharedRef<STextBlock> Label(const FText& Text, int32 Size, const FLinearColor& Color = FLinearColor::White)
{
    return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).ColorAndOpacity(Color);
}
}

void SVSSessionMenu::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
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
                        SAssignNew(Pages, SWidgetSwitcher)
                        + SWidgetSwitcher::Slot() [ MakeSessionPage() ]
                        + SWidgetSwitcher::Slot() [ MakeSettingsPage() ]
                    ]
                ]
            ]
        ]
        ]
    ];
}

TSharedRef<SWidget> SVSSessionMenu::MakeSessionPage()
{
    TSharedPtr<SButton> ConnectButton, CancelButton, ResumeButton, DisconnectButton, QuitButton;
    const UVSSessionState* State = Controller.IsValid() ? Controller->GetSessionState() : nullptr;
    TSharedRef<SWidget> Page =
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
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
            [
                SAssignNew(ResumeButton, SButton).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                .IsEnabled_Lambda([this]() { return !IsConnecting(); })
                .OnClicked_Lambda([this]() { if (Controller.IsValid()) Controller->SetSessionMenuOpen(false); return FReply::Handled(); })
                [ Label(NSLOCTEXT("ValStrike", "Resume", "Resume  ·  Esc"), 15) ]
            ]
            + SHorizontalBox::Slot().FillWidth(1)
            [
                SAssignNew(SettingsButton, SButton).Tag(FName(TEXT("VS.Settings"))).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                .IsEnabled_Lambda([this]() { return !IsConnecting(); })
                .OnClicked_Lambda([this]() { OpenSettings(); return FReply::Handled(); })
                [ Label(NSLOCTEXT("ValStrike", "Settings", "Settings"), 15) ]
            ]
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
        [ Label(NSLOCTEXT("ValStrike", "MenuKeys", "Tab / Shift+Tab: navigate     Enter: select     Esc: resume"), 12, MenuMuted) ];
    Navigation = {AddressInput, ConnectButton, CancelButton, ResumeButton, SettingsButton, DisconnectButton, QuitButton};
    return Page;
}

TSharedRef<SWidget> SVSSessionMenu::MakeSettingsPage()
{
    TSharedPtr<SSlider> Slider;
    TSharedPtr<SButton> ApplyButton, BackButton;
    TSharedRef<SWidget> Page =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [ Label(NSLOCTEXT("ValStrike", "MenuBrand", "V A L S T R I K E"), 13, Mint) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 4)
        [ Label(NSLOCTEXT("ValStrike", "SettingsTitle", "SETTINGS"), 28) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).ColorAndOpacity(MenuMuted).AutoWrapText(true)
            .Text(NSLOCTEXT("ValStrike", "SettingsHelp", "Saved on this computer only. Never sent to a server."))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
        [ Label(NSLOCTEXT("ValStrike", "SensitivityLabel", "MOUSE SENSITIVITY"), 12, MenuMuted) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 12, 0)
            [
                SAssignNew(Slider, SSlider).Tag(FName(TEXT("VS.SensitivitySlider")))
                .MinValue(VSUserSettings::MinMouseSensitivity).MaxValue(VSUserSettings::MaxMouseSensitivity)
                .StepSize(VSUserSettings::MouseSensitivityStep)
                // Arrow keys change the value directly; no gamepad-style accept lock.
                .RequiresControllerLock(false)
                .SliderHandleColor(Mint).SliderBarColor(MenuMuted)
                .Value_Lambda([this]() { return GetSensitivity(); })
                .OnValueChanged_Lambda([this](float Value) { SetSensitivity(Value); })
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SAssignNew(SensitivityInput, SEditableTextBox).Tag(FName(TEXT("VS.SensitivityValue")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 17)).Padding(FMargin(10, 6))
                .MinDesiredWidth(76).Justification(ETextJustify::Center)
                .Text(FText::FromString(VSUserSettings::FormatMouseSensitivity(GetSensitivity())))
                .SelectAllTextWhenFocused(true).SelectAllTextOnCommit(true)
                .ClearKeyboardFocusOnCommit(false)
                .OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit) {
                    if (Commit == ETextCommit::OnCleared) SyncSensitivityText();
                    else CommitSensitivityText(true);
                })
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 12)
        [
            Label(FText::Format(NSLOCTEXT("ValStrike", "SensitivityRange", "{0} to {1}. Default {2}. Applies immediately."),
                FText::FromString(VSUserSettings::FormatMouseSensitivity(VSUserSettings::MinMouseSensitivity)),
                FText::FromString(VSUserSettings::FormatMouseSensitivity(VSUserSettings::MaxMouseSensitivity)),
                FText::FromString(VSUserSettings::FormatMouseSensitivity(VSUserSettings::DefaultMouseSensitivity))), 12, MenuMuted)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
        [ Label(NSLOCTEXT("ValStrike", "WindowModeLabel", "WINDOW MODE"), 12, MenuMuted) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
        [
            MakeSelector(TEXT("VS.WindowMode"), ModeButton,
                [this]() { return VSUserSettings::WindowModeText(PendingMode); },
                [this](int32 Direction) { CycleWindowMode(Direction); },
                []() { return true; })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
        [ Label(NSLOCTEXT("ValStrike", "ResolutionLabel", "RESOLUTION"), 12, MenuMuted) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            MakeSelector(TEXT("VS.Resolution"), ResolutionButton,
                [this]() {
                    const FText Size = VSUserSettings::ResolutionText(GetPendingResolution());
                    return PendingMode == EWindowMode::WindowedFullscreen
                        ? FText::Format(NSLOCTEXT("ValStrike", "DesktopResolution", "Desktop  ·  {0}"), Size) : Size;
                },
                [this](int32 Direction) { CycleResolution(Direction); },
                [this]() { return CanChooseResolution(); })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
        [
            SNew(SBox).MinDesiredHeight(36)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).AutoWrapText(true)
                .ColorAndOpacity_Lambda([this]() { return bSettingsError ? MenuError : Mint; })
                .Text_Lambda([this]() { return SettingsStatus; })
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
            [
                SAssignNew(ApplyButton, SButton).Tag(FName(TEXT("VS.ApplyDisplay"))).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                .ButtonColorAndOpacity(Mint)
                .OnClicked(this, &SVSSessionMenu::ApplyDisplay)
                [ Label(NSLOCTEXT("ValStrike", "ApplyDisplay", "Apply display"), 15) ]
            ]
            + SHorizontalBox::Slot().FillWidth(1)
            [
                SAssignNew(BackButton, SButton).Tag(FName(TEXT("VS.SettingsBack"))).HAlign(HAlign_Center).ContentPadding(FMargin(12, 9))
                .OnClicked_Lambda([this]() { CloseSettings(true); return FReply::Handled(); })
                [ Label(NSLOCTEXT("ValStrike", "SettingsBack", "Back  ·  Esc"), 15) ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
        [ Label(NSLOCTEXT("ValStrike", "SettingsKeys", "Tab: navigate     Left/Right: change     Enter: select     Esc: back"), 12, MenuMuted) ];
    SettingsNavigation = {Slider, SensitivityInput, ModeButton, ResolutionButton, ApplyButton, BackButton};
    return Page;
}

TSharedRef<SWidget> SVSSessionMenu::MakeSelector(FName Tag, TSharedPtr<SButton>& OutValueButton, TFunction<FText()> Value,
    TFunction<void(int32)> Cycle, TFunction<bool()> Enabled)
{
    // Only the value is a Tab stop; Left/Right change it. The arrows are for mouse use.
    const auto Arrow = [Tag, Cycle, Enabled](int32 Direction, const TCHAR* Glyph) {
        return SNew(SButton).Tag(FName(*FString::Printf(TEXT("%s.%s"), *Tag.ToString(), Direction < 0 ? TEXT("Previous") : TEXT("Next"))))
            .IsFocusable(false).HAlign(HAlign_Center).ContentPadding(FMargin(14, 7))
            .IsEnabled_Lambda(Enabled)
            .OnClicked_Lambda([Cycle, Direction]() { Cycle(Direction); return FReply::Handled(); })
            [ Label(FText::FromString(Glyph), 17) ];
    };
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth() [ Arrow(-1, TEXT("‹")) ]
        + SHorizontalBox::Slot().FillWidth(1).Padding(6, 0)
        [
            SAssignNew(OutValueButton, SButton).Tag(Tag).HAlign(HAlign_Center).VAlign(VAlign_Center).ContentPadding(FMargin(12, 8))
            .IsEnabled_Lambda(Enabled)
            .OnClicked_Lambda([Cycle]() { Cycle(1); return FReply::Handled(); })
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 15)).Text_Lambda(Value)
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth() [ Arrow(1, TEXT("›")) ];
}

bool SVSSessionMenu::IsConnecting() const
{
    const UVSSessionState* State = Controller.IsValid() ? Controller->GetSessionState() : nullptr;
    return State && State->bConnecting;
}

const TArray<TSharedPtr<SWidget>>& SVSSessionMenu::ActiveNavigation() const
{
    return bSettingsOpen ? SettingsNavigation : Navigation;
}

TSharedRef<SWidget> SVSSessionMenu::InitialFocus() const
{
    if (bSettingsOpen)
    {
        return SettingsNavigation[0].ToSharedRef();
    }
    return Navigation[IsConnecting() ? 2 : 3].ToSharedRef();
}

void SVSSessionMenu::FocusWidget(const TSharedPtr<SWidget>& Widget) const
{
    if (Widget && FSlateApplication::IsInitialized())
    {
        // Slate caches enabled attributes; refresh before assigning focus.
        Widget->UpdateAllAttributes();
        FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::SetDirectly);
    }
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

void SVSSessionMenu::OpenSettings()
{
    if (bSettingsOpen || IsConnecting() || !Pages)
    {
        return;
    }
    const VSUserSettings::FWindowState Current = VSUserSettings::CurrentWindowState();
    PendingMode = Current.Mode;
    WindowedResolution = Current.Mode == EWindowMode::Windowed ? Current.Resolution : FIntPoint::ZeroValue;
    RefreshResolutionOptions(Current.Resolution);
    SetSettingsStatus(FText::GetEmpty(), false);
    SyncSensitivityText();
    bSettingsOpen = true;
    Pages->SetActiveWidgetIndex(1);
    for (const TSharedPtr<SWidget>& Widget : SettingsNavigation)
    {
        Widget->UpdateAllAttributes();
    }
    FocusWidget(SettingsNavigation[0]);
}

void SVSSessionMenu::CloseSettings(bool bFocusSettingsButton)
{
    if (!bSettingsOpen || !Pages)
    {
        return;
    }
    // A typed value counts when the player leaves the page without pressing Enter.
    CommitSensitivityText(false);
    bSettingsOpen = false;
    Pages->SetActiveWidgetIndex(0);
    if (Controller.IsValid())
    {
        Controller->SaveUserSettings();
    }
    if (bFocusSettingsButton)
    {
        FocusWidget(SettingsButton);
    }
}

FIntPoint SVSSessionMenu::GetPendingResolution() const
{
    return ResolutionOptions.IsValidIndex(ResolutionIndex) ? ResolutionOptions[ResolutionIndex] : FIntPoint::ZeroValue;
}

float SVSSessionMenu::GetSensitivity() const
{
    return Controller.IsValid() ? Controller->GetMouseSensitivity() : VSUserSettings::DefaultMouseSensitivity;
}

void SVSSessionMenu::SetSensitivity(float Value)
{
    if (Controller.IsValid())
    {
        Controller->SetMouseSensitivity(Value);
    }
    SyncSensitivityText();
}

void SVSSessionMenu::SyncSensitivityText()
{
    if (SensitivityInput)
    {
        SensitivityInput->SetText(FText::FromString(VSUserSettings::FormatMouseSensitivity(GetSensitivity())));
    }
}

void SVSSessionMenu::CommitSensitivityText(bool bReportErrors)
{
    if (!SensitivityInput)
    {
        return;
    }
    const FString Text = SensitivityInput->GetText().ToString();
    if (Text == VSUserSettings::FormatMouseSensitivity(GetSensitivity()))
    {
        return;
    }
    float Value = GetSensitivity();
    if (VSUserSettings::ParseMouseSensitivity(Text, Value))
    {
        SetSensitivity(Value);
        if (bSettingsError)
        {
            SetSettingsStatus(FText::GetEmpty(), false);
        }
        return;
    }
    SyncSensitivityText();
    if (bReportErrors)
    {
        SetSettingsStatus(FText::Format(NSLOCTEXT("ValStrike", "SensitivityInvalid", "Enter a number from {0} to {1}."),
            FText::FromString(VSUserSettings::FormatMouseSensitivity(VSUserSettings::MinMouseSensitivity)),
            FText::FromString(VSUserSettings::FormatMouseSensitivity(VSUserSettings::MaxMouseSensitivity))), true);
    }
}

bool SVSSessionMenu::CanChooseResolution() const
{
    return PendingMode != EWindowMode::WindowedFullscreen && ResolutionOptions.Num() > 1;
}

void SVSSessionMenu::RefreshResolutionOptions(FIntPoint Preferred)
{
    ResolutionOptions = VSUserSettings::ResolutionOptions(PendingMode, WindowedResolution);
    ResolutionIndex = VSUserSettings::ClosestResolutionIndex(ResolutionOptions, Preferred);
    if (ResolutionButton)
    {
        ResolutionButton->UpdateAllAttributes();
    }
}

void SVSSessionMenu::CycleWindowMode(int32 Direction)
{
    const FIntPoint Previous = GetPendingResolution();
    PendingMode = VSUserSettings::CycleWindowMode(PendingMode, Direction);
    RefreshResolutionOptions(Previous);
    SetSettingsStatus(NSLOCTEXT("ValStrike", "DisplayPending", "Press Apply display to switch."), false);
}

void SVSSessionMenu::CycleResolution(int32 Direction)
{
    if (!CanChooseResolution())
    {
        return;
    }
    ResolutionIndex = (ResolutionIndex + (Direction < 0 ? -1 : 1) + ResolutionOptions.Num()) % ResolutionOptions.Num();
    SetSettingsStatus(NSLOCTEXT("ValStrike", "DisplayPending", "Press Apply display to switch."), false);
}

FReply SVSSessionMenu::ApplyDisplay()
{
    const FIntPoint Resolution = GetPendingResolution();
    if (VSUserSettings::ApplyWindowSettings(PendingMode, Resolution))
    {
        if (PendingMode == EWindowMode::Windowed)
        {
            WindowedResolution = Resolution;
        }
        SetSettingsStatus(FText::Format(NSLOCTEXT("ValStrike", "DisplayApplied", "Applied: {0}  ·  {1}"),
            VSUserSettings::WindowModeText(PendingMode), VSUserSettings::ResolutionText(Resolution)), false);
    }
    else
    {
        SetSettingsStatus(NSLOCTEXT("ValStrike", "DisplayFailed", "Display settings could not be applied."), true);
    }
    return FReply::Handled();
}

void SVSSessionMenu::SetSettingsStatus(const FText& Message, bool bError)
{
    SettingsStatus = Message;
    bSettingsError = bError;
}

FReply SVSSessionMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape)
    {
        if (!Event.IsRepeat() && Controller.IsValid())
        {
            if (bSettingsOpen) CloseSettings(true);
            else if (IsConnecting()) Controller->CancelConnection();
            else Controller->SetSessionMenuOpen(false);
        }
        return FReply::Handled();
    }
    if (bSettingsOpen && (Event.GetKey() == EKeys::Left || Event.GetKey() == EKeys::Right))
    {
        const int32 Direction = Event.GetKey() == EKeys::Left ? -1 : 1;
        if (ModeButton && ModeButton->HasKeyboardFocus())
        {
            CycleWindowMode(Direction);
            return FReply::Handled();
        }
        if (ResolutionButton && ResolutionButton->HasKeyboardFocus())
        {
            CycleResolution(Direction);
            return FReply::Handled();
        }
    }
    if (Event.GetKey() == EKeys::Tab)
    {
        const TArray<TSharedPtr<SWidget>>& Items = ActiveNavigation();
        int32 Focused = INDEX_NONE;
        for (int32 Index = 0; Index < Items.Num(); ++Index)
            if (Items[Index]->HasKeyboardFocus() || Items[Index]->HasFocusedDescendants()) Focused = Index;
        const int32 Direction = Event.IsShiftDown() ? -1 : 1;
        for (int32 Step = 1; Step <= Items.Num(); ++Step)
        {
            const int32 Next = (Focused + Direction * Step + Items.Num() * 2) % Items.Num();
            if (Items[Next]->IsEnabled())
                return FReply::Handled().SetUserFocus(Items[Next].ToSharedRef(), EFocusCause::Navigation);
        }
        return FReply::Handled();
    }
    return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
}
