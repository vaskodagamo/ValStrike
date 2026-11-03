// Standalone checks for the shot-effects replay/coalescing/pool rules.
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Effects/shot_effects_state_test.cpp -o Saved/Tests/Effects/shot_effects_state_test
#include "../../Source/ValStrike/Effects/VSShotEffectsState.h"

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

using VSShotEffects::Decision;
using VSShotEffects::ShotCounter;
constexpr double Interval = 0.12;

void FirstSightNeverReplays()
{
    ShotCounter Counter;
    const Decision First = Counter.Observe(57, 10.0, Interval);
    EXPECT(First.baseline && !First.spawn && First.shots == 0);
    EXPECT(Counter.IsInitialized() && Counter.Last() == 57);
    const Decision Same = Counter.Observe(57, 10.1, Interval);
    EXPECT(!Same.spawn && !Same.baseline);
}

void EachSpacedShotSpawnsOnce()
{
    ShotCounter Counter;
    Counter.Observe(0, 0.0, Interval);
    int Spawns = 0;
    for (unsigned Shot = 1; Shot <= 10; ++Shot)
    {
        const Decision Result = Counter.Observe(Shot, Shot * 0.5, Interval);
        EXPECT(Result.spawn && Result.shots == 1 && !Result.baseline);
        // Duplicate snapshots of the same counter are quiet.
        EXPECT(!Counter.Observe(Shot, Shot * 0.5 + 0.016, Interval).spawn);
        Spawns += Result.spawn ? 1 : 0;
    }
    EXPECT(Spawns == 10);
}

void BatchesCoalesceIntoOneEffect()
{
    ShotCounter Counter;
    Counter.Observe(4, 1.0, Interval);
    // Three shots fired over ~0.36 s arrive in one replication update.
    const Decision Batch = Counter.Observe(7, 1.4, Interval);
    EXPECT(Batch.spawn && Batch.shots == 3 && !Batch.baseline);
    // Jitter: two shots arrive almost together after a single fire interval.
    const Decision Jitter = Counter.Observe(9, 1.55, Interval);
    EXPECT(Jitter.spawn && Jitter.shots == 2);
    // A long client hitch delivers a full magazine at once; it is still one effect.
    const Decision Hitch = Counter.Observe(39, 1.55 + 30 * Interval, Interval);
    EXPECT(Hitch.spawn && Hitch.shots == 30);
}

void StaleHistoryBecomesBaseline()
{
    // A default-valued first snapshot followed by the real counter one frame later.
    ShotCounter Counter;
    Counter.Observe(0, 5.0, Interval);
    const Decision Late = Counter.Observe(57, 5.016, Interval);
    EXPECT(Late.baseline && !Late.spawn);
    const Decision Next = Counter.Observe(58, 5.5, Interval);
    EXPECT(Next.spawn && Next.shots == 1);
    // Just inside the jitter allowance still spawns; just outside adopts.
    ShotCounter Edge;
    Edge.Observe(0, 0.0, Interval);
    EXPECT(Edge.Observe(ShotCounter::BatchSlack, 0.001, Interval).spawn);
    ShotCounter Over;
    Over.Observe(0, 0.0, Interval);
    EXPECT(Over.Observe(ShotCounter::BatchSlack + 1, 0.001, Interval).baseline);
}

void ResetAndWrapAdoptQuietly()
{
    ShotCounter Counter;
    Counter.Observe(20, 0.0, Interval);
    const Decision Reset = Counter.Observe(0, 1.0, Interval);
    EXPECT(Reset.baseline && !Reset.spawn && Counter.Last() == 0);
    EXPECT(Counter.Observe(1, 2.0, Interval).spawn);
    ShotCounter Wrap;
    Wrap.Observe(0xFFFFFFFFu, 0.0, Interval);
    const Decision Wrapped = Wrap.Observe(2u, 1.0, Interval);
    EXPECT(Wrapped.baseline && !Wrapped.spawn);
}

void InvalidIntervalsAndClockStayBounded()
{
    ShotCounter Counter;
    Counter.Observe(0, 10.0, 0.0);
    // A zero/negative interval falls back to the server's minimum; time going
    // backwards never grants extra allowance.
    EXPECT(Counter.Observe(2, 9.0, -1.0).spawn);
    EXPECT(Counter.Observe(100, 9.0, 0.0).baseline);
    ShotCounter Long;
    Long.Observe(0, 0.0, Interval);
    EXPECT(Long.Observe(1000, 1.0e9, Interval).spawn);
}

void MarkOpacityHoldsThenFades()
{
    using VSShotEffects::MarkOpacity;
    EXPECT(MarkOpacity(0.0, 2.5, 1.5) == 1.0f);
    EXPECT(MarkOpacity(2.5, 2.5, 1.5) == 1.0f);
    const float Mid = MarkOpacity(3.25, 2.5, 1.5);
    EXPECT(Mid > 0.49f && Mid < 0.51f);
    EXPECT(MarkOpacity(4.0, 2.5, 1.5) == 0.0f);
    EXPECT(MarkOpacity(9.0, 2.5, 1.5) == 0.0f);
    EXPECT(MarkOpacity(0.3, 0.12, 0.0) == 0.0f);
    EXPECT(MarkOpacity(-1.0, 2.5, 1.5) == 1.0f);
}

void PoolReusesFreeSlotsThenRecyclesOldest()
{
    std::vector<bool> Active;
    std::vector<double> Spawned;
    const int Cap = 4;
    int Recycled = 0;
    for (int Shot = 0; Shot < 10; ++Shot)
    {
        const auto Choice = VSShotEffects::ChooseSlot(static_cast<int>(Active.size()), Cap,
            [&](int I) { return static_cast<bool>(Active[I]); }, [&](int I) { return Spawned[I]; });
        EXPECT(Choice.index >= 0 && Choice.index < Cap);
        if (Choice.grow) { Active.push_back(false); Spawned.push_back(0.0); }
        Recycled += Choice.recycled ? 1 : 0;
        Active[Choice.index] = true;
        Spawned[Choice.index] = Shot;
        EXPECT(static_cast<int>(Active.size()) <= Cap);
    }
    EXPECT(Recycled == 6);
    // The oldest (shot 6) was in slot 2 after round-robin recycling.
    const auto Next = VSShotEffects::ChooseSlot(Cap, Cap, [&](int I) { return static_cast<bool>(Active[I]); },
        [&](int I) { return Spawned[I]; });
    EXPECT(Next.recycled && Spawned[Next.index] == 6.0);
    // A free slot is preferred to growing or recycling.
    Active[3] = false;
    const auto Free = VSShotEffects::ChooseSlot(Cap, Cap, [&](int I) { return static_cast<bool>(Active[I]); },
        [&](int I) { return Spawned[I]; });
    EXPECT(Free.index == 3 && !Free.grow && !Free.recycled);
    const auto Empty = VSShotEffects::ChooseSlot(0, 0, [&](int) { return true; }, [&](int) { return 0.0; });
    EXPECT(Empty.index == -1);
}
}

int main()
{
    FirstSightNeverReplays();
    EachSpacedShotSpawnsOnce();
    BatchesCoalesceIntoOneEffect();
    StaleHistoryBecomesBaseline();
    ResetAndWrapAdoptQuietly();
    InvalidIntervalsAndClockStayBounded();
    MarkOpacityHoldsThenFades();
    PoolReusesFreeSlotsThenRecyclesOldest();
    std::printf("%d/%d shot-effects state checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
