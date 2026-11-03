#include "VSDamageModel.h"

#include <cmath>

namespace ValStrike::Damage
{
namespace
{
// Unity builds share one translation unit: keep file-local names unique.
float VSDamageModelClamp(float Value, float Low, float High, float Fallback)
{
    if (!std::isfinite(Value)) { return Fallback; }
    return Value < Low ? Low : (Value > High ? High : Value);
}

std::size_t VSDamageModelZoneIndex(HitZone Zone) { return static_cast<std::size_t>(Zone); }

// Proposal multipliers shared by both weapon classes as a starting point. They
// remain per weapon in the table so a later weapon can diverge.
std::array<float, ZoneCount> VSDamageModelAnatomy()
{
    std::array<float, ZoneCount> Zones{};
    Zones[VSDamageModelZoneIndex(HitZone::Head)] = 4.00f; // Used only when HeadIsLethal is off.
    Zones[VSDamageModelZoneIndex(HitZone::UpperTorso)] = 1.00f;
    Zones[VSDamageModelZoneIndex(HitZone::LowerTorso)] = 0.90f;
    Zones[VSDamageModelZoneIndex(HitZone::UpperArm)] = 0.80f;
    Zones[VSDamageModelZoneIndex(HitZone::Forearm)] = 0.70f;
    Zones[VSDamageModelZoneIndex(HitZone::Thigh)] = 0.80f;
    Zones[VSDamageModelZoneIndex(HitZone::LowerLeg)] = 0.70f;
    Zones[VSDamageModelZoneIndex(HitZone::Foot)] = 0.55f;
    return Zones;
}

DamageTable VSDamageModelBuildDefault()
{
    DamageTable Table;
    WeaponProfile& Rifle = Table.Weapons[static_cast<std::size_t>(WeaponClass::Rifle)];
    Rifle.BaseDamage = 30.0f; // Matches the current rifle body damage: four upper-torso hits.
    Rifle.ZoneMultiplier = VSDamageModelAnatomy();
    Rifle.HeadIsLethal = true;
    Rifle.DistanceFalloff = Falloff{false, 2500.0f, 5000.0f, 0.85f};

    WeaponProfile& Pistol = Table.Weapons[static_cast<std::size_t>(WeaponClass::Pistol)];
    Pistol.BaseDamage = 20.0f; // The Weapons stream's pistol body damage: five upper-torso hits.
    Pistol.ZoneMultiplier = VSDamageModelAnatomy();
    Pistol.HeadIsLethal = true;
    Pistol.DistanceFalloff = Falloff{false, 1500.0f, 3500.0f, 0.75f};
    return Table;
}

Falloff VSDamageModelSanitizeFalloff(const Falloff& Input)
{
    Falloff Result;
    Result.Enabled = Input.Enabled;
    Result.StartCm = VSDamageModelClamp(Input.StartCm, 0.0f, MaxDistanceCm, 0.0f);
    Result.EndCm = VSDamageModelClamp(Input.EndCm, 0.0f, MaxDistanceCm, Result.StartCm);
    if (Result.EndCm < Result.StartCm) { Result.EndCm = Result.StartCm; }
    Result.MinScale = VSDamageModelClamp(Input.MinScale, 0.0f, 1.0f, 1.0f);
    return Result;
}

WeaponProfile VSDamageModelSanitizeProfile(const WeaponProfile& Input)
{
    WeaponProfile Result;
    Result.BaseDamage = VSDamageModelClamp(Input.BaseDamage, 0.0f, MaxBaseDamage, 0.0f);
    for (std::size_t Index = 1; Index < ZoneCount; ++Index)
    {
        Result.ZoneMultiplier[Index] = VSDamageModelClamp(Input.ZoneMultiplier[Index], 0.0f, MaxZoneMultiplier, 0.0f);
    }
    Result.ZoneMultiplier[0] = 0.0f; // HitZone::None never deals damage.
    Result.HeadIsLethal = Input.HeadIsLethal;
    Result.DistanceFalloff = VSDamageModelSanitizeFalloff(Input.DistanceFalloff);
    return Result;
}
}

const DamageTable& DefaultTable()
{
    static const DamageTable Table = VSDamageModelBuildDefault();
    return Table;
}

DamageTable Sanitized(const DamageTable& Table)
{
    DamageTable Result;
    for (std::size_t Index = 0; Index < WeaponCount; ++Index)
    {
        Result.Weapons[Index] = VSDamageModelSanitizeProfile(Table.Weapons[Index]);
    }
    return Result;
}

float FalloffScale(const Falloff& Input, float DistanceCm)
{
    const Falloff Profile = VSDamageModelSanitizeFalloff(Input);
    if (!Profile.Enabled) { return 1.0f; }
    const double Distance = VSDamageModelClamp(DistanceCm, 0.0f, MaxDistanceCm, 0.0f);
    double Progress = 0.0;
    if (Profile.EndCm > Profile.StartCm)
    {
        Progress = (Distance - Profile.StartCm) / (static_cast<double>(Profile.EndCm) - Profile.StartCm);
        Progress = Progress < 0.0 ? 0.0 : (Progress > 1.0 ? 1.0 : Progress);
    }
    else
    {
        Progress = Distance > Profile.StartCm ? 1.0 : 0.0;
    }
    return static_cast<float>(1.0 + (static_cast<double>(Profile.MinScale) - 1.0) * Progress);
}

HitResult Compute(const DamageTable& Table, const HitRequest& Request)
{
    HitResult Result;
    if (!IsValidWeapon(Request.Weapon))
    {
        Result.Status = HitStatus::InvalidWeapon;
        return Result;
    }
    if (!IsValidZone(Request.Zone))
    {
        Result.Status = HitStatus::InvalidZone;
        return Result;
    }
    const WeaponProfile Profile = VSDamageModelSanitizeProfile(Table.Weapons[static_cast<std::size_t>(Request.Weapon)]);
    Result.Status = HitStatus::Applied;
    Result.Multiplier = Profile.ZoneMultiplier[VSDamageModelZoneIndex(Request.Zone)];
    if (Request.Zone == HitZone::Head && Profile.HeadIsLethal && Profile.BaseDamage > 0.0f)
    {
        Result.Damage = LethalDamage;
        Result.Lethal = true;
        return Result;
    }
    Result.FalloffScale = FalloffScale(Profile.DistanceFalloff, Request.DistanceCm);
    const double Raw = static_cast<double>(Profile.BaseDamage) * Result.Multiplier * Result.FalloffScale;
    if (!(Raw > 0.0)) { return Result; }
    // Whole hit points keep health readable; any positive hit removes at least one.
    double Rounded = std::floor(Raw + 0.5);
    Rounded = Rounded < 1.0 ? 1.0 : (Rounded > MaxHitDamage ? MaxHitDamage : Rounded);
    Result.Damage = static_cast<float>(Rounded);
    return Result;
}

const char* ZoneName(HitZone Zone)
{
    switch (Zone)
    {
    case HitZone::Head: return "Head";
    case HitZone::UpperTorso: return "UpperTorso";
    case HitZone::LowerTorso: return "LowerTorso";
    case HitZone::UpperArm: return "UpperArm";
    case HitZone::Forearm: return "Forearm";
    case HitZone::Thigh: return "Thigh";
    case HitZone::LowerLeg: return "LowerLeg";
    case HitZone::Foot: return "Foot";
    default: return "None";
    }
}

const char* WeaponName(WeaponClass Weapon)
{
    switch (Weapon)
    {
    case WeaponClass::Rifle: return "Rifle";
    case WeaponClass::Pistol: return "Pistol";
    default: return "Invalid";
    }
}
}
