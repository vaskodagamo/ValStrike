#include "VSServerAddress.h"

#include "VSJoinKey.h"

#include <string>

#define LOCTEXT_NAMESPACE "VSServerAddress"

namespace
{
    bool IsASCIIDigit(TCHAR Character)
    {
        return Character >= TEXT('0') && Character <= TEXT('9');
    }

    bool IsASCIIAlpha(TCHAR Character)
    {
        return (Character >= TEXT('a') && Character <= TEXT('z')) ||
            (Character >= TEXT('A') && Character <= TEXT('Z'));
    }
}

bool VSServerAddress::Validate(const FString& Input, FString& OutAddress, FText& OutError)
{
    // Copy before clearing the output so callers may normalize a string in place.
    const FString Address = Input.TrimStartAndEnd();
    OutAddress.Reset();
    OutError = FText::GetEmpty();
    const auto Fail = [&OutError](const FText& Message)
    {
        OutError = Message;
        return false;
    };

    if (Address.IsEmpty())
    {
        return Fail(LOCTEXT("Empty", "Enter a server address, for example 192.168.1.20:7777."));
    }
    // 253 hostname characters, one colon and five port digits.
    if (Address.Len() > 259)
    {
        return Fail(LOCTEXT("TooLong", "The server address is too long."));
    }

    int32 ColonIndex = INDEX_NONE;
    int32 LastColonIndex = INDEX_NONE;
    Address.FindChar(TEXT(':'), ColonIndex);
    Address.FindLastChar(TEXT(':'), LastColonIndex);
    if (ColonIndex != LastColonIndex || Address.Contains(TEXT("[")) || Address.Contains(TEXT("]")))
    {
        return Fail(LOCTEXT("IPv6", "IPv6 is not supported here yet. Use an IPv4 address or hostname."));
    }
    for (TCHAR Character : Address)
    {
        if (!IsASCIIDigit(Character) && !IsASCIIAlpha(Character) &&
            Character != TEXT('.') && Character != TEXT('-') && Character != TEXT(':'))
        {
            return Fail(LOCTEXT("Characters", "Use only a hostname or IPv4 address and optional :port. Remove spaces, URLs and extra options."));
        }
    }

    const FString Host = ColonIndex == INDEX_NONE ? Address : Address.Left(ColonIndex);
    int32 Port = 7777;
    if (ColonIndex != INDEX_NONE)
    {
        const FString PortText = Address.Mid(ColonIndex + 1);
        if (PortText.IsEmpty() || PortText.Len() > 5)
        {
            return Fail(LOCTEXT("Port", "The port must be a number from 1 to 65535."));
        }
        for (TCHAR Character : PortText)
        {
            if (!IsASCIIDigit(Character))
            {
                return Fail(LOCTEXT("Port", "The port must be a number from 1 to 65535."));
            }
        }
        Port = FCString::Atoi(*PortText);
        if (Port < 1 || Port > 65535)
        {
            return Fail(LOCTEXT("Port", "The port must be a number from 1 to 65535."));
        }
    }

    if (Host.IsEmpty() || Host.Len() > 253)
    {
        return Fail(LOCTEXT("HostLength", "Enter a hostname or IPv4 address before the port."));
    }
    if (Host.Len() == 1)
    {
        return Fail(LOCTEXT("SingleCharacterHost", "Use an IPv4 address or a hostname with at least two characters."));
    }
    TArray<FString> Labels;
    Host.ParseIntoArray(Labels, TEXT("."), false);
    bool bOnlyDigitsAndDots = true;
    for (const FString& Label : Labels)
    {
        if (Label.IsEmpty() || Label.Len() > 63 || Label[0] == TEXT('-') || Label[Label.Len() - 1] == TEXT('-'))
        {
            return Fail(LOCTEXT("Hostname", "Check the hostname: each part must contain 1–63 letters or digits, with hyphens only inside."));
        }
        for (TCHAR Character : Label)
        {
            bOnlyDigitsAndDots &= IsASCIIDigit(Character);
        }
    }
    if (bOnlyDigitsAndDots)
    {
        // Disallow shorthand, out-of-range octets and ambiguous leading zeros.
        if (Labels.Num() != 4)
        {
            return Fail(LOCTEXT("IPv4", "Use four IPv4 numbers from 0 to 255, for example 192.168.1.20."));
        }
        for (const FString& Label : Labels)
        {
            if (Label.Len() > 3 || (Label.Len() > 1 && Label[0] == TEXT('0')) || FCString::Atoi(*Label) > 255)
            {
                return Fail(LOCTEXT("IPv4", "Use four IPv4 numbers from 0 to 255, for example 192.168.1.20."));
            }
        }
    }

    // Pair with the fixed unreal:// prefix at the travel call. Without it,
    // FURL can interpret a single-label host as a protocol or local map.
    OutAddress = FString::Printf(TEXT("%s:%d"), *Host.ToLower(), Port);
    return true;
}

bool VSServerAddress::ValidateJoinKey(const FString& Input, FString& OutKey, FText& OutError)
{
    OutKey.Reset();
    OutError = FText::GetEmpty();
    std::string Key;
    switch (ValStrike::JoinKey::Parse(std::string(TCHAR_TO_UTF8(*Input)), Key))
    {
    case ValStrike::JoinKey::EInput::None:
        return true;
    case ValStrike::JoinKey::EInput::Valid:
        OutKey = UTF8_TO_TCHAR(Key.c_str());
        return true;
    default:
        OutError = LOCTEXT("JoinKeyRule", "The join key must be 8 to 64 letters, digits, - or _. Leave it empty if the server has no key.");
        return false;
    }
}

bool VSServerAddress::JoinDestination(const FString& ValidAddress, const FString& Key, FString& OutDestination)
{
    std::string Destination;
    const bool bComposed = ValStrike::JoinKey::ComposeDestination(
        std::string(TCHAR_TO_UTF8(*ValidAddress)), std::string(TCHAR_TO_UTF8(*Key)), Destination);
    OutDestination = bComposed ? FString(UTF8_TO_TCHAR(Destination.c_str())) : FString();
    return bComposed;
}

#undef LOCTEXT_NAMESPACE
