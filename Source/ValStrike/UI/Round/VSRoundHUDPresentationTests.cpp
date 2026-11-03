#if WITH_DEV_AUTOMATION_TESTS

#include "VSRoundHUDPresentation.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVSRoundHUDPresentationTest, "ValStrike.UI.RoundPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVSRoundHUDPresentationTest::RunTest(const FString& Parameters)
{
    FVSMatchHUDData Data;
    Data.Phase = EVSMatchHUDPhase::MatchComplete;
    Data.Winner = EVSMatchHUDTeam::Two;
    Data.TeamOneScore = 5;
    Data.bHasPhaseDeadline = true;
    Data.PhaseDeadlineSeconds = 30;
    Data.bCanRequestRematch = true;
    Data.StatusText = TEXT("Stale result");
    auto View = VSRoundHUDPresentation::Build(Data);
    TestFalse(TEXT("Unavailable snapshot suppresses stale match state"), View.bAvailable);
    TestTrue(TEXT("Unavailable snapshot has no score"), View.TeamOneScore.IsEmpty());
    TestFalse(TEXT("Unavailable snapshot has no timer"), View.bShowTimer);
    TestFalse(TEXT("Unavailable snapshot has no result"), View.bShowResult);
    TestTrue(TEXT("Unavailable snapshot has no rematch hint"), View.RematchHint.IsEmpty());

    Data = {};
    Data.bAvailable = true;
    Data.LocalTeam = EVSMatchHUDTeam::One;
    Data.AttackingTeam = EVSMatchHUDTeam::One;
    Data.TeamOneScore = 2;
    Data.TeamTwoScore = 1;
    Data.CompletedRounds = 3;
    Data.Phase = EVSMatchHUDPhase::Preparation;
    View = VSRoundHUDPresentation::Build(Data);
    TestEqual(TEXT("Current round follows completed rounds"), View.RoundLabel, FString(TEXT("ROUND 4 / 6")));
    TestEqual(TEXT("Stable team one score before swap"), View.TeamOneScore, FString(TEXT("2")));
    TestEqual(TEXT("Initial local attacking role"), View.LocalRole, FString(TEXT("TEAM 1  ·  ATTACKING")));
    Data.AttackingTeam = EVSMatchHUDTeam::Two;
    View = VSRoundHUDPresentation::Build(Data);
    TestEqual(TEXT("Role swap does not swap team one score"), View.TeamOneScore, FString(TEXT("2")));
    TestEqual(TEXT("Role swap does not swap team two score"), View.TeamTwoScore, FString(TEXT("1")));
    TestEqual(TEXT("Local team identity survives role swap"), View.LocalRole, FString(TEXT("TEAM 1  ·  DEFENDING")));
    TestEqual(TEXT("Team two takes attacking role"), View.TeamTwoRole, FString(TEXT("ATTACKING")));

    Data.ServerNowSeconds = 100.1;
    Data.PhaseDeadlineSeconds = 160.0;
    TestFalse(TEXT("No timer invented without availability flag"), VSRoundHUDPresentation::Build(Data).bShowTimer);
    Data.bHasPhaseDeadline = true;
    TestEqual(TEXT("Fractional remaining time rounds upward"), VSRoundHUDPresentation::Build(Data).TimerText, FString(TEXT("01:00")));
    Data.ServerNowSeconds = 160.0;
    TestEqual(TEXT("Exact deadline shows zero"), VSRoundHUDPresentation::Build(Data).TimerText, FString(TEXT("00:00")));
    Data.ServerNowSeconds = 161.0;
    TestEqual(TEXT("Expired deadline never goes negative"), VSRoundHUDPresentation::Build(Data).TimerText, FString(TEXT("00:00")));
    Data.ServerNowSeconds = 0.0;
    Data.PhaseDeadlineSeconds = 1e20;
    TestEqual(TEXT("Huge finite deadline cannot overflow formatting"), VSRoundHUDPresentation::Build(Data).TimerText, FString(TEXT("99:59+")));
    Data.ServerNowSeconds = -std::numeric_limits<double>::max();
    Data.PhaseDeadlineSeconds = std::numeric_limits<double>::max();
    TestFalse(TEXT("Overflowing time difference is unavailable"), VSRoundHUDPresentation::Build(Data).bShowTimer);
    Data.ServerNowSeconds = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("NaN server time is unavailable"), VSRoundHUDPresentation::Build(Data).bShowTimer);
    Data.ServerNowSeconds = 0.0;
    Data.PhaseDeadlineSeconds = std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Infinite deadline is unavailable"), VSRoundHUDPresentation::Build(Data).bShowTimer);

    Data = {};
    Data.bAvailable = true;
    Data.Phase = EVSMatchHUDPhase::Planted;
    Data.Objective = EVSMatchHUDObjective::Planted;
    Data.InteractionPrompt = TEXT("  Hold E\tto\r\ndefuse  ");
    Data.bInteractionActive = true;
    Data.InteractionProgress = 0.45f;
    View = VSRoundHUDPresentation::Build(Data);
    TestEqual(TEXT("Provider controls eligible interaction instruction"), View.InteractionPrompt, FString(TEXT("Hold E to defuse")));
    TestTrue(TEXT("Active interaction has progress"), View.bShowInteractionProgress);
    TestEqual(TEXT("Progress retained"), View.InteractionProgress, 0.45f);
    Data.InteractionProgress = 10.0f;
    TestEqual(TEXT("Progress bounded above"), VSRoundHUDPresentation::Build(Data).InteractionProgress, 1.0f);
    Data.InteractionProgress = -10.0f;
    TestEqual(TEXT("Progress bounded below"), VSRoundHUDPresentation::Build(Data).InteractionProgress, 0.0f);
    Data.InteractionProgress = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Nonfinite progress is not rendered"), VSRoundHUDPresentation::Build(Data).bShowInteractionProgress);
    Data.InteractionProgress = 0.9f;
    Data.InteractionPrompt = TEXT("\n\t ");
    TestTrue(TEXT("Active progress remains visible without a prompt"), VSRoundHUDPresentation::Build(Data).bShowInteractionProgress);
    Data.InteractionPrompt = TEXT("Hold E to defuse");
    Data.bInteractionActive = false;
    TestFalse(TEXT("Cancelled interaction hides old progress"), VSRoundHUDPresentation::Build(Data).bShowInteractionProgress);
    Data.bInteractionActive = true;
    Data.bSpectating = true;
    View = VSRoundHUDPresentation::Build(Data);
    TestTrue(TEXT("Spectator presentation works without any pawn dependency"), View.bSpectating);
    TestEqual(TEXT("Unassigned spectator identity is explicit"), View.LocalRole, FString(TEXT("SPECTATING")));
    TestTrue(TEXT("Spectator does not inherit stale eligible actions"), View.InteractionPrompt.IsEmpty());
    TestFalse(TEXT("Spectator does not inherit stale interaction progress"), View.bShowInteractionProgress);

    Data.bSpectating = false;
    Data.Phase = EVSMatchHUDPhase::Resolved;
    Data.CompletedRounds = 6;
    Data.TeamOneScore = 4;
    Data.TeamTwoScore = 2;
    Data.Winner = EVSMatchHUDTeam::None;
    Data.bCanRequestRematch = true;
    View = VSRoundHUDPresentation::Build(Data);
    TestTrue(TEXT("Resolved state shows a result"), View.bShowResult);
    TestEqual(TEXT("Scores never infer a winner"), View.ResultHeadline, FString(TEXT("ROUND COMPLETE")));
    TestEqual(TEXT("Completed final round does not display round seven"), View.RoundLabel, FString(TEXT("6 / 6 COMPLETE")));
    TestTrue(TEXT("Result suppresses stale interaction action"), View.InteractionPrompt.IsEmpty());
    TestFalse(TEXT("Result suppresses stale progress"), View.bShowInteractionProgress);
    TestTrue(TEXT("Round result cannot offer rematch"), View.RematchHint.IsEmpty());
    Data.Phase = EVSMatchHUDPhase::MatchComplete;
    Data.Winner = EVSMatchHUDTeam::Two;
    Data.LocalTeam = EVSMatchHUDTeam::One;
    View = VSRoundHUDPresentation::Build(Data);
    TestEqual(TEXT("Winner uses authoritative field even if scores disagree"), View.ResultHeadline, FString(TEXT("TEAM 2 WINS MATCH")));
    TestFalse(TEXT("Loss does not use local-winner treatment"), View.bLocalWinner);
    TestEqual(TEXT("Eligible match participant sees fixed rematch key"), View.RematchHint, FString(TEXT("ENTER  REQUEST REMATCH")));
    Data.LocalTeam = EVSMatchHUDTeam::Two;
    TestTrue(TEXT("Winning local identity is stable"), VSRoundHUDPresentation::Build(Data).bLocalWinner);
    Data.bCanRequestRematch = false;
    TestTrue(TEXT("No rematch hint without provider permission"), VSRoundHUDPresentation::Build(Data).RematchHint.IsEmpty());
    Data.bDraw = true;
    View = VSRoundHUDPresentation::Build(Data);
    TestEqual(TEXT("Draw flag takes precedence over stale winner"), View.ResultHeadline, FString(TEXT("MATCH DRAW")));
    TestFalse(TEXT("Draw does not highlight either team as local winner"), View.bLocalWinner);
    Data.Phase = EVSMatchHUDPhase::Active;
    View = VSRoundHUDPresentation::Build(Data);
    TestFalse(TEXT("Next active round hides old result"), View.bShowResult);
    TestTrue(TEXT("Next active round has no old headline"), View.ResultHeadline.IsEmpty());

    Data.StatusText = FString::ChrN(300, TEXT('A'));
    Data.InteractionPrompt = FString::ChrN(200, TEXT('B'));
    Data.TotalRounds = 0;
    Data.TeamOneScore = -1;
    View = VSRoundHUDPresentation::Build(Data);
    TestTrue(TEXT("Status text has a bounded layout length"), View.StatusText.Len() <= 256 && View.StatusText.EndsWith(TEXT("...")));
    TestTrue(TEXT("Interaction text has a bounded layout length"), View.InteractionPrompt.Len() <= 128 && View.InteractionPrompt.EndsWith(TEXT("...")));
    TestEqual(TEXT("Missing round total does not invent a round"), View.RoundLabel, FString(TEXT("ROUND —")));
    TestEqual(TEXT("Invalid score is not fabricated as zero"), View.TeamOneScore, FString(TEXT("—")));
    Data.TotalRounds = TNumericLimits<int32>::Max();
    Data.CompletedRounds = TNumericLimits<int32>::Max();
    TestEqual(TEXT("Current-round formatting avoids signed overflow"), VSRoundHUDPresentation::Build(Data).RoundLabel,
        FString(TEXT("ROUND 2147483647 / 2147483647")));
    return true;
}

#endif
