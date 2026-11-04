#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Engine-free validation of a weapon's server fire and reload requests, used by
// UVSWeaponComponent and the standalone test in Tests/Weapons. Times are seconds,
// distances centimetres. The component gathers the inputs with engine types, keeps
// the order and reason names below, and owns every side effect (ammo, trace, timers).
//
// Order matters: the first failing check names the rejection. The combat-phase gate
// is passed as a callable because the match's live gate processes expired deadlines;
// it runs only when every earlier check has passed, exactly as before the refactor.
namespace VSFireRequestRules
{
enum class Result : std::uint8_t
{
    Accept,
    Owner,
    Sequence,
    Dead,
    Holstered,
    Equip,
    Reload,
    Empty,
    Cadence,
    Nonfinite,
    Origin,
    Direction,
    Phase,
    // Reload only: the request is dropped without counting a rejection.
    Ignored,
};

// Reason recorded by UVSWeaponComponent::Reject; nullptr when nothing is recorded.
inline const char* ReasonName(Result R)
{
    switch (R)
    {
    case Result::Owner: return "owner";
    case Result::Sequence: return "sequence";
    case Result::Dead: return "dead";
    case Result::Holstered: return "holstered";
    case Result::Equip: return "equip";
    case Result::Reload: return "reload";
    case Result::Empty: return "empty";
    case Result::Cadence: return "cadence";
    case Result::Nonfinite: return "nonfinite";
    case Result::Origin: return "origin";
    case Result::Direction: return "direction";
    case Result::Phase: return "phase";
    case Result::Accept:
    case Result::Ignored: return nullptr;
    }
    return nullptr;
}

// Mirrors EVSWeaponReadiness. A pawn without a loadout component counts as Ready.
enum class Readiness : std::uint8_t { Ready, Holstered, Equipping };

struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Same operand order as FVector::DistSquared / SizeSquared / DotProduct.
inline double DistSquared(const Vec3& A, const Vec3& B)
{
    const double X = B.x - A.x;
    const double Y = B.y - A.y;
    const double Z = B.z - A.z;
    return X * X + Y * Y + Z * Z;
}
inline double SizeSquared(const Vec3& V) { return V.x * V.x + V.y * V.y + V.z * V.z; }
inline double Dot(const Vec3& A, const Vec3& B) { return A.x * B.x + A.y * B.y + A.z * B.z; }
// FVector::ContainsNaN also rejects infinities.
inline bool IsFinite(const Vec3& V) { return std::isfinite(V.x) && std::isfinite(V.y) && std::isfinite(V.z); }

// Request IDs: zero is reserved; a request must be a positive serial step of at
// most MaxSequenceDelta past the last consumed ID (unsigned, so it wraps).
constexpr std::uint32_t MaxSequenceDelta = 1024;
// Client origin may be at most this far from the server's view location.
constexpr double MaxOriginDistance = 100.0;
// Direction must be unit length within this tolerance on its squared size...
constexpr double UnitSizeSquaredTolerance = 0.01;
// ...and within 20 degrees of the server's base aim (cos 20 deg).
constexpr double MinAimDot = 0.9396926;

// Absorbs one 60 Hz scheduling frame without granting sustained rate credit.
inline double ArrivalTolerance(double FireInterval) { return std::min(0.02, FireInterval * 0.2); }

inline bool IsValidSequence(std::uint32_t ShotId, std::uint32_t LastRequestId)
{
    const std::uint32_t Delta = ShotId - LastRequestId;
    return ShotId != 0 && Delta != 0 && Delta <= MaxSequenceDelta;
}

struct FireInput
{
    // Possession check (UVSWeaponComponent::HasValidOwner). When false, nothing
    // below is read.
    bool bValidOwner = false;
    std::uint32_t ShotId = 0;
    std::uint32_t LastRequestId = 0;
    bool bAlive = false;
    Readiness WeaponReadiness = Readiness::Ready;
    bool bReloading = false;
    std::int32_t Magazine = 0;
    double Now = 0.0;
    double NextServerFireTime = 0.0;
    double FireInterval = 0.0;
    // Client-sent values.
    Vec3 Origin;
    Vec3 Direction;
    // Server's view of the pawn: view location and base aim unit vector.
    Vec3 ServerOrigin;
    Vec3 BaseAim;
};

struct FireVerdict
{
    Result Outcome = Result::Accept;
    // True once the sequence check passed: the caller stores ShotId as the last
    // request ID even when a later check rejects, so the ID cannot be replayed.
    bool bConsumesShotId = false;
};

// CombatAllowed: bool() callable, evaluated only if every earlier check passed.
template <typename CombatGate>
FireVerdict EvaluateFire(const FireInput& In, CombatGate&& CombatAllowed)
{
    FireVerdict V;
    const auto Reject = [&V](Result R) { V.Outcome = R; return V; };
    if (!In.bValidOwner) { return Reject(Result::Owner); }
    if (!IsValidSequence(In.ShotId, In.LastRequestId)) { return Reject(Result::Sequence); }
    V.bConsumesShotId = true;
    if (!In.bAlive) { return Reject(Result::Dead); }
    // Only the equipped weapon may fire, and only after its equip time.
    if (In.WeaponReadiness == Readiness::Holstered) { return Reject(Result::Holstered); }
    if (In.WeaponReadiness == Readiness::Equipping) { return Reject(Result::Equip); }
    if (In.bReloading) { return Reject(Result::Reload); }
    if (In.Magazine <= 0) { return Reject(Result::Empty); }
    if (In.Now + ArrivalTolerance(In.FireInterval) < In.NextServerFireTime) { return Reject(Result::Cadence); }
    if (!IsFinite(In.Origin) || !IsFinite(In.Direction)) { return Reject(Result::Nonfinite); }
    if (DistSquared(In.Origin, In.ServerOrigin) > MaxOriginDistance * MaxOriginDistance) { return Reject(Result::Origin); }
    if (!(std::fabs(SizeSquared(In.Direction) - 1.0) <= UnitSizeSquaredTolerance)
        || Dot(In.Direction, In.BaseAim) < MinAimDot)
    {
        return Reject(Result::Direction);
    }
    if (!CombatAllowed()) { return Reject(Result::Phase); }
    return V;
}

struct ReloadInput
{
    bool bValidOwner = false;
    bool bAlive = false;
    Readiness WeaponReadiness = Readiness::Ready;
    bool bReloading = false;
    std::int32_t Magazine = 0;
    std::int32_t MagazineCapacity = 0;
    std::int32_t Reserve = 0;
};

// Owner and death drop the request silently; a reload already running, a full
// magazine or an empty reserve are silent no-ops. Phase is checked before readiness.
template <typename CombatGate>
Result EvaluateReload(const ReloadInput& In, CombatGate&& CombatAllowed)
{
    if (!In.bValidOwner || !In.bAlive) { return Result::Ignored; }
    if (!CombatAllowed()) { return Result::Phase; }
    if (In.WeaponReadiness == Readiness::Holstered) { return Result::Holstered; }
    if (In.WeaponReadiness == Readiness::Equipping) { return Result::Equip; }
    if (In.bReloading || In.Magazine >= In.MagazineCapacity || In.Reserve <= 0) { return Result::Ignored; }
    return Result::Accept;
}
}
