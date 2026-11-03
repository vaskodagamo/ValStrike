// Standalone tests for the locational damage model and hit-zone geometry.
// Built by Scripts/test_damage.py; no Unreal dependency.
#include "Damage/VSDamageModel.h"
#include "Damage/VSHitZoneLayout.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>

using namespace ValStrike::Damage;

namespace
{
int Checks = 0;
int Failures = 0;
const char* CurrentTest = "";

bool Check(bool Condition, const char* Expression, int Line)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::fprintf(stderr, "FAIL %s:%d: %s\n", CurrentTest, Line, Expression);
    }
    return Condition;
}

#define CHECK(Expression) Check((Expression), #Expression, __LINE__)
#define REQUIRE(Expression) do { if (!CHECK(Expression)) { return; } } while (false)

bool Near(double A, double B, double Tolerance = 1e-9) { return std::fabs(A - B) <= Tolerance; }

// Field-wise comparison; struct padding makes memcmp unreliable.
bool SameTable(const DamageTable& A, const DamageTable& B)
{
    for (std::size_t Weapon = 0; Weapon < WeaponCount; ++Weapon)
    {
        const WeaponProfile& X = A.Weapons[Weapon];
        const WeaponProfile& Y = B.Weapons[Weapon];
        if (X.BaseDamage != Y.BaseDamage || X.HeadIsLethal != Y.HeadIsLethal || X.ZoneMultiplier != Y.ZoneMultiplier ||
            X.DistanceFalloff.Enabled != Y.DistanceFalloff.Enabled || X.DistanceFalloff.StartCm != Y.DistanceFalloff.StartCm ||
            X.DistanceFalloff.EndCm != Y.DistanceFalloff.EndCm || X.DistanceFalloff.MinScale != Y.DistanceFalloff.MinScale)
        {
            return false;
        }
    }
    return true;
}

const HitZone BodyZones[] = {
    HitZone::UpperTorso, HitZone::LowerTorso, HitZone::UpperArm, HitZone::Forearm,
    HitZone::Thigh, HitZone::LowerLeg, HitZone::Foot,
};

HitResult Hit(WeaponClass Weapon, HitZone Zone, float Distance = 0.0f, const DamageTable& Table = DefaultTable())
{
    HitRequest Request;
    Request.Weapon = Weapon;
    Request.Zone = Zone;
    Request.DistanceCm = Distance;
    return Compute(Table, Request);
}

void EveryZoneAndWeaponMatchesProposalTable()
{
    struct Expected { WeaponClass Weapon; HitZone Zone; float Damage; };
    const Expected Table[] = {
        {WeaponClass::Rifle, HitZone::UpperTorso, 30}, {WeaponClass::Rifle, HitZone::LowerTorso, 27},
        {WeaponClass::Rifle, HitZone::UpperArm, 24}, {WeaponClass::Rifle, HitZone::Forearm, 21},
        {WeaponClass::Rifle, HitZone::Thigh, 24}, {WeaponClass::Rifle, HitZone::LowerLeg, 21},
        {WeaponClass::Rifle, HitZone::Foot, 17},
        {WeaponClass::Pistol, HitZone::UpperTorso, 22}, {WeaponClass::Pistol, HitZone::LowerTorso, 20},
        {WeaponClass::Pistol, HitZone::UpperArm, 18}, {WeaponClass::Pistol, HitZone::Forearm, 15},
        {WeaponClass::Pistol, HitZone::Thigh, 18}, {WeaponClass::Pistol, HitZone::LowerLeg, 15},
        {WeaponClass::Pistol, HitZone::Foot, 12},
    };
    for (const Expected& Row : Table)
    {
        const HitResult Result = Hit(Row.Weapon, Row.Zone);
        if (!CHECK(Result.Status == HitStatus::Applied && Result.Damage == Row.Damage && !Result.Lethal &&
            Result.FalloffScale == 1.0f))
        {
            std::fprintf(stderr, "  %s %s -> %.3f\n", WeaponName(Row.Weapon), ZoneName(Row.Zone), Result.Damage);
        }
    }
    // Shots to kill a 100 HP player stay ordered by zone importance.
    CHECK(std::ceil(100.0f / Hit(WeaponClass::Rifle, HitZone::UpperTorso).Damage) == 4);
    CHECK(std::ceil(100.0f / Hit(WeaponClass::Rifle, HitZone::Foot).Damage) == 6);
    CHECK(std::ceil(100.0f / Hit(WeaponClass::Pistol, HitZone::UpperTorso).Damage) == 5);
}

void ProposalTableKeepsAnatomyOrdering()
{
    for (std::size_t Weapon = 0; Weapon < WeaponCount; ++Weapon)
    {
        const WeaponProfile& Profile = DefaultTable().Weapons[Weapon];
        const auto M = [&](HitZone Zone) { return Profile.ZoneMultiplier[static_cast<std::size_t>(Zone)]; };
        CHECK(M(HitZone::None) == 0.0f);
        CHECK(M(HitZone::Head) > M(HitZone::UpperTorso));
        for (HitZone Zone : BodyZones) { CHECK(M(Zone) <= M(HitZone::UpperTorso) && M(Zone) >= M(HitZone::Foot)); }
        CHECK(M(HitZone::LowerTorso) > M(HitZone::Thigh));
        CHECK(M(HitZone::UpperArm) > M(HitZone::Forearm));
        CHECK(M(HitZone::Thigh) > M(HitZone::LowerLeg) && M(HitZone::LowerLeg) > M(HitZone::Foot));
        CHECK(Profile.HeadIsLethal);
        CHECK(!Profile.DistanceFalloff.Enabled);
    }
}

void HeadIsLethalForRifleAndPistolAtAnyRange()
{
    for (WeaponClass Weapon : {WeaponClass::Rifle, WeaponClass::Pistol})
    {
        for (float Distance : {0.0f, 1500.0f, 99999.0f})
        {
            const HitResult Result = Hit(Weapon, HitZone::Head, Distance);
            CHECK(Result.Status == HitStatus::Applied && Result.Lethal && Result.Damage == LethalDamage);
            CHECK(Result.Damage >= 100.0f);
        }
    }
    // Lethal ignores enabled falloff too.
    DamageTable Table = DefaultTable();
    Table.Weapons[0].DistanceFalloff.Enabled = true;
    CHECK(Hit(WeaponClass::Rifle, HitZone::Head, 1e6f, Table).Damage == LethalDamage);
}

void HeadCanBeConfiguredNonLethal()
{
    DamageTable Table = DefaultTable();
    for (WeaponProfile& Profile : Table.Weapons) { Profile.HeadIsLethal = false; }
    const HitResult Rifle = Hit(WeaponClass::Rifle, HitZone::Head, 0.0f, Table);
    CHECK(!Rifle.Lethal && Rifle.Damage == 120.0f && Rifle.Multiplier == 4.0f);
    const HitResult Pistol = Hit(WeaponClass::Pistol, HitZone::Head, 0.0f, Table);
    CHECK(!Pistol.Lethal && Pistol.Damage == 88.0f);
    // The lethal rule never turns a zero-damage weapon into a kill.
    Table = DefaultTable();
    Table.Weapons[0].BaseDamage = 0.0f;
    const HitResult Harmless = Hit(WeaponClass::Rifle, HitZone::Head, 0.0f, Table);
    CHECK(!Harmless.Lethal && Harmless.Damage == 0.0f && Harmless.Status == HitStatus::Applied);
}

void FalloffIsOffByDefault()
{
    for (float Distance : {0.0f, 2500.0f, 5000.0f, 1e6f})
    {
        CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, Distance).Damage == 30.0f);
        CHECK(Hit(WeaponClass::Pistol, HitZone::UpperTorso, Distance).Damage == 22.0f);
    }
}

void FalloffInterpolatesAndClampsWhenEnabled()
{
    DamageTable Table = DefaultTable();
    for (WeaponProfile& Profile : Table.Weapons) { Profile.DistanceFalloff.Enabled = true; }
    // Rifle: full to 25 m, 0.85 at 50 m and beyond.
    CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, 0.0f, Table).Damage == 30.0f);
    CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, 2500.0f, Table).Damage == 30.0f);
    const HitResult Mid = Hit(WeaponClass::Rifle, HitZone::UpperTorso, 3750.0f, Table);
    CHECK(Near(Mid.FalloffScale, 0.925, 1e-6) && Mid.Damage == 28.0f); // 27.75 rounds to 28
    const HitResult End = Hit(WeaponClass::Rifle, HitZone::UpperTorso, 5000.0f, Table);
    CHECK(Near(End.FalloffScale, 0.85, 1e-6) && End.Damage == 26.0f); // 25.5 rounds up
    CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, 1e6f, Table).Damage == 26.0f);
    // Pistol: full to 15 m, 0.75 at 35 m.
    CHECK(Hit(WeaponClass::Pistol, HitZone::UpperTorso, 3500.0f, Table).Damage == 17.0f); // 16.5
    CHECK(Hit(WeaponClass::Pistol, HitZone::Foot, 3500.0f, Table).Damage == 9.0f); // 9.075
    // Monotone non-increasing with distance.
    float Previous = 1e9f;
    for (float Distance = 0.0f; Distance <= 6000.0f; Distance += 50.0f)
    {
        const float Damage = Hit(WeaponClass::Rifle, HitZone::Thigh, Distance, Table).Damage;
        CHECK(Damage <= Previous);
        Previous = Damage;
    }
    // A zero-width falloff is a step at its start distance.
    Falloff Step{true, 1000.0f, 1000.0f, 0.5f};
    CHECK(FalloffScale(Step, 1000.0f) == 1.0f && FalloffScale(Step, 1000.5f) == 0.5f);
}

void InvalidInputsAreRejectedOrClamped()
{
    CHECK(Hit(WeaponClass::Rifle, HitZone::None).Status == HitStatus::InvalidZone);
    CHECK(Hit(WeaponClass::Rifle, HitZone::None).Damage == 0.0f);
    CHECK(Hit(WeaponClass::Rifle, static_cast<HitZone>(200)).Status == HitStatus::InvalidZone);
    CHECK(Hit(WeaponClass::Rifle, HitZone::Count).Status == HitStatus::InvalidZone);
    CHECK(Hit(static_cast<WeaponClass>(9), HitZone::UpperTorso).Status == HitStatus::InvalidWeapon);
    CHECK(Hit(WeaponClass::Count, HitZone::Head).Damage == 0.0f);

    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Inf = std::numeric_limits<float>::infinity();
    DamageTable Table = DefaultTable();
    Table.Weapons[0].DistanceFalloff.Enabled = true;
    for (float Distance : {NaN, Inf, -Inf, -500.0f})
    {
        // Treated as point blank, never as NaN damage.
        const HitResult Result = Hit(WeaponClass::Rifle, HitZone::UpperTorso, Distance, Table);
        CHECK(Result.Damage == 30.0f);
        CHECK(std::isfinite(Result.Damage) && std::isfinite(Result.FalloffScale));
    }

    DamageTable Bad = DefaultTable();
    Bad.Weapons[0].BaseDamage = NaN;
    CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, 0.0f, Bad).Damage == 0.0f);
    Bad.Weapons[0].BaseDamage = 1e9f;
    Bad.Weapons[0].ZoneMultiplier[static_cast<std::size_t>(HitZone::UpperTorso)] = 1e9f;
    const HitResult Huge = Hit(WeaponClass::Rifle, HitZone::UpperTorso, 0.0f, Bad);
    CHECK(Huge.Damage == MaxHitDamage && Huge.Multiplier == MaxZoneMultiplier);
    Bad.Weapons[0].ZoneMultiplier[static_cast<std::size_t>(HitZone::UpperTorso)] = -3.0f;
    CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, 0.0f, Bad).Damage == 0.0f);
    Bad.Weapons[0].ZoneMultiplier[static_cast<std::size_t>(HitZone::UpperTorso)] = Inf;
    CHECK(Hit(WeaponClass::Rifle, HitZone::UpperTorso, 0.0f, Bad).Damage == 0.0f);

    // Tiny positive damage still removes one point; rounding never makes it vanish.
    DamageTable Tiny = DefaultTable();
    Tiny.Weapons[0].BaseDamage = 0.2f;
    CHECK(Hit(WeaponClass::Rifle, HitZone::Foot, 0.0f, Tiny).Damage == 1.0f);

    // Falloff fields are clamped: inverted range, NaN scale, scale above one.
    Falloff Odd{true, 5000.0f, 1000.0f, NaN};
    const Falloff Clean = Sanitized(DamageTable{}).Weapons[0].DistanceFalloff;
    CHECK(!Clean.Enabled && Clean.MinScale == 1.0f);
    CHECK(FalloffScale(Odd, 4000.0f) == 1.0f && FalloffScale(Odd, 6000.0f) == 1.0f);
    Odd.MinScale = 7.0f;
    CHECK(FalloffScale(Odd, 6000.0f) == 1.0f);
    Odd.MinScale = -1.0f;
    CHECK(FalloffScale(Odd, 6000.0f) == 0.0f);
}

void SanitizedTableIsIdempotentAndBounded()
{
    DamageTable Messy = DefaultTable();
    Messy.Weapons[1].BaseDamage = -40.0f;
    Messy.Weapons[1].ZoneMultiplier[0] = 5.0f;
    Messy.Weapons[1].ZoneMultiplier[3] = 99.0f;
    Messy.Weapons[1].DistanceFalloff = Falloff{true, -1.0f, std::numeric_limits<float>::quiet_NaN(), 2.0f};
    const DamageTable Once = Sanitized(Messy);
    const DamageTable Twice = Sanitized(Once);
    CHECK(SameTable(Once, Twice));
    CHECK(Once.Weapons[1].BaseDamage == 0.0f && Once.Weapons[1].ZoneMultiplier[0] == 0.0f);
    CHECK(Once.Weapons[1].ZoneMultiplier[3] == MaxZoneMultiplier);
    CHECK(Once.Weapons[1].DistanceFalloff.StartCm == 0.0f && Once.Weapons[1].DistanceFalloff.EndCm == 0.0f);
    CHECK(Once.Weapons[1].DistanceFalloff.MinScale == 1.0f);
    const DamageTable Defaults = Sanitized(DefaultTable());
    CHECK(SameTable(Defaults, DefaultTable()));
}

void ResultsAreDeterministic()
{
    DamageTable Table = DefaultTable();
    for (WeaponProfile& Profile : Table.Weapons) { Profile.DistanceFalloff.Enabled = true; }
    const DamageTable Copy = Table;
    for (std::size_t Weapon = 0; Weapon < WeaponCount; ++Weapon)
    {
        for (std::size_t Zone = 0; Zone <= ZoneCount; ++Zone)
        {
            for (float Distance : {0.0f, 1234.5f, 3333.3f, 8000.0f})
            {
                const HitResult A = Hit(static_cast<WeaponClass>(Weapon), static_cast<HitZone>(Zone), Distance, Table);
                const HitResult B = Hit(static_cast<WeaponClass>(Weapon), static_cast<HitZone>(Zone), Distance, Copy);
                CHECK(A.Status == B.Status && A.Damage == B.Damage && A.Lethal == B.Lethal &&
                    A.Multiplier == B.Multiplier && A.FalloffScale == B.FalloffScale);
                CHECK(A.Damage == std::floor(A.Damage)); // whole hit points
            }
        }
    }
    CHECK(std::strcmp(ZoneName(HitZone::Forearm), "Forearm") == 0);
    CHECK(std::strcmp(ZoneName(static_cast<HitZone>(77)), "None") == 0);
    CHECK(std::strcmp(WeaponName(WeaponClass::Pistol), "Pistol") == 0);
    CHECK(IsHeadshot(HitZone::Head) && !IsHeadshot(HitZone::UpperTorso));
}

// ---------------------------------------------------------------- layout

const ZoneVolume* FindVolume(const Layout& Value, const char* Name)
{
    for (const ZoneVolume& Volume : Value.Volumes)
    {
        if (std::strcmp(Volume.Name, Name) == 0) { return &Volume; }
    }
    return nullptr;
}

double Bottom(const ZoneVolume& Volume) { return Volume.Center[2] - Volume.HalfExtent[2]; }
double Top(const ZoneVolume& Volume) { return Volume.Center[2] + Volume.HalfExtent[2]; }

void StandingLayoutMatchesPrototypeSilhouette()
{
    const Layout Standing = BuildLayout(88.0);
    REQUIRE(Standing.Valid && Standing.CapsuleHalfHeight == 88.0);
    const ZoneVolume* Head = FindVolume(Standing, "Head");
    REQUIRE(Head && Head->Shape == ZoneShape::Sphere && Head->Zone == HitZone::Head);
    // Same as the current HeadHitRegion and head visual: radius 16, touching the capsule top.
    CHECK(Head->Radius == 16.0 && Head->Center[2] == 72.0 && Head->Center[0] == 0.0 && Head->Center[1] == 0.0);
    // Body volumes partition the current BodyHitRegion box (extent 25 x 20 x 72 at z -16).
    double Volume = 0.0;
    double MinZ = 1e9, MaxZ = -1e9;
    std::size_t Boxes = 0;
    for (const ZoneVolume& Box : Standing.Volumes)
    {
        if (Box.Shape != ZoneShape::Box) { continue; }
        ++Boxes;
        Volume += 8.0 * Box.HalfExtent[0] * Box.HalfExtent[1] * Box.HalfExtent[2];
        MinZ = Box.Center[2] - Box.HalfExtent[2] < MinZ ? Box.Center[2] - Box.HalfExtent[2] : MinZ;
        MaxZ = Box.Center[2] + Box.HalfExtent[2] > MaxZ ? Box.Center[2] + Box.HalfExtent[2] : MaxZ;
        CHECK(Box.HalfExtent[0] == 25.0 && Box.Center[0] == 0.0);
        CHECK(std::fabs(Box.Center[1]) + Box.HalfExtent[1] <= 20.0 + 1e-9);
        CHECK(Box.HalfExtent[1] > 0.0 && Box.HalfExtent[2] > 0.0);
    }
    CHECK(Boxes == VolumeCount - 1);
    CHECK(Near(Volume, 50.0 * 40.0 * 144.0, 1e-6));
    CHECK(Near(MinZ, -88.0) && Near(MaxZ, 56.0));
    // Band heights from the floor: feet 10, shins 46, thighs 84, stomach 112, chest 144.
    CHECK(Near(Top(*FindVolume(Standing, "Foot_L")) + 88.0, 10.0));
    CHECK(Near(Top(*FindVolume(Standing, "LowerLeg_R")) + 88.0, 46.0));
    CHECK(Near(Top(*FindVolume(Standing, "Thigh_L")) + 88.0, 84.0));
    CHECK(Near(Top(*FindVolume(Standing, "LowerTorso")) + 88.0, 112.0));
    CHECK(Near(Top(*FindVolume(Standing, "UpperTorso")) + 88.0, 144.0));
    // Arms are the outer 7 cm strips beside the torso core, both sides.
    const ZoneVolume* ArmL = FindVolume(Standing, "UpperArm_L");
    const ZoneVolume* ArmR = FindVolume(Standing, "UpperArm_R");
    const ZoneVolume* Core = FindVolume(Standing, "UpperTorso");
    REQUIRE(ArmL && ArmR && Core);
    CHECK(Near(ArmL->Center[1], -16.5) && Near(ArmR->Center[1], 16.5) && Near(ArmL->HalfExtent[1], 3.5));
    CHECK(Near(Core->HalfExtent[1], 13.0));
    CHECK(ArmL->Side == BodySide::Left && ArmR->Side == BodySide::Right && Core->Side == BodySide::Center);
    CHECK(Near(Bottom(*ArmL), Bottom(*Core)) && Near(Top(*ArmL), Top(*Core)));
    CHECK(FindVolume(Standing, "Forearm_R")->Zone == HitZone::Forearm);
    CHECK(Near(Bottom(*FindVolume(Standing, "Forearm_R")), Bottom(*FindVolume(Standing, "LowerTorso"))));
}

void BandsAreContiguousWithoutOverlap()
{
    for (double HalfHeight : {88.0, 56.0, 70.0, 17.0})
    {
        const Layout Value = BuildLayout(HalfHeight);
        REQUIRE(Value.Valid);
        // Center column from feet to chest, left column, right column.
        const char* Left[] = {"Foot_L", "LowerLeg_L", "Thigh_L", "Forearm_L", "UpperArm_L"};
        const char* Right[] = {"Foot_R", "LowerLeg_R", "Thigh_R", "Forearm_R", "UpperArm_R"};
        for (const char* const* Column : {Left, Right})
        {
            double Expected = -HalfHeight;
            for (int Index = 0; Index < 5; ++Index)
            {
                const ZoneVolume* Volume = FindVolume(Value, Column[Index]);
                REQUIRE(Volume);
                CHECK(Near(Bottom(*Volume), Expected, 1e-9));
                Expected = Top(*Volume);
            }
            CHECK(Near(Expected, HalfHeight - 32.0, 1e-9));
        }
        // Every pair of boxes is disjoint in volume (touching faces only).
        for (std::size_t A = 1; A < VolumeCount; ++A)
        {
            for (std::size_t B = A + 1; B < VolumeCount; ++B)
            {
                const ZoneVolume& X = Value.Volumes[A];
                const ZoneVolume& Y = Value.Volumes[B];
                bool Separated = false;
                for (int Axis = 0; Axis < 3; ++Axis)
                {
                    const double Gap = std::fabs(X.Center[Axis] - Y.Center[Axis]) - (X.HalfExtent[Axis] + Y.HalfExtent[Axis]);
                    Separated |= Gap >= -1e-9;
                }
                CHECK(Separated);
            }
        }
        // Left and right are mirror images.
        for (const ZoneVolume& Volume : Value.Volumes)
        {
            if (Volume.Side != BodySide::Left) { continue; }
            bool Mirrored = false;
            for (const ZoneVolume& Other : Value.Volumes)
            {
                Mirrored |= Other.Side == BodySide::Right && Other.Zone == Volume.Zone &&
                    Near(Other.Center[1], -Volume.Center[1]) && Near(Other.Center[2], Volume.Center[2]) &&
                    Near(Other.HalfExtent[1], Volume.HalfExtent[1]);
            }
            CHECK(Mirrored);
        }
    }
}

void CrouchedLayoutFollowsCapsule()
{
    // VSCharacter crouches to a 56 cm half-height; the prototype body becomes 80 cm.
    const Layout Crouched = BuildLayout(56.0);
    REQUIRE(Crouched.Valid);
    const ZoneVolume* Head = FindVolume(Crouched, "Head");
    CHECK(Head->Center[2] == 40.0 && Head->Radius == 16.0);
    CHECK(Near(Bottom(*FindVolume(Crouched, "Foot_R")), -56.0));
    CHECK(Near(Top(*FindVolume(Crouched, "UpperTorso")), 24.0));
    CHECK(Near(Top(*FindVolume(Crouched, "Foot_R")) + 56.0, 80.0 * 10.0 / 144.0));
    CHECK(Near(Top(*FindVolume(Crouched, "Thigh_R")) + 56.0, 80.0 * 84.0 / 144.0));
    // Widths do not change when crouching, only heights.
    const Layout Standing = BuildLayout(88.0);
    for (std::size_t Index = 0; Index < VolumeCount; ++Index)
    {
        CHECK(Standing.Volumes[Index].Zone == Crouched.Volumes[Index].Zone);
        CHECK(std::strcmp(Standing.Volumes[Index].Name, Crouched.Volumes[Index].Name) == 0);
        CHECK(Standing.Volumes[Index].HalfExtent[0] == Crouched.Volumes[Index].HalfExtent[0]);
        CHECK(Standing.Volumes[Index].HalfExtent[1] == Crouched.Volumes[Index].HalfExtent[1]);
        CHECK(Standing.Volumes[Index].Center[1] == Crouched.Volumes[Index].Center[1]);
        if (Index > 0) { CHECK(Crouched.Volumes[Index].HalfExtent[2] < Standing.Volumes[Index].HalfExtent[2]); }
    }
}

void EveryZoneIsRepresentedWithBones()
{
    const Layout Standing = BuildLayout(88.0);
    for (std::size_t Zone = 1; Zone < ZoneCount; ++Zone)
    {
        int Count = 0;
        for (const ZoneVolume& Volume : Standing.Volumes) { Count += Volume.Zone == static_cast<HitZone>(Zone) ? 1 : 0; }
        const HitZone Value = static_cast<HitZone>(Zone);
        CHECK(Count == (Value == HitZone::Head || Value == HitZone::UpperTorso || Value == HitZone::LowerTorso ? 1 : 2));
    }
    for (std::size_t A = 0; A < VolumeCount; ++A)
    {
        CHECK(std::strlen(Standing.Volumes[A].Bone) > 0 && std::strlen(Standing.Volumes[A].Name) > 0);
        for (std::size_t B = A + 1; B < VolumeCount; ++B)
        {
            CHECK(std::strcmp(Standing.Volumes[A].Name, Standing.Volumes[B].Name) != 0);
            CHECK(std::strcmp(Standing.Volumes[A].Bone, Standing.Volumes[B].Bone) != 0);
        }
    }
}

void InvalidLayoutInputsAreRejected()
{
    CHECK(!BuildLayout(16.0).Valid);
    CHECK(!BuildLayout(0.0).Valid);
    CHECK(!BuildLayout(-88.0).Valid);
    CHECK(!BuildLayout(std::numeric_limits<double>::quiet_NaN()).Valid);
    CHECK(!BuildLayout(std::numeric_limits<double>::infinity()).Valid);
    LayoutConfig Config;
    Config.ArmWidth = 20.0;
    CHECK(!IsValidConfig(Config) && !BuildLayout(88.0, Config).Valid);
    Config = LayoutConfig();
    Config.ThighTop = Config.LowerLegTop;
    CHECK(!BuildLayout(88.0, Config).Valid);
    Config = LayoutConfig();
    Config.LowerTorsoTop = 1.0;
    CHECK(!BuildLayout(88.0, Config).Valid);
    Config = LayoutConfig();
    Config.HeadRadius = std::numeric_limits<double>::quiet_NaN();
    CHECK(!BuildLayout(88.0, Config).Valid);
    CHECK(IsValidConfig(LayoutConfig()));
}

struct TestCase
{
    const char* Name;
    void (*Run)();
};
}

int main()
{
    const TestCase Tests[] = {
        {"every zone and weapon matches proposal table", EveryZoneAndWeaponMatchesProposalTable},
        {"proposal table keeps anatomy ordering", ProposalTableKeepsAnatomyOrdering},
        {"head is lethal for rifle and pistol", HeadIsLethalForRifleAndPistolAtAnyRange},
        {"head can be configured non-lethal", HeadCanBeConfiguredNonLethal},
        {"falloff is off by default", FalloffIsOffByDefault},
        {"falloff interpolates and clamps", FalloffInterpolatesAndClampsWhenEnabled},
        {"invalid inputs rejected or clamped", InvalidInputsAreRejectedOrClamped},
        {"sanitized table idempotent and bounded", SanitizedTableIsIdempotentAndBounded},
        {"results are deterministic", ResultsAreDeterministic},
        {"standing layout matches prototype silhouette", StandingLayoutMatchesPrototypeSilhouette},
        {"bands contiguous without overlap", BandsAreContiguousWithoutOverlap},
        {"crouched layout follows capsule", CrouchedLayoutFollowsCapsule},
        {"every zone represented with bones", EveryZoneIsRepresentedWithBones},
        {"invalid layout inputs rejected", InvalidLayoutInputsAreRejected},
    };
    int Passed = 0;
    for (const TestCase& Test : Tests)
    {
        CurrentTest = Test.Name;
        const int Before = Failures;
        Test.Run();
        if (Failures == Before)
        {
            ++Passed;
            std::printf("PASS %s\n", Test.Name);
        }
    }
    const int Total = static_cast<int>(sizeof(Tests) / sizeof(Tests[0]));
    std::printf("Damage model: %d/%d cases passed, %d checks, %d failures\n", Passed, Total, Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
