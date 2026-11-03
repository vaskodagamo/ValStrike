#include "VSSettingsSmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "SVSSessionMenu.h"
#include "VSUserSettings.h"
#include "../VSPlayerController.h"
#include "../ValStrike.h"
#include "Camera/PlayerCameraManager.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Layout/Children.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
// Raw MouseX/MouseY counts injected per look check. With the engine's default
// mouse axis scale this turns 70 degrees per 1.0 sensitivity (8.4 at 0.12), the
// same relationship the mouse-look regression asserts.
constexpr float SettingsLookCounts = 1000.0f;
constexpr float SettingsDegreesPerSensitivity = 70.0f;
constexpr float SettingsChosenSensitivity = 0.36f;
const FIntPoint SettingsChosenWindow(1280, 720);
const FIntPoint SettingsInitialWindow(960, 540);

class FSettingsSmoke
{
public:
    FSettingsSmoke()
    {
        bEnabled = FParse::Param(FCommandLine::Get(), TEXT("VSSettingsSmoke"));
        FParse::Value(FCommandLine::Get(), TEXT("VSSettingsSmokePhase="), Phase);
        bExclusiveFullscreen = FParse::Param(FCommandLine::Get(), TEXT("VSSettingsSmokeExclusiveFullscreen"));
        ScreenshotDirectory = FPaths::ProjectSavedDir() / TEXT("Tests/Settings/Screenshots");
        FParse::Value(FCommandLine::Get(), TEXT("VSSettingsSmokeScreenshotDir="), ScreenshotDirectory);
        bRestart = Phase == TEXT("restart");
    }

    void Tick(AVSPlayerController* PC)
    {
        if (!bEnabled || bFinished || !PC || !PC->IsLocalController() || !PC->GetWorld()->IsGameWorld())
            return;
        const double Now = FPlatformTime::Seconds();
        if (Started == 0.0)
        {
            Started = Now;
            Entered = Now;
            if (Phase != TEXT("first") && Phase != TEXT("restart"))
            {
                Check(TEXT("known_phase"), false, Phase);
                Finish(PC);
                return;
            }
        }
        if (Now - Started > 100.0)
        {
            Check(TEXT("runtime_completed_before_deadline"), false, FString::Printf(TEXT("stage=%d"), Stage));
            Finish(PC);
            return;
        }
        Menu = PC->GetSessionMenu();
        const double Age = Now - Entered;
        ++StageTicks;
        if (bRestart)
            TickRestart(PC, Age, Now);
        else
            TickFirst(PC, Age, Now);
        // Never keep Slate widgets alive in this static object (engine exit).
        Menu.Reset();
    }

private:
    void TickFirst(AVSPlayerController* PC, double Age, double Now)
    {
        switch (Stage)
        {
        case 0:
            if (!PC->GetPawn() || !PC->GetPawn()->InputComponent || !Menu || Age < 2.0) return;
            {
                const FConfigFile* File = GConfig ? GConfig->FindConfigFile(GGameUserSettingsIni) : nullptr;
                FString Stored;
                Check(TEXT("fresh_settings_have_no_sensitivity"), File && !File->GetString(VSUserSettings::ConfigSection, VSUserSettings::MouseSensitivityKey, Stored), Stored);
            }
            Check(TEXT("unset_sensitivity_uses_default"), FMath::IsNearlyEqual(PC->GetMouseSensitivity(), VSUserSettings::DefaultMouseSensitivity),
                FString::Printf(TEXT("%.4f"), PC->GetMouseSensitivity()));
            {
                const VSUserSettings::FWindowState Window = LogWindow(TEXT("initial"), PC);
                Check(TEXT("initial_window_is_windowed_960x540"), Window.Mode == EWindowMode::Windowed && Window.Resolution == SettingsInitialWindow, Describe(Window));
            }
            BeginLook(PC, Now);
            break;
        case 1: case 2:
            if (!LookStep(PC, Age, Now)) return;
            if (Stage != 3) return;
            BaselineYaw = LookYaw;
            CheckLook(TEXT("default_sensitivity_turns_expected_degrees"), VSUserSettings::DefaultMouseSensitivity);
            Screenshot(TEXT("baseline"));
            break;
        case 3:
            // Let the baseline frame render before the menu opens.
            if (Age < 0.4) return;
            Key(PC, EKeys::Escape, true);
            Next(Now);
            break;
        case 4:
            if (Age < 0.15) return;
            Key(PC, EKeys::Escape, false);
            Check(TEXT("escape_opens_menu"), PC->IsSessionMenuOpen());
            Next(Now);
            break;
        case 5:
            if (Age < 0.4) return;
            Screenshot(TEXT("menu"));
            Check(TEXT("menu_focuses_resume"), FocusedTag() == NAME_None && FocusedText() == TEXT("Resume  ·  Esc"), FocusedText());
            Stage = 50;
            Entered = Now;
            break;
        case 50:
            // Let the session-page screenshot render before Settings opens.
            if (Age < 0.4) return;
            SlateKey(EKeys::Tab);
            Check(TEXT("tab_reaches_settings_button"), IsFocused(TEXT("VS.Settings")), FocusedTag().ToString());
            SlateKey(EKeys::Enter);
            Stage = 6;
            Entered = Now;
            break;
        case 6:
            if (Age < 0.3) return;
            Check(TEXT("enter_opens_settings"), Menu->IsSettingsOpen());
            Check(TEXT("settings_focuses_slider"), IsFocused(TEXT("VS.SensitivitySlider")), FocusedTag().ToString());
            Check(TEXT("settings_show_default_sensitivity"), SliderValue() == VSUserSettings::DefaultMouseSensitivity && ValueText() == TEXT("0.120"),
                FString::Printf(TEXT("slider=%.4f text=%s"), SliderValue(), *ValueText()));
            Check(TEXT("settings_show_current_window"), Menu->GetPendingWindowMode() == EWindowMode::Windowed && Menu->GetPendingResolution() == SettingsInitialWindow,
                FString::Printf(TEXT("mode=%d pending=%s options=%s"), int32(Menu->GetPendingWindowMode()), *Menu->GetPendingResolution().ToString(), *Options()));
            Next(Now);
            break;
        case 7:
            if (Age < 0.4) return;
            Screenshot(TEXT("settings"));
            Next(Now);
            break;
        case 8:
            if (Age < 0.3) return;
            SlateKey(EKeys::Right);
            Next(Now);
            break;
        case 9:
            if (Age < 0.2) return;
            Check(TEXT("slider_right_arrow_applies_immediately"), PC->GetMouseSensitivity() == 0.13f && ValueText() == TEXT("0.130"),
                FString::Printf(TEXT("sensitivity=%.4f text=%s"), PC->GetMouseSensitivity(), *ValueText()));
            SlateKey(EKeys::Left);
            Next(Now);
            break;
        case 10:
            if (Age < 0.2) return;
            Check(TEXT("slider_left_arrow_steps_back"), PC->GetMouseSensitivity() == VSUserSettings::DefaultMouseSensitivity,
                FString::Printf(TEXT("%.4f"), PC->GetMouseSensitivity()));
            SlateKey(EKeys::Tab);
            Next(Now);
            break;
        case 11:
            if (Age < 0.2) return;
            Check(TEXT("tab_reaches_sensitivity_value"), IsFocused(TEXT("VS.SensitivityValue")), FocusedTag().ToString());
            Type(TEXT("5"));
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 12:
            if (Age < 0.2) return;
            Check(TEXT("typed_value_clamps_to_maximum"), PC->GetMouseSensitivity() == VSUserSettings::MaxMouseSensitivity && ValueText() == TEXT("1.000"),
                FString::Printf(TEXT("sensitivity=%.4f text=%s"), PC->GetMouseSensitivity(), *ValueText()));
            Type(TEXT("abc"));
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 13:
            if (Age < 0.3) return;
            Check(TEXT("invalid_text_rejected_with_error"), PC->GetMouseSensitivity() == VSUserSettings::MaxMouseSensitivity
                && ValueText() == TEXT("1.000") && Menu->HasSettingsError(), Menu->GetSettingsStatus().ToString());
            Screenshot(TEXT("settings-invalid"));
            Next(Now);
            break;
        case 14:
            if (Age < 0.4) return;
            Type(TEXT("0.36"));
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 15:
            if (Age < 0.2) return;
            Check(TEXT("typed_value_applies_immediately"), PC->GetMouseSensitivity() == SettingsChosenSensitivity && ValueText() == TEXT("0.360")
                && SliderValue() == SettingsChosenSensitivity && !Menu->HasSettingsError(),
                FString::Printf(TEXT("sensitivity=%.4f slider=%.4f text=%s"), PC->GetMouseSensitivity(), SliderValue(), *ValueText()));
            Check(TEXT("sensitivity_in_game_user_settings"), StoredSensitivity() == TEXT("0.360"), StoredSensitivity());
            SlateKey(EKeys::Tab);
            Next(Now);
            break;
        case 16:
            if (Age < 0.2) return;
            Check(TEXT("tab_reaches_window_mode"), IsFocused(TEXT("VS.WindowMode")), FocusedTag().ToString());
            SlateKey(EKeys::Left);
            Next(Now);
            break;
        case 17:
            if (Age < 0.2) return;
            Check(TEXT("left_selects_windowed_fullscreen"), Menu->GetPendingWindowMode() == EWindowMode::WindowedFullscreen,
                FString::Printf(TEXT("mode=%d"), int32(Menu->GetPendingWindowMode())));
            Check(TEXT("window_mode_waits_for_apply"), VSUserSettings::CurrentWindowState().Mode == EWindowMode::Windowed);
            Screenshot(TEXT("settings-pending-windowed-fullscreen"));
            Next(Now);
            break;
        case 18:
            if (Age < 0.4) return;
            SlateKey(EKeys::Tab);
            Check(TEXT("tab_skips_fixed_desktop_resolution"), IsFocused(TEXT("VS.ApplyDisplay")), FocusedTag().ToString());
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 19:
            if (!WaitForWindow(EWindowMode::WindowedFullscreen, FIntPoint::ZeroValue, Age)) return;
            {
                const VSUserSettings::FWindowState Window = LogWindow(TEXT("windowed_fullscreen"), PC);
                const FIntPoint Desktop = VSUserSettings::DesktopResolution();
                Check(TEXT("windowed_fullscreen_applied"), Window.Mode == EWindowMode::WindowedFullscreen
                    && Window.Resolution.X > SettingsInitialWindow.X && Window.Resolution.Y > SettingsInitialWindow.Y
                    && Window.Resolution.X <= Desktop.X && Window.Resolution.Y <= Desktop.Y,
                    Describe(Window) + TEXT(" desktop=") + Desktop.ToString());
                Check(TEXT("windowed_fullscreen_saved_to_game_user_settings"), StoredGameUserSettingsMode() == EWindowMode::WindowedFullscreen);
            }
            Screenshot(TEXT("windowed-fullscreen"));
            Next(Now);
            break;
        case 20:
            if (Age < 0.5) return;
            // Apply -> (fixed resolution skipped) -> window mode.
            SlateKey(EKeys::Tab, true);
            Check(TEXT("shift_tab_returns_to_window_mode"), IsFocused(TEXT("VS.WindowMode")), FocusedTag().ToString());
            if (!bExclusiveFullscreen)
            {
                Stage = 30;
                Entered = Now;
                return;
            }
            SlateKey(EKeys::Left);
            Check(TEXT("left_selects_fullscreen"), Menu->GetPendingWindowMode() == EWindowMode::Fullscreen);
            Next(Now);
            break;
        case 21:
            if (Age < 0.3) return;
            LogLine(FString::Printf(TEXT("VSSET_STATE {\"phase\":\"fullscreen_options\",\"pending\":\"%s\",\"options\":\"%s\"}"),
                *Menu->GetPendingResolution().ToString(), *Options()));
            Check(TEXT("click_apply_display"), Click(TEXT("VS.ApplyDisplay")));
            Next(Now);
            break;
        case 22:
            if (!WaitForWindow(EWindowMode::Fullscreen, FIntPoint::ZeroValue, Age)) return;
            {
                const VSUserSettings::FWindowState Window = LogWindow(TEXT("fullscreen"), PC);
                Check(TEXT("exclusive_fullscreen_applied"), Window.Mode == EWindowMode::Fullscreen, Describe(Window));
            }
            Screenshot(TEXT("fullscreen"));
            Next(Now);
            break;
        case 23:
            if (Age < 0.5) return;
            Stage = 30;
            Entered = Now;
            break;
        case 30:
            if (Age < 0.2) return;
            // Mouse: the window-mode arrow, then keyboard for the resolution.
            Check(TEXT("mouse_click_window_mode_arrow"), Click(bExclusiveFullscreen ? TEXT("VS.WindowMode.Previous") : TEXT("VS.WindowMode.Next")));
            Next(Now);
            break;
        case 31:
            if (Age < 0.2) return;
            Check(TEXT("arrow_selects_windowed"), Menu->GetPendingWindowMode() == EWindowMode::Windowed,
                FString::Printf(TEXT("mode=%d"), int32(Menu->GetPendingWindowMode())));
            FocusTagged(TEXT("VS.WindowMode"));
            SlateKey(EKeys::Tab);
            Check(TEXT("tab_reaches_resolution"), IsFocused(TEXT("VS.Resolution")), FocusedTag().ToString());
            {
                const int32 Target = Menu->GetResolutionOptions().Find(SettingsChosenWindow);
                const int32 Current = Menu->GetResolutionOptions().Find(Menu->GetPendingResolution());
                Check(TEXT("windowed_options_include_1280x720"), Target != INDEX_NONE && Current != INDEX_NONE, Options());
                for (int32 Step = 0; Target != INDEX_NONE && Current != INDEX_NONE && Step < FMath::Abs(Target - Current); ++Step)
                    SlateKey(Target > Current ? EKeys::Right : EKeys::Left);
            }
            Check(TEXT("arrow_keys_select_1280x720"), Menu->GetPendingResolution() == SettingsChosenWindow, Menu->GetPendingResolution().ToString());
            Next(Now);
            break;
        case 32:
            if (Age < 0.3) return;
            Screenshot(TEXT("settings-pending-windowed"));
            Next(Now);
            break;
        case 33:
            if (Age < 0.4) return;
            SlateKey(EKeys::Tab);
            Check(TEXT("tab_reaches_apply"), IsFocused(TEXT("VS.ApplyDisplay")), FocusedTag().ToString());
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 34:
            if (!WaitForWindow(EWindowMode::Windowed, SettingsChosenWindow, Age)) return;
            {
                const VSUserSettings::FWindowState Window = LogWindow(TEXT("windowed_1280x720"), PC);
                Check(TEXT("windowed_1280x720_applied"), Window.Mode == EWindowMode::Windowed && Window.Resolution == SettingsChosenWindow, Describe(Window));
                Check(TEXT("windowed_saved_to_game_user_settings"), StoredGameUserSettingsMode() == EWindowMode::Windowed);
                Check(TEXT("apply_reports_result"), !Menu->HasSettingsError() && Menu->GetSettingsStatus().ToString().StartsWith(TEXT("Applied: Windowed")),
                    Menu->GetSettingsStatus().ToString());
            }
            Screenshot(TEXT("windowed-1280x720"));
            Next(Now);
            break;
        case 35:
            if (Age < 0.5) return;
            // Windowed mouse use, after macOS has activated the game for the
            // fullscreen round trip. A window on an inactive Space is not hit-testable.
            Check(TEXT("mouse_click_back_button"), Click(TEXT("VS.SettingsBack")));
            Next(Now);
            break;
        case 36:
            if (Age < 0.3) return;
            Check(TEXT("mouse_back_returns_to_session_page"), !Menu->IsSettingsOpen() && PC->IsSessionMenuOpen());
            Check(TEXT("mouse_click_settings_button"), Click(TEXT("VS.Settings")));
            Stage = 360;
            Entered = Now;
            break;
        case 360:
            if (Age < 0.3) return;
            Check(TEXT("mouse_click_opens_settings"), Menu->IsSettingsOpen());
            Check(TEXT("reopened_settings_show_applied_window"), Menu->GetPendingWindowMode() == EWindowMode::Windowed
                && Menu->GetPendingResolution() == SettingsChosenWindow, Menu->GetPendingResolution().ToString());
            SlateKey(EKeys::Escape);
            Check(TEXT("escape_returns_to_session_page"), !Menu->IsSettingsOpen() && PC->IsSessionMenuOpen(), FocusedText());
            Check(TEXT("back_focuses_settings_button"), IsFocused(TEXT("VS.Settings")), FocusedTag().ToString());
            Stage = 361;
            Entered = Now;
            break;
        case 361:
            if (Age < 0.3) return;
            SlateKey(EKeys::Escape);
            Stage = 37;
            Entered = Now;
            break;
        case 37:
            if (Age < 0.3) return;
            Check(TEXT("escape_resumes_with_game_input"), !PC->IsSessionMenuOpen() && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor);
            BeginLook(PC, Now);
            Stage = 38;
            break;
        case 38: case 39:
            if (!LookStep(PC, Age, Now, 38)) return;
            if (Stage != 40) return;
            CheckLook(TEXT("new_sensitivity_scales_look"), SettingsChosenSensitivity);
            Check(TEXT("look_ratio_matches_sensitivity_ratio"), BaselineYaw > 0.0f
                && FMath::IsNearlyEqual(LookYaw / BaselineYaw, SettingsChosenSensitivity / VSUserSettings::DefaultMouseSensitivity, 0.01f),
                FString::Printf(TEXT("ratio=%.4f"), BaselineYaw > 0.0f ? LookYaw / BaselineYaw : 0.0f));
            Screenshot(TEXT("after-resume"));
            Next(Now);
            break;
        case 41:
            if (Age < 0.5) return;
            Finish(PC);
            break;
        }
    }

    void TickRestart(AVSPlayerController* PC, double Age, double Now)
    {
        switch (Stage)
        {
        case 0:
            if (!PC->GetPawn() || !PC->GetPawn()->InputComponent || !Menu || Age < 2.0) return;
            Check(TEXT("restart_loads_saved_sensitivity"), PC->GetMouseSensitivity() == SettingsChosenSensitivity,
                FString::Printf(TEXT("%.4f"), PC->GetMouseSensitivity()));
            {
                const VSUserSettings::FWindowState Window = LogWindow(TEXT("restart"), PC);
                Check(TEXT("restart_restores_windowed_1280x720"), Window.Mode == EWindowMode::Windowed && Window.Resolution == SettingsChosenWindow, Describe(Window));
            }
            BeginLook(PC, Now);
            break;
        case 1: case 2:
            if (!LookStep(PC, Age, Now)) return;
            if (Stage != 3) return;
            CheckLook(TEXT("restart_look_uses_saved_sensitivity"), SettingsChosenSensitivity);
            Key(PC, EKeys::Escape, true);
            break;
        case 3:
            if (Age < 0.15) return;
            Key(PC, EKeys::Escape, false);
            Check(TEXT("restart_escape_opens_menu"), PC->IsSessionMenuOpen());
            Next(Now);
            break;
        case 4:
            if (Age < 0.4) return;
            SlateKey(EKeys::Tab);
            Check(TEXT("restart_tab_reaches_settings_button"), IsFocused(TEXT("VS.Settings")), FocusedTag().ToString());
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 5:
            if (Age < 0.3) return;
            Check(TEXT("restart_enter_opens_settings"), Menu->IsSettingsOpen());
            Check(TEXT("restart_settings_show_saved_values"), SliderValue() == SettingsChosenSensitivity && ValueText() == TEXT("0.360")
                && Menu->GetPendingWindowMode() == EWindowMode::Windowed && Menu->GetPendingResolution() == SettingsChosenWindow,
                FString::Printf(TEXT("slider=%.4f text=%s mode=%d pending=%s"), SliderValue(), *ValueText(),
                    int32(Menu->GetPendingWindowMode()), *Menu->GetPendingResolution().ToString()));
            Next(Now);
            break;
        case 6:
            if (Age < 0.4) return;
            Screenshot(TEXT("settings-after-restart"));
            Next(Now);
            break;
        case 7:
            if (Age < 0.4) return;
            SlateKey(EKeys::Escape);
            SlateKey(EKeys::Escape);
            Next(Now);
            break;
        case 8:
            if (Age < 0.3) return;
            Check(TEXT("restart_escape_twice_resumes"), !PC->IsSessionMenuOpen() && !PC->IsLookInputIgnored());
            Next(Now);
            break;
        case 9:
            if (Age < 0.3) return;
            Finish(PC);
            break;
        }
    }

    // Look measurement: reset, inject one MouseX and one MouseY event, read back.
    void BeginLook(AVSPlayerController* PC, double Now)
    {
        PC->FlushPressedKeys();
        PC->SetControlRotation(FRotator::ZeroRotator);
        Next(Now);
    }

    bool LookStep(AVSPlayerController* PC, double Age, double Now, int32 FirstStage = 1)
    {
        // Measure the change across the injection over a few frames only, so a
        // physical mouse touched during the run has little chance to interfere.
        if (Stage == FirstStage)
        {
            if (Age < 0.3) return false;
            LookStart = PC->GetControlRotation();
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, SettingsLookCounts, 1));
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, SettingsLookCounts, 1));
            Next(Now);
            return false;
        }
        if (StageTicks < 3) return false;
        const FRotator Control = PC->GetControlRotation();
        const FRotator Delta = (Control - LookStart).GetNormalized();
        const FRotator Camera = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().GetNormalized() : FRotator::ZeroRotator;
        LookYaw = Delta.Yaw;
        LookPitch = Delta.Pitch;
        LogLine(FString::Printf(TEXT("VSSET_LOOK {\"sensitivity\":%.4f,\"ticks\":%d,\"delta_yaw\":%.4f,\"delta_pitch\":%.4f,\"control_yaw\":%.4f,\"control_pitch\":%.4f,\"camera_yaw\":%.4f,\"camera_pitch\":%.4f}"),
            PC->GetMouseSensitivity(), StageTicks, Delta.Yaw, Delta.Pitch, Control.GetNormalized().Yaw, Control.GetNormalized().Pitch, Camera.Yaw, Camera.Pitch));
        Next(Now);
        return true;
    }

    void CheckLook(const TCHAR* Name, float Sensitivity)
    {
        const float Expected = Sensitivity * SettingsDegreesPerSensitivity;
        Check(Name, FMath::IsNearlyEqual(LookYaw, Expected, 0.05f) && FMath::IsNearlyEqual(LookPitch, Expected, 0.05f),
            FString::Printf(TEXT("expected=%.3f yaw=%.3f pitch=%.3f"), Expected, LookYaw, LookPitch));
    }

    bool WaitForWindow(EWindowMode::Type Mode, FIntPoint Size, double Age)
    {
        // macOS animates Spaces transitions; allow it to settle, then check once.
        const VSUserSettings::FWindowState Window = VSUserSettings::CurrentWindowState();
        const bool bReached = Window.Mode == Mode && (Size == FIntPoint::ZeroValue || Window.Resolution == Size);
        if (bReached && ReachedAt == 0.0) ReachedAt = Age;
        if (ReachedAt > 0.0 ? Age - ReachedAt < 1.5 : Age < 10.0) return false;
        LogLine(FString::Printf(TEXT("VSSET_TRANSITION {\"mode\":%d,\"reached\":%s,\"seconds\":%.2f}"),
            int32(Mode), ReachedAt > 0.0 ? TEXT("true") : TEXT("false"), ReachedAt));
        ReachedAt = 0.0;
        return true;
    }

    VSUserSettings::FWindowState LogWindow(const TCHAR* Name, AVSPlayerController* PC) const
    {
        const VSUserSettings::FWindowState Window = VSUserSettings::CurrentWindowState();
        const FIntPoint Desktop = VSUserSettings::DesktopResolution();
        const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
        const FIntPoint Stored = Settings ? Settings->GetScreenResolution() : FIntPoint::ZeroValue;
        LogLine(FString::Printf(TEXT("VSSET_WINDOW {\"phase\":\"%s\",\"mode\":%d,\"viewport\":[%d,%d],\"desktop\":[%d,%d],\"settings_mode\":%d,\"settings_resolution\":[%d,%d]}"),
            Name, int32(Window.Mode), Window.Resolution.X, Window.Resolution.Y, Desktop.X, Desktop.Y,
            Settings ? int32(Settings->GetFullscreenMode()) : -1, Stored.X, Stored.Y));
        return Window;
    }

    static FString Describe(const VSUserSettings::FWindowState& Window)
    {
        return FString::Printf(TEXT("mode=%d viewport=%s"), int32(Window.Mode), *Window.Resolution.ToString());
    }

    static int32 StoredGameUserSettingsMode()
    {
        int32 Mode = -1;
        if (GConfig) GConfig->GetInt(TEXT("/Script/Engine.GameUserSettings"), TEXT("FullscreenMode"), Mode, GGameUserSettingsIni);
        return Mode;
    }

    static FString StoredSensitivity()
    {
        FString Stored;
        if (const FConfigFile* File = GConfig ? GConfig->FindConfigFile(GGameUserSettingsIni) : nullptr)
            File->GetString(VSUserSettings::ConfigSection, VSUserSettings::MouseSensitivityKey, Stored);
        return Stored;
    }

    FString Options() const
    {
        TArray<FString> Items;
        if (Menu)
            for (const FIntPoint& Size : Menu->GetResolutionOptions()) Items.Add(Size.ToString());
        return FString::Join(Items, TEXT(" | "));
    }

    static TSharedPtr<SWidget> FindTagged(const TSharedRef<SWidget>& Root, FName Tag)
    {
        if (Root->GetTag() == Tag) return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
            if (TSharedPtr<SWidget> Found = FindTagged(Children->GetChildAt(Index), Tag)) return Found;
        return nullptr;
    }

    TSharedPtr<SWidget> Tagged(const TCHAR* Tag) const
    {
        return Menu ? FindTagged(Menu.ToSharedRef(), FName(Tag)) : nullptr;
    }

    float SliderValue() const
    {
        const TSharedPtr<SWidget> Widget = Tagged(TEXT("VS.SensitivitySlider"));
        return Widget ? StaticCastSharedPtr<SSlider>(Widget)->GetValue() : -1.0f;
    }

    FString ValueText() const
    {
        const TSharedPtr<SWidget> Widget = Tagged(TEXT("VS.SensitivityValue"));
        return Widget ? StaticCastSharedPtr<SEditableTextBox>(Widget)->GetText().ToString() : FString();
    }

    static TSharedPtr<SWidget> Focused()
    {
        return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetUserFocusedWidget(0) : nullptr;
    }

    static FName FocusedTag()
    {
        const TSharedPtr<SWidget> Widget = Focused();
        return Widget ? Widget->GetTag() : FName(TEXT("<none>"));
    }

    static FString FocusedText()
    {
        const TSharedPtr<SWidget> Widget = Focused();
        if (Widget && Widget->GetChildren() && Widget->GetChildren()->Num() == 1)
        {
            const TSharedRef<SWidget> Content = Widget->GetChildren()->GetChildAt(0);
            if (Content->GetTypeAsString() == TEXT("STextBlock"))
                return StaticCastSharedRef<STextBlock>(Content)->GetText().ToString();
        }
        return Widget ? Widget->GetTypeAsString() : TEXT("<none>");
    }

    bool IsFocused(const TCHAR* Tag) const
    {
        // Composite widgets (SEditableTextBox) give focus to an inner child.
        const TSharedPtr<SWidget> Widget = Tagged(Tag);
        return Widget && (Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants());
    }

    void FocusTagged(const TCHAR* Tag) const
    {
        if (const TSharedPtr<SWidget> Widget = Tagged(Tag))
            FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
    }

    // Real Slate hit-testing at the widget's painted centre; the OS cursor is not moved.
    bool Click(const TCHAR* Tag) const
    {
        const TSharedPtr<SWidget> Widget = Tagged(Tag);
        if (!Widget || !FSlateApplication::IsInitialized()) return false;
        // Tick-space geometry is desktop space, which Slate's hit-testing uses.
        const FVector2D Position = FVector2D(Widget->GetTickSpaceGeometry().GetAbsolutePositionAtCoordinates(FVector2f(0.5f, 0.5f)));
        const TSet<FKey> None;
        const TSet<FKey> Left = {EKeys::LeftMouseButton};
        FSlateApplication& App = FSlateApplication::Get();
        const TSharedPtr<SWindow> Window = App.FindWidgetWindow(Widget.ToSharedRef());
        const bool bHitTestable = Window && Window->IsScreenspaceMouseWithin(Position);
        App.ProcessMouseMoveEvent(FPointerEvent(FSlateApplicationBase::CursorPointerIndex, Position, Position, None, EKeys::Invalid, 0.0f, FModifierKeysState()));
        App.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(FSlateApplicationBase::CursorPointerIndex, Position, Position, Left, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
        App.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplicationBase::CursorPointerIndex, Position, Position, None, EKeys::LeftMouseButton, 0.0f, FModifierKeysState()));
        LogLine(FString::Printf(TEXT("VSSET_CLICK %s at (%.1f, %.1f) hit_testable=%s"), Tag, Position.X, Position.Y,
            bHitTestable ? TEXT("true") : TEXT("false")));
        return bHitTestable;
    }

    static void Type(const TCHAR* Text)
    {
        for (const TCHAR* Character = Text; *Character; ++Character)
            FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(*Character, FModifierKeysState(), 0, false));
    }

    static void Key(AVSPlayerController* PC, FKey Name, bool bDown)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
    }

    static void SlateKey(FKey Name, bool bShift = false)
    {
        const FModifierKeysState Modifiers(bShift, false, false, false, false, false, false, false, false);
        const FKeyEvent Event(Name, Modifiers, 0, false, 0, 0);
        FSlateApplication::Get().ProcessKeyDownEvent(Event);
        FSlateApplication::Get().ProcessKeyUpEvent(Event);
    }

    void Screenshot(const TCHAR* Name) const
    {
        IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
        const FString Path = ScreenshotDirectory / (FString(Name) + TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        LogLine(FString::Printf(TEXT("VSSET_SCREENSHOT %s"), *Path));
    }

    static void LogLine(const FString& Line)
    {
        UE_LOG(LogValStrike, Display, TEXT("%s"), *Line);
    }

    void Next(double Now) { ++Stage; Entered = Now; StageTicks = 0; }

    void Check(const TCHAR* Name, bool bPassed, const FString& Detail = FString())
    {
        ++Checks;
        if (!bPassed) ++Failures;
        UE_LOG(LogValStrike, Display, TEXT("VSSET_CHECK %s %s %s"), Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail.Replace(TEXT("\n"), TEXT(" ")));
    }

    void Finish(AVSPlayerController* PC)
    {
        bFinished = true;
        UE_LOG(LogValStrike, Display, TEXT("VSSET_DONE checks=%d failures=%d"), Checks, Failures);
        PC->QuitSession();
    }

    bool bEnabled = false;
    bool bFinished = false;
    bool bRestart = false;
    bool bExclusiveFullscreen = false;
    FString Phase;
    FString ScreenshotDirectory;
    TSharedPtr<SVSSessionMenu> Menu;
    int32 Stage = 0;
    int32 StageTicks = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    double Started = 0.0;
    double Entered = 0.0;
    double ReachedAt = 0.0;
    float BaselineYaw = 0.0f;
    float LookYaw = 0.0f;
    float LookPitch = 0.0f;
    FRotator LookStart = FRotator::ZeroRotator;
};
}

void VSSettingsSmoke::Tick(AVSPlayerController* Controller, float DeltaTime)
{
    static FSettingsSmoke SettingsSmoke;
    SettingsSmoke.Tick(Controller);
}

#endif
