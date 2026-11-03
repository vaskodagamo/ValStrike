#if WITH_DEV_AUTOMATION_TESTS

#include "VSServerAddress.h"
#include "Engine/EngineBaseTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVSJoinKeyTest, "ValStrike.UI.JoinKey",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVSJoinKeyTest::RunTest(const FString& Parameters)
{
    // The menu field: empty means no key; the server's rule otherwise.
    struct FKeyCase
    {
        const TCHAR* Input;
        bool bAccepted;
        const TCHAR* Key;
    };
    const FKeyCase Cases[] = {
        {TEXT(""), true, TEXT("")},
        {TEXT("   "), true, TEXT("")},
        {TEXT("lan-Key_0001"), true, TEXT("lan-Key_0001")},
        {TEXT("  lan-Key_0001\t"), true, TEXT("lan-Key_0001")},
        {TEXT("abcdefgh"), true, TEXT("abcdefgh")},
        {TEXT("abcdefg"), false, TEXT("")},
        {TEXT("lan-Key_0001?listen"), false, TEXT("")},
        {TEXT("lan Key_0001"), false, TEXT("")},
        {TEXT("län-Key_0001"), false, TEXT("")},
    };
    for (const FKeyCase& Case : Cases)
    {
        FString Key = TEXT("stale");
        FText Error;
        TestTrue(FString::Printf(TEXT("Expected result for case %d"), int32(&Case - Cases)), VSServerAddress::ValidateJoinKey(Case.Input, Key, Error) == Case.bAccepted);
        TestEqual(TEXT("Returned key"), Key, FString(Case.Key));
        TestTrue(TEXT("Error only on rejection"), Error.IsEmpty() == Case.bAccepted);
    }
    {
        FString Key;
        FText Error;
        const FString Long = FString::ChrN(65, TEXT('k'));
        TestFalse(TEXT("65 characters rejected"), VSServerAddress::ValidateJoinKey(Long, Key, Error));
        TestFalse(TEXT("Error never echoes the key"), Error.ToString().Contains(Long.Left(8)));
        TestTrue(TEXT("64 characters accepted"), VSServerAddress::ValidateJoinKey(FString::ChrN(64, TEXT('k')), Key, Error));
    }

    // A key typed into the address field is refused by the unchanged address validation.
    {
        FString Address;
        FText Error;
        TestFalse(TEXT("Address with JoinKey option refused"), VSServerAddress::Validate(TEXT("127.0.0.1:7883?JoinKey=lan-Key_0001"), Address, Error));
        TestTrue(TEXT("Refused address is cleared"), Address.IsEmpty());
        FString Destination = TEXT("stale");
        TestFalse(TEXT("Composition refuses option syntax"), VSServerAddress::JoinDestination(TEXT("127.0.0.1:7883?JoinKey=lan-Key_0001"), FString(), Destination));
        TestTrue(TEXT("Refused destination is cleared"), Destination.IsEmpty());
    }

    // The composed destination parses with Unreal's travel URL parser, and the
    // server's ParseOption finds exactly this key in the login options.
    struct FDestinationCase
    {
        const TCHAR* Address;
        const TCHAR* Key;
        const TCHAR* Host;
        int32 Port;
    };
    const FDestinationCase Destinations[] = {
        {TEXT("127.0.0.1:7883"), TEXT("lan-Key_0001"), TEXT("127.0.0.1"), 7883},
        {TEXT("lan-pc:7777"), TEXT("Z0-_abcdefgh"), TEXT("lan-pc"), 7777},
        {TEXT("127.0.0.1:7883"), TEXT(""), TEXT("127.0.0.1"), 7883},
    };
    for (const FDestinationCase& Case : Destinations)
    {
        FString ValidAddress;
        FText Error;
        if (!TestTrue(TEXT("Example address valid"), VSServerAddress::Validate(Case.Address, ValidAddress, Error)))
        {
            continue;
        }
        FString Destination;
        TestTrue(TEXT("Destination composed"), VSServerAddress::JoinDestination(ValidAddress, Case.Key, Destination));
        const FURL URL(nullptr, *(FString(TEXT("unreal://")) + Destination), TRAVEL_Absolute);
        TestTrue(TEXT("Destination is a valid remote URL"), URL.Valid && URL.Host == Case.Host && URL.Port == Case.Port);
        FString Options;
        for (const FString& Option : URL.Op)
        {
            Options += TEXT("?") + Option;
        }
        TestEqual(TEXT("Server-side ParseOption sees the key"), UGameplayStatics::ParseOption(Options, TEXT("JoinKey")), FString(Case.Key));
        TestEqual(TEXT("JoinKey is the only option"), URL.Op.Num(), FCString::Strlen(Case.Key) ? 1 : 0);
    }
    return true;
}

#endif
