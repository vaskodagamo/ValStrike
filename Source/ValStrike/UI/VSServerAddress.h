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
}
