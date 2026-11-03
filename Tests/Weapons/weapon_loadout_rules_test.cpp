// Standalone checks for Combat/VSWeaponLoadoutRules.h (no Unreal process).
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/weapon_loadout_rules_test.cpp \
//   -o Saved/Tests/Weapons/weapon_loadout_rules_test && Saved/Tests/Weapons/weapon_loadout_rules_test
#include "../../Source/ValStrike/Combat/VSWeaponLoadoutRules.h"

#include <cstdio>

namespace
{
using namespace VSWeaponLoadoutRules;
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

Request Base()
{
    Request In;
    In.now = 10.0;
    In.lastSwitch = 5.0;
    return In;
}

bool Is(const Result& Out, Verdict V, Slot S) { return Out.verdict == V && Out.target == S; }
}

int main()
{
    {
        Request In = Base();
        In.raw = RequestPistol;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Pistol), "pistol accepted from rifle");
        In.raw = RequestRifle;
        Expect(Is(Evaluate(In), Verdict::Duplicate, Slot::Rifle), "rifle while rifle equipped is duplicate");
        In.raw = RequestLast;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Pistol), "last resolves to server previous slot");
        In.equipped = Slot::Pistol;
        In.previous = Slot::Rifle;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Rifle), "last toggles back to rifle");
        In.raw = RequestPistol;
        Expect(Is(Evaluate(In), Verdict::Duplicate, Slot::Pistol), "pistol while pistol equipped is duplicate");
    }
    {
        bool bAllInvalid = true;
        for (int Raw = RequestLast + 1; Raw <= 255; ++Raw)
        {
            Request In = Base();
            In.raw = static_cast<std::uint8_t>(Raw);
            bAllInvalid = bAllInvalid && Is(Evaluate(In), Verdict::Invalid, Slot::Rifle);
        }
        Expect(bAllInvalid, "every byte above Last is invalid and keeps the equipped slot");
    }
    {
        // Precedence: each earlier failure masks every later one.
        Request In = Base();
        In.raw = 9;
        In.alive = false;
        In.combatAllowed = false;
        In.lastSwitch = In.now;
        Expect(Evaluate(In).verdict == Verdict::Invalid, "invalid before dead");
        In.raw = RequestRifle;
        Expect(Evaluate(In).verdict == Verdict::Dead, "dead before phase and duplicate");
        In.alive = true;
        Expect(Evaluate(In).verdict == Verdict::Phase, "phase before duplicate");
        In.combatAllowed = true;
        Expect(Evaluate(In).verdict == Verdict::Duplicate, "duplicate before spam");
        In.raw = RequestPistol;
        Expect(Evaluate(In).verdict == Verdict::Spam, "spam within interval");
    }
    {
        // Rate limit boundary: exactly MinInterval later is allowed (binary-exact values).
        Request In = Base();
        In.raw = RequestPistol;
        In.minInterval = 0.125;
        In.lastSwitch = 9.9375;
        Expect(Evaluate(In).verdict == Verdict::Spam, "0.0625 s after a switch is spam");
        In.lastSwitch = 9.875;
        Expect(Evaluate(In).verdict == Verdict::Accept, "exactly the interval is accepted");
        In.minInterval = 0.0;
        In.lastSwitch = In.now;
        Expect(Evaluate(In).verdict == Verdict::Accept, "zero interval disables the rate limit");
    }
    {
        Request In = Base();
        In.raw = RequestPistol;
        In.alive = false;
        Expect(Is(Evaluate(In), Verdict::Dead, Slot::Rifle), "dead owner keeps the equipped slot");
        In.alive = true;
        In.combatAllowed = false;
        Expect(Is(Evaluate(In), Verdict::Phase, Slot::Rifle), "closed gate keeps the equipped slot");
    }
    Expect(!IsReady(false, 10.0, 0.0), "holstered weapon is never ready");
    Expect(!IsReady(true, 10.0, 10.5), "equipping weapon is not ready");
    Expect(IsReady(true, 10.5, 10.5), "ready exactly at equip completion");
    Expect(IsReady(true, 11.0, 10.5), "ready after equip completion");
    std::printf("%d/%d weapon loadout rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
