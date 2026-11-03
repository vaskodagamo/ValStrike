#include "VSRoundHUDPresentation.h"

namespace
{
bool IsTeam(EVSMatchHUDTeam Team)
{
    return Team == EVSMatchHUDTeam::One || Team == EVSMatchHUDTeam::Two;
}

FString TeamName(EVSMatchHUDTeam Team)
{
    return Team == EVSMatchHUDTeam::One ? TEXT("TEAM 1") : TEXT("TEAM 2");
}

FString TeamRole(EVSMatchHUDTeam Team, EVSMatchHUDTeam AttackingTeam)
{
    if (!IsTeam(AttackingTeam)) return TEXT("ROLE UNASSIGNED");
    return Team == AttackingTeam ? TEXT("ATTACKING") : TEXT("DEFENDING");
}

// Provider messages are plain text. Keep user-facing lines bounded and prevent
// tabs/newlines/control characters from escaping the HUD's measured layout.
FString CleanText(const FString& Input, int32 Limit)
{
    FString Result;
    bool bPendingSpace = false;
    for (TCHAR Character : Input)
    {
        if (FChar::IsWhitespace(Character))
        {
            bPendingSpace = !Result.IsEmpty();
            continue;
        }
        if (Character < TEXT(' ') || Character == 0x7f) continue;
        if (bPendingSpace) Result.AppendChar(TEXT(' '));
        Result.AppendChar(Character);
        bPendingSpace = false;
        if (Result.Len() > Limit)
        {
            return Result.Left(Limit - 3).TrimEnd() + TEXT("...");
        }
    }
    return Result;
}
}

FVSRoundHUDPresentation VSRoundHUDPresentation::Build(const FVSMatchHUDData& Data)
{
    FVSRoundHUDPresentation View;
    if (!Data.bAvailable) return View;
    View.bAvailable = true;
    View.bSpectating = Data.bSpectating;
    View.TeamOneScore = Data.TeamOneScore >= 0 ? FString::FromInt(Data.TeamOneScore) : TEXT("—");
    View.TeamTwoScore = Data.TeamTwoScore >= 0 ? FString::FromInt(Data.TeamTwoScore) : TEXT("—");
    View.TeamOneRole = TeamRole(EVSMatchHUDTeam::One, Data.AttackingTeam);
    View.TeamTwoRole = TeamRole(EVSMatchHUDTeam::Two, Data.AttackingTeam);
    if (IsTeam(Data.LocalTeam))
    {
        View.LocalRole = TeamName(Data.LocalTeam) + TEXT("  ·  ") + TeamRole(Data.LocalTeam, Data.AttackingTeam);
        if (Data.bSpectating) View.LocalRole = TEXT("SPECTATING  ·  ") + TeamName(Data.LocalTeam);
    }
    else
    {
        View.LocalRole = Data.bSpectating ? TEXT("SPECTATING") : TEXT("TEAM UNASSIGNED");
    }

    switch (Data.Phase)
    {
    case EVSMatchHUDPhase::Waiting: View.PhaseLabel = TEXT("WAITING"); break;
    case EVSMatchHUDPhase::Preparation: View.PhaseLabel = TEXT("PREPARATION"); break;
    case EVSMatchHUDPhase::Active: View.PhaseLabel = TEXT("ACTIVE"); break;
    case EVSMatchHUDPhase::Planted: View.PhaseLabel = TEXT("PLANTED"); break;
    case EVSMatchHUDPhase::Resolved: View.PhaseLabel = TEXT("ROUND OVER"); break;
    case EVSMatchHUDPhase::MatchComplete: View.PhaseLabel = TEXT("MATCH OVER"); break;
    default: View.PhaseLabel = TEXT("MATCH"); break;
    }

    if (Data.TotalRounds <= 0 || Data.CompletedRounds < 0 || Data.CompletedRounds > Data.TotalRounds)
    {
        View.RoundLabel = TEXT("ROUND —");
    }
    else if (Data.Phase == EVSMatchHUDPhase::Preparation || Data.Phase == EVSMatchHUDPhase::Active || Data.Phase == EVSMatchHUDPhase::Planted)
    {
        const int64 Round = FMath::Min(static_cast<int64>(Data.CompletedRounds) + 1, static_cast<int64>(Data.TotalRounds));
        View.RoundLabel = FString::Printf(TEXT("ROUND %lld / %d"), Round, Data.TotalRounds);
    }
    else
    {
        View.RoundLabel = FString::Printf(TEXT("%d / %d COMPLETE"), Data.CompletedRounds, Data.TotalRounds);
    }

    const double Remaining = Data.PhaseDeadlineSeconds - Data.ServerNowSeconds;
    if (Data.bHasPhaseDeadline && FMath::IsFinite(Data.ServerNowSeconds) && FMath::IsFinite(Data.PhaseDeadlineSeconds) && FMath::IsFinite(Remaining))
    {
        View.bShowTimer = true;
        switch (Data.Phase)
        {
        case EVSMatchHUDPhase::Preparation: View.TimerLabel = TEXT("STARTS IN"); break;
        case EVSMatchHUDPhase::Active: View.TimerLabel = TEXT("ROUND TIME"); break;
        case EVSMatchHUDPhase::Planted: View.TimerLabel = TEXT("DETONATES IN"); break;
        case EVSMatchHUDPhase::Resolved: View.TimerLabel = TEXT("NEXT ROUND"); break;
        default: View.TimerLabel = TEXT("TIME"); break;
        }
        // Clamp before integer conversion. The suffix makes a presentation cap
        // explicit instead of silently presenting a shorter authoritative time.
        const int32 Seconds = FMath::CeilToInt(FMath::Clamp(Remaining, 0.0, 5999.0));
        View.TimerText = FString::Printf(TEXT("%02d:%02d%s"), Seconds / 60, Seconds % 60, Remaining > 5999.0 ? TEXT("+") : TEXT(""));
    }

    switch (Data.Objective)
    {
    case EVSMatchHUDObjective::Carried:
        View.ObjectiveLabel = Data.bLocalCarrier && !Data.bSpectating ? TEXT("YOU CARRY THE DEVICE") : TEXT("DEVICE CARRIED");
        break;
    case EVSMatchHUDObjective::PlacementPending: View.ObjectiveLabel = TEXT("PLACING DEVICE"); break;
    case EVSMatchHUDObjective::Dropped: View.ObjectiveLabel = TEXT("DEVICE DROPPED"); break;
    case EVSMatchHUDObjective::Planted: View.ObjectiveLabel = TEXT("DEVICE PLANTED"); break;
    case EVSMatchHUDObjective::Defused: View.ObjectiveLabel = TEXT("DEVICE DEFUSED"); break;
    case EVSMatchHUDObjective::Detonated: View.ObjectiveLabel = TEXT("DEVICE DETONATED"); break;
    default: break;
    }
    View.StatusText = CleanText(Data.StatusText, 256);
    View.bShowResult = Data.Phase == EVSMatchHUDPhase::Resolved || Data.Phase == EVSMatchHUDPhase::MatchComplete;
    if (View.bShowResult)
    {
        const bool bMatchComplete = Data.Phase == EVSMatchHUDPhase::MatchComplete;
        View.bDraw = Data.bDraw;
        if (Data.bDraw)
        {
            View.ResultHeadline = bMatchComplete ? TEXT("MATCH DRAW") : TEXT("ROUND DRAW");
        }
        else if (IsTeam(Data.Winner))
        {
            View.ResultHeadline = TeamName(Data.Winner) + (bMatchComplete ? TEXT(" WINS MATCH") : TEXT(" WINS ROUND"));
            View.bLocalWinner = IsTeam(Data.LocalTeam) && Data.LocalTeam == Data.Winner;
        }
        else
        {
            View.ResultHeadline = bMatchComplete ? TEXT("MATCH COMPLETE") : TEXT("ROUND COMPLETE");
        }
        if (bMatchComplete && Data.bCanRequestRematch)
            View.RematchHint = TEXT("ENTER  REQUEST REMATCH");
    }
    else if (!Data.bSpectating)
    {
        View.InteractionPrompt = CleanText(Data.InteractionPrompt, 128);
        View.bShowInteractionProgress = Data.bInteractionActive && FMath::IsFinite(Data.InteractionProgress);
        if (View.bShowInteractionProgress) View.InteractionProgress = FMath::Clamp(Data.InteractionProgress, 0.0f, 1.0f);
    }
    return View;
}
