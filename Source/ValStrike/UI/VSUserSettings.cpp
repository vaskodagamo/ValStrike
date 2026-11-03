#include "VSUserSettings.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

const TCHAR* const VSUserSettings::ConfigSection = TEXT("ValStrike.UserSettings");
const TCHAR* const VSUserSettings::MouseSensitivityKey = TEXT("MouseSensitivity");

float VSUserSettings::ClampMouseSensitivity(float Value)
{
    if (!FMath::IsFinite(Value))
    {
        return DefaultMouseSensitivity;
    }
    const float Clamped = FMath::Clamp(Value, MinMouseSensitivity, MaxMouseSensitivity);
    return FMath::RoundToFloat(Clamped * 1000.0f) / 1000.0f;
}

bool VSUserSettings::ParseMouseSensitivity(const FString& Text, float& OutValue)
{
    FString Number = Text.TrimStartAndEnd();
    Number.ReplaceCharInline(TEXT(','), TEXT('.'));
    if (Number.IsEmpty() || Number.Len() > 16)
    {
        return false;
    }
    int32 Digits = 0;
    int32 Points = 0;
    for (int32 Index = 0; Index < Number.Len(); ++Index)
    {
        const TCHAR Character = Number[Index];
        if (Character >= TEXT('0') && Character <= TEXT('9'))
        {
            ++Digits;
        }
        else if (Character == TEXT('.'))
        {
            ++Points;
        }
        else if (Index != 0 || (Character != TEXT('+') && Character != TEXT('-')))
        {
            return false;
        }
    }
    if (Digits == 0 || Points > 1)
    {
        return false;
    }
    const double Value = FCString::Atod(*Number);
    if (!FMath::IsFinite(Value))
    {
        return false;
    }
    OutValue = ClampMouseSensitivity(static_cast<float>(Value));
    return true;
}

FString VSUserSettings::FormatMouseSensitivity(float Value)
{
    return FString::Printf(TEXT("%.3f"), ClampMouseSensitivity(Value));
}

float VSUserSettings::ReadMouseSensitivity(const FConfigFile& File)
{
    FString Stored;
    float Value = DefaultMouseSensitivity;
    if (File.GetString(ConfigSection, MouseSensitivityKey, Stored) && ParseMouseSensitivity(Stored, Value))
    {
        return Value;
    }
    return DefaultMouseSensitivity;
}

void VSUserSettings::WriteMouseSensitivity(FConfigFile& File, float Value)
{
    File.SetString(ConfigSection, MouseSensitivityKey, *FormatMouseSensitivity(Value));
}

float VSUserSettings::LoadMouseSensitivity()
{
    const FConfigFile* File = GConfig ? GConfig->FindConfigFile(GGameUserSettingsIni) : nullptr;
    return File ? ReadMouseSensitivity(*File) : DefaultMouseSensitivity;
}

void VSUserSettings::StoreMouseSensitivity(float Value)
{
    FConfigFile* File = GConfig ? GConfig->FindConfigFile(GGameUserSettingsIni) : nullptr;
    FString Stored;
    if (File && (!File->GetString(ConfigSection, MouseSensitivityKey, Stored) || Stored != FormatMouseSensitivity(Value)))
    {
        WriteMouseSensitivity(*File, Value);
    }
}

bool VSUserSettings::SaveUserSettings()
{
    if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        // SaveConfig flushes the whole GameUserSettings file, including our section.
        Settings->SaveSettings();
        return true;
    }
    return GConfig && GConfig->Flush(false, GGameUserSettingsIni);
}

EWindowMode::Type VSUserSettings::CycleWindowMode(EWindowMode::Type Mode, int32 Direction)
{
    static constexpr EWindowMode::Type Order[] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
    int32 Index = 2;
    for (int32 Candidate = 0; Candidate < 3; ++Candidate)
    {
        if (Order[Candidate] == Mode)
        {
            Index = Candidate;
        }
    }
    const int32 Step = Direction < 0 ? -1 : Direction > 0 ? 1 : 0;
    return Order[(Index + Step + 3) % 3];
}

FText VSUserSettings::WindowModeText(EWindowMode::Type Mode)
{
    switch (Mode)
    {
    case EWindowMode::Fullscreen:
        return NSLOCTEXT("ValStrike", "ModeFullscreen", "Fullscreen");
    case EWindowMode::WindowedFullscreen:
        return NSLOCTEXT("ValStrike", "ModeWindowedFullscreen", "Windowed Fullscreen");
    default:
        return NSLOCTEXT("ValStrike", "ModeWindowed", "Windowed");
    }
}

FText VSUserSettings::ResolutionText(FIntPoint Resolution)
{
    return FText::FromString(FString::Printf(TEXT("%d × %d"), Resolution.X, Resolution.Y));
}

VSUserSettings::FWindowState VSUserSettings::CurrentWindowState()
{
    FWindowState State;
    UGameViewportClient* Client = GEngine ? GEngine->GameViewport.Get() : nullptr;
    if (Client)
    {
        if (const TSharedPtr<SWindow> Window = Client->GetWindow())
        {
            State.Mode = Window->GetWindowMode();
        }
        if (Client->Viewport)
        {
            State.Resolution = Client->Viewport->GetSizeXY();
        }
    }
    else if (const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        State.Mode = Settings->GetFullscreenMode();
        State.Resolution = Settings->GetScreenResolution();
    }
    return State;
}

FIntPoint VSUserSettings::DesktopResolution()
{
    const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    return Settings ? Settings->GetDesktopResolution() : FIntPoint::ZeroValue;
}

void VSUserSettings::NormalizeResolutions(TArray<FIntPoint>& Resolutions)
{
    Resolutions.RemoveAll([](const FIntPoint& Size) { return Size.X <= 0 || Size.Y <= 0; });
    Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) {
        const int64 AreaA = int64(A.X) * A.Y;
        const int64 AreaB = int64(B.X) * B.Y;
        return AreaA != AreaB ? AreaA < AreaB : A.X < B.X;
    });
    for (int32 Index = Resolutions.Num() - 1; Index > 0; --Index)
    {
        if (Resolutions[Index] == Resolutions[Index - 1])
        {
            Resolutions.RemoveAt(Index);
        }
    }
}

FIntPoint VSUserSettings::WorkAreaSize()
{
    FDisplayMetrics Metrics;
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().GetCachedDisplayMetrics(Metrics);
    }
    else
    {
        FDisplayMetrics::RebuildDisplayMetrics(Metrics);
    }
    const FPlatformRect& Area = Metrics.PrimaryDisplayWorkAreaRect;
    return FIntPoint(Area.Right - Area.Left, Area.Bottom - Area.Top);
}

TArray<FIntPoint> VSUserSettings::StandardWindowedResolutions(FIntPoint WorkArea)
{
    static const FIntPoint Sizes[] = {
        {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3200, 1800}, {3840, 2160},
        {1280, 800}, {1440, 900}, {1680, 1050}, {1920, 1200}, {2560, 1600},
    };
    TArray<FIntPoint> Result;
    for (const FIntPoint& Size : Sizes)
    {
        // Strictly smaller leaves room for the window frame, as the engine does.
        if (Size.X < WorkArea.X && Size.Y < WorkArea.Y)
        {
            Result.Add(Size);
        }
    }
    NormalizeResolutions(Result);
    return Result;
}

TArray<FIntPoint> VSUserSettings::ResolutionOptions(EWindowMode::Type Mode, FIntPoint CurrentWindowed)
{
    TArray<FIntPoint> Options;
    if (Mode == EWindowMode::Windowed)
    {
        Options = StandardWindowedResolutions(WorkAreaSize());
        Options.Add(CurrentWindowed);
    }
    else if (Mode == EWindowMode::Fullscreen)
    {
        UKismetSystemLibrary::GetSupportedFullscreenResolutions(Options);
    }
    if (Mode == EWindowMode::WindowedFullscreen || Options.Num() == 0)
    {
        Options = {DesktopResolution()};
    }
    NormalizeResolutions(Options);
    return Options;
}

int32 VSUserSettings::ClosestResolutionIndex(const TArray<FIntPoint>& Options, FIntPoint Target)
{
    int32 Best = INDEX_NONE;
    int64 BestDistance = MAX_int64;
    for (int32 Index = 0; Index < Options.Num(); ++Index)
    {
        const int64 Distance = FMath::Abs(int64(Options[Index].X) - Target.X) + FMath::Abs(int64(Options[Index].Y) - Target.Y);
        if (Distance < BestDistance)
        {
            Best = Index;
            BestDistance = Distance;
        }
    }
    return Best;
}

bool VSUserSettings::ApplyWindowSettings(EWindowMode::Type Mode, FIntPoint Resolution)
{
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!Settings || Resolution.X <= 0 || Resolution.Y <= 0)
    {
        return false;
    }
    Settings->SetFullscreenMode(Mode);
    Settings->SetScreenResolution(Resolution);
    Settings->ConfirmVideoMode();
    // The resolution half of ApplySettings. ApplySettings would also re-apply
    // scalability, VSync and frame-rate limits, which this menu does not edit.
    Settings->ApplyResolutionSettings(false);
    Settings->SaveSettings();
    return true;
}
