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

    // Master volume: whole percent, 5 % steps, clamped; malformed stored text keeps the default.
    TestEqual(TEXT("Volume below range clamps to 0"), ClampMasterVolume(-20.0), MinMasterVolume);
    TestEqual(TEXT("Volume above range clamps to 100"), ClampMasterVolume(250.0), MaxMasterVolume);
    TestEqual(TEXT("Volume snaps down to 5 %"), ClampMasterVolume(42.0), 40);
    TestEqual(TEXT("Volume snaps up to 5 %"), ClampMasterVolume(43.0), 45);
    TestEqual(TEXT("Volume NaN falls back to default"), ClampMasterVolume(std::numeric_limits<double>::quiet_NaN()), DefaultMasterVolume);
    TestEqual(TEXT("Volume infinity falls back to default"), ClampMasterVolume(std::numeric_limits<double>::infinity()), DefaultMasterVolume);
    TestTrue(TEXT("Volume default lies inside the range"), MinMasterVolume <= DefaultMasterVolume && DefaultMasterVolume <= MaxMasterVolume
        && DefaultMasterVolume % MasterVolumeStep == 0);
    struct FVolume
    {
        const TCHAR* Text;
        int32 Expected;
    };
    const FVolume AcceptedVolume[] = {
        {TEXT("80"), 80}, {TEXT("  75  "), 75}, {TEXT("50%"), 50}, {TEXT(" 65 % "), 65}, {TEXT("0"), 0}, {TEXT("100"), 100},
        {TEXT("+5"), 5}, {TEXT("-10"), 0}, {TEXT("250"), 100}, {TEXT("42"), 40}, {TEXT("56.9"), 55}, {TEXT("2,4"), 0},
    };
    for (const FVolume& Example : AcceptedVolume)
    {
        int32 Percent = -1;
        TestTrue(FString::Printf(TEXT("Accept volume '%s'"), Example.Text), ParseMasterVolume(Example.Text, Percent));
        TestEqual(FString::Printf(TEXT("Volume of '%s'"), Example.Text), Percent, Example.Expected);
    }
    const TCHAR* RejectedVolume[] = {
        TEXT(""), TEXT("  "), TEXT("%"), TEXT("loud"), TEXT("50%%"), TEXT("%50"), TEXT("5 0"), TEXT("1e2"), TEXT("nan"), TEXT("0x40"),
    };
    for (const TCHAR* Text : RejectedVolume)
    {
        int32 Percent = 35;
        TestFalse(FString::Printf(TEXT("Reject volume '%s'"), Text), ParseMasterVolume(Text, Percent));
        TestEqual(FString::Printf(TEXT("Rejected volume '%s' leaves value"), Text), Percent, 35);
    }
    TestEqual(TEXT("Default volume text"), FormatMasterVolume(DefaultMasterVolume), FString(TEXT("100")));
    TestEqual(TEXT("Volume formatting clamps and snaps"), FormatMasterVolume(103), FString(TEXT("100")));
    for (int32 Percent = MinMasterVolume; Percent <= MaxMasterVolume; Percent += MasterVolumeStep)
    {
        int32 Parsed = -1;
        if (!ParseMasterVolume(FormatMasterVolume(Percent), Parsed) || Parsed != Percent)
        {
            AddError(FString::Printf(TEXT("Volume round trip changed %d to %d"), Percent, Parsed));
            break;
        }
    }
    TestEqual(TEXT("Silent gain"), MasterVolumeGain(0), 0.0f);
    TestEqual(TEXT("Half gain"), MasterVolumeGain(50), 0.5f);
    TestEqual(TEXT("Full gain"), MasterVolumeGain(100), 1.0f);
    TestEqual(TEXT("Unset volume keeps the default"), ReadMasterVolume(Empty), DefaultMasterVolume);
    const FVolume StoredVolume[] = {
        {TEXT(""), DefaultMasterVolume}, {TEXT("quiet"), DefaultMasterVolume}, {TEXT("inf"), DefaultMasterVolume},
        {TEXT("0.5.5"), DefaultMasterVolume}, {TEXT("400"), 100}, {TEXT("-1"), 0}, {TEXT("0"), 0}, {TEXT("35"), 35},
    };
    for (const FVolume& Example : StoredVolume)
    {
        FConfigFile File;
        File.SetString(ConfigSection, MasterVolumeKey, Example.Text);
        TestEqual(FString::Printf(TEXT("Stored volume '%s'"), Example.Text), ReadMasterVolume(File), Example.Expected);
    }
    {
        FConfigFile File;
        WriteMouseSensitivity(File, 0.36f);
        WriteMasterVolume(File, 63);
        FString Raw;
        TestTrue(TEXT("Volume stored in the settings section"), File.GetString(ConfigSection, MasterVolumeKey, Raw) && Raw == TEXT("65"));
        TestEqual(TEXT("Volume reads back"), ReadMasterVolume(File), 65);
        TestEqual(TEXT("Volume write keeps sensitivity"), ReadMouseSensitivity(File), 0.36f);
    }
    TestFalse(TEXT("No world, no volume change"), ApplyMasterVolume(nullptr, 50));

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
    const TArray<FIntPoint> Laptop = StandardWindowedResolutions(FIntPoint(1710, 1000));
    const TArray<FIntPoint> LaptopExpected = {{1280, 720}, {1280, 800}, {1366, 768}, {1440, 900}, {1600, 900}};
    TestTrue(TEXT("Standard sizes that fit a 1710x1000 work area"), Laptop == LaptopExpected);
    TestTrue(TEXT("Exact work-area size is excluded"), StandardWindowedResolutions(FIntPoint(1280, 720)).Num() == 0);
    TestTrue(TEXT("4K work area offers 16:9 and 16:10 up to 3200x1800"),
        StandardWindowedResolutions(FIntPoint(3840, 2100)).Contains(FIntPoint(3200, 1800))
        && !StandardWindowedResolutions(FIntPoint(3840, 2100)).Contains(FIntPoint(3840, 2160)));
    TestEqual(TEXT("Resolution text"), ResolutionText(FIntPoint(1280, 720)).ToString(), FString(TEXT("1280 × 720")));
    return true;
}

#endif
