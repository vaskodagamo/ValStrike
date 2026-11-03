#pragma once

class AVSPlayerController;

namespace VSUISmoke
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
// Opt-in, process-local driver; no replicated state or release-build controls.
void Tick(AVSPlayerController* Controller, float DeltaTime);
#endif
}
