// Standalone checks for gameplay-audio once-only emission, footstep cadence,
// the device beep curve and the placeholder generator.
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Audio/gameplay_audio_state_test.cpp -o Saved/Tests/Audio/gameplay_audio_state_test
#include "../../Source/ValStrike/Audio/VSGameplayAudioState.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

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

using namespace VSGameplayAudio;
constexpr double Run = 450.0; // AVSCharacter MaxWalkSpeed.
constexpr double Dt = 1.0 / 60.0;

int StepsOver(double Seconds, double Speed, bool OnGround, bool Crouched, FootstepCadence& Cadence)
{
    int Steps = 0;
    for (int Frame = 0; Frame < static_cast<int>(Seconds / Dt + 0.5); ++Frame)
    {
        Steps += Cadence.Step(Dt, Speed, Run, OnGround, Crouched) ? 1 : 0;
    }
    return Steps;
}

void RunningIsAudibleWalkingAndCrouchingAreSilent()
{
    FootstepCadence Cadence;
    // 450 cm/s over 140 cm strides: about 3.2 steps per second.
    const int Running = StepsOver(10.0, Run, true, false, Cadence);
    EXPECT(Running >= 31 && Running <= 33);
    FootstepCadence Walk;
    EXPECT(StepsOver(10.0, Run * 0.45, true, false, Walk) == 0);
    FootstepCadence Crouch;
    EXPECT(StepsOver(10.0, 180.0, true, true, Crouch) == 0);
    // Crouch is silent even at a speed that would otherwise be audible.
    FootstepCadence CrouchFast;
    EXPECT(StepsOver(2.0, Run, true, true, CrouchFast) == 0);
    FootstepCadence Air;
    EXPECT(StepsOver(2.0, Run, false, false, Air) == 0);
}

void CadenceFollowsSpeedAndQuietDistanceIsNotBanked()
{
    EXPECT(FootstepInterval(Run * 0.45, Run, false) == 0.0);
    EXPECT(FootstepInterval(Run, Run, true) == 0.0);
    const double Fast = FootstepInterval(Run, Run, false);
    const double Slower = FootstepInterval(Run * 0.7, Run, false);
    EXPECT(Fast > 0.0 && Slower > Fast);
    EXPECT(std::fabs(Fast - 140.0 / Run) < 1e-9);

    FootstepCadence Cadence;
    StepsOver(30.0, Run * 0.45, true, false, Cadence); // A long quiet walk.
    // Switching to a run gives the first step after half a stride, not instantly.
    EXPECT(!Cadence.Step(Dt, Run, Run, true, false));
    int Frames = 1;
    while (!Cadence.Step(Dt, Run, Run, true, false) && Frames < 600) { ++Frames; }
    EXPECT(Frames >= 8 && Frames <= 10);
    // Bad input never steps.
    EXPECT(!Cadence.Step(-1.0, Run, Run, true, false));
    EXPECT(!Cadence.Step(Dt, NAN, Run, true, false));
    EXPECT(!Cadence.Step(Dt, Run, 0.0, true, false));
}

void LandingNeedsARealFall()
{
    LandingDetector Land;
    EXPECT(!Land.Observe(true, -900.0)); // First sight mid-air: baseline.
    EXPECT(!Land.Observe(false, 0.0));   // ...so that touchdown is quiet.
    EXPECT(!Land.Observe(true, -120.0)); // Step-down off a ledge.
    EXPECT(!Land.Observe(false, 0.0));
    EXPECT(!Land.Observe(true, 420.0));  // Jump up...
    EXPECT(!Land.Observe(true, -450.0)); // ...and down.
    EXPECT(Land.Observe(false, 0.0));
    EXPECT(!Land.Observe(false, 0.0));   // Standing does not repeat it.
}

void OwnDamageAndDeathPlayOnce()
{
    LifeWatch Life;
    EXPECT(Life.Observe(60.0).baseline); // Rejoin with damage taken earlier: no replay.
    EXPECT(!Life.Observe(60.0).hurt);
    const LifeDecision Hit = Life.Observe(35.0);
    EXPECT(Hit.hurt && !Hit.death);
    EXPECT(!Life.Observe(35.0).hurt);
    const LifeDecision Kill = Life.Observe(0.0);
    EXPECT(Kill.death && !Kill.hurt);
    EXPECT(!Life.Observe(0.0).death);
    const LifeDecision Respawn = Life.Observe(100.0);
    EXPECT(!Respawn.hurt && !Respawn.death);
    LifeWatch Dead;
    Dead.Observe(0.0);
    EXPECT(!Dead.Observe(0.0).death); // Joining as a corpse.
    LifeWatch Bad;
    EXPECT(Bad.Observe(NAN).baseline);
    EXPECT(Bad.Observe(50.0).baseline);
}

void ObjectiveCuesPlayOnceAndNeverReplay()
{
    constexpr std::uint8_t Carried = 1, Planted = ObjectiveWatch::Planted, Defused = ObjectiveWatch::Defused;
    ObjectiveWatch Watch;
    EXPECT(Watch.Observe(1, Carried, 0).baseline);
    const ObjectiveDecision Plant = Watch.Observe(1, Carried, 11);
    EXPECT(Plant.plantStart && !Plant.defuseStart);
    EXPECT(!Watch.Observe(1, Carried, 11).plantStart); // Same token, repeated snapshot.
    EXPECT(!Watch.Observe(1, Planted, 0).plantStart);
    const ObjectiveDecision Defuse = Watch.Observe(1, Planted, 12);
    EXPECT(Defuse.defuseStart && !Defuse.plantStart);
    const ObjectiveDecision Done = Watch.Observe(1, Defused, 0);
    EXPECT(Done.defuseComplete);
    EXPECT(!Watch.Observe(1, Defused, 0).defuseComplete);

    // Joining mid-defuse or after a defuse replays nothing.
    ObjectiveWatch Late;
    EXPECT(Late.Observe(4, Planted, 30).baseline);
    EXPECT(!Late.Observe(4, Planted, 30).defuseStart);
    ObjectiveWatch After;
    After.Observe(4, Defused, 0);
    EXPECT(!After.Observe(4, Defused, 0).defuseComplete);
    // A new round on the same frame as a stale Planted snapshot is not a defuse.
    ObjectiveWatch Round;
    Round.Observe(5, Planted, 0);
    EXPECT(!Round.Observe(6, Defused, 0).defuseComplete);
}

void BeepSpeedsUpTowardDetonation()
{
    EXPECT(std::fabs(BeepInterval(30.0, 30.0) - 1.0) < 1e-9);
    EXPECT(std::fabs(BeepInterval(0.0, 30.0) - 0.12) < 1e-9);
    double Previous = 2.0;
    for (double Remaining = 30.0; Remaining >= 0.0; Remaining -= 1.0)
    {
        const double Interval = BeepInterval(Remaining, 30.0);
        EXPECT(Interval <= Previous && Interval >= 0.12 && Interval <= 1.0);
        Previous = Interval;
    }
    EXPECT(BeepInterval(-5.0, 30.0) == BeepInterval(0.0, 30.0));
    EXPECT(BeepInterval(10.0, 0.0) == 1.0);

    BeepScheduler Beeps;
    int First10 = 0, Last5 = 0;
    for (double Now = 0.0; Now < 30.0; Now += Dt)
    {
        const bool Beep = Beeps.Observe(true, 100.0 + Now, 130.0);
        if (Now < 10.0 && Beep) { ++First10; }
        if (Now >= 25.0 && Beep) { ++Last5; }
    }
    EXPECT(First10 >= 10 && First10 <= 14);
    EXPECT(Last5 > 20);
    EXPECT(!Beeps.Observe(false, 131.0, 130.0));
    EXPECT(!Beeps.Observe(true, 131.0, 130.0)); // Past the deadline.
    BeepScheduler Idle;
    EXPECT(!Idle.Observe(false, 10.0, 40.0));
}

void GeneratorIsBoundedDeterministicAndAudible()
{
    for (int Index = 0; Index < CueCount; ++Index)
    {
        const Cue Kind = static_cast<Cue>(Index);
        for (int Variant = 0; Variant < Placeholder::VariantCount(Kind); ++Variant)
        {
            for (int Rate : {22050, 44100, 48000})
            {
                std::vector<float> A, B;
                Placeholder::Generate(Kind, Variant, Rate, A);
                Placeholder::Generate(Kind, Variant, Rate, B);
                EXPECT(!A.empty() && A == B);
                // Short: every placeholder is under a second.
                EXPECT(A.size() <= static_cast<std::size_t>(Rate));
                double Peak = 0.0;
                bool Finite = true;
                for (float Sample : A)
                {
                    Finite = Finite && std::isfinite(Sample);
                    Peak = std::fabs(Sample) > Peak ? std::fabs(Sample) : Peak;
                }
                EXPECT(Finite && Peak <= 1.0 && Peak > 0.05);
            }
        }
    }
    std::vector<float> Step0, Step1, Bad;
    Placeholder::Generate(Cue::Footstep, 0, 48000, Step0);
    Placeholder::Generate(Cue::Footstep, 1, 48000, Step1);
    EXPECT(Step0 != Step1);
    Placeholder::Generate(Cue::RifleShot, 0, 100, Bad);
    EXPECT(Bad.empty());
    Placeholder::Generate(Cue::Count, 0, 48000, Bad);
    EXPECT(Bad.empty());
}
}

int main()
{
    RunningIsAudibleWalkingAndCrouchingAreSilent();
    CadenceFollowsSpeedAndQuietDistanceIsNotBanked();
    LandingNeedsARealFall();
    OwnDamageAndDeathPlayOnce();
    ObjectiveCuesPlayOnceAndNeverReplay();
    BeepSpeedsUpTowardDetonation();
    GeneratorIsBoundedDeterministicAndAudible();
    std::printf("%d checks, %d failures\n", Checks, Failures);
    return Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
