#pragma once

#include "CoreMinimal.h"

// Read-only presentation contract, independent of pawn lifetime and rules internals.
// Match runtime builds this from authoritative replicated state. UI never writes it.
enum class EVSMatchHUDPhase : uint8
{
    Waiting, Preparation, Active, Planted, Resolved, MatchComplete
};

enum class EVSMatchHUDTeam : uint8 { None, One, Two };
enum class EVSMatchHUDObjective : uint8
{
    Unavailable, Carried, PlacementPending, Dropped, Planted, Defused, Detonated
};

struct FVSMatchHUDData
{
    bool bAvailable = false;
    EVSMatchHUDPhase Phase = EVSMatchHUDPhase::Waiting;
    EVSMatchHUDTeam LocalTeam = EVSMatchHUDTeam::None;
    EVSMatchHUDTeam AttackingTeam = EVSMatchHUDTeam::None;
    EVSMatchHUDTeam Winner = EVSMatchHUDTeam::None;
    bool bDraw = false;
    bool bSpectating = false;
    int32 TeamOneScore = 0;
    int32 TeamTwoScore = 0;
    int32 CompletedRounds = 0;
    int32 TotalRounds = 6;

    // Provider uses synchronized server time; UI must not use a local wall clock.
    bool bHasPhaseDeadline = false;
    double ServerNowSeconds = 0.0;
    double PhaseDeadlineSeconds = 0.0;
    EVSMatchHUDObjective Objective = EVSMatchHUDObjective::Unavailable;
    bool bLocalCarrier = false;
    bool bInteractionActive = false;
    float InteractionProgress = 0.0f; // Provider clamps to [0, 1].
    FString InteractionPrompt; // Only actions currently eligible for this player.
    FString StatusText; // Waiting, spectator, round-result or match-result feedback.
    bool bCanRequestRematch = false;
    // Dead participant following a living teammate; bSpectating is also set then.
    FString SpectatingName;
};
