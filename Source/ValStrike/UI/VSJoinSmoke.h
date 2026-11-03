#pragma once

class AVSPlayerController;

namespace VSJoinSmoke
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
// Opt-in (-VSJoinSmoke) rendered join-key harness. Process-local; it drives the
// local player's menu like a player would. The key comes from the environment
// (VSJOINSMOKE_KEY), never from the command line, and is never logged.
void Tick(AVSPlayerController* Controller, float DeltaTime);
#endif
}
