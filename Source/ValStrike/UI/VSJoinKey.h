#pragma once

#include <string>

// Client side of the private-server join key. Standard library only, so
// Tests/UI/test_join_key_rule.py compiles it without Unreal. The key rule itself
// is the server's (ValStrike::Server::IsValidJoinKey: [A-Za-z0-9_-]{8,64}), so
// client and server cannot drift apart.
namespace ValStrike::JoinKey
{
enum class EInput
{
    None,    // Empty or whitespace only: join without a key.
    Valid,   // OutKey holds the trimmed key.
    Invalid, // Not 8-64 letters, digits, '-' or '_'.
};

// Trims surrounding ASCII whitespace, then applies the server's rule.
EInput Parse(const std::string& Input, std::string& OutKey);

// Builds "host:port" or "host:port?JoinKey=<key>" from a host:port that the
// address validation already produced. Refuses (and clears OutDestination) when
// that text carries any URL syntax, so a key can only come from the key field.
bool ComposeDestination(const std::string& HostPort, const std::string& Key, std::string& OutDestination);
}
