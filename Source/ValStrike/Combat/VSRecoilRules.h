#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Engine-free recoil and spread model, shared by the server's authoritative shot
// trace, the owning client's prediction and the standalone test in Tests/Weapons.
// Angles are degrees (positive pitch up, positive yaw right); times are seconds.
//
// A shot's direction offset depends only on the tuning, the shot index (recent
// consecutive shots), the shooter's stance and a seed derived from the shot id.
// The client never sends an offset: server and client each evaluate ShotOffset().
namespace VSRecoilRules
{
enum class Stance : std::uint8_t { Crouched, Standing, Moving, Airborne };

struct Tuning
{
    // Heat: one unit per accepted shot; after recoveryDelay without a shot it drains
    // at recoveryRate units per second. A shot's index is floor(heat) before it.
    double recoveryDelay = 0.18;
    double recoveryRate = 10.0;
    // Pattern: climb per index up to maxClimb; horizontal drift from driftStartIndex,
    // swinging within +-maxDrift over driftPeriodShots.
    double climbPerShot = 0.0;
    double maxClimb = 0.0;
    std::uint32_t driftStartIndex = 0;
    double maxDrift = 0.0;
    double driftPeriodShots = 8.0;
    // Spread cone half-angle: base plus bloom per index (capped), scaled when crouched;
    // moving and airborne add a flat penalty. Clamped to maxSpread.
    double baseSpread = 0.0;
    double bloomPerShot = 0.0;
    double maxBloom = 0.0;
    double crouchScale = 1.0;
    double movingSpread = 0.0;
    double airborneSpread = 0.0;
    double maxSpread = 0.0;
    // Horizontal speed (cm/s) above which a grounded shooter counts as moving.
    double movingSpeed = 200.0;
    // Owning-client view kick per accepted shot (yaw magnitude; its sign comes from
    // the seed) and its recovery after viewRecoveryDelay, in degrees per second.
    double viewKickPitch = 0.0;
    double viewKickYaw = 0.0;
    double viewRecoveryDelay = 0.0;
    double viewRecoveryRate = 0.0;
};

// Playtest proposals, not final balance. Both keep the first shot of a standing or
// crouched shooter exact so aimed single shots land where they point.
inline Tuning RifleTuning()
{
    Tuning T;
    T.recoveryDelay = 0.18;   // Longer than the 0.12 s cadence: held fire keeps climbing.
    T.recoveryRate = 10.0;
    T.climbPerShot = 0.3;
    T.maxClimb = 3.0;
    T.driftStartIndex = 5;
    T.maxDrift = 1.2;
    T.driftPeriodShots = 10.0;
    T.baseSpread = 0.0;
    T.bloomPerShot = 0.15;
    T.maxBloom = 1.5;
    T.crouchScale = 0.6;
    T.movingSpread = 2.5;
    T.airborneSpread = 6.0;
    T.maxSpread = 8.0;
    T.viewKickPitch = 0.1;
    T.viewKickYaw = 0.04;
    T.viewRecoveryDelay = 0.18;
    T.viewRecoveryRate = 6.0;
    return T;
}

inline Tuning PistolTuning()
{
    Tuning T;
    T.recoveryDelay = 0.0;    // Fast recovery: only spam clicking accumulates kick.
    T.recoveryRate = 3.0;
    T.climbPerShot = 0.5;
    T.maxClimb = 2.0;
    T.baseSpread = 0.0;
    T.bloomPerShot = 0.4;
    T.maxBloom = 2.0;
    T.crouchScale = 0.6;
    T.movingSpread = 1.5;
    T.airborneSpread = 5.0;
    T.maxSpread = 7.0;
    T.viewKickPitch = 0.6;
    T.viewKickYaw = 0.15;
    T.viewRecoveryDelay = 0.05;
    T.viewRecoveryRate = 6.0;
    return T;
}

constexpr double MaxHeat = 64.0;
constexpr double FullTurnRadians = 6.283185307179586;

struct State
{
    double heat = 0.0;
    double lastShotTime = 0.0;
    bool bHasShot = false;
};

struct Offset
{
    double pitch = 0.0;
    double yaw = 0.0;
};

// Airborne beats moving beats crouched; crouch-walking stays below movingSpeed.
inline Stance ClassifyStance(bool bFalling, bool bCrouched, double HorizontalSpeed, const Tuning& T)
{
    if (bFalling) { return Stance::Airborne; }
    if (HorizontalSpeed > T.movingSpeed) { return Stance::Moving; }
    return bCrouched ? Stance::Crouched : Stance::Standing;
}

// Heat remaining at Now. Time running backwards or non-finite counts as no time passed.
inline double HeatAt(const State& S, const Tuning& T, double Now)
{
    if (!S.bHasShot) { return 0.0; }
    const double Elapsed = Now - S.lastShotTime;
    const double Draining = std::isfinite(Elapsed) ? std::max(0.0, Elapsed - T.recoveryDelay) : 0.0;
    return std::max(0.0, S.heat - Draining * T.recoveryRate);
}

inline std::uint32_t ShotIndexAt(const State& S, const Tuning& T, double Now)
{
    return static_cast<std::uint32_t>(std::floor(std::min(HeatAt(S, T, Now), MaxHeat)));
}

// Records an accepted shot at Now and returns its index (0 = first shot after recovery).
inline std::uint32_t RegisterShot(State& S, const Tuning& T, double Now)
{
    const double Heat = std::min(HeatAt(S, T, Now), MaxHeat);
    const std::uint32_t Index = static_cast<std::uint32_t>(std::floor(Heat));
    S.heat = std::min(Heat + 1.0, MaxHeat);
    if (std::isfinite(Now) && (!S.bHasShot || Now > S.lastShotTime)) { S.lastShotTime = Now; }
    S.bHasShot = true;
    return Index;
}

// Integer avalanche hash (lowbias32): identical on every platform.
inline std::uint32_t Mix(std::uint32_t X)
{
    X ^= X >> 16;
    X *= 0x7feb352dU;
    X ^= X >> 15;
    X *= 0x846ca68bU;
    X ^= X >> 16;
    return X;
}

inline std::uint32_t ShotSeed(std::uint32_t ShotId, std::uint32_t WeaponSalt)
{
    return Mix(ShotId ^ Mix(WeaponSalt + 0x9e3779b9U));
}

// Uniform value in [0, 1) from 24 hash bits.
inline double UnitValue(std::uint32_t Seed, std::uint32_t Stream)
{
    return static_cast<double>(Mix(Seed ^ Mix(Stream)) >> 8) * (1.0 / 16777216.0);
}

inline double SpreadDegrees(const Tuning& T, std::uint32_t Index, Stance S)
{
    double Spread = T.baseSpread + std::min(T.bloomPerShot * Index, T.maxBloom);
    switch (S)
    {
    case Stance::Crouched: Spread *= T.crouchScale; break;
    case Stance::Standing: break;
    case Stance::Moving: Spread += T.movingSpread; break;
    case Stance::Airborne: Spread += T.airborneSpread; break;
    }
    return std::min(std::max(Spread, 0.0), T.maxSpread);
}

// The learnable part: identical for every shot with the same index.
inline Offset PatternOffset(const Tuning& T, std::uint32_t Index)
{
    Offset O;
    O.pitch = std::min(T.climbPerShot * Index, T.maxClimb);
    if (Index >= T.driftStartIndex && T.maxDrift > 0.0 && T.driftPeriodShots > 0.0)
    {
        O.yaw = T.maxDrift * std::sin(FullTurnRadians * (Index - T.driftStartIndex) / T.driftPeriodShots);
    }
    return O;
}

// Pattern plus a uniform sample of the spread cone.
inline Offset ShotOffset(const Tuning& T, std::uint32_t Index, Stance S, std::uint32_t Seed)
{
    Offset O = PatternOffset(T, Index);
    const double Radius = SpreadDegrees(T, Index, S) * std::sqrt(UnitValue(Seed, 2 * Index + 1));
    const double Angle = FullTurnRadians * UnitValue(Seed, 2 * Index + 2);
    O.pitch += Radius * std::sin(Angle);
    O.yaw += Radius * std::cos(Angle);
    return O;
}

inline Offset ViewKick(const Tuning& T, std::uint32_t Seed)
{
    return {T.viewKickPitch, T.viewKickYaw * (UnitValue(Seed, 0) < 0.5 ? -1.0 : 1.0)};
}

// The part of an outstanding view kick to remove this frame. SinceKick is the time
// from the latest kick to the end of this frame; recovery moves straight back to zero.
inline Offset RecoverView(const Offset& Outstanding, const Tuning& T, double SinceKick, double DeltaTime)
{
    const double Active = std::min(std::max(DeltaTime, 0.0), std::max(0.0, SinceKick - T.viewRecoveryDelay));
    const double Magnitude = std::sqrt(Outstanding.pitch * Outstanding.pitch + Outstanding.yaw * Outstanding.yaw);
    if (!(Active > 0.0) || !(Magnitude > 0.0)) { return {}; }
    const double Step = T.viewRecoveryRate * Active;
    if (Step >= Magnitude) { return Outstanding; }
    const double Fraction = Step / Magnitude;
    return {Outstanding.pitch * Fraction, Outstanding.yaw * Fraction};
}
}
