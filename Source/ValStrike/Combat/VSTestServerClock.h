#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "UObject/UnrealType.h"
#include "../ValStrike.h"

/**
 * Server-clock estimate for opt-in multi-process test harnesses only.
 *
 * AGameStateBase::GetServerWorldTimeSeconds() on a client averages up to 250
 * offset samples. A few connection-time hitches (map load, first shader or
 * pipeline compiles) therefore bias a client by about a second for tens of
 * seconds, so client-scheduled test inputs reach the server late. Each sample
 * (replicated server time minus local time when first seen) can only
 * underestimate the true offset by its delivery delay, so the best sample in
 * a short window is used; the window follows offset changes after clamped
 * hitches. Gameplay never uses this.
 */
class FVSTestServerClock
{
public:
    double Now(const UWorld* World)
    {
        const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
        if (!GameState) { return 0.0; }
        if (World->GetNetMode() != NM_Client) { return GameState->GetServerWorldTimeSeconds(); }
        static const FDoubleProperty* Replicated = FindFProperty<FDoubleProperty>(
            AGameStateBase::StaticClass(), TEXT("ReplicatedWorldTimeSecondsDouble"));
        const double Local = World->GetTimeSeconds();
        if (Replicated)
        {
            const double Server = Replicated->GetPropertyValue_InContainer(GameState);
            if (Server > 0.0 && Server != LastServer)
            {
                LastServer = Server;
                Samples.Add({Local, Server - Local});
            }
        }
        Samples.RemoveAll([Local](const FSample& Sample) { return Sample.At < Local - WindowSeconds; });
        if (Samples.IsEmpty()) { return GameState->GetServerWorldTimeSeconds(); }
        double Offset = Samples[0].Offset;
        for (const FSample& Sample : Samples) { Offset = FMath::Max(Offset, Sample.Offset); }
        if (FMath::Abs(Offset - LoggedOffset) > 0.2)
        {
            // Diagnostic trail for timing-sensitive harness failures.
            UE_LOG(LogValStrike, Display, TEXT("VSCLOCK local=%.3f server=%.3f offset=%.3f engine_offset=%.3f samples=%d"),
                Local, LastServer, Offset, GameState->GetServerWorldTimeSeconds() - Local, Samples.Num());
            LoggedOffset = Offset;
        }
        return Local + Offset;
    }

private:
    struct FSample
    {
        double At = 0.0;
        double Offset = 0.0;
    };
    static constexpr double WindowSeconds = 3.0;
    TArray<FSample> Samples;
    double LastServer = -1.0;
    double LoggedOffset = 0.0;
};
#endif
