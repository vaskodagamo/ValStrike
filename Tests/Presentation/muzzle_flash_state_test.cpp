#include "../../Source/ValStrike/Presentation/VSMuzzleFlashState.h"

#include <cstdio>
#include <initializer_list>
#include <limits>

namespace
{
using VSMuzzleFlash::State;

int Checks = 0;
int Failures = 0;

void Expect(const char* Name, bool Condition)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::fprintf(stderr, "FAIL %s\n", Name);
    }
}

// Mirrors the component's tick order: Advance, then Trigger on a pulse, then render.
int VisibleFrames(float Duration, float Frame)
{
    State Flash;
    int Frames = 0;
    for (int Tick = 0; Tick < 100; ++Tick)
    {
        Flash.Advance(Frame);
        if (Tick == 0) { Flash.Trigger(1, Duration); }
        Frames += Flash.Current().visible ? 1 : 0;
    }
    return Frames;
}

void Duration()
{
    Expect("idle flash is hidden", !State().Current().visible);
    Expect("40 ms at 60 fps shows 3 frames", VisibleFrames(0.04f, 1.0f / 60.0f) == 3);
    Expect("40 ms at 30 fps shows 2 frames", VisibleFrames(0.04f, 1.0f / 30.0f) == 2);
    Expect("40 ms at 144 fps shows 6 frames", VisibleFrames(0.04f, 1.0f / 144.0f) == 6);
    Expect("a hitch still shows the trigger frame", VisibleFrames(0.04f, 0.5f) == 1);
    Expect("zero duration never shows", VisibleFrames(0.0f, 1.0f / 60.0f) == 0);
    Expect("negative duration never shows", VisibleFrames(-1.0f, 1.0f / 60.0f) == 0);

    State Flash;
    Flash.Trigger(4, 0.04f);
    Flash.Advance(0.0f);
    Flash.Advance(-1.0f);
    Expect("zero/negative frame time does not consume the flash", Flash.IsActive());
}

void PulsesAndCleanup()
{
    State Flash;
    Flash.Trigger(1, 0.04f);
    Flash.Advance(0.03f);
    Flash.Trigger(2, 0.04f);
    Flash.Advance(0.03f);
    Expect("pulse during a flash restarts it", Flash.IsActive());
    Expect("restarted flash uses the newest shot", Flash.Current().rollDegrees == VSMuzzleFlash::RollForShot(2));
    Flash.Advance(0.03f);
    Expect("restart does not stack durations", !Flash.IsActive());

    Flash.Trigger(3, 0.04f);
    Flash.Clear();
    Expect("clear drops an active flash", !Flash.Current().visible);
    Flash.Advance(1.0f);
    Expect("cleared flash never returns without a pulse", !Flash.Current().visible);
}

void DeterministicShape()
{
    using VSMuzzleFlash::LengthForShot;
    using VSMuzzleFlash::RollForShot;
    const unsigned Max = std::numeric_limits<unsigned>::max();
    Expect("same shot gives the same roll", RollForShot(17) == RollForShot(17));
    Expect("consecutive shots differ in roll", RollForShot(1) != RollForShot(2) && RollForShot(2) != RollForShot(3));
    Expect("roll is first shot 137 degrees", RollForShot(1) == 137.0f);
    bool InRange = true;
    for (unsigned Shot : {0u, 1u, 2u, 3u, 359u, 360u, 1000003u, Max})
    {
        InRange = InRange && RollForShot(Shot) >= 0.0f && RollForShot(Shot) < 360.0f
            && LengthForShot(Shot) >= 0.85f && LengthForShot(Shot) <= 1.16f;
    }
    Expect("roll and length stay bounded through counter wrap", InRange);

    State Owner;
    State Observer;
    Owner.Trigger(42, 0.04f);
    Observer.Trigger(42, 0.04f);
    Expect("owner and observer derive the same shape",
        Owner.Current().rollDegrees == Observer.Current().rollDegrees
        && Owner.Current().lengthScale == Observer.Current().lengthScale);
}
}

int main()
{
    Duration();
    PulsesAndCleanup();
    DeterministicShape();
    std::printf("%d/%d muzzle flash checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
