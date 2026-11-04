// Standalone checks for the hit-feedback dedup/priority/expiry/respawn rules.
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Feedback/hit_feedback_state_test.cpp -o Saved/Tests/Feedback/hit_feedback_state_test
#include "../../Source/ValStrike/Feedback/VSHitFeedbackState.h"

#include <cstdio>
#include <cstdlib>

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

using namespace VSHitFeedback;

Event Make(std::uint32_t Sequence, std::uint8_t Flags, float Yaw = 0.0f)
{
    Event Result;
    Result.sequence = Sequence;
    Result.flags = Flags;
    Result.damage = 25.0f;
    Result.sourceYawDeg = Yaw;
    Result.weapon = 1;
    return Result;
}

std::size_t LiveIndicators(const State& Feedback, double Now)
{
    Indicator Out[MaxIndicators];
    return Feedback.CurrentIndicators(Now, Out, MaxIndicators);
}

void MarkersExpireByVariant()
{
    State Feedback;
    EXPECT(Feedback.OnHitConfirmed(Make(1, 0), 10.0) == Outcome::Shown);
    EXPECT(Feedback.CurrentMarker(10.0).kind == MarkerKind::Hit);
    EXPECT(Feedback.CurrentMarker(10.0).alpha == 1.0f);
    const float Fading = Feedback.CurrentMarker(10.0 + HitSeconds * 0.8).alpha;
    EXPECT(Fading > 0.0f && Fading < 1.0f);
    EXPECT(Feedback.CurrentMarker(10.0 + HitSeconds).kind == MarkerKind::None);

    EXPECT(Feedback.OnHitConfirmed(Make(2, FlagHeadshot), 20.0) == Outcome::Shown);
    EXPECT(Feedback.CurrentMarker(20.0 + HitSeconds).kind == MarkerKind::Headshot);
    EXPECT(Feedback.CurrentMarker(20.0 + HeadshotSeconds).kind == MarkerKind::None);

    EXPECT(Feedback.OnHitConfirmed(Make(3, FlagKilled | FlagHeadshot), 30.0) == Outcome::Shown);
    EXPECT(Feedback.CurrentMarker(30.0 + HeadshotSeconds).kind == MarkerKind::Kill);
    EXPECT(Feedback.CurrentMarker(30.0 + KillSeconds).kind == MarkerKind::None);
}

void KillAndHeadshotOutrankHits()
{
    State Feedback;
    Feedback.OnHitConfirmed(Make(1, FlagKilled), 0.0);
    // A trailing body hit must not downgrade the kill marker while it shows.
    EXPECT(Feedback.OnHitConfirmed(Make(2, 0), 0.1) == Outcome::Ignored);
    EXPECT(Feedback.CurrentMarker(0.1).kind == MarkerKind::Kill);
    // A headshot replaces a plain hit and restarts its lifetime.
    State Upgrade;
    Upgrade.OnHitConfirmed(Make(1, 0), 0.0);
    EXPECT(Upgrade.OnHitConfirmed(Make(2, FlagHeadshot), 0.2) == Outcome::Shown);
    EXPECT(Upgrade.CurrentMarker(0.2 + HitSeconds).kind == MarkerKind::Headshot);
    // Once the strong marker expired, a plain hit shows again.
    EXPECT(Feedback.OnHitConfirmed(Make(3, 0), KillSeconds + 0.01) == Outcome::Shown);
    EXPECT(Feedback.CurrentMarker(KillSeconds + 0.01).kind == MarkerKind::Hit);
}

void DuplicatesAreNeverReplayed()
{
    State Feedback;
    EXPECT(Feedback.OnHitConfirmed(Make(5, 0), 0.0) == Outcome::Shown);
    EXPECT(Feedback.OnHitConfirmed(Make(5, 0), 1.0) == Outcome::Duplicate);
    EXPECT(Feedback.CurrentMarker(1.0).kind == MarkerKind::None);
    EXPECT(Feedback.OnHitConfirmed(Make(0, 0), 1.0) == Outcome::Stale);
    // A reliable kill retransmitted behind a later unreliable hit shows once.
    EXPECT(Feedback.OnHitConfirmed(Make(7, 0), 2.0) == Outcome::Shown);
    EXPECT(Feedback.OnHitConfirmed(Make(6, FlagKilled), 2.05) == Outcome::Shown);
    EXPECT(Feedback.CurrentMarker(2.05).kind == MarkerKind::Kill);
    EXPECT(Feedback.OnHitConfirmed(Make(6, FlagKilled), 3.0) == Outcome::Duplicate);
    EXPECT(Feedback.CurrentMarker(3.0).kind == MarkerKind::None);
    // Far older than the window is dropped; shooter and victim share one counter.
    EXPECT(Feedback.OnDamageTaken(Make(7 + SequenceWindow, FlagHasSource), 4.0) == Outcome::Shown);
    EXPECT(Feedback.OnHitConfirmed(Make(7, 0), 4.0) == Outcome::Stale);
    EXPECT(Feedback.OnHitConfirmed(Make(8, 0), 4.0) == Outcome::Shown);
    EXPECT(Feedback.OnDamageTaken(Make(7 + SequenceWindow, FlagHasSource), 4.1) == Outcome::Duplicate);
    // A large jump clears the window and keeps working.
    EXPECT(Feedback.OnHitConfirmed(Make(100000, 0), 5.0) == Outcome::Shown);
    EXPECT(Feedback.OnHitConfirmed(Make(99999, 0), 5.0) == Outcome::Shown);
    EXPECT(Feedback.NewestSequence() == 100000);
}

void IndicatorsMergeExpireAndStayBounded()
{
    State Feedback;
    Feedback.OnDamageTaken(Make(1, FlagHasSource, 90.0f), 0.0);
    // Same direction refreshes instead of stacking; the 180/-180 seam counts as near.
    Feedback.OnDamageTaken(Make(2, FlagHasSource, 100.0f), 0.5);
    EXPECT(LiveIndicators(Feedback, 0.5) == 1);
    EXPECT(LiveIndicators(Feedback, IndicatorSeconds + 0.1) == 1);
    EXPECT(LiveIndicators(Feedback, 0.5 + IndicatorSeconds) == 0);
    State Seam;
    Seam.OnDamageTaken(Make(1, FlagHasSource, 175.0f), 0.0);
    Seam.OnDamageTaken(Make(2, FlagHasSource, -175.0f), 0.1);
    EXPECT(LiveIndicators(Seam, 0.1) == 1);
    // Distinct directions stack up to the cap; the oldest is replaced.
    State Many;
    for (std::uint32_t I = 0; I < 6; ++I)
        Many.OnDamageTaken(Make(I + 1, FlagHasSource, -150.0f + 60.0f * static_cast<float>(I)), 0.1 * I);
    Indicator Out[MaxIndicators];
    const std::size_t Count = Many.CurrentIndicators(0.5, Out, MaxIndicators);
    EXPECT(Count == MaxIndicators);
    bool OldestGone = true;
    for (std::size_t I = 0; I < Count; ++I) OldestGone = OldestGone && Out[I].sourceYawDeg != -150.0f;
    EXPECT(OldestGone);
    // No source means no indicator, but the event still counts.
    State NoSource;
    EXPECT(NoSource.OnDamageTaken(Make(1, 0), 0.0) == Outcome::Shown);
    EXPECT(LiveIndicators(NoSource, 0.0) == 0);
    EXPECT(DeltaDegrees(170.0f, -170.0f) == 20.0f);
    EXPECT(DeltaDegrees(0.0f, 180.0f) == -180.0f);
}

void DeathHoldsUntilRespawnWithoutStacking()
{
    State Feedback;
    EXPECT(Feedback.OnDamageTaken(Make(1, FlagHasSource | FlagKilled | FlagHeadshot, 45.0f), 0.0) == Outcome::Shown);
    EXPECT(Feedback.CurrentDeath().eliminated && Feedback.CurrentDeath().headshot);
    EXPECT(Feedback.CurrentDeath().weapon == 1);
    // The death text has no lifetime; only respawn clears it.
    EXPECT(Feedback.CurrentDeath().eliminated);
    // Nothing stacks after the kill within the same life.
    EXPECT(Feedback.OnDamageTaken(Make(2, FlagHasSource | FlagKilled, -90.0f), 0.1) == Outcome::Ignored);
    EXPECT(LiveIndicators(Feedback, 0.1) == 1);
    EXPECT(Feedback.CurrentDeath().headshot);
}

void RespawnClearsAndNeverReplays()
{
    State Feedback;
    Feedback.OnHitConfirmed(Make(1, FlagKilled), 0.0);
    Feedback.OnDamageTaken(Make(2, FlagHasSource | FlagKilled, 30.0f), 0.0);
    Feedback.BeginLife();
    EXPECT(Feedback.CurrentMarker(0.0).kind == MarkerKind::None);
    EXPECT(LiveIndicators(Feedback, 0.0) == 0);
    EXPECT(!Feedback.CurrentDeath().eliminated);
    // Re-delivered events from the previous life stay suppressed after respawn.
    EXPECT(Feedback.OnDamageTaken(Make(2, FlagHasSource | FlagKilled, 30.0f), 0.2) == Outcome::Duplicate);
    EXPECT(Feedback.OnHitConfirmed(Make(1, FlagKilled), 0.2) == Outcome::Duplicate);
    EXPECT(!Feedback.CurrentDeath().eliminated);
    // New events in the new life show normally.
    EXPECT(Feedback.OnHitConfirmed(Make(3, 0), 0.3) == Outcome::Shown);
    EXPECT(Feedback.CurrentMarker(0.3).kind == MarkerKind::Hit);
}
}

int main()
{
    MarkersExpireByVariant();
    KillAndHeadshotOutrankHits();
    DuplicatesAreNeverReplayed();
    IndicatorsMergeExpireAndStayBounded();
    DeathHoldsUntilRespawnWithoutStacking();
    RespawnClearsAndNeverReplays();
    std::printf("%d/%d hit-feedback state checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
