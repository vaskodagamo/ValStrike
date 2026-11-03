#pragma once

#include <cstdint>

namespace VSRiflePresentation
{
struct Snapshot
{
    bool available = false;
    bool alive = false;
    bool reloading = false;
    std::uint32_t shots = 0;
};

struct Update
{
    bool shotPulse = false;
    bool reloadStarted = false;
    bool reloadStopped = false;
    bool lifeChanged = false;
    bool visible = false;
    bool reloading = false;
};

/** Derives cosmetic transitions from read-only accepted state. No gameplay writes. */
class State
{
public:
    // Call for each new attachment/pawn. Show the current pose without replaying
    // shots, reload starts or life changes observed before attachment.
    Update Initialize(const Snapshot& Current)
    {
        Previous = Current;
        bInitialized = true;
        return Pose(Current);
    }

    Update Observe(const Snapshot& Current)
    {
        if (!bInitialized)
        {
            return Initialize(Current);
        }

        Update Result = Pose(Current);
        const Update PriorPose = Pose(Previous);
        Result.lifeChanged = Previous.available && Current.available
            && Previous.alive != Current.alive;
        Result.reloadStopped = PriorPose.reloading && !Result.reloading;

        // An availability gap or revival establishes a fresh baseline. Neither
        // can prove when a received shot/reload happened, so only its pose is shown.
        if (PriorPose.visible && Result.visible)
        {
            Result.reloadStarted = !PriorPose.reloading && Result.reloading;
            // A replicated batch is one pulse, never a queue of historical shots.
            // Counter decreases (including wrap) are treated as reset baselines.
            Result.shotPulse = Current.shots > Previous.shots;
        }

        Previous = Current;
        return Result;
    }

private:
    static Update Pose(const Snapshot& Value)
    {
        Update Result;
        Result.visible = Value.available && Value.alive;
        Result.reloading = Result.visible && Value.reloading;
        return Result;
    }

    Snapshot Previous;
    bool bInitialized = false;
};
}
