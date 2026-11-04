// Standalone checks for weapon weight, the CS2-scaled jump arc and the crouch-jump tuck.
// clang++ -std=c++17 -Wall -Wextra -Wshadow -Werror -pedantic Tests/Movement/movement_rules_test.cpp -o Saved/Tests/Movement/movement_rules_test
#include "../../Source/ValStrike/VSMovementRules.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
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

bool Near(double A, double B, double Epsilon) { return std::fabs(A - B) <= Epsilon; }

using namespace VSMovementRules;

const double NaN = std::numeric_limits<double>::quiet_NaN();
const double Inf = std::numeric_limits<double>::infinity();

void WeaponWeight()
{
    // CS2 ratios: knife 250, pistol 240, rifle 215.
    EXPECT(SpeedMultiplier(WeaponKind::Unarmed) == 1.0);
    // The knife moves at the unweighted base speed (450 cm/s running).
    EXPECT(SpeedMultiplier(WeaponKind::Knife) == 1.0);
    EXPECT(MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Knife) == 450.0);
    EXPECT(MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Pistol) < MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Knife));
    EXPECT(Near(SpeedMultiplier(WeaponKind::Pistol), 0.96, 1e-12));
    EXPECT(Near(SpeedMultiplier(WeaponKind::Rifle), 0.86, 1e-12));
    // Heavier is slower, at every base speed (run, crouch).
    EXPECT(MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Rifle) < MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Pistol));
    EXPECT(MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Pistol) < MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Unarmed));
    EXPECT(Near(MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Rifle), 387.0, 1e-9));
    EXPECT(Near(MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Pistol), 432.0, 1e-9));
    EXPECT(Near(MaxSpeed(BaseMaxWalkSpeedCrouched, WeaponKind::Rifle), 154.8, 1e-9));
    EXPECT(Near(MaxSpeed(BaseMaxWalkSpeedCrouched, WeaponKind::Pistol), 172.8, 1e-9));
    // Slow-walk is the 0.45 input scale of the weighted max (AVSCharacter::WalkInputScale).
    EXPECT(Near(0.45 * MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Rifle), 174.15, 1e-9));
    // Invalid bases never produce motion.
    for (const double Base : {0.0, -450.0, NaN, Inf})
        EXPECT(MaxSpeed(Base, WeaponKind::Rifle) == 0.0);
}

void JumpArc()
{
    // CS2 itself: apex 57 u, airtime about 0.755 s.
    EXPECT(Near(JumpApex(Cs2::JumpImpulse, Cs2::Gravity), 57.0, 1e-3));
    EXPECT(Near(Airtime(Cs2::JumpImpulse, Cs2::Gravity), 0.755, 1e-3));
    // Scaled to 176 cm: the same apex/height ratio (0.79) and the same airtime.
    EXPECT(Near(HeightScale, 176.0 / 72.0, 1e-12));
    const double Apex = JumpApex(JumpZVelocity, Gravity);
    EXPECT(Near(Apex / PlayerHeight, 57.0 / 72.0, 1e-4));
    EXPECT(Near(Apex, 139.33, 0.01));
    EXPECT(Near(Airtime(JumpZVelocity, Gravity), Airtime(Cs2::JumpImpulse, Cs2::Gravity), 1e-12));
    EXPECT(Near(GravityScale, 1.9955, 1e-4));
    EXPECT(Near(JumpZVelocity, 738.2, 0.05));
    // Snappier than the old tuning (JumpZ 420, gravity 980: 90 cm, 0.857 s).
    EXPECT(Near(JumpApex(420.0, EngineGravity), 90.0, 0.01));
    EXPECT(Airtime(JumpZVelocity, Gravity) < Airtime(420.0, EngineGravity));
    // Invalid arcs are empty.
    for (const double Bad : {0.0, -1.0, NaN, Inf})
    {
        EXPECT(JumpApex(Bad, Gravity) == 0.0);
        EXPECT(JumpApex(JumpZVelocity, Bad) == 0.0);
        EXPECT(Airtime(Bad, Gravity) == 0.0);
        EXPECT(Airtime(JumpZVelocity, Bad) == 0.0);
    }
}

void CrouchJump()
{
    // UE tucks a falling capsule about its center: 88 - 56 = 32 cm.
    EXPECT(EngineAirTuck == 32.0);
    EXPECT(Near(CrouchJumpGainTarget, 22.0, 1e-9));
    EXPECT(Near(AirTuckDrop(EngineAirTuck, CrouchJumpGainTarget), 10.0, 1e-9));
    const double Apex = JumpApex(JumpZVelocity, Gravity);
    const double Clearance = CrouchJumpClearance(Apex, EngineAirTuck, CrouchJumpGainTarget);
    EXPECT(Near(Clearance - Apex, 22.0, 1e-9));
    EXPECT(Near(Clearance, 161.33, 0.01));
    // CS2 ratio of crouch-jump to jump reach (66/57) is kept.
    EXPECT(Near(Clearance / Apex, 66.0 / 57.0, 1e-3));
    // The tuck never lifts: a target above the engine tuck gives only the engine tuck.
    EXPECT(AirTuckDrop(EngineAirTuck, 40.0) == 0.0);
    EXPECT(CrouchJumpClearance(Apex, EngineAirTuck, 40.0) == Apex + EngineAirTuck);
    EXPECT(AirTuckDrop(EngineAirTuck, -5.0) == EngineAirTuck);
    for (const double Bad : {NaN, Inf, 0.0, -32.0})
        EXPECT(AirTuckDrop(Bad, CrouchJumpGainTarget) == 0.0);
    EXPECT(AirTuckDrop(EngineAirTuck, NaN) == 0.0);
}

void RunningJump()
{
    // Same launch velocity at any ground speed: farther, not higher.
    const double Rifle = MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Rifle);
    const double Pistol = MaxSpeed(BaseMaxWalkSpeed, WeaponKind::Pistol);
    EXPECT(JumpDistance(0.0, JumpZVelocity, Gravity) == 0.0);
    EXPECT(Near(JumpDistance(Rifle, JumpZVelocity, Gravity), 292.2, 0.1));
    EXPECT(Near(JumpDistance(Pistol, JumpZVelocity, Gravity), 326.2, 0.1));
    EXPECT(JumpDistance(Rifle, JumpZVelocity, Gravity) < JumpDistance(Pistol, JumpZVelocity, Gravity));
    for (const double Bad : {-1.0, NaN, Inf})
        EXPECT(JumpDistance(Bad, JumpZVelocity, Gravity) == 0.0);
}
}

int main()
{
    WeaponWeight();
    JumpArc();
    CrouchJump();
    RunningJump();
    std::printf("%d/%d movement rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
