#pragma once

#include "CoreMinimal.h"

namespace VSServerAddress
{
    /**
     * Accepts an ASCII hostname or dotted IPv4 address with an optional port.
     * Returns a canonical host:port. Prefix the fixed string "unreal://" when
     * passing it to ClientTravel to disambiguate single-label hostnames.
     * The default private-server port is 7777. IPv6 and travel options are not
     * supported by this initial join UI. One-character hostnames are rejected
     * because Unreal treats them as drive paths. Failure clears OutAddress.
     */
    bool Validate(const FString& Input, FString& OutAddress, FText& OutError);

    /**
     * Optional private-server join key from its own field, with the server's
     * rule (8-64 letters, digits, '-' or '_'; surrounding spaces trimmed). Empty
     * input is valid and means no key. Never echoes the key in OutError.
     */
    bool ValidateJoinKey(const FString& Input, FString& OutKey, FText& OutError);

    /**
     * "host:port" or "host:port?JoinKey=<key>" for an address that Validate
     * produced and a key that ValidateJoinKey produced. JoinKey is the only URL
     * option this client ever adds; it never comes from the address text.
     */
    bool JoinDestination(const FString& ValidAddress, const FString& Key, FString& OutDestination);
}
