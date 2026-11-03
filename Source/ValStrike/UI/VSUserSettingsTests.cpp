#if WITH_DEV_AUTOMATION_TESTS

#include "VSUserSettings.h"
#include "../VSCharacter.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVSUserSettingsTest, "ValStrike.UI.Settings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVSUserSettingsTest::RunTest(const FString& Parameters)
{
    using namespace VSUserSettings;

    // Clamp: range, rounding to the stored precision, and non-finite input.
    TestEqual(TEXT("Below range clamps to minimum"), ClampMouseSensitivity(0.0f), MinMouseSensitivity);
    TestEqual(TEXT("Negative clamps to minimum"), ClampMouseSensitivity(-3.0f), MinMouseSensitivity);
    TestEqual(TEXT("Above range clamps to maximum"), ClampMouseSensitivity(7.5f), MaxMouseSensitivity);
    TestEqual(TEXT("Rounds down to 0.001"), ClampMouseSensitivity(0.12349f), 0.123f);
    TestEqual(TEXT("Rounds up to 0.001"), ClampMouseSensitivity(0.12351f), 0.124f);
    TestEqual(TEXT("NaN falls back to default"), ClampMouseSensitivity(std::numeric_limits<float>::quiet_NaN()), DefaultMouseSensitivity);
    TestEqual(TEXT("Infinity falls back to default"), ClampMouseSensitivity(std::numeric_limits<float>::infinity()), DefaultMouseSensitivity);
    TestTrue(TEXT("Default lies inside the range"), MinMouseSensitivity < DefaultMouseSensitivity && DefaultMouseSensitivity < MaxMouseSensitivity);

    // Parse: accepted forms are clamped; everything else leaves the value untouched.
    struct FAccepted
    {
        const TCHAR* Text;
        float Expected;
    };
    const FAccepted Accepted[] = {
        {TEXT("0.25"), 0.25f}, {TEXT("  0.5  "), 0.5f}, {TEXT(".5"), 0.5f}, {TEXT("0,36"), 0.36f},
        {TEXT("+0.3"), 0.3f}, {TEXT("1"), 1.0f}, {TEXT("5"), 1.0f}, {TEXT("-1"), 0.02f},
        {TEXT("0.0001"), 0.02f}, {TEXT("0.1234"), 0.123f}, {TEXT("0.120"), 0.12f},
    };
    for (const FAccepted& Example : Accepted)
    {
        float Value = -1.0f;
        TestTrue(FString::Printf(TEXT("Accept '%s'"), Example.Text), ParseMouseSensitivity(Example.Text, Value));
        TestEqual(FString::Printf(TEXT("Value of '%s'"), Example.Text), Value, Example.Expected);
    }
    const TCHAR* Rejected[] = {
        TEXT(""), TEXT("   "), TEXT("abc"), TEXT("0.3x"), TEXT("1.2.3"), TEXT("0..3"), TEXT("nan"), TEXT("inf"),
        TEXT("1e-2"), TEXT("0x10"), TEXT("-"), TEXT("."), TEXT("+-1"), TEXT("1-"), TEXT("0.5 0.6"),
        TEXT("12345678901234567"),
    };
    for (const TCHAR* Text : Rejected)
    {
        float Value = 0.77f;
        TestFalse(FString::Printf(TEXT("Reject '%s'"), Text), ParseMouseSensitivity(Text, Value));
        TestEqual(FString::Printf(TEXT("Rejected '%s' leaves value"), Text), Value, 0.77f);
    }

    // Format: fixed precision, and every slider step survives a text round trip.
    TestEqual(TEXT("Default text"), FormatMouseSensitivity(DefaultMouseSensitivity), FString(TEXT("0.120")));
    TestEqual(TEXT("Maximum text"), FormatMouseSensitivity(MaxMouseSensitivity), FString(TEXT("1.000")));
    TestEqual(TEXT("Formatting clamps"), FormatMouseSensitivity(9.0f), FString(TEXT("1.000")));
    for (int32 Thousandths = 20; Thousandths <= 1000; ++Thousandths)
    {
        const float Value = ClampMouseSensitivity(Thousandths / 1000.0f);
        float Parsed = -1.0f;
        if (!ParseMouseSensitivity(FormatMouseSensitivity(Value), Parsed) || Parsed != Value)
        {
            AddError(FString::Printf(TEXT("Round trip changed %.6f to %.6f"), Value, Parsed));
            break;
        }
    }

    // Persist: an unset or malformed stored value keeps the default.
    FConfigFile Empty;
    TestEqual(TEXT("Unset value keeps the default"), ReadMouseSensitivity(Empty), DefaultMouseSensitivity);
    struct FStored
    {
        const TCHAR* Text;
        float Expected;
    };
    const FStored Stored[] = {
        {TEXT(""), DefaultMouseSensitivity}, {TEXT("fast"), DefaultMouseSensitivity}, {TEXT("nan"), DefaultMouseSensitivity},
        {TEXT("7"), MaxMouseSensitivity}, {TEXT("0.001"), MinMouseSensitivity}, {TEXT("0.45"), 0.45f},
    };
    for (const FStored& Example : Stored)
    {
        FConfigFile File;
        File.SetString(ConfigSection, MouseSensitivityKey, Example.Text);
        TestEqual(FString::Printf(TEXT("Stored '%s'"), Example.Text), ReadMouseSensitivity(File), Example.Expected);
    }

    // Persist: write to a real file and read it back in a fresh config object.
    // This never touches the running process's GameUserSettings.
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Tests/UI");
    const FString Path = Directory / FString::Printf(TEXT("SettingsUnit_%s.ini"), *FGuid::NewGuid().ToString());
    IFileManager::Get().MakeDirectory(*Directory, true);
    {
        FConfigFile File;
        WriteMouseSensitivity(File, 0.36f);
        File.SetString(TEXT("Other.Section"), TEXT("Kept"), TEXT("yes"));
        TestEqual(TEXT("Written value reads back in memory"), ReadMouseSensitivity(File), 0.36f);
        TestTrue(TEXT("Settings file written"), File.Write(Path));
    }
    {
        FConfigFile File;
        File.Read(Path);
        FString Raw;
        TestTrue(TEXT("Section and key on disk"), File.GetString(ConfigSection, MouseSensitivityKey, Raw));
        TestEqual(TEXT("Stored text"), Raw, FString(TEXT("0.360")));
        TestEqual(TEXT("Value survives disk round trip"), ReadMouseSensitivity(File), 0.36f);
        FString Other;
        TestTrue(TEXT("Other sections are preserved"), File.GetString(TEXT("Other.Section"), TEXT("Kept"), Other) && Other == TEXT("yes"));
        WriteMouseSensitivity(File, 2.0f);
        TestEqual(TEXT("Writing clamps"), ReadMouseSensitivity(File), MaxMouseSensitivity);
    }
    IFileManager::Get().Delete(*Path);

    // The character fallback and the settings default must not drift; the
    // mouse-look regression expects both to be 0.12.
    const FFloatProperty* Property = FindFProperty<FFloatProperty>(AVSCharacter::StaticClass(), TEXT("MouseSensitivity"));
    TestNotNull(TEXT("Character sensitivity property"), Property);
    if (Property)
    {
        TestEqual(TEXT("Character fallback equals settings default"),
            Property->GetPropertyValue_InContainer(GetDefault<AVSCharacter>()), DefaultMouseSensitivity);
    }

    // Window mode selector order and wrapping.
    const auto Cycle = [](EWindowMode::Type Mode, int32 Direction) { return static_cast<int32>(CycleWindowMode(Mode, Direction)); };
    TestEqual(TEXT("Windowed wraps to Fullscreen"), Cycle(EWindowMode::Windowed, 1), static_cast<int32>(EWindowMode::Fullscreen));
    TestEqual(TEXT("Fullscreen wraps back to Windowed"), Cycle(EWindowMode::Fullscreen, -1), static_cast<int32>(EWindowMode::Windowed));
    TestEqual(TEXT("Fullscreen then Windowed Fullscreen"), Cycle(EWindowMode::Fullscreen, 1), static_cast<int32>(EWindowMode::WindowedFullscreen));
    TestEqual(TEXT("Windowed Fullscreen then Windowed"), Cycle(EWindowMode::WindowedFullscreen, 1), static_cast<int32>(EWindowMode::Windowed));
    for (EWindowMode::Type Mode : {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed})
    {
        TestEqual(TEXT("Three steps return to the start"),
            static_cast<int32>(CycleWindowMode(CycleWindowMode(CycleWindowMode(Mode, 1), 1), 1)), static_cast<int32>(Mode));
        TestEqual(TEXT("Back and forth is identity"), Cycle(CycleWindowMode(Mode, 1), -1), static_cast<int32>(Mode));
    }

    // Resolution list hygiene and nearest-choice selection.
    TArray<FIntPoint> Sizes = {{1920, 1080}, {1280, 720}, {0, 720}, {1280, 720}, {1600, 900}, {-5, 4}, {1280, 800}};
    NormalizeResolutions(Sizes);
    const TArray<FIntPoint> Expected = {{1280, 720}, {1280, 800}, {1600, 900}, {1920, 1080}};
    TestTrue(TEXT("Sorted, unique and valid sizes"), Sizes == Expected);
    TestEqual(TEXT("Exact match"), ClosestResolutionIndex(Sizes, FIntPoint(1600, 900)), 2);
    TestEqual(TEXT("Nearest match"), ClosestResolutionIndex(Sizes, FIntPoint(1300, 790)), 1);
    TestEqual(TEXT("Larger than all"), ClosestResolutionIndex(Sizes, FIntPoint(3000, 2000)), 3);
    TestEqual(TEXT("Empty list"), ClosestResolutionIndex({}, FIntPoint(1280, 720)), INDEX_NONE);
    TestEqual(TEXT("Resolution text"), ResolutionText(FIntPoint(1280, 720)).ToString(), FString(TEXT("1280 × 720")));
    return true;
}

#endif
