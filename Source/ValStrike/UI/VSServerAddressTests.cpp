#if WITH_DEV_AUTOMATION_TESTS

#include "VSServerAddress.h"
#include "Engine/EngineBaseTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVSServerAddressTest, "ValStrike.UI.ServerAddress",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVSServerAddressTest::RunTest(const FString& Parameters)
{
    struct FValidAddress
    {
        const TCHAR* Input;
        const TCHAR* Expected;
    };
    const FValidAddress ValidAddresses[] = {
        {TEXT("127.0.0.1"), TEXT("127.0.0.1:7777")},
        {TEXT("  192.168.1.20:7778  "), TEXT("192.168.1.20:7778")},
        {TEXT("10.0.0.12:1"), TEXT("10.0.0.12:1")},
        {TEXT("192.168.0.255:65535"), TEXT("192.168.0.255:65535")},
        {TEXT("localhost"), TEXT("localhost:7777")},
        {TEXT("LAN-PC:07890"), TEXT("lan-pc:7890")},
        {TEXT("match.Example.com"), TEXT("match.example.com:7777")},
        {TEXT("2v2.example.com:7777"), TEXT("2v2.example.com:7777")},
    };
    for (const FValidAddress& Example : ValidAddresses)
    {
        FString Address;
        FText Error;
        TestTrue(FString::Printf(TEXT("Accept %s"), Example.Input), VSServerAddress::Validate(Example.Input, Address, Error));
        TestEqual(TEXT("Canonical destination"), Address, FString(Example.Expected));
        TestTrue(TEXT("Success clears the error"), Error.IsEmpty());

        // Exercise the actual travel parser, including single-label hostnames.
        const FString TravelURL = FString(TEXT("unreal://")) + Address;
        FURL URL(nullptr, *TravelURL, TRAVEL_Absolute);
        TestTrue(TEXT("Accepted destination parses as a remote URL"), URL.Valid && !URL.Host.IsEmpty());
        FString ExpectedHost;
        FString ExpectedPort;
        Address.Split(TEXT(":"), &ExpectedHost, &ExpectedPort);
        TestEqual(TEXT("Travel preserves the validated host"), URL.Host, ExpectedHost);
        TestEqual(TEXT("Travel preserves the validated port"), URL.Port, FCString::Atoi(*ExpectedPort));
        TestEqual(TEXT("Travel uses the game protocol"), URL.Protocol, FString(TEXT("unreal")));
        TestEqual(TEXT("No user-supplied travel options"), URL.Op.Num(), 0);
    }

    const TCHAR* InvalidAddresses[] = {
        TEXT(""), TEXT("   "), TEXT(":7777"), TEXT("localhost:"), TEXT("a"), TEXT("a:7777"),
        TEXT("localhost:0"), TEXT("localhost:65536"), TEXT("localhost:99999999999999999"),
        TEXT("localhost:-1"), TEXT("localhost:+7777"), TEXT("localhost:7a77"),
        TEXT("localhost:77.77"), TEXT("localhost:7777:8"),
        TEXT("https://example.com"), TEXT("unreal://127.0.0.1:7777"),
        TEXT("127.0.0.1:7777?listen"), TEXT("127.0.0.1:7777?Name=Player"),
        TEXT("127.0.0.1/Map"), TEXT("/Game/Maps/VS_Graybox"),
        TEXT("127.0.0.1\\Map"), TEXT("127.0.0.1#fragment"), TEXT("user@host"),
        TEXT("localhost;quit"), TEXT("localhost|quit"), TEXT("localhost&quit"),
        TEXT("localhost quit"), TEXT("local\thost"), TEXT("local\nhost"),
        TEXT("\"localhost\""), TEXT("%31%32%37.0.0.1"),
        TEXT("[::1]:7777"), TEXT("::1"), TEXT("[127.0.0.1]:7777"),
        TEXT("a..example.com"), TEXT(".example.com"), TEXT("example.com."),
        TEXT("-host.example"), TEXT("host-.example"), TEXT("bad_host.example"),
        TEXT("münchen.example"), TEXT("localhost:７７７７"),
        TEXT("256.1.1.1"), TEXT("1.1.1.999"), TEXT("127.1"), TEXT("123"),
        TEXT("127.000.0.1"), TEXT("01.2.3.4"), TEXT("1.2.3.4.5"),
    };
    for (const TCHAR* Input : InvalidAddresses)
    {
        FString Address(TEXT("stale-address:7777"));
        FText Error;
        TestFalse(FString::Printf(TEXT("Reject %s"), Input), VSServerAddress::Validate(Input, Address, Error));
        TestTrue(TEXT("Failure cannot leave a travel destination"), Address.IsEmpty());
        TestFalse(TEXT("Failure explains what to fix"), Error.IsEmpty());
    }

    FString Address;
    FText Error;
    const FString MaxLabel = FString::ChrN(63, TEXT('a'));
    TestTrue(TEXT("63-character DNS label accepted"), VSServerAddress::Validate(MaxLabel + TEXT(".example"), Address, Error));
    TestFalse(TEXT("64-character DNS label rejected"), VSServerAddress::Validate(MaxLabel + TEXT("a.example"), Address, Error));
    const FString MaxHost = MaxLabel + TEXT(".") + MaxLabel + TEXT(".") + MaxLabel + TEXT(".") + FString::ChrN(61, TEXT('a'));
    TestTrue(TEXT("253-character DNS name accepted"), VSServerAddress::Validate(MaxHost + TEXT(":65535"), Address, Error));
    TestFalse(TEXT("254-character DNS name rejected"), VSServerAddress::Validate(MaxHost + TEXT("a"), Address, Error));
    TestFalse(TEXT("Overlong input rejected"), VSServerAddress::Validate(FString::ChrN(260, TEXT('a')), Address, Error));

    Address = TEXT(" LOCALHOST ");
    Error = FText::FromString(TEXT("stale error"));
    TestTrue(TEXT("In-place normalization supported"), VSServerAddress::Validate(Address, Address, Error));
    TestEqual(TEXT("In-place result"), Address, FString(TEXT("localhost:7777")));
    TestTrue(TEXT("Stale error cleared"), Error.IsEmpty());
    return true;
}

#endif
