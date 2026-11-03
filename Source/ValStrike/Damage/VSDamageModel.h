#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Engine-independent locational damage rules. The server turns an authoritative
// trace into (weapon class, hit zone, distance) and applies the returned amount.
// Nothing here reads client input, health or time, so results are deterministic.
namespace ValStrike::Damage
{
// Order is part of the table layout below; append new zones before Count only.
enum class HitZone : std::uint8_t
{
    None,
    Head,
    UpperTorso, // chest, shoulders, upper back
    LowerTorso, // stomach, lower back, pelvis
    UpperArm,
    Forearm,    // forearms and hands
    Thigh,
    LowerLeg,
    Foot,
    Count
};
constexpr std::size_t ZoneCount = static_cast<std::size_t>(HitZone::Count);

enum class WeaponClass : std::uint8_t { Rifle, Pistol, Count };
constexpr std::size_t WeaponCount = static_cast<std::size_t>(WeaponClass::Count);

// Health is 100 today. A lethal hit uses an amount far above any configured
// health so the existing server clamp ends the life without a special path.
constexpr float LethalDamage = 10000.0f;
// Sanitizing bounds for every tunable value and input.
constexpr float MaxBaseDamage = 500.0f;
constexpr float MaxZoneMultiplier = 10.0f;
constexpr float MaxHitDamage = 1000.0f;
constexpr float MaxDistanceCm = 1000000.0f;

struct Falloff
{
    bool Enabled = false;   // Off by default for every weapon in the proposal table.
    float StartCm = 0.0f;   // Full damage up to this distance.
    float EndCm = 0.0f;     // Damage reaches MinScale here and stays there beyond it.
    float MinScale = 1.0f;  // 0..1
};

struct WeaponProfile
{
    float BaseDamage = 0.0f; // Damage of an upper-torso hit at point blank.
    std::array<float, ZoneCount> ZoneMultiplier{}; // Indexed by HitZone; None stays 0.
    bool HeadIsLethal = false; // A head hit kills regardless of health/distance.
    Falloff DistanceFalloff{};
};

struct DamageTable
{
    std::array<WeaponProfile, WeaponCount> Weapons{};
};

struct HitRequest
{
    WeaponClass Weapon = WeaponClass::Rifle;
    HitZone Zone = HitZone::None;
    float DistanceCm = 0.0f; // Non-finite or negative values are treated as 0.
};

enum class HitStatus : std::uint8_t { Applied, InvalidWeapon, InvalidZone };

struct HitResult
{
    HitStatus Status = HitStatus::InvalidZone;
    float Damage = 0.0f;       // Whole hit points; LethalDamage for a lethal head hit.
    bool Lethal = false;       // True only for the lethal-head rule.
    float Multiplier = 0.0f;   // Sanitized zone multiplier that was used.
    float FalloffScale = 1.0f; // Applied distance scale (1 when disabled or lethal).
};

// The proposal table (ValStrike's own values; see docs/workstreams/damage-scoreboard.md).
const DamageTable& DefaultTable();
// Every field clamped into its documented range; non-finite values become safe defaults.
DamageTable Sanitized(const DamageTable& Table);
// Pure function of (table, request). Sanitizes the profile it reads.
HitResult Compute(const DamageTable& Table, const HitRequest& Request);
float FalloffScale(const Falloff& Profile, float DistanceCm);

const char* ZoneName(HitZone Zone);
const char* WeaponName(WeaponClass Weapon);
inline bool IsHeadshot(HitZone Zone) { return Zone == HitZone::Head; }
inline bool IsValidZone(HitZone Zone) { return Zone > HitZone::None && Zone < HitZone::Count; }
inline bool IsValidWeapon(WeaponClass Weapon) { return Weapon < WeaponClass::Count; }
}
