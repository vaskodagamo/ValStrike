// Standalone tests for the kill feed ring and client row selection.
// No Unreal dependency. Build from the repository root, for example:
//   clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -ISource/ValStrike \
//     Tests/Scoreboard/VSKillFeedModelTests.cpp Source/ValStrike/Scoreboard/VSStatsLedger.cpp -o kill_feed_tests && ./kill_feed_tests
#include "Scoreboard/VSKillFeedModel.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using namespace ValStrike;
using namespace ValStrike::KillFeed;

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

std::vector<Stamp> Stamps(const Ring<int>& Feed)
{
    std::vector<Stamp> Out;
    for (const Ring<int>::Item& Item : Feed.Items()) { Out.push_back({Item.Number, Item.TimeSeconds}); }
    return Out;
}

void AppendAssignsMonotonicSequence()
{
    CurrentTest = "AppendAssignsMonotonicSequence";
    Ring<int> Feed;
    CHECK(Feed.LastSeq() == 0 && Feed.Items().empty());
    CHECK(Feed.Append(1.0, 10) == 1);
    CHECK(Feed.Append(2.0, 20) == 2);
    REQUIRE(Feed.Items().size() == 2);
    CHECK(Feed.Items()[0].Number == 1 && Feed.Items()[0].Data == 10 && Feed.Items()[0].TimeSeconds == 1.0);
    CHECK(Feed.Items()[1].Number == 2 && Feed.Items()[1].Data == 20);
    // A non-finite clock never reaches the wire.
    Feed.Append(std::numeric_limits<double>::quiet_NaN(), 30);
    CHECK(Feed.Items().back().TimeSeconds == 0.0);
}

void RingKeepsNewestFive()
{
    CurrentTest = "RingKeepsNewestFive";
    Ring<int> Feed;
    for (int Index = 1; Index <= 8; ++Index) { Feed.Append(Index, Index); }
    REQUIRE(Feed.Items().size() == MaxEntries);
    CHECK(Feed.Items().front().Number == 4 && Feed.Items().back().Number == 8);
    CHECK(Feed.LastSeq() == 8);
    Ring<int> Tiny(0); // A zero cap is clamped to one row.
    Tiny.Append(0.0, 1);
    Tiny.Append(0.0, 2);
    CHECK(Tiny.Items().size() == 1 && Tiny.Items()[0].Data == 2);
}

void ResetClearsButSequenceContinues()
{
    CurrentTest = "ResetClearsButSequenceContinues";
    Ring<int> Feed;
    Feed.Append(1.0, 1);
    Feed.Append(1.0, 2);
    CHECK(Feed.Clear());
    CHECK(Feed.Items().empty());
    CHECK(!Feed.Clear()); // Already empty: nothing to publish.
    CHECK(Feed.Append(2.0, 3) == 3);
    CHECK(Select(Stamps(Feed), 2.0).size() == 1);
}

void SelectNewestFirstAndCapped()
{
    CurrentTest = "SelectNewestFirstAndCapped";
    const std::vector<Stamp> Input = {{3, 10.0}, {1, 10.0}, {7, 10.0}, {5, 10.0}, {2, 10.0}, {6, 10.0}, {4, 10.0}};
    const std::vector<Visible> Rows = Select(Input, 10.0);
    REQUIRE(Rows.size() == MaxEntries);
    CHECK(Input[Rows[0].Index].Number == 7);
    CHECK(Input[Rows[4].Index].Number == 3);
    CHECK(Rows[0].Alpha == 1.0f);
    CHECK(Select(Input, 10.0, DefaultLifetime, DefaultFade, 2).size() == 2);
}

void SelectDedupesSequence()
{
    CurrentTest = "SelectDedupesSequence";
    // A repeated or reordered delivery of the same sequence draws one row.
    const std::vector<Stamp> Input = {{2, 1.0}, {1, 1.0}, {2, 1.0}, {0, 1.0}};
    const std::vector<Visible> Rows = Select(Input, 1.5);
    REQUIRE(Rows.size() == 2);
    CHECK(Input[Rows[0].Index].Number == 2 && Rows[0].Index == 0);
    CHECK(Input[Rows[1].Index].Number == 1);
}

void SelectExpiresAndFades()
{
    CurrentTest = "SelectExpiresAndFades";
    const std::vector<Stamp> Input = {{1, 0.0}, {2, 3.0}};
    CHECK(Select(Input, 4.0).size() == 2);
    const std::vector<Visible> Late = Select(Input, 4.75);
    REQUIRE(Late.size() == 2);
    CHECK(Late[0].Alpha == 1.0f);                                   // Seq 2, age 1.75 s.
    CHECK(Late[1].Alpha > 0.49f && Late[1].Alpha < 0.51f);          // Seq 1, 0.25 s left of 0.5 s fade.
    const std::vector<Visible> Expired = Select(Input, 5.0);
    REQUIRE(Expired.size() == 1);                                   // Seq 1 reached exactly 5 s.
    CHECK(Input[Expired[0].Index].Number == 2);
    CHECK(Select(Input, 8.0).empty());
    // Late joiner: old ring contents are not replayed.
    CHECK(Select(Input, 100.0).empty());
    // Server clock slightly ahead of the client estimate: age 0, fully shown.
    const std::vector<Visible> Ahead = Select({{9, 12.0}}, 11.0);
    REQUIRE(Ahead.size() == 1);
    CHECK(Ahead[0].Alpha == 1.0f);
}

void SelectRejectsBadInput()
{
    CurrentTest = "SelectRejectsBadInput";
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    CHECK(Select({{1, 0.0}}, NaN).empty());
    CHECK(Select({{1, NaN}}, 0.0).empty());
    CHECK(Select({{1, 0.0}}, 0.0, 0.0).empty());
    CHECK(Select({{1, 0.0}}, 0.0, NaN).empty());
    CHECK(Select({{1, 0.0}}, 0.0, DefaultLifetime, NaN).size() == 1);
    CHECK(Select({}, 0.0).empty());
}

void OutcomesMapToOneEntryKind()
{
    CurrentTest = "OutcomesMapToOneEntryKind";
    Kind Out = Kind::World;
    CHECK(KindFromOutcome(Stats::HitOutcome::Kill, Out) && Out == Kind::Kill);
    CHECK(KindFromOutcome(Stats::HitOutcome::TeamKill, Out) && Out == Kind::TeamKill);
    CHECK(KindFromOutcome(Stats::HitOutcome::SelfKill, Out) && Out == Kind::SelfKill);
    CHECK(KindFromOutcome(Stats::HitOutcome::Unattributed, Out) && Out == Kind::World);
    CHECK(!KindFromOutcome(Stats::HitOutcome::Damage, Out));
    CHECK(!KindFromOutcome(Stats::HitOutcome::Ignored, Out));
}

void LedgerResolvesExactlyOneEntryPerDeath()
{
    CurrentTest = "LedgerResolvesExactlyOneEntryPerDeath";
    // The subsystem appends only on a resolving outcome; a repeated kill report is ignored.
    Stats::Ledger Ledger;
    Ledger.AddPlayer(1, 1);
    Ledger.AddPlayer(2, 2);
    Ledger.BeginLife(1);
    Ledger.BeginLife(2);
    Ring<int> Feed;
    Stats::HitReport Report;
    Report.Attacker = 1;
    Report.Victim = 2;
    Report.AppliedDamage = 100.0;
    Report.Killed = true;
    Ledger.RecordDeath(2); // The health observer can count the death first.
    for (int Repeat = 0; Repeat < 3; ++Repeat)
    {
        Kind Out = Kind::World;
        if (KindFromOutcome(Ledger.RecordHit(Report), Out)) { Feed.Append(1.0, static_cast<int>(Out)); }
    }
    REQUIRE(Feed.Items().size() == 1);
    CHECK(Feed.Items()[0].Data == static_cast<int>(Kind::Kill));
}

void LabelsAndHighlight()
{
    CurrentTest = "LabelsAndHighlight";
    CHECK(std::strcmp(WeaponLabel(Weapon::Rifle), "RIFLE") == 0);
    CHECK(std::strcmp(WeaponLabel(Weapon::Pistol), "PISTOL") == 0);
    CHECK(std::strcmp(WeaponLabel(Weapon::Unknown), "") == 0);
    CHECK(std::strcmp(KindLabel(Kind::Kill), "") == 0);
    CHECK(std::strcmp(KindLabel(Kind::TeamKill), "TEAMKILL") == 0);
    CHECK(std::strcmp(KindLabel(Kind::SelfKill), "SELF") == 0);
    CHECK(std::strcmp(KindLabel(Kind::World), "DIED") == 0);
    CHECK(InvolvesPlayer(1, 2, 1) && InvolvesPlayer(1, 2, 2));
    CHECK(!InvolvesPlayer(1, 2, 3));
    CHECK(!InvolvesPlayer(0, 2, 0)); // Unknown local key highlights nothing.
}
}

int main()
{
    AppendAssignsMonotonicSequence();
    RingKeepsNewestFive();
    ResetClearsButSequenceContinues();
    SelectNewestFirstAndCapped();
    SelectDedupesSequence();
    SelectExpiresAndFades();
    SelectRejectsBadInput();
    OutcomesMapToOneEntryKind();
    LedgerResolvesExactlyOneEntryPerDeath();
    LabelsAndHighlight();
    std::printf("%d checks, %d failures\n", Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
