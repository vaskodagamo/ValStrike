#include "VSRoundHUDPreview.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "../../VSPlayerController.h"
#include "../../ValStrike.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
const TCHAR* FixtureNames[] = {
    TEXT("unavailable"), TEXT("waiting"), TEXT("preparation"), TEXT("active_carrier"),
    TEXT("planting"), TEXT("planted_defusing"), TEXT("dropped"), TEXT("placement_pending"),
    TEXT("resolved"), TEXT("role_swap"), TEXT("round_draw"), TEXT("complete_win"),
    TEXT("complete_draw"), TEXT("long_status"), TEXT("spectator_no_pawn"),
    TEXT("unavailable_no_pawn"), TEXT("menu_open"), TEXT("resolved_defused")
};

FVSMatchHUDData MakeFixture(int32 Index)
{
    FVSMatchHUDData Data;
    if (Index == 0 || Index == 15) return Data;

    Data.bAvailable = true;
    Data.LocalTeam = EVSMatchHUDTeam::One;
    Data.AttackingTeam = EVSMatchHUDTeam::One;
    Data.TotalRounds = 6;
    // These are frozen fixture timestamps. They are never an authoritative clock.
    Data.ServerNowSeconds = 100.0;
    switch (Index)
    {
    case 1:
        Data.Phase = EVSMatchHUDPhase::Waiting;
        Data.StatusText = TEXT("Waiting for a player on each team.");
        break;
    case 2:
        Data.Phase = EVSMatchHUDPhase::Preparation;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 107.6;
        Data.StatusText = TEXT("Prepare to attack site A.");
        break;
    case 3:
    case 4:
        Data.Phase = EVSMatchHUDPhase::Active;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 184.2;
        Data.Objective = EVSMatchHUDObjective::Carried;
        Data.bLocalCarrier = true;
        Data.InteractionPrompt = TEXT("Hold E to plant at site A  |  G to drop");
        Data.StatusText = TEXT("You carry the device. Reach site A.");
        if (Index == 4)
        {
            Data.bInteractionActive = true;
            Data.InteractionProgress = 0.42f;
            Data.InteractionPrompt = TEXT("Hold E to plant");
        }
        break;
    case 5:
        Data.Phase = EVSMatchHUDPhase::Planted;
        Data.LocalTeam = EVSMatchHUDTeam::Two;
        Data.Objective = EVSMatchHUDObjective::Planted;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 121.2;
        Data.bInteractionActive = true;
        Data.InteractionProgress = 0.64f;
        Data.InteractionPrompt = TEXT("Hold E to defuse");
        Data.StatusText = TEXT("Device armed at site A.");
        break;
    case 6:
        Data.Phase = EVSMatchHUDPhase::Active;
        Data.Objective = EVSMatchHUDObjective::Dropped;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 158.0;
        Data.InteractionPrompt = TEXT("Press E to retrieve the device");
        Data.StatusText = TEXT("The device has been dropped.");
        break;
    case 7:
        Data.Phase = EVSMatchHUDPhase::Active;
        Data.Objective = EVSMatchHUDObjective::PlacementPending;
        Data.StatusText = TEXT("Device placement is being confirmed.");
        break;
    case 8:
        Data.Phase = EVSMatchHUDPhase::Resolved;
        Data.Winner = EVSMatchHUDTeam::One;
        Data.TeamOneScore = 2;
        Data.TeamTwoScore = 1;
        Data.CompletedRounds = 3;
        Data.Objective = EVSMatchHUDObjective::Detonated;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 102.2;
        Data.StatusText = TEXT("Device detonated. Teams swap attack and defense next round.");
        break;
    case 9:
        Data.Phase = EVSMatchHUDPhase::Active;
        Data.AttackingTeam = EVSMatchHUDTeam::Two;
        Data.TeamOneScore = 2;
        Data.TeamTwoScore = 1;
        Data.CompletedRounds = 3;
        Data.Objective = EVSMatchHUDObjective::Carried;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 177.4;
        Data.StatusText = TEXT("Team 1 now defends site A.");
        break;
    case 10:
        Data.Phase = EVSMatchHUDPhase::Resolved;
        Data.bDraw = true;
        Data.TeamOneScore = 2;
        Data.TeamTwoScore = 2;
        Data.CompletedRounds = 4;
        Data.StatusText = TEXT("Round ended in a draw.");
        break;
    case 11:
    case 12:
        Data.Phase = EVSMatchHUDPhase::MatchComplete;
        Data.AttackingTeam = EVSMatchHUDTeam::Two;
        Data.CompletedRounds = 6;
        Data.TeamOneScore = Index == 11 ? 4 : 3;
        Data.TeamTwoScore = Index == 11 ? 2 : 3;
        Data.bDraw = Index == 12;
        Data.Winner = Index == 11 ? EVSMatchHUDTeam::One : EVSMatchHUDTeam::None;
        Data.bCanRequestRematch = true;
        Data.StatusText = TEXT("The six-round match is complete.");
        break;
    case 13:
        Data.Phase = EVSMatchHUDPhase::Active;
        Data.Objective = EVSMatchHUDObjective::Carried;
        Data.bLocalCarrier = true;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 100.1;
        Data.StatusText = TEXT("Presentation stress test: this deliberately long provider message checks wrapping and clipping at the supported minimum window size, while health, ammunition and objective interaction guidance remain readable.");
        Data.InteractionPrompt = TEXT("Hold E to plant while standing inside site A; release E or move away to cancel the interaction.");
        break;
    case 14:
    case 16:
        Data.Phase = EVSMatchHUDPhase::Planted;
        Data.LocalTeam = EVSMatchHUDTeam::None;
        Data.AttackingTeam = EVSMatchHUDTeam::Two;
        Data.TeamOneScore = 2;
        Data.TeamTwoScore = 2;
        Data.CompletedRounds = 4;
        Data.bSpectating = true;
        Data.Objective = EVSMatchHUDObjective::Planted;
        Data.bHasPhaseDeadline = true;
        Data.PhaseDeadlineSeconds = 112.0;
        Data.StatusText = TEXT("Spectating. Join the next preparation phase.");
        break;
    case 17:
        Data.Phase = EVSMatchHUDPhase::Resolved;
        Data.Winner = EVSMatchHUDTeam::Two;
        Data.TeamOneScore = 2;
        Data.TeamTwoScore = 3;
        Data.CompletedRounds = 5;
        Data.Objective = EVSMatchHUDObjective::Defused;
        Data.StatusText = TEXT("Device defused. Team 2 wins the round.");
        break;
    }
    return Data;
}

class FPreviewSequence
{
public:
    FPreviewSequence()
    {
        bEnabled = FParse::Param(FCommandLine::Get(), TEXT("VSRoundHUDPreview"));
        OutputDirectory = FPaths::ProjectSavedDir() / TEXT("Tests/UI/Round/screenshots");
        FParse::Value(FCommandLine::Get(), TEXT("VSRoundHUDPreviewDir="), OutputDirectory);
    }

    bool Tick(AVSPlayerController* PC, FVSMatchHUDData& Data)
    {
        if (!bEnabled || bFinished || !PC || !PC->IsLocalController()
            || !PC->GetWorld()->IsGameWorld() || PC->GetNetMode() != NM_Standalone)
            return false;

        const double Now = FPlatformTime::Seconds();
        if (Started == 0.0)
        {
            Started = Now;
            Entered = Now;
            IFileManager::Get().MakeDirectory(*OutputDirectory, true);
            Check(TEXT("practice_provider_is_unavailable"), !Data.bAvailable);
            Check(TEXT("preview_starts_with_real_pawn"), PC->GetPawn() != nullptr);
            UE_LOG(LogValStrike, Display, TEXT("VSROUND_PREVIEW_BEGIN fixtures=%d presentation_only=true"), UE_ARRAY_COUNT(FixtureNames));
        }
        if (Now - Started > 75.0)
        {
            Check(TEXT("bounded_preview_completed"), false);
            Finish(PC);
            return true;
        }
        ++TicksInFixture;
        Data = MakeFixture(Index);

        if (!bEnteredFixture)
        {
            if (Index == 14)
            {
                PC->StopMovement();
                PC->UnPossess();
            }
            if (Index == 16) PC->SetSessionMenuOpen(true);
            if (Index == 17) PC->SetSessionMenuOpen(false);
            bEnteredFixture = true;
        }

        // Allow both Canvas and Slate a stable frame before requesting their capture.
        if (!bCaptureRequested && Now - Entered >= 0.75 && TicksInFixture >= 12)
        {
            const bool bPawnExpected = Index < 14;
            Check(*FString::Printf(TEXT("%s_pawn_lifetime"), FixtureNames[Index]), (PC->GetPawn() != nullptr) == bPawnExpected);
            if (Index == 16)
            {
                Check(TEXT("menu_open_without_pawn"), PC->IsSessionMenuOpen());
                Check(TEXT("menu_does_not_pause_preview_world"), !PC->GetWorld()->IsPaused());
            }
            UE_LOG(LogValStrike, Display, TEXT("VSROUND_PREVIEW_STATE {\"fixture\":\"%s\",\"available\":%s,\"phase\":%d,\"has_pawn\":%s,\"spectating\":%s,\"menu\":%s,\"team_one_score\":%d,\"team_two_score\":%d,\"completed_rounds\":%d,\"local_team\":%d,\"attacking_team\":%d,\"has_deadline\":%s,\"server_now\":%.3f,\"deadline\":%.3f,\"objective\":%d,\"progress\":%.3f}"),
                FixtureNames[Index], Data.bAvailable ? TEXT("true") : TEXT("false"), static_cast<int32>(Data.Phase),
                PC->GetPawn() ? TEXT("true") : TEXT("false"), Data.bSpectating ? TEXT("true") : TEXT("false"),
                PC->IsSessionMenuOpen() ? TEXT("true") : TEXT("false"), Data.TeamOneScore, Data.TeamTwoScore,
                Data.CompletedRounds, static_cast<int32>(Data.LocalTeam), static_cast<int32>(Data.AttackingTeam),
                Data.bHasPhaseDeadline ? TEXT("true") : TEXT("false"), Data.ServerNowSeconds, Data.PhaseDeadlineSeconds,
                static_cast<int32>(Data.Objective), Data.InteractionProgress);
            const FString Path = OutputDirectory / (FString(FixtureNames[Index]) + TEXT(".png"));
            FScreenshotRequest::RequestScreenshot(Path, true, false);
            bCaptureRequested = true;
            CapturedAt = Now;
        }
        if (bCaptureRequested && Now - CapturedAt >= 0.5 && !FScreenshotRequest::IsScreenshotRequested())
        {
            ++Index;
            if (Index == UE_ARRAY_COUNT(FixtureNames))
            {
                Finish(PC);
                return true;
            }
            Entered = Now;
            TicksInFixture = 0;
            bCaptureRequested = false;
            bEnteredFixture = false;
            Data = MakeFixture(Index);
        }
        return true;
    }

    FString Name() const
    {
        return Index < UE_ARRAY_COUNT(FixtureNames) ? FString(FixtureNames[Index]) : TEXT("complete");
    }

private:
    void Check(const TCHAR* Name, bool bPassed)
    {
        ++Checks;
        if (!bPassed) ++Failures;
        UE_LOG(LogValStrike, Display, TEXT("VSROUND_PREVIEW_CHECK %s %s"), Name, bPassed ? TEXT("PASS") : TEXT("FAIL"));
    }

    void Finish(AVSPlayerController* PC)
    {
        bFinished = true;
        UE_LOG(LogValStrike, Display, TEXT("VSROUND_PREVIEW_DONE checks=%d failures=%d fixtures=%d"), Checks, Failures, Index);
        PC->QuitSession();
    }

    bool bEnabled = false;
    bool bFinished = false;
    bool bEnteredFixture = false;
    bool bCaptureRequested = false;
    int32 Index = 0;
    int32 TicksInFixture = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    double Started = 0.0;
    double Entered = 0.0;
    double CapturedAt = 0.0;
    FString OutputDirectory;
};

FPreviewSequence& Sequence()
{
    static FPreviewSequence Instance;
    return Instance;
}
}

bool VSRoundHUDPreview::Tick(AVSPlayerController* Controller, FVSMatchHUDData& InOutData)
{
    return Sequence().Tick(Controller, InOutData);
}

FString VSRoundHUDPreview::GetFixtureName()
{
    return Sequence().Name();
}

#else

bool VSRoundHUDPreview::Tick(AVSPlayerController*, FVSMatchHUDData&) { return false; }
FString VSRoundHUDPreview::GetFixtureName() { return {}; }

#endif
