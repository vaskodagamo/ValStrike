#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"
#include "Widgets/SCompoundWidget.h"

class AVSPlayerController;
class SButton;
class SEditableTextBox;
class SSlider;
class SWidgetSwitcher;

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

    // Local settings page. Sensitivity and volume apply as they change; leaving
    // the page commits typed text and saves. Window mode changes wait for Apply.
    bool IsSettingsOpen() const { return bSettingsOpen; }
    void OpenSettings();
    void CloseSettings(bool bFocusSettingsButton);
    EWindowMode::Type GetPendingWindowMode() const { return PendingMode; }
    FIntPoint GetPendingResolution() const;
    const TArray<FIntPoint>& GetResolutionOptions() const { return ResolutionOptions; }
    const FText& GetSettingsStatus() const { return SettingsStatus; }
    bool HasSettingsError() const { return bSettingsError; }
    int32 GetMasterVolume() const { return MasterVolume; }

private:
    TSharedRef<SWidget> MakeSessionPage();
    TSharedRef<SWidget> MakeSettingsPage();
    TSharedRef<SWidget> MakeSelector(FName Tag, TSharedPtr<SButton>& OutValueButton, TFunction<FText()> Value,
        TFunction<void(int32)> Cycle, TFunction<bool()> Enabled);
    const TArray<TSharedPtr<SWidget>>& ActiveNavigation() const;
    void FocusWidget(const TSharedPtr<SWidget>& Widget) const;
    bool IsConnecting() const;
    FReply Connect();

    float GetSensitivity() const;
    void SetSensitivity(float Value);
    void SyncSensitivityText();
    void CommitSensitivityText(bool bReportErrors);
    void SetMasterVolume(int32 Percent);
    void SyncVolumeText();
    void CommitVolumeText(bool bReportErrors);
    void CycleWindowMode(int32 Direction);
    void CycleResolution(int32 Direction);
    void RefreshResolutionOptions(FIntPoint Preferred);
    bool CanChooseResolution() const;
    FReply ApplyDisplay();
    void SetSettingsStatus(const FText& Message, bool bError);

    TWeakObjectPtr<AVSPlayerController> Controller;
    TSharedPtr<SWidgetSwitcher> Pages;
    TSharedPtr<SEditableTextBox> AddressInput;
    TSharedPtr<SEditableTextBox> JoinKeyInput;
    TSharedPtr<SButton> CancelButton;
    TSharedPtr<SButton> ResumeButton;
    TSharedPtr<SButton> SettingsButton;
    TSharedPtr<SSlider> SensitivitySlider;
    TSharedPtr<SEditableTextBox> SensitivityInput;
    TSharedPtr<SSlider> VolumeSlider;
    TSharedPtr<SEditableTextBox> VolumeInput;
    TSharedPtr<SButton> ModeButton;
    TSharedPtr<SButton> ResolutionButton;
    TArray<TSharedPtr<SWidget>> Navigation;
    TArray<TSharedPtr<SWidget>> SettingsNavigation;

    bool bSettingsOpen = false;
    // Client-local; the menu owns it because the player controller does not.
    int32 MasterVolume = 100;
    bool bVolumeDirty = false;
    EWindowMode::Type PendingMode = EWindowMode::Windowed;
    TArray<FIntPoint> ResolutionOptions;
    int32 ResolutionIndex = INDEX_NONE;
    // Current window size, offered as a windowed choice even if it is not a preset.
    FIntPoint WindowedResolution = FIntPoint::ZeroValue;
    FText SettingsStatus;
    bool bSettingsError = false;
};
