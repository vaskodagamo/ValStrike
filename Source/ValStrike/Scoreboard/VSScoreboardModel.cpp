#include "VSScoreboardModel.h"

#include "../Match/VSMatchGameState.h"
#include "../Rules/VSMatchRules.h"
#include "../VSMatchHUDData.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

namespace
{
const TCHAR* VSScoreboardPhaseLabel(uint8 Value)
{
    using VSBoardPhase = ValStrike::Rules::Phase;
    switch (static_cast<VSBoardPhase>(Value))
    {
    case VSBoardPhase::Waiting: return TEXT("WAITING");
    case VSBoardPhase::Preparation: return TEXT("PREPARATION");
    case VSBoardPhase::Active: return TEXT("ACTIVE");
    case VSBoardPhase::Planted: return TEXT("PLANTED");
    case VSBoardPhase::Resolved: return TEXT("ROUND OVER");
    case VSBoardPhase::MatchComplete: return TEXT("MATCH OVER");
    default: return TEXT("MATCH");
    }
}
}

FString VSScoreboardModel::DisplayName(const FString& Raw)
{
    FString Clean;
    for (const TCHAR Character : Raw)
    {
        if (Character >= TEXT(' ') && Character != 0x7f) { Clean.AppendChar(Character); }
    }
    Clean.TrimStartAndEndInline();
    if (Clean.IsEmpty()) { return TEXT("Player"); }
    return Clean.Len() > 20 ? Clean.Left(19) + TEXT("…") : Clean;
}

FVSScoreboardModel VSScoreboardModel::Build(const AVSScoreboardState* Stats, const AGameStateBase* GameState,
    const APlayerState* LocalPlayer)
{
    FVSScoreboardModel Model;
    const AVSMatchGameState* Match = Cast<AVSMatchGameState>(GameState);
    Model.bHasStats = Stats != nullptr;
    Model.bHasMatch = Match != nullptr;
    if (Match)
    {
        const int32 Total = FVSMatchHUDData().TotalRounds;
        const int32 Round = FMath::Clamp(Match->Match.CompletedRounds + 1, 1, Total);
        Model.Status = Match->Match.Phase == static_cast<uint8>(ValStrike::Rules::Phase::MatchComplete)
            ? FString::Printf(TEXT("%d / %d ROUNDS  ·  %s"), Match->Match.CompletedRounds, Total, VSScoreboardPhaseLabel(Match->Match.Phase))
            : FString::Printf(TEXT("ROUND %d / %d  ·  %s"), Round, Total, VSScoreboardPhaseLabel(Match->Match.Phase));
        for (uint8 TeamId : {static_cast<uint8>(1), static_cast<uint8>(2)})
        {
            FVSScoreboardTeam& Team = Model.Teams.AddDefaulted_GetRef();
            Team.TeamId = TeamId;
            Team.Title = FString::Printf(TEXT("TEAM %d"), TeamId);
            Team.Role = Match->Match.AttackingTeam == TeamId ? TEXT("ATTACKING") : TEXT("DEFENDING");
            Team.Score = TeamId == 1 ? Match->Match.ScoreOne : Match->Match.ScoreTwo;
        }
    }
    else
    {
        Model.Status = TEXT("PRACTICE");
        FVSScoreboardTeam& Everyone = Model.Teams.AddDefaulted_GetRef();
        Everyone.Title = TEXT("PLAYERS");
    }
    if (!Stats)
    {
        return Model;
    }
    for (const FVSScoreboardRow& Row : Stats->GetRows())
    {
        FVSScoreboardTeam* Team = nullptr;
        if (Match)
        {
            Team = Model.Teams.FindByPredicate([&Row](const FVSScoreboardTeam& Item) { return Item.TeamId == Row.TeamId && Item.TeamId != 0; });
            if (!Team)
            {
                Team = Model.Teams.FindByPredicate([](const FVSScoreboardTeam& Item) { return Item.TeamId == 0; });
                if (!Team)
                {
                    Team = &Model.Teams.AddDefaulted_GetRef();
                    Team->Title = TEXT("UNASSIGNED");
                }
            }
        }
        else
        {
            Team = &Model.Teams[0];
        }
        FVSScoreboardLine& Line = Team->Lines.AddDefaulted_GetRef();
        const APlayerState* Player = Row.PlayerState.Get();
        Line.Name = DisplayName(Player ? Player->GetPlayerName() : Row.PlayerName);
        Line.Kills = Row.Kills;
        Line.Deaths = Row.Deaths;
        Line.Assists = Row.Assists;
        Line.HeadshotKills = Row.HeadshotKills;
        Line.DamageDealt = Row.DamageDealt;
        const float Ping = Player ? Player->GetPingInMilliseconds() : -1.0f;
        Line.PingMs = FMath::IsFinite(Ping) && Ping >= 0.0f ? FMath::Min(999, FMath::RoundToInt32(Ping)) : -1;
        Line.Life = Row.Life;
        Line.bLocal = Player && Player == LocalPlayer;
    }
    return Model;
}

FString FVSScoreboardModel::Signature() const
{
    FString Result = FString::Printf(TEXT("%d%d|%s"), bHasStats ? 1 : 0, bHasMatch ? 1 : 0, *Status);
    for (const FVSScoreboardTeam& Team : Teams)
    {
        Result += FString::Printf(TEXT("#%d|%s|%s|%d"), Team.TeamId, *Team.Title, *Team.Role, Team.Score);
        for (const FVSScoreboardLine& Line : Team.Lines)
        {
            Result += FString::Printf(TEXT(";%s|%d|%d|%d|%d|%d|%d|%d|%d"), *Line.Name, Line.Kills, Line.Deaths,
                Line.Assists, Line.HeadshotKills, Line.DamageDealt, Line.PingMs, static_cast<int32>(Line.Life), Line.bLocal ? 1 : 0);
        }
    }
    return Result;
}

const FVSScoreboardLine* FVSScoreboardModel::FindLine(const FString& Name) const
{
    for (const FVSScoreboardTeam& Team : Teams)
    {
        for (const FVSScoreboardLine& Line : Team.Lines)
        {
            if (Line.Name == Name) { return &Line; }
        }
    }
    return nullptr;
}

const FVSScoreboardTeam* FVSScoreboardModel::FindTeam(uint8 TeamId) const
{
    return Teams.FindByPredicate([TeamId](const FVSScoreboardTeam& Team) { return Team.TeamId == TeamId; });
}
