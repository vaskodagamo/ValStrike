#pragma once

#include <cstdint>

namespace VSMuzzleFlash
{
struct Pose
{
    bool visible = false;
    float rollDegrees = 0.0f;
    float lengthScale = 1.0f;
};

// Random-free variation from the accepted shot counter, so the owner and every
// observer derive the same shape for the same shot. 137° steps keep
// neighbouring shots in a burst visibly different.
inline float RollForShot(std::uint32_t Shot)
{
    return static_cast<float>((Shot % 360u) * 137u % 360u);
}

inline float LengthForShot(std::uint32_t Shot)
{
    return 0.85f + 0.1f * static_cast<float>(Shot % 4u);
}

/** One brief cosmetic flash per accepted-shot pulse. No queue, no replay. */
class State
{
public:
    // Call once per pulse, after Advance for the same frame, so the trigger frame
    // always renders. A pulse during an active flash restarts it (no stacking).
    void Trigger(std::uint32_t Shot, float Duration)
    {
        Remaining = Duration > 0.0f ? Duration : 0.0f;
        LastShot = Shot;
    }

    void Advance(float DeltaSeconds)
    {
        if (DeltaSeconds > 0.0f)
        {
            Remaining = DeltaSeconds >= Remaining ? 0.0f : Remaining - DeltaSeconds;
        }
    }

    // Hidden, dead, stowed or reloading views drop any flash in progress.
    void Clear() { Remaining = 0.0f; }

    bool IsActive() const { return Remaining > 0.0f; }

    Pose Current() const
    {
        Pose Result;
        Result.visible = IsActive();
        Result.rollDegrees = RollForShot(LastShot);
        Result.lengthScale = LengthForShot(LastShot);
        return Result;
    }

private:
    float Remaining = 0.0f;
    std::uint32_t LastShot = 0;
};
}
