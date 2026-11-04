// Standalone checks for Combat/VSRecoilRules.h (no Unreal process).
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/recoil_rules_test.cpp \
//   -o Saved/Tests/Weapons/recoil_rules_test && Saved/Tests/Weapons/recoil_rules_test
#include "../../Source/ValStrike/Combat/VSRecoilRules.h"

#include <cmath>
#include <cstdio>

namespace
{
using namespace VSRecoilRules;
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

double Length(const Offset& O) { return std::sqrt(O.pitch * O.pitch + O.yaw * O.yaw); }
bool Same(const Offset& A, const Offset& B) { return A.pitch == B.pitch && A.yaw == B.yaw; }

// Fires Count shots Interval apart from Start and returns the index of the last one.
std::uint32_t Spray(State& S, const Tuning& T, double Start, double Interval, int Count)
{
    std::uint32_t Index = 0;
    for (int Shot = 0; Shot < Count; ++Shot) { Index = RegisterShot(S, T, Start + Shot * Interval); }
    return Index;
}

const Stance AllStances[] = {Stance::Crouched, Stance::Standing, Stance::Moving, Stance::Airborne};
}

int main()
{
    const Tuning Rifle = RifleTuning();
    const Tuning Pistol = PistolTuning();

    // Server/client agreement: the offset is a pure function of its inputs.
    {
        bool bDeterministic = true;
        bool bVaries = false;
        for (std::uint32_t Id = 1; Id < 200; ++Id)
        {
            const std::uint32_t Seed = ShotSeed(Id, 0);
            bDeterministic = bDeterministic && Seed == ShotSeed(Id, 0)
                && Same(ShotOffset(Rifle, 6, Stance::Moving, Seed), ShotOffset(Rifle, 6, Stance::Moving, ShotSeed(Id, 0)))
                && Same(ViewKick(Pistol, Seed), ViewKick(Pistol, ShotSeed(Id, 0)));
            bVaries = bVaries || !Same(ShotOffset(Rifle, 6, Stance::Moving, Seed),
                ShotOffset(Rifle, 6, Stance::Moving, ShotSeed(Id + 1, 0)));
        }
        Expect(bDeterministic, "same shot id, index and stance give the same offset and kick");
        Expect(bVaries, "different shot ids sample different points of the cone");
        Expect(ShotSeed(7, 0) != ShotSeed(7, 1), "weapon salt separates rifle and pistol seeds");
        Expect(UnitValue(0, 0) >= 0.0 && UnitValue(0xffffffffU, 0xffffffffU) < 1.0, "unit values stay in [0, 1)");
    }

    // First-shot accuracy: standing or crouched shots after recovery are exact.
    for (const Tuning* T : {&Rifle, &Pistol})
    {
        bool bExact = true;
        for (std::uint32_t Id = 1; Id < 500; ++Id)
        {
            bExact = bExact && Same(ShotOffset(*T, 0, Stance::Standing, ShotSeed(Id, 0)), Offset())
                && Same(ShotOffset(*T, 0, Stance::Crouched, ShotSeed(Id, 1)), Offset());
        }
        Expect(bExact, T == &Rifle ? "rifle first shot is exact" : "pistol first shot is exact");
        Expect(SpreadDegrees(*T, 0, Stance::Moving) > 0.0, "moving first shot is not exact");
    }

    // Shot index: held rifle fire climbs by one per shot; a pause recovers.
    {
        State S;
        Expect(ShotIndexAt(S, Rifle, 5.0) == 0 && RegisterShot(S, Rifle, 5.0) == 0, "fresh weapon fires index 0");
        Expect(Spray(S, Rifle, 5.12, 0.12, 9) == 9, "ten held rifle shots reach index 9");
        Expect(ShotIndexAt(S, Rifle, S.lastShotTime + 0.1) == 10, "no recovery during the delay");
        Expect(ShotIndexAt(S, Rifle, S.lastShotTime + Rifle.recoveryDelay + 0.45) == 5, "partial recovery after a short pause");
        Expect(ShotIndexAt(S, Rifle, 7.5) == 0, "full recovery after a long pause");
        Expect(RegisterShot(S, Rifle, 7.5) == 0, "first shot after recovery is index 0 again");
        State Tapped;
        RegisterShot(Tapped, Rifle, 1.0);
        Expect(RegisterShot(Tapped, Rifle, 1.3) == 0, "rifle taps 0.3 s apart stay at index 0");
        State Backwards;
        RegisterShot(Backwards, Rifle, 2.0);
        Expect(RegisterShot(Backwards, Rifle, 1.0) == 1 && Backwards.lastShotTime == 2.0,
            "time running backwards neither recovers nor rewinds");
        State Long;
        Expect(Spray(Long, Rifle, 0.0, 0.12, 500) == static_cast<std::uint32_t>(MaxHeat), "heat is capped");
        Expect(RegisterShot(Long, Rifle, std::nan("")) == static_cast<std::uint32_t>(MaxHeat),
            "non-finite time does not recover");
    }

    // Pistol: semi-auto taps 0.3 s apart (weapon smoke) stay exact; spam accumulates.
    {
        State S;
        RegisterShot(S, Pistol, 3.6);
        Expect(RegisterShot(S, Pistol, 3.9) == 0, "pistol taps 0.3 s apart stay at index 0");
        State Spam;
        Expect(Spray(Spam, Pistol, 0.0, 0.2, 6) >= 1, "pistol spam at the 0.2 s cadence accumulates kick");
        Expect(ShotIndexAt(Spam, Pistol, 1.0 + 1.0) == 0, "pistol recovers within a second");
    }

    // Pattern: bounded climb and drift.
    {
        bool bClimbs = true;
        bool bBounded = true;
        double MaxYaw = 0.0;
        for (std::uint32_t Index = 0; Index < 64; ++Index)
        {
            const Offset P = PatternOffset(Rifle, Index);
            if (Index > 0 && Index * Rifle.climbPerShot <= Rifle.maxClimb)
            {
                bClimbs = bClimbs && P.pitch > PatternOffset(Rifle, Index - 1).pitch;
            }
            bBounded = bBounded && P.pitch >= 0.0 && P.pitch <= Rifle.maxClimb && std::fabs(P.yaw) <= Rifle.maxDrift + 1e-12;
            MaxYaw = std::max(MaxYaw, std::fabs(P.yaw));
        }
        Expect(bClimbs, "rifle pattern climbs every shot until its cap");
        Expect(bBounded, "rifle climb and drift stay within their bounds");
        Expect(MaxYaw > 0.5 * Rifle.maxDrift, "rifle drifts horizontally under sustained fire");
        Expect(PatternOffset(Rifle, Rifle.driftStartIndex - 1).yaw == 0.0, "no drift before the drift start");
        Expect(PatternOffset(Pistol, 1).pitch == Pistol.climbPerShot && PatternOffset(Pistol, 30).pitch == Pistol.maxClimb
            && PatternOffset(Pistol, 30).yaw == 0.0, "pistol kicks straight up to its cap");
    }

    // Spread: grows with consecutive shots, ordered by stance, capped; samples stay inside.
    for (const Tuning* T : {&Rifle, &Pistol})
    {
        bool bGrows = true;
        bool bOrdered = true;
        bool bInside = true;
        for (std::uint32_t Index = 0; Index < 64; ++Index)
        {
            for (const Stance S : AllStances)
            {
                if (Index > 0) { bGrows = bGrows && SpreadDegrees(*T, Index, S) >= SpreadDegrees(*T, Index - 1, S); }
                for (std::uint32_t Id = 1; Id < 50; ++Id)
                {
                    const Offset O = ShotOffset(*T, Index, S, ShotSeed(Id, 0));
                    const Offset P = PatternOffset(*T, Index);
                    bInside = bInside && Length({O.pitch - P.pitch, O.yaw - P.yaw}) <= SpreadDegrees(*T, Index, S) + 1e-9;
                }
            }
            if (Index > 0)
            {
                bOrdered = bOrdered && SpreadDegrees(*T, Index, Stance::Crouched) < SpreadDegrees(*T, Index, Stance::Standing);
            }
            bOrdered = bOrdered && SpreadDegrees(*T, Index, Stance::Crouched) <= SpreadDegrees(*T, Index, Stance::Standing)
                && SpreadDegrees(*T, Index, Stance::Standing) < SpreadDegrees(*T, Index, Stance::Moving)
                && SpreadDegrees(*T, Index, Stance::Moving) < SpreadDegrees(*T, Index, Stance::Airborne)
                && SpreadDegrees(*T, Index, Stance::Airborne) <= T->maxSpread;
        }
        Expect(SpreadDegrees(*T, 3, Stance::Standing) > SpreadDegrees(*T, 0, Stance::Standing), "spread blooms with consecutive shots");
        Expect(bGrows, "spread never shrinks as the index grows");
        Expect(bOrdered, "spread order: crouched < standing < moving < airborne, within the cap");
        Expect(bInside, "every sample lies inside the spread cone around the pattern");
        // The server accepts aim within 20 degrees of the pawn's base aim; offsets stay well inside it.
        Expect(T->maxClimb + T->maxDrift + T->maxSpread < 15.0, "worst-case offset stays far below the aim validation cone");
    }

    // Stance classification.
    Expect(ClassifyStance(true, true, 0.0, Rifle) == Stance::Airborne, "falling is airborne even when crouched");
    Expect(ClassifyStance(false, true, 180.0, Rifle) == Stance::Crouched, "crouch-walking at crouch speed stays crouched");
    Expect(ClassifyStance(false, false, 450.0, Rifle) == Stance::Moving, "walking speed counts as moving");
    Expect(ClassifyStance(false, false, Rifle.movingSpeed, Rifle) == Stance::Standing, "exactly the threshold is standing");

    // View kick: once per shot, yaw sign from the seed, recovers fully after its delay.
    {
        bool bLeft = false;
        bool bRight = false;
        for (std::uint32_t Id = 1; Id < 64; ++Id)
        {
            const Offset K = ViewKick(Rifle, ShotSeed(Id, 0));
            bLeft = bLeft || K.yaw < 0.0;
            bRight = bRight || K.yaw > 0.0;
            Expect(K.pitch == Rifle.viewKickPitch && std::fabs(K.yaw) == Rifle.viewKickYaw, "kick has the tuned magnitude");
        }
        Expect(bLeft && bRight, "kick yaw goes both ways");

        const Offset Kick = ViewKick(Pistol, ShotSeed(1, 1));
        Expect(Same(RecoverView(Kick, Pistol, Pistol.viewRecoveryDelay, 0.016), Offset()), "no view recovery during the delay");
        const Offset Partial = RecoverView(Kick, Pistol, Pistol.viewRecoveryDelay + 0.01, 0.016);
        Expect(std::fabs(Length(Partial) - Pistol.viewRecoveryRate * 0.01) < 1e-12, "only the frame time past the delay recovers");
        Offset Outstanding = Kick;
        double Since = 0.0;
        for (int Frame = 0; Frame < 120; ++Frame)
        {
            Since += 1.0 / 60.0;
            const Offset Step = RecoverView(Outstanding, Pistol, Since, 1.0 / 60.0);
            Outstanding.pitch -= Step.pitch;
            Outstanding.yaw -= Step.yaw;
        }
        Expect(Outstanding.pitch == 0.0 && Outstanding.yaw == 0.0, "view kick returns exactly to zero");
        Expect(Same(RecoverView(Kick, Pistol, 1.0, -1.0), Offset()), "negative frame time recovers nothing");
    }

    std::printf("%d/%d recoil rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
