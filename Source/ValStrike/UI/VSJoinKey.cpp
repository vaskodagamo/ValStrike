#include "UI/VSJoinKey.h"

#include "Server/VSServerContract.h"

namespace ValStrike::JoinKey
{
namespace
{
bool IsJoinKeySpace(char Character)
{
    return Character == ' ' || Character == '\t' || Character == '\r' || Character == '\n';
}

// Everything a validated host:port can contain: ASCII hostname or IPv4, ':' and digits.
bool IsHostPortCharacter(char Character)
{
    return (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z')
        || (Character >= '0' && Character <= '9') || Character == '.' || Character == '-' || Character == ':';
}
}

EInput Parse(const std::string& Input, std::string& OutKey)
{
    OutKey.clear();
    std::size_t Begin = 0;
    std::size_t End = Input.size();
    while (Begin < End && IsJoinKeySpace(Input[Begin]))
    {
        ++Begin;
    }
    while (End > Begin && IsJoinKeySpace(Input[End - 1]))
    {
        --End;
    }
    if (Begin == End)
    {
        return EInput::None;
    }
    std::string Key = Input.substr(Begin, End - Begin);
    if (!Server::IsValidJoinKey(Key))
    {
        return EInput::Invalid;
    }
    OutKey = Key;
    return EInput::Valid;
}

bool ComposeDestination(const std::string& HostPort, const std::string& Key, std::string& OutDestination)
{
    OutDestination.clear();
    if (HostPort.empty() || HostPort.size() > 300)
    {
        return false;
    }
    for (const char Character : HostPort)
    {
        if (!IsHostPortCharacter(Character))
        {
            return false;
        }
    }
    if (!Key.empty() && !Server::IsValidJoinKey(Key))
    {
        return false;
    }
    OutDestination = Key.empty() ? HostPort : HostPort + "?JoinKey=" + Key;
    return true;
}
}
