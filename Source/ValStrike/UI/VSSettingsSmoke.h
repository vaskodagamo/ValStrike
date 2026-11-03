#pragma once

class AVSPlayerController;

namespace VSSettingsSmoke
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
// Opt-in (-VSSettingsSmoke) rendered Settings harness. Process-local; it only
// drives the local player's menu, look input and window like a player would.
void Tick(AVSPlayerController* Controller, float DeltaTime);
#endif
}
