#pragma once

#include <cstddef>
#include <cstdint>

// Engine-independent death-spectate target rule. Presentation only: the client
// chooses among pawns it already receives, and never an enemy's point of view.
namespace ValStrike::Spectate
{
using Id = std::uint64_t;
constexpr double DelaySeconds = 2.0;

struct Candidate
{
    Id PlayerId = 0; // Server connection id; 0 is never a valid target.
    std::uint8_t TeamId = 0;
    bool Participating = false;
    bool Alive = false; // Replicated life state and a live pawn, both required.
};

struct Viewer
{
    Id PlayerId = 0;
    std::uint8_t TeamId = 0;
};

inline bool CanWatch(const Candidate& Target, const Viewer& Self)
{
    return Target.PlayerId != 0 && Target.PlayerId != Self.PlayerId && Self.TeamId != 0 &&
        Target.TeamId == Self.TeamId && Target.Participating && Target.Alive;
}

// The eligible teammate following Current in ascending id order, wrapping
// around. Current may itself be gone or dead, which gives the auto-advance;
// Current 0 selects the lowest id. Returns 0 when nobody can be watched.
inline Id Next(const Candidate* Candidates, std::size_t Count, const Viewer& Self, Id Current)
{
    Id After = 0;
    Id Lowest = 0;
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        const Candidate& Target = Candidates[Index];
        if (!CanWatch(Target, Self)) { continue; }
        if (Lowest == 0 || Target.PlayerId < Lowest) { Lowest = Target.PlayerId; }
        if (Target.PlayerId > Current && (After == 0 || Target.PlayerId < After)) { After = Target.PlayerId; }
    }
    return After != 0 ? After : Lowest;
}

// Keep a still-eligible target; otherwise move on as Next would.
inline Id Keep(const Candidate* Candidates, std::size_t Count, const Viewer& Self, Id Current)
{
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        if (Candidates[Index].PlayerId == Current && CanWatch(Candidates[Index], Self)) { return Current; }
    }
    return Next(Candidates, Count, Self, Current);
}

// A dead participant of a live or just-resolved round may spectate once the
// death view has been shown for DelaySeconds. Late joins never participate.
inline bool ShouldSpectate(bool bRoundLive, bool bParticipating, bool bAlive, double SecondsSinceDeath)
{
    return bRoundLive && bParticipating && !bAlive && SecondsSinceDeath >= DelaySeconds;
}
}
