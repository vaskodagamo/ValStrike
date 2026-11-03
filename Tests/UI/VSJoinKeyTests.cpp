// Standalone tests for the client join-key rule (UI/VSJoinKey). No Unreal.
// Run through Tests/UI/test_join_key_rule.py. With --classify, prints
// none/valid/invalid for each stdin line so the runner can compare the C++ rule
// with Scripts/ue.py's join_key().

#include "UI/VSJoinKey.h"

#include <cstdio>
#include <iostream>
#include <string>

namespace
{
using ValStrike::JoinKey::EInput;

int Failures = 0;
int Checks = 0;

void Expect(bool bCondition, const char* Name)
{
    ++Checks;
    if (!bCondition)
    {
        ++Failures;
        std::printf("FAIL %s\n", Name);
    }
}

EInput ParseOnly(const std::string& Input)
{
    std::string Key;
    return ValStrike::JoinKey::Parse(Input, Key);
}

const char* Name(EInput Value)
{
    return Value == EInput::None ? "none" : Value == EInput::Valid ? "valid" : "invalid";
}

int Classify()
{
    std::string Line;
    while (std::getline(std::cin, Line))
    {
        std::printf("%s\n", Name(ParseOnly(Line)));
    }
    return 0;
}
}

int main(int ArgumentCount, char** Arguments)
{
    if (ArgumentCount == 2 && std::string(Arguments[1]) == "--classify")
    {
        return Classify();
    }

    // Empty means no key.
    Expect(ParseOnly("") == EInput::None, "empty is no key");
    Expect(ParseOnly("   \t ") == EInput::None, "whitespace only is no key");

    // Length boundaries 8..64.
    Expect(ParseOnly(std::string(7, 'a')) == EInput::Invalid, "7 characters rejected");
    Expect(ParseOnly(std::string(8, 'a')) == EInput::Valid, "8 characters accepted");
    Expect(ParseOnly(std::string(64, 'Z')) == EInput::Valid, "64 characters accepted");
    Expect(ParseOnly(std::string(65, 'Z')) == EInput::Invalid, "65 characters rejected");
    Expect(ParseOnly("lan-Key_0001") == EInput::Valid, "documented example accepted");
    Expect(ParseOnly("0123456789_-AZaz") == EInput::Valid, "full alphabet accepted");

    // Surrounding whitespace is trimmed; the key itself is returned exactly.
    {
        std::string Key = "stale";
        Expect(ValStrike::JoinKey::Parse("  lan-Key_0001\t", Key) == EInput::Valid && Key == "lan-Key_0001", "trimmed key returned");
        Expect(ValStrike::JoinKey::Parse("bad key here", Key) == EInput::Invalid && Key.empty(), "invalid clears key");
        Key = "stale";
        Expect(ValStrike::JoinKey::Parse("", Key) == EInput::None && Key.empty(), "none clears key");
    }

    // Characters outside [A-Za-z0-9_-], including URL syntax that could add options.
    const char* Bad[] = {"lan Key_0001", "lan-Key.0001", "lan-Key_0001?", "lan-Key=0001", "lan-Key&0001",
                         "lan-Key/0001", "lan-Key+0001", "lan-Key%200001", "lan-Key#0001", "lan-Key_0001?listen",
                         "l\xC3\xA4n-Key_0001", "lan-Key_00\"1", "lan-Key;0001"};
    for (const char* Value : Bad)
    {
        Expect(ParseOnly(Value) == EInput::Invalid, Value);
    }
    Expect(ParseOnly(std::string("lan-Key\0_0001", 13)) == EInput::Invalid, "embedded NUL rejected");

    // Composition only accepts a plain host:port from the address validation.
    std::string Destination = "stale";
    Expect(ValStrike::JoinKey::ComposeDestination("127.0.0.1:7777", "", Destination) && Destination == "127.0.0.1:7777",
           "no key keeps host:port");
    Expect(ValStrike::JoinKey::ComposeDestination("lan-pc:7883", "lan-Key_0001", Destination)
           && Destination == "lan-pc:7883?JoinKey=lan-Key_0001", "key appended as the only option");
    // A key typed into the address field never reaches the URL.
    Expect(!ValStrike::JoinKey::ComposeDestination("127.0.0.1:7777?JoinKey=lan-Key_0001", "", Destination) && Destination.empty(),
           "key in address refused");
    Expect(!ValStrike::JoinKey::ComposeDestination("127.0.0.1:7777?JoinKey=lan-Key_0001", "lan-Key_0001", Destination),
           "key in address refused even with a key");
    const char* BadAddresses[] = {"", "host:7777#x", "host:7777/Map", "host 7777", "host:7777?listen", "unreal://host:7777"};
    for (const char* Address : BadAddresses)
    {
        Expect(!ValStrike::JoinKey::ComposeDestination(Address, "lan-Key_0001", Destination) && Destination.empty(), Address);
    }
    Expect(!ValStrike::JoinKey::ComposeDestination("127.0.0.1:7777", "short", Destination), "invalid key refused");
    Expect(!ValStrike::JoinKey::ComposeDestination("127.0.0.1:7777", "lan-Key_0001?x=1", Destination), "key with option refused");

    std::printf("join key checks=%d failures=%d\n", Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
