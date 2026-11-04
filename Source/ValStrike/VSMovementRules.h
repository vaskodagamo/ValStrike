#pragma once

#include <cmath>
#include <cstdint>

// Engine-free movement feel tuning: weapon weight, the jump arc and the
// crouch-jump tuck, scaled from CS2 to this character's height. Shared by
// UVSCharacterMovementComponent and Tests/Movement/movement_rules_test.cpp.
// Lengths are centimeters (ours) or Source units (CS2), times are seconds.
namespace VSMovementRules
{
// What the player holds, for weapon weight. Every loadout slot maps to one.
enum class WeaponKind : std::uint8_t { Unarmed, Pistol, Rifle, Knife };

// CS2 reference values (Source units, u/s and u/s^2).
namespace Cs2
{
constexpr double PlayerHeight = 72.0;
constexpr double Gravity = 800.0;          // sv_gravity
constexpr double JumpImpulse = 301.993377; // sv_jump_impulse: sqrt(2 * 800 * 57)
constexpr double CrouchJumpGain = 9.0;     // 57 u standing reach vs about 66 u crouch-jump reach
constexpr double UnarmedSpeed = 250.0;     // knife
constexpr double PistolSpeed = 240.0;      // Glock / USP-S / P2000
constexpr double RifleSpeed = 215.0;       // AK-47
}

// This character (AVSCharacter): capsule 34 x 88, crouched half-height 56.
constexpr double StandingHalfHeight = 88.0;
constexpr double CrouchedHalfHeight = 56.0;
constexpr double PlayerHeight = 2.0 * StandingHalfHeight;
constexpr double EngineGravity = 980.0; // |DefaultGravityZ| at GravityScale 1
constexpr double BaseMaxWalkSpeed = 450.0;
constexpr double BaseMaxWalkSpeedCrouched = 180.0;

// Centimeters per CS2 unit for vertical motion: our height over theirs.
constexpr double HeightScale = PlayerHeight / Cs2::PlayerHeight;

// Weapon weight: CS2 max speed relative to unarmed. Ground (run, slow-walk,
// crouch) and air speed all take the same factor.
constexpr double SpeedMultiplier(WeaponKind Kind)
{
    switch (Kind)
    {
    case WeaponKind::Pistol: return Cs2::PistolSpeed / Cs2::UnarmedSpeed;
    case WeaponKind::Rifle: return Cs2::RifleSpeed / Cs2::UnarmedSpeed;
    // CS2's unarmed reference speed is the knife: the unweighted base speed.
    case WeaponKind::Knife:
    case WeaponKind::Unarmed: break;
    }
    return 1.0;
}

// Base speed (MaxWalkSpeed or MaxWalkSpeedCrouched) scaled by weapon weight.
// Invalid bases give 0, which the engine treats as no movement.
inline double MaxSpeed(double BaseSpeed, WeaponKind Kind)
{
    return std::isfinite(BaseSpeed) && BaseSpeed > 0.0 ? BaseSpeed * SpeedMultiplier(Kind) : 0.0;
}

// Ballistic jump arc from launch velocity and gravity magnitude (no air drag,
// landing at launch height). Invalid inputs give 0.
inline double JumpApex(double JumpVelocity, double GravityCmS2)
{
    if (!std::isfinite(JumpVelocity) || !std::isfinite(GravityCmS2) || JumpVelocity <= 0.0 || GravityCmS2 <= 0.0) { return 0.0; }
    return JumpVelocity * JumpVelocity / (2.0 * GravityCmS2);
}

inline double Airtime(double JumpVelocity, double GravityCmS2)
{
    if (!std::isfinite(JumpVelocity) || !std::isfinite(GravityCmS2) || JumpVelocity <= 0.0 || GravityCmS2 <= 0.0) { return 0.0; }
    return 2.0 * JumpVelocity / GravityCmS2;
}

// Scaling both gravity and jump velocity by HeightScale scales the apex by
// HeightScale and keeps CS2's airtime (2v/g is unchanged).
constexpr double Gravity = Cs2::Gravity * HeightScale;
constexpr double GravityScale = Gravity / EngineGravity;
constexpr double JumpZVelocity = Cs2::JumpImpulse * HeightScale;

// Crouch-jump. UE crouches a falling capsule about its center, so the feet rise
// by the half-height difference (the engine tuck). The CS2 gain, scaled, is
// smaller; the rest is taken back by moving the tucked capsule down. Nothing
// adds vertical velocity, so the gain exists only while tucked.
constexpr double EngineAirTuck = StandingHalfHeight - CrouchedHalfHeight;
constexpr double CrouchJumpGainTarget = Cs2::CrouchJumpGain * HeightScale;

// Downward move applied right after an in-air crouch. A target above the engine
// tuck cannot be reached without extra velocity, so it never moves up.
inline double AirTuckDrop(double EngineTuck, double TargetGain)
{
    if (!std::isfinite(EngineTuck) || !std::isfinite(TargetGain) || EngineTuck <= 0.0) { return 0.0; }
    const double Gain = TargetGain < 0.0 ? 0.0 : TargetGain;
    return Gain >= EngineTuck ? 0.0 : EngineTuck - Gain;
}

// Highest feet clearance of a jump from flat ground, tucked at or before the apex.
inline double CrouchJumpClearance(double Apex, double EngineTuck, double TargetGain)
{
    return Apex + EngineTuck - AirTuckDrop(EngineTuck, TargetGain);
}

// Ground distance covered while airborne. A running jump keeps its launch speed
// (no air friction, air speed capped by the same weapon max), so it goes
// farther but not higher; landing never exceeds the ground max either.
inline double JumpDistance(double HorizontalSpeed, double JumpVelocity, double GravityCmS2)
{
    if (!std::isfinite(HorizontalSpeed) || HorizontalSpeed <= 0.0) { return 0.0; }
    return HorizontalSpeed * Airtime(JumpVelocity, GravityCmS2);
}
}
