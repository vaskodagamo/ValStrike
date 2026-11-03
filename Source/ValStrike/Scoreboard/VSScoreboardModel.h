#pragma once

#include "CoreMinimal.h"
#include "VSScoreboardState.h"

class AGameStateBase;
class APlayerState;

/** Display-ready scoreboard snapshot. Built from replicated state only. */
struct FVSScoreboardLine
{
    FString Name;
    int32 Kills = 0;
    int32 Deaths = 0;
    int32 Assists = 0;
    int32 HeadshotKills = 0;
    int32 DamageDealt = 0;
    int32 PingMs = -1; // -1 while unknown
    EVSScoreboardLife Life = EVSScoreboardLife::Spectating;
    bool bLocal = false;
};

struct FVSScoreboardTeam
{
    uint8 TeamId = 0;
    FString Title; // "TEAM 1", "TEAM 2", "UNASSIGNED" or "PLAYERS"
    FString Role;  // "ATTACKING" / "DEFENDING" in match mode
    int32 Score = -1; // Round score from the replicated match state; -1 without a match
    TArray<FVSScoreboardLine> Lines;
};

struct FVSScoreboardModel
{
    bool bHasStats = false; // The replicated stats actor exists on this machine.
    bool bHasMatch = false; // A replicated match state exists (Site01 match mode).
    FString Status;         // "ROUND 2 / 6 · ACTIVE", "PRACTICE" ...
    TArray<FVSScoreboardTeam> Teams;

    // Every displayed value, for cheap change detection.
    FString Signature() const;
    const FVSScoreboardLine* FindLine(const FString& Name) const;
    const FVSScoreboardTeam* FindTeam(uint8 TeamId) const;
};

namespace VSScoreboardModel
{
FVSScoreboardModel Build(const AVSScoreboardState* Stats, const AGameStateBase* GameState, const APlayerState* LocalPlayer);
FString DisplayName(const FString& Raw);
}
