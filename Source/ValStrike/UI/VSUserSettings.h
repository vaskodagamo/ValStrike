#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericWindow.h"

class FConfigFile;

/**
 * Client-local player settings. Mouse sensitivity is stored in its own section
 * of this machine's GameUserSettings.ini; window mode and resolution use the
 * engine's UGameUserSettings. Nothing here is replicated or sent to a server.
 */
namespace VSUserSettings
{
    inline constexpr float DefaultMouseSensitivity = 0.12f;
    inline constexpr float MinMouseSensitivity = 0.02f;
    inline constexpr float MaxMouseSensitivity = 1.0f;
    /** Keyboard step of the menu slider. Typed values keep 0.001 precision. */
    inline constexpr float MouseSensitivityStep = 0.01f;

    extern const TCHAR* const ConfigSection;
    extern const TCHAR* const MouseSensitivityKey;

    /** Clamps to [Min, Max] and rounds to 0.001. Non-finite input yields the default. */
    float ClampMouseSensitivity(float Value);
    /**
     * Accepts a plain decimal with optional surrounding spaces, sign and a
     * decimal point or comma ("0.25", ".5", "0,5"). Accepted values are clamped.
     * Leaves OutValue unchanged and returns false for anything else.
     */
    bool ParseMouseSensitivity(const FString& Text, float& OutValue);
    /** Three decimals; used for both the menu value and the stored value. */
    FString FormatMouseSensitivity(float Value);

    /** Missing or malformed stored values fall back to the default. */
    float ReadMouseSensitivity(const FConfigFile& File);
    void WriteMouseSensitivity(FConfigFile& File, float Value);

    /** Reads this machine's GameUserSettings (in memory; loaded at startup). */
    float LoadMouseSensitivity();
    /** Updates the in-memory GameUserSettings. SaveUserSettings writes it to disk. */
    void StoreMouseSensitivity(float Value);
    /**
     * Writes GameUserSettings.ini through UGameUserSettings::SaveSettings, so
     * the engine's settings version is stored with our section. The engine
     * discards a GameUserSettings file without a valid version at startup.
     */
    bool SaveUserSettings();

    /** Cycles Fullscreen -> Windowed Fullscreen -> Windowed, wrapping in both directions. */
    EWindowMode::Type CycleWindowMode(EWindowMode::Type Mode, int32 Direction);
    FText WindowModeText(EWindowMode::Type Mode);
    FText ResolutionText(FIntPoint Resolution);

    struct FWindowState
    {
        EWindowMode::Type Mode = EWindowMode::Windowed;
        FIntPoint Resolution = FIntPoint::ZeroValue;
    };
    /** The game window's actual mode and viewport size, not the stored request. */
    FWindowState CurrentWindowState();
    /** Desktop size used by Windowed Fullscreen. */
    FIntPoint DesktopResolution();
    /** Primary display work area (desktop minus menu bar/taskbar/dock). */
    FIntPoint WorkAreaSize();
    /**
     * Standard 16:9 and 16:10 window sizes from 1280x720 that fit inside the
     * work area. (The engine's "convenient" sizes are display fractions such
     * as 1283x721 on a 1710-wide desktop, which players do not recognise.)
     */
    TArray<FIntPoint> StandardWindowedResolutions(FIntPoint WorkArea);
    /**
     * Sorted, unique choices for a mode. Windowed uses the standard sizes plus
     * CurrentWindowed (when valid); Fullscreen uses the display's supported
     * modes; Windowed Fullscreen is the desktop size only.
     */
    TArray<FIntPoint> ResolutionOptions(EWindowMode::Type Mode, FIntPoint CurrentWindowed);
    /** Sorts by area, then width, and removes duplicates and invalid sizes. */
    void NormalizeResolutions(TArray<FIntPoint>& Resolutions);
    /** Index of the option nearest to Target, or INDEX_NONE for an empty list. */
    int32 ClosestResolutionIndex(const TArray<FIntPoint>& Options, FIntPoint Target);
    /**
     * Requests the mode/resolution through UGameUserSettings, marks it confirmed
     * (the explicit Apply is the confirmation) and saves GameUserSettings.ini.
     */
    bool ApplyWindowSettings(EWindowMode::Type Mode, FIntPoint Resolution);
}
