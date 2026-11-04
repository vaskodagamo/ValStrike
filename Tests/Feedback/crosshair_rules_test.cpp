// Standalone checks for the spread crosshair projection, clamp and smoothing rules.
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Feedback/crosshair_rules_test.cpp -o Saved/Tests/Feedback/crosshair_rules_test
#include "../../Source/ValStrike/Feedback/VSCrosshairRules.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
int Failures = 0;
int Checks = 0;

void Expect(bool Condition, const char* What, int Line)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::printf("FAIL line %d: %s\n", Line, What);
    }
}
#define EXPECT(Condition) Expect((Condition), #Condition, __LINE__)

bool Near(double A, double B, double Epsilon = 1e-6) { return std::fabs(A - B) <= Epsilon; }

using namespace VSCrosshairRules;

const double NaN = std::numeric_limits<double>::quiet_NaN();
const double Inf = std::numeric_limits<double>::infinity();
const double MinGap = 4.0;
const double MaxGap = 60.0;

void Projection()
{
    // 90 degree horizontal FOV: tan(45) = 1, so the gap is tan(spread) * half width.
    EXPECT(Near(ProjectGapPx(2.5, 90.0, 1280.0), std::tan(2.5 * 3.14159265358979323846 / 180.0) * 640.0));
    // Narrower FOV magnifies the same cone; a wider canvas scales it linearly.
    EXPECT(ProjectGapPx(2.5, 60.0, 1280.0) > ProjectGapPx(2.5, 90.0, 1280.0));
    EXPECT(Near(ProjectGapPx(2.5, 90.0, 2560.0), 2.0 * ProjectGapPx(2.5, 90.0, 1280.0)));
    // Inputs that cannot describe a projection give no gap.
    for (const double Spread : {0.0, -1.0, NaN, Inf, 45.0, 90.0})
        EXPECT(ProjectGapPx(Spread, 90.0, 1280.0) == 0.0);
    for (const double Fov : {0.0, -90.0, 180.0, NaN, Inf})
        EXPECT(ProjectGapPx(2.5, Fov, 1280.0) == 0.0);
    for (const double Width : {0.0, -1280.0, NaN, Inf})
        EXPECT(ProjectGapPx(2.5, 90.0, Width) == 0.0);
}

void Target()
{
    // Zero, NaN or out-of-range spread is exactly today's static gap.
    for (const double Spread : {0.0, -3.0, NaN, Inf})
        EXPECT(TargetGapPx(Spread, 90.0, 1280.0, MinGap, MaxGap) == MinGap);
    EXPECT(TargetGapPx(2.5, NaN, 1280.0, MinGap, MaxGap) == MinGap);
    // A tiny cone never draws inside the static gap; a huge one stops at the maximum.
    EXPECT(TargetGapPx(0.01, 90.0, 1280.0, MinGap, MaxGap) == MinGap);
    EXPECT(TargetGapPx(8.0, 50.0, 2560.0, MinGap, MaxGap) == MaxGap);
    const double Mid = ProjectGapPx(2.5, 90.0, 1280.0);
    EXPECT(Mid > MinGap && Mid < MaxGap);
    EXPECT(TargetGapPx(2.5, 90.0, 1280.0, MinGap, MaxGap) == Mid);
    // A maximum below the minimum cannot invert the clamp.
    EXPECT(TargetGapPx(8.0, 90.0, 1280.0, MinGap, 1.0) == MinGap);
}

void Smoothing()
{
    // The first step after a reset starts at the static gap.
    State S;
    EXPECT(Step(S, MinGap, 0.016, MinGap, MaxGap) == MinGap);
    EXPECT(S.active);

    // Opens toward the target without overshooting, monotonically.
    double Previous = S.gapPx;
    for (int Frame = 0; Frame < 10; ++Frame)
    {
        const double Gap = Step(S, 30.0, 0.016, MinGap, MaxGap);
        EXPECT(Gap > Previous || Gap == 30.0);
        EXPECT(Gap <= 30.0);
        Previous = Gap;
    }
    // Settles exactly on the target.
    for (int Frame = 0; Frame < 200; ++Frame) Step(S, 30.0, 0.016, MinGap, MaxGap);
    EXPECT(S.gapPx == 30.0);

    // Recovery closes slower than opening and lands exactly on the static gap.
    State Open;
    Step(Open, MinGap, 0.0, MinGap, MaxGap);
    Step(Open, 30.0, 0.05, MinGap, MaxGap);
    const double Opened = Open.gapPx - MinGap;
    State Close = S;
    Step(Close, MinGap, 0.05, MinGap, MaxGap);
    const double Closed = 30.0 - Close.gapPx;
    EXPECT(Opened / (30.0 - MinGap) > Closed / (30.0 - MinGap));
    for (int Frame = 0; Frame < 400; ++Frame) Step(Close, MinGap, 0.016, MinGap, MaxGap);
    EXPECT(Close.gapPx == MinGap);

    // Frame-rate independent: one 100 ms frame equals ten 10 ms frames.
    State Coarse, Fine;
    Step(Coarse, MinGap, 0.0, MinGap, MaxGap);
    Step(Fine, MinGap, 0.0, MinGap, MaxGap);
    Step(Coarse, 40.0, 0.1, MinGap, MaxGap);
    for (int Frame = 0; Frame < 10; ++Frame) Step(Fine, 40.0, 0.01, MinGap, MaxGap);
    EXPECT(Near(Coarse.gapPx, Fine.gapPx, 1e-9));

    // Zero, negative or NaN time changes nothing; a long hitch is bounded.
    State Hold = Fine;
    for (const double Dt : {0.0, -1.0, NaN})
        EXPECT(Step(Hold, 50.0, Dt, MinGap, MaxGap) == Fine.gapPx);
    State Hitch = Fine, Bounded = Fine;
    Step(Hitch, 50.0, 10.0, MinGap, MaxGap);
    Step(Bounded, 50.0, MaxStepSeconds, MinGap, MaxGap);
    EXPECT(Hitch.gapPx == Bounded.gapPx);

    // Non-finite targets close to the static gap; a corrupted state restarts there.
    State Bad = S;
    for (int Frame = 0; Frame < 400; ++Frame) Step(Bad, NaN, 0.016, MinGap, MaxGap);
    EXPECT(Bad.gapPx == MinGap);
    Bad.gapPx = NaN;
    EXPECT(Step(Bad, MinGap, 0.016, MinGap, MaxGap) == MinGap);

    // A smaller canvas scale clamps a wide gap into the new limits at once.
    State Shrink = S;
    EXPECT(Step(Shrink, 10.0, 0.0, 3.0, 20.0) == 20.0);

    // Reset returns to the inactive state; the next step starts at the static gap.
    S.Reset();
    EXPECT(!S.active && S.gapPx == 0.0);
    EXPECT(Step(S, 30.0, 0.0, MinGap, MaxGap) == MinGap);
}
}

int main()
{
    Projection();
    Target();
    Smoothing();
    std::printf("%d/%d crosshair rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
