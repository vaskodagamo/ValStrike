// Standalone checks for Combat/VSFireRequestRules.h (no Unreal process).
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/fire_request_rules_test.cpp \
//   -o Saved/Tests/Weapons/fire_request_rules_test && Saved/Tests/Weapons/fire_request_rules_test
#include "../../Source/ValStrike/Combat/VSFireRequestRules.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
using namespace VSFireRequestRules;
int Failures = 0;
int Checks = 0;

void Expect(bool Condition, const char* Name)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::printf("FAIL %s\n", Name);
    }
}

bool Named(Result R, const char* Expected)
{
    const char* Name = ReasonName(R);
    return Name && std::strcmp(Name, Expected) == 0;
}

// Counts how often the phase gate ran, so tests can prove it stays lazy.
struct Gate
{
    bool bOpen = true;
    int Calls = 0;
    bool operator()() { ++Calls; return bOpen; }
};

// A request every check accepts: rifle cadence, on time, aimed straight ahead.
FireInput Valid()
{
    FireInput In;
    In.bValidOwner = true;
    In.ShotId = 11;
    In.LastRequestId = 10;
    In.bAlive = true;
    In.WeaponReadiness = Readiness::Ready;
    In.bReloading = false;
    In.Magazine = 30;
    In.Now = 5.0;
    In.NextServerFireTime = 5.0;
    In.FireInterval = static_cast<double>(0.12f);
    In.ServerOrigin = {100.0, -50.0, 160.0};
    In.Origin = In.ServerOrigin;
    In.BaseAim = {1.0, 0.0, 0.0};
    In.Direction = In.BaseAim;
    return In;
}

FireVerdict Fire(const FireInput& In, Gate& G) { return EvaluateFire(In, G); }

ReloadInput ValidReload()
{
    ReloadInput In;
    In.bValidOwner = true;
    In.bAlive = true;
    In.Magazine = 10;
    In.MagazineCapacity = 30;
    In.Reserve = 60;
    return In;
}
}

int main()
{
    const double Nan = std::numeric_limits<double>::quiet_NaN();
    const double Inf = std::numeric_limits<double>::infinity();

    // Reason names are the rejection counter keys the smoke suites read.
    {
        Expect(Named(Result::Owner, "owner") && Named(Result::Sequence, "sequence") && Named(Result::Dead, "dead")
            && Named(Result::Holstered, "holstered") && Named(Result::Equip, "equip") && Named(Result::Reload, "reload")
            && Named(Result::Empty, "empty") && Named(Result::Cadence, "cadence") && Named(Result::Nonfinite, "nonfinite")
            && Named(Result::Origin, "origin") && Named(Result::Direction, "direction") && Named(Result::Phase, "phase"),
            "every rejection keeps its counter name");
        Expect(!ReasonName(Result::Accept) && !ReasonName(Result::Ignored), "accept and ignore record no reason");
    }

    // One case per fire verdict.
    {
        Gate G;
        const FireVerdict V = Fire(Valid(), G);
        Expect(V.Outcome == Result::Accept && V.bConsumesShotId && G.Calls == 1, "valid request is accepted after the phase gate");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.bValidOwner = false;
        const FireVerdict V = Fire(In, G);
        Expect(V.Outcome == Result::Owner && !V.bConsumesShotId && G.Calls == 0, "unpossessed owner: rejected, id not consumed");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.ShotId = 0;
        Expect(Fire(In, G).Outcome == Result::Sequence && !Fire(In, G).bConsumesShotId, "zero id is reserved");
        In.ShotId = In.LastRequestId;
        Expect(Fire(In, G).Outcome == Result::Sequence && !Fire(In, G).bConsumesShotId, "duplicate id");
        In.ShotId = In.LastRequestId - 1;
        Expect(Fire(In, G).Outcome == Result::Sequence, "stale id (delta wraps past the window)");
        In.ShotId = In.LastRequestId + MaxSequenceDelta;
        Expect(Fire(In, G).Outcome == Result::Accept, "delta of exactly 1024 is accepted");
        In.ShotId = In.LastRequestId + MaxSequenceDelta + 1;
        Expect(Fire(In, G).Outcome == Result::Sequence, "delta of 1025 is rejected");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.LastRequestId = 0xFFFFFFFFu;
        In.ShotId = 1;
        Expect(Fire(In, G).Outcome == Result::Accept, "wrap-around: id 1 after 0xFFFFFFFF");
        In.ShotId = 0;
        Expect(Fire(In, G).Outcome == Result::Sequence, "wrap-around never accepts the reserved zero");
        In.LastRequestId = 0;
        In.ShotId = 1;
        Expect(Fire(In, G).Outcome == Result::Accept, "first id of a pawn's life");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.bAlive = false;
        const FireVerdict V = Fire(In, G);
        Expect(V.Outcome == Result::Dead && V.bConsumesShotId, "dead owner: rejected, id consumed");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.WeaponReadiness = Readiness::Holstered;
        Expect(Fire(In, G).Outcome == Result::Holstered && Fire(In, G).bConsumesShotId, "holstered weapon");
        In.WeaponReadiness = Readiness::Equipping;
        Expect(Fire(In, G).Outcome == Result::Equip && Fire(In, G).bConsumesShotId, "weapon still equipping");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.bReloading = true;
        Expect(Fire(In, G).Outcome == Result::Reload && Fire(In, G).bConsumesShotId, "shot while reloading");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.Magazine = 0;
        Expect(Fire(In, G).Outcome == Result::Empty && Fire(In, G).bConsumesShotId, "empty magazine");
        In.Magazine = -1;
        Expect(Fire(In, G).Outcome == Result::Empty, "negative magazine counts as empty");
        In.Magazine = 1;
        Expect(Fire(In, G).Outcome == Result::Accept, "last round fires");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.NextServerFireTime = In.Now + 0.05;
        Expect(Fire(In, G).Outcome == Result::Cadence && Fire(In, G).bConsumesShotId, "back-to-back shot too early");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.Origin.y = Nan;
        Expect(Fire(In, G).Outcome == Result::Nonfinite && Fire(In, G).bConsumesShotId, "NaN origin");
        In = Valid();
        In.Direction.z = Nan;
        Expect(Fire(In, G).Outcome == Result::Nonfinite, "NaN direction");
        In = Valid();
        In.Origin.x = Inf;
        Expect(Fire(In, G).Outcome == Result::Nonfinite, "infinite origin (ContainsNaN covers infinities)");
        In = Valid();
        In.Direction.x = -Inf;
        Expect(Fire(In, G).Outcome == Result::Nonfinite, "infinite direction");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.Origin.x += 500.0;
        Expect(Fire(In, G).Outcome == Result::Origin && Fire(In, G).bConsumesShotId, "500 cm forged origin");
    }
    {
        Gate G;
        FireInput In = Valid();
        In.Direction = {-1.0, 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Direction && Fire(In, G).bConsumesShotId, "opposite direction");
        In.Direction = {0.0, 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Direction, "zero direction");
        In.Direction = {0.5, 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Direction, "aimed but not unit length");
        In.Direction = {std::sqrt(0.995), 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Accept, "squared size 0.995 is within tolerance");
        In.Direction = {std::sqrt(1.005), 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Accept, "squared size 1.005 is within tolerance");
        In.Direction = {std::sqrt(0.985), 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Direction, "squared size 0.985 is outside tolerance");
        In.Direction = {std::sqrt(1.015), 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Direction, "squared size 1.015 is outside tolerance");
    }
    {
        Gate G;
        G.bOpen = false;
        const FireVerdict V = Fire(Valid(), G);
        Expect(V.Outcome == Result::Phase && V.bConsumesShotId && G.Calls == 1, "closed combat phase rejects last");
    }

    // The phase gate has side effects in the match, so it only runs after every other check passed.
    {
        Gate G;
        FireInput In = Valid();
        In.Direction = {-1.0, 0.0, 0.0};
        Fire(In, G);
        In = Valid();
        In.ShotId = 0;
        Fire(In, G);
        In = Valid();
        In.bValidOwner = false;
        Fire(In, G);
        Expect(G.Calls == 0, "phase gate is not evaluated when an earlier check rejects");
    }

    // Order: when several checks fail, the earliest names the rejection.
    {
        Gate G;
        G.bOpen = false;
        FireInput In = Valid();
        In.bAlive = false;
        In.bReloading = true;
        In.Magazine = 0;
        In.NextServerFireTime = In.Now + 1.0;
        In.Origin.x = Nan;
        In.Direction = {-1.0, 0.0, 0.0};
        Expect(Fire(In, G).Outcome == Result::Dead, "dead wins over every later failure");
        In.bAlive = true;
        In.WeaponReadiness = Readiness::Equipping;
        Expect(Fire(In, G).Outcome == Result::Equip, "equip wins over reload");
        In.WeaponReadiness = Readiness::Ready;
        Expect(Fire(In, G).Outcome == Result::Reload, "reload wins over empty");
        In.bReloading = false;
        Expect(Fire(In, G).Outcome == Result::Empty, "empty wins over cadence");
        In.Magazine = 5;
        Expect(Fire(In, G).Outcome == Result::Cadence, "cadence wins over nonfinite");
        In.NextServerFireTime = In.Now;
        Expect(Fire(In, G).Outcome == Result::Nonfinite, "nonfinite wins over direction");
        In.Origin = In.ServerOrigin;
        In.Origin.z += 200.0;
        Expect(Fire(In, G).Outcome == Result::Origin, "origin wins over direction");
        In.Origin = In.ServerOrigin;
        Expect(Fire(In, G).Outcome == Result::Direction, "direction wins over phase");
        Expect(G.Calls == 0, "gate still untouched while direction rejects");
    }

    // Boundary: cadence exactly at the arrival tolerance.
    {
        Gate G;
        const double Interval = static_cast<double>(0.12f);
        Expect(ArrivalTolerance(Interval) == 0.02, "rifle tolerance is the 20 ms cap");
        Expect(ArrivalTolerance(static_cast<double>(0.05f)) == static_cast<double>(0.05f) * 0.2, "fast weapon tolerance is 20% of its interval");
        FireInput In = Valid();
        In.FireInterval = Interval;
        In.NextServerFireTime = In.Now + ArrivalTolerance(Interval);
        Expect(Fire(In, G).Outcome == Result::Accept, "arrival exactly at the tolerance is accepted");
        In.NextServerFireTime = std::nextafter(In.NextServerFireTime, Inf);
        Expect(Fire(In, G).Outcome == Result::Cadence, "arrival just beyond the tolerance is rejected");

        In.FireInterval = static_cast<double>(0.05f);
        In.NextServerFireTime = In.Now + ArrivalTolerance(In.FireInterval);
        Expect(Fire(In, G).Outcome == Result::Accept, "proportional tolerance boundary is accepted");
        In.NextServerFireTime = std::nextafter(In.NextServerFireTime, Inf);
        Expect(Fire(In, G).Outcome == Result::Cadence, "proportional tolerance plus one ulp is rejected");
    }

    // Boundary: origin at exactly 100 cm.
    {
        Gate G;
        FireInput In = Valid();
        In.Origin = In.ServerOrigin;
        In.Origin.x += 100.0;
        Expect(DistSquared(In.Origin, In.ServerOrigin) == 10000.0, "test origin is exactly 100 cm away");
        Expect(Fire(In, G).Outcome == Result::Accept, "origin exactly 100 cm away is accepted");
        In.Origin.x = std::nextafter(In.Origin.x, Inf);
        Expect(Fire(In, G).Outcome == Result::Origin, "origin just beyond 100 cm is rejected");
        In.Origin = {In.ServerOrigin.x + 60.0, In.ServerOrigin.y + 80.0, In.ServerOrigin.z};
        Expect(Fire(In, G).Outcome == Result::Accept, "diagonal 60/80 offset is exactly 100 cm");
    }

    // Boundary: direction dot product exactly 0.9396926 (20 degrees).
    {
        Gate G;
        FireInput In = Valid();
        const double Side = std::sqrt(1.0 - MinAimDot * MinAimDot);
        In.Direction = {MinAimDot, Side, 0.0};
        Expect(Dot(In.Direction, In.BaseAim) == MinAimDot, "test direction sits exactly on the cone");
        Expect(Fire(In, G).Outcome == Result::Accept, "dot of exactly 0.9396926 is accepted");
        In.Direction.x = std::nextafter(MinAimDot, 0.0);
        Expect(Fire(In, G).Outcome == Result::Direction, "dot just below 0.9396926 is rejected");
    }

    // Reload request verdicts.
    {
        Gate G;
        Expect(EvaluateReload(ValidReload(), G) == Result::Accept && G.Calls == 1, "valid reload starts");
    }
    {
        Gate G;
        ReloadInput In = ValidReload();
        In.bValidOwner = false;
        Expect(EvaluateReload(In, G) == Result::Ignored, "unpossessed owner is dropped silently");
        In = ValidReload();
        In.bAlive = false;
        Expect(EvaluateReload(In, G) == Result::Ignored, "dead owner is dropped silently");
        Expect(G.Calls == 0, "phase gate not evaluated for owner or death");
    }
    {
        Gate G;
        G.bOpen = false;
        ReloadInput In = ValidReload();
        In.WeaponReadiness = Readiness::Holstered;
        In.bReloading = true;
        Expect(EvaluateReload(In, G) == Result::Phase, "reload phase check precedes readiness");
    }
    {
        Gate G;
        ReloadInput In = ValidReload();
        In.WeaponReadiness = Readiness::Holstered;
        Expect(EvaluateReload(In, G) == Result::Holstered, "reload of a holstered weapon");
        In.WeaponReadiness = Readiness::Equipping;
        Expect(EvaluateReload(In, G) == Result::Equip, "reload while equipping");
    }
    {
        Gate G;
        ReloadInput In = ValidReload();
        In.bReloading = true;
        Expect(EvaluateReload(In, G) == Result::Ignored, "reload already running is a no-op");
        In = ValidReload();
        In.Magazine = In.MagazineCapacity;
        Expect(EvaluateReload(In, G) == Result::Ignored, "full magazine is a no-op");
        In = ValidReload();
        In.Reserve = 0;
        Expect(EvaluateReload(In, G) == Result::Ignored, "empty reserve is a no-op");
        In = ValidReload();
        In.Magazine = In.MagazineCapacity - 1;
        In.Reserve = 1;
        Expect(EvaluateReload(In, G) == Result::Accept, "one missing round with one in reserve reloads");
    }

    std::printf("%d/%d fire request rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
