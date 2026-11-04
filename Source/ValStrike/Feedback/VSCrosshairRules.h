#pragma once

// Engine-free crosshair gap rules: project the equipped weapon's current spread
// cone onto the canvas, clamp it and smooth it toward the target. Presentation
// only; the HUD falls back to the static reticle whenever a rule says "inactive".

#include <algorithm>
#include <cmath>

namespace VSCrosshairRules
{
// Spread cones above this half-angle are not a real weapon state.
constexpr double MaxSpreadDegrees = 45.0;
// Opening follows a shot almost at once; recovery closes a little slower.
constexpr double OpenRatePerSecond = 30.0;
constexpr double CloseRatePerSecond = 12.0;
// Within this distance of the target the gap snaps onto it exactly.
constexpr double SnapPx = 0.05;
// A frame longer than this (hitch, pause) is treated as this long.
constexpr double MaxStepSeconds = 0.25;

inline bool IsFinite(double Value) { return std::isfinite(Value); }

// A spread worth showing: finite, positive and below a sane half-angle.
inline bool IsValidSpread(double SpreadDeg)
{
    return IsFinite(SpreadDeg) && SpreadDeg > 0.0 && SpreadDeg < MaxSpreadDegrees;
}

// gapPx = tan(spread) / tan(hfov / 2) * (canvasWidth / 2). Zero for any input
// that cannot describe a real projection.
inline double ProjectGapPx(double SpreadDeg, double HorizontalFovDeg, double CanvasWidthPx)
{
    constexpr double DegToRad = 3.14159265358979323846 / 180.0;
    if (!IsValidSpread(SpreadDeg) || !IsFinite(HorizontalFovDeg) || HorizontalFovDeg <= 1.0
        || HorizontalFovDeg >= 179.0 || !IsFinite(CanvasWidthPx) || CanvasWidthPx <= 0.0)
    {
        return 0.0;
    }
    return std::tan(SpreadDeg * DegToRad) / std::tan(HorizontalFovDeg * 0.5 * DegToRad)
        * (CanvasWidthPx * 0.5);
}

// The gap the reticle should settle on: the projection clamped to
// [MinGapPx, MaxGapPx], or exactly MinGapPx when the spread is not showable.
inline double TargetGapPx(double SpreadDeg, double HorizontalFovDeg, double CanvasWidthPx,
    double MinGapPx, double MaxGapPx)
{
    const double Projected = ProjectGapPx(SpreadDeg, HorizontalFovDeg, CanvasWidthPx);
    if (Projected <= 0.0) { return MinGapPx; }
    return std::min(std::max(Projected, MinGapPx), std::max(MinGapPx, MaxGapPx));
}

struct State
{
    double gapPx = 0.0;
    bool active = false;

    void Reset()
    {
        gapPx = 0.0;
        active = false;
    }
};

// One frame-rate independent exponential step toward Target. The first step
// after a reset starts from MinGapPx (today's static reticle). Never overshoots
// and lands exactly on the target once close enough. Returns the gap to draw.
inline double Step(State& S, double TargetPx, double DeltaSeconds, double MinGapPx, double MaxGapPx)
{
    const double Hi = std::max(MinGapPx, MaxGapPx);
    const double Target = IsFinite(TargetPx) ? std::min(std::max(TargetPx, MinGapPx), Hi) : MinGapPx;
    if (!S.active || !IsFinite(S.gapPx))
    {
        S.gapPx = MinGapPx;
        S.active = true;
    }
    // The canvas scale can change between frames; keep the gap inside the limits.
    S.gapPx = std::min(std::max(S.gapPx, MinGapPx), Hi);

    const double Dt = IsFinite(DeltaSeconds) ? std::min(std::max(DeltaSeconds, 0.0), MaxStepSeconds) : 0.0;
    const double Rate = Target > S.gapPx ? OpenRatePerSecond : CloseRatePerSecond;
    const double Keep = std::exp(-Rate * Dt);
    S.gapPx = Target + (S.gapPx - Target) * Keep;
    if (std::fabs(S.gapPx - Target) <= SnapPx) { S.gapPx = Target; }
    return S.gapPx;
}
}
