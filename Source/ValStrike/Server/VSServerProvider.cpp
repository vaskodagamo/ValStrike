#include "VSServerProvider.h"

#include "../ValStrike.h"
#include "Kismet/GameplayStatics.h"

// Log lines use a fixed VSSERVER_* prefix and key=value fields so runners and hosts can parse them.
void FVSLocalServerProvider::OnTransition(const ValStrike::Server::Transition& Transition, int32 Players)
{
    UE_LOG(LogValStrike, Display, TEXT("VSSERVER_STATE from=%s to=%s reason=%s at_ms=%lld players=%d"),
        UTF8_TO_TCHAR(ValStrike::Server::ToString(Transition.From)),
        UTF8_TO_TCHAR(ValStrike::Server::ToString(Transition.To)),
        UTF8_TO_TCHAR(ValStrike::Server::ToString(Transition.Why)),
        static_cast<int64>(Transition.At), Players);
}

void FVSLocalServerProvider::OnHealth(const FVSServerHealth& Health)
{
    UE_LOG(LogValStrike, Display,
        TEXT("VSSERVER_HEALTH state=%s players=%d frames=%d frame_ms_avg=%.2f frame_ms_max=%.2f tick_hz=%.1f uptime_s=%.0f"),
        UTF8_TO_TCHAR(ValStrike::Server::ToString(Health.State)), Health.Players, Health.Frames,
        Health.AverageFrameMs, Health.MaxFrameMs, Health.AverageTickHz, Health.UptimeSeconds);
}

void FVSLocalServerProvider::OnMatchResult(const FString& ResultJson)
{
    UE_LOG(LogValStrike, Display, TEXT("VSSERVER_RESULT %s"), *ResultJson);
}

bool FVSJoinKeyAdmission::Admit(const FVSAdmissionRequest& Request, FString& OutError) const
{
    const FString Supplied = UGameplayStatics::ParseOption(Request.Options, TEXT("JoinKey"));
    if (ValStrike::Server::JoinKeyMatches(Key, std::string(TCHAR_TO_UTF8(*Supplied)))) { return true; }
    // Neither key is logged or returned to the client.
    OutError = TEXT("Join key missing or wrong");
    return false;
}
