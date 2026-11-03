// Standalone tests for the per-player kill/death/assist ledger.
// Built by Scripts/test_damage.py; no Unreal dependency.
#include "Scoreboard/VSStatsLedger.h"

#include <cstdio>
#include <limits>
#include <vector>

using namespace ValStrike::Stats;

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

// Two per team, all alive: 1 and 2 on team 1, 3 and 4 on team 2.
Ledger TwoVersusTwo()
{
    Ledger Value;
    Value.AddPlayer(1, 1);
    Value.AddPlayer(2, 1);
    Value.AddPlayer(3, 2);
    Value.AddPlayer(4, 2);
    for (PlayerKey Key = 1; Key <= 4; ++Key) { Value.BeginLife(Key); }
    return Value;
}

HitReport Shot(PlayerKey Attacker, PlayerKey Victim, double Damage, bool Killed = false, bool Headshot = false)
{
    HitReport Report;
    Report.Attacker = Attacker;
    Report.Victim = Victim;
    Report.AppliedDamage = Damage;
    Report.Killed = Killed;
    Report.Headshot = Headshot;
    return Report;
}

void RosterRules()
{
    Ledger Value;
    CHECK(Value.AddPlayer(5, 1));
    CHECK(!Value.AddPlayer(5, 2));
    CHECK(!Value.AddPlayer(0, 1) && !Value.AddPlayer(-3, 1));
    CHECK(Value.SetTeam(5, 2) && !Value.SetTeam(5, 2) && !Value.SetTeam(6, 1));
    CHECK(Value.Find(5)->Team == 2 && !Value.Find(5)->Alive);
    CHECK(Value.RemovePlayer(5) && !Value.RemovePlayer(5) && Value.Lines().empty());
    for (PlayerKey Key = 1; Key <= static_cast<PlayerKey>(MaxPlayers); ++Key) { CHECK(Value.AddPlayer(Key, 0)); }
    CHECK(!Value.AddPlayer(1000, 0));
}

void EnemyKillCreditsKillDeathAndHeadshot()
{
    Ledger Value = TwoVersusTwo();
    CHECK(Value.RecordHit(Shot(1, 3, 24)) == HitOutcome::Damage);
    CHECK(Value.RecordHit(Shot(1, 3, 76, true, true)) == HitOutcome::Kill);
    const PlayerLine* Killer = Value.Find(1);
    const PlayerLine* Victim = Value.Find(3);
    CHECK(Killer->Kills == 1 && Killer->HeadshotKills == 1 && Killer->Deaths == 0 && Killer->DamageDealt == 100.0);
    CHECK(Victim->Deaths == 1 && Victim->Kills == 0 && !Victim->Alive);
    CHECK(Value.Find(2)->Kills == 0 && Value.Find(4)->Deaths == 0);
    // A body-zone kill is not a headshot kill.
    CHECK(Value.RecordHit(Shot(2, 4, 100, true, false)) == HitOutcome::Kill);
    CHECK(Value.Find(2)->Kills == 1 && Value.Find(2)->HeadshotKills == 0);
}

void DeathObservedBeforeKillReportIsCountedOnce()
{
    // The health delegate fires inside ApplyPointDamage, before the hook reports the kill.
    Ledger Value = TwoVersusTwo();
    CHECK(Value.RecordDeath(3));
    CHECK(!Value.RecordDeath(3));
    CHECK(Value.RecordHit(Shot(1, 3, 100, true, true)) == HitOutcome::Kill);
    CHECK(Value.Find(3)->Deaths == 1 && Value.Find(1)->Kills == 1);
    // Duplicate kill report for the same life is ignored entirely.
    CHECK(Value.RecordHit(Shot(1, 3, 100, true, true)) == HitOutcome::Ignored);
    CHECK(Value.RecordHit(Shot(4, 3, 50, true)) == HitOutcome::Ignored);
    CHECK(Value.Find(1)->Kills == 1 && Value.Find(1)->DamageDealt == 100.0 && Value.Find(4)->Kills == 0);
    // Later damage to the dead life is stale.
    CHECK(Value.RecordHit(Shot(2, 3, 30)) == HitOutcome::Ignored);
    CHECK(Value.Find(2)->DamageDealt == 0.0);
}

void UnattributedTeamAndSelfDeaths()
{
    Ledger Value = TwoVersusTwo();
    // Health observer only (for example the Phase 1 rifle path or a development reset).
    CHECK(Value.RecordDeath(1));
    CHECK(Value.Find(1)->Deaths == 1);
    for (const PlayerLine& Line : Value.Lines()) { CHECK(Line.Kills == 0); }
    // Unknown attacker.
    CHECK(Value.RecordHit(Shot(0, 2, 100, true)) == HitOutcome::Unattributed);
    CHECK(Value.RecordHit(Shot(99, 3, 100, true)) == HitOutcome::Unattributed);
    // Teammate: death, no credit, no damage dealt.
    CHECK(Value.RecordHit(Shot(3, 4, 100, true, true)) == HitOutcome::TeamKill);
    CHECK(Value.Find(3)->Kills == 0 && Value.Find(3)->HeadshotKills == 0 && Value.Find(3)->DamageDealt == 0.0);
    CHECK(Value.Find(4)->Deaths == 1);
    // Self.
    Value.BeginLife(4);
    CHECK(Value.RecordHit(Shot(4, 4, 100, true)) == HitOutcome::SelfKill);
    CHECK(Value.Find(4)->Deaths == 2 && Value.Find(4)->Kills == 0);
}

void UnassignedPlayersAreEnemies()
{
    Ledger Value;
    Value.AddPlayer(1, 0);
    Value.AddPlayer(2, 0);
    Value.BeginLife(1);
    Value.BeginLife(2);
    CHECK(Value.AreEnemies(1, 2) && !Value.AreEnemies(1, 1) && !Value.AreEnemies(1, 9));
    CHECK(Value.RecordHit(Shot(1, 2, 100, true, true)) == HitOutcome::Kill);
    CHECK(Value.Find(1)->Kills == 1);
}

void AssistsNeedThresholdAndEnemy()
{
    Ledger Value = TwoVersusTwo();
    CHECK(Value.RecordHit(Shot(1, 3, 39)) == HitOutcome::Damage); // below the 40 proposal
    CHECK(Value.RecordHit(Shot(2, 3, 20)) == HitOutcome::Damage);
    CHECK(Value.RecordHit(Shot(2, 3, 20)) == HitOutcome::Damage); // merges to 40
    CHECK(Value.RecordHit(Shot(4, 3, 50)) == HitOutcome::Damage); // teammate damage: never credited
    CHECK(Value.RecordHit(Shot(1, 3, 21, true)) == HitOutcome::Kill);
    CHECK(Value.Find(1)->Assists == 0 && Value.Find(1)->Kills == 1);
    CHECK(Value.Find(2)->Assists == 1 && Value.Find(2)->Kills == 0);
    CHECK(Value.Find(4)->Assists == 0 && Value.Find(4)->DamageDealt == 0.0);
    CHECK(Value.Find(2)->DamageDealt == 40.0 && Value.Find(1)->DamageDealt == 60.0);
}

void NewLifeClearsContributions()
{
    Ledger Value = TwoVersusTwo();
    CHECK(Value.RecordHit(Shot(2, 3, 60)) == HitOutcome::Damage);
    // Victim survives the round; the next round's fresh pawn starts clean.
    CHECK(Value.BeginLife(3));
    CHECK(Value.RecordHit(Shot(1, 3, 100, true)) == HitOutcome::Kill);
    CHECK(Value.Find(2)->Assists == 0);
    // Unattributed death keeps no stale contributions into the next life.
    Value.BeginLife(3);
    CHECK(Value.RecordHit(Shot(2, 3, 60)) == HitOutcome::Damage);
    CHECK(Value.RecordDeath(3));
    Value.BeginLife(3);
    CHECK(Value.RecordHit(Shot(1, 3, 100, true)) == HitOutcome::Kill);
    CHECK(Value.Find(2)->Assists == 0 && Value.Find(3)->Deaths == 3);
}

void InvalidReportsAreIgnored()
{
    Ledger Value = TwoVersusTwo();
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    CHECK(Value.RecordHit(Shot(1, 3, NaN, true)) == HitOutcome::Ignored);
    CHECK(Value.RecordHit(Shot(1, 3, std::numeric_limits<double>::infinity())) == HitOutcome::Ignored);
    CHECK(Value.RecordHit(Shot(1, 3, 0.0, true)) == HitOutcome::Ignored);
    CHECK(Value.RecordHit(Shot(1, 3, -5.0)) == HitOutcome::Ignored);
    CHECK(Value.RecordHit(Shot(1, 42, 30.0, true)) == HitOutcome::Ignored);
    CHECK(Value.Find(3)->Alive && Value.Find(3)->Deaths == 0 && Value.Find(1)->DamageDealt == 0.0);
    CHECK(!Value.RecordDeath(42) && !Value.BeginLife(42));
    // A non-finite assist threshold falls back to the 40-point proposal.
    Ledger Odd(NaN);
    Odd.AddPlayer(1, 1); Odd.AddPlayer(2, 2); Odd.AddPlayer(3, 2);
    for (PlayerKey Key = 1; Key <= 3; ++Key) { Odd.BeginLife(Key); }
    CHECK(Odd.RecordHit(Shot(2, 1, 39)) == HitOutcome::Damage);
    CHECK(Odd.RecordHit(Shot(3, 1, 61, true)) == HitOutcome::Kill);
    CHECK(Odd.Find(2)->Assists == 0 && Odd.Find(2)->DamageDealt == 39.0);
}

void RemovedPlayersStopContributing()
{
    Ledger Value = TwoVersusTwo();
    Value.RecordHit(Shot(2, 3, 60));
    CHECK(Value.RemovePlayer(2));
    CHECK(Value.RecordHit(Shot(1, 3, 40, true)) == HitOutcome::Kill);
    CHECK(Value.Find(1)->Assists == 0 && Value.Find(2) == nullptr);
    CHECK(Value.RecordHit(Shot(2, 4, 100, true)) == HitOutcome::Unattributed);
}

void ResetKeepsRosterAndLife()
{
    Ledger Value = TwoVersusTwo();
    Value.RecordHit(Shot(1, 3, 100, true, true));
    Value.RecordHit(Shot(4, 2, 50));
    Value.ResetStats();
    for (const PlayerLine& Line : Value.Lines())
    {
        CHECK(Line.Kills == 0 && Line.Deaths == 0 && Line.Assists == 0 && Line.HeadshotKills == 0 && Line.DamageDealt == 0.0);
    }
    CHECK(Value.Lines().size() == 4 && !Value.Find(3)->Alive && Value.Find(2)->Alive);
    // Contributions from before the reset never become assists.
    CHECK(Value.RecordHit(Shot(3, 2, 50, true)) == HitOutcome::Kill);
    CHECK(Value.Find(4)->Assists == 0);
}

void ScoreboardOrderIsTotalAndStable()
{
    Ledger Value;
    Value.AddPlayer(7, 2);
    Value.AddPlayer(3, 0);
    Value.AddPlayer(5, 1);
    Value.AddPlayer(2, 1);
    Value.AddPlayer(9, 2);
    for (PlayerKey Key : {7, 3, 5, 2, 9}) { Value.BeginLife(Key); }
    Value.RecordHit(Shot(5, 7, 100, true)); // 5: 1 kill
    Value.BeginLife(7);
    Value.RecordHit(Shot(9, 2, 30));        // 9: 30 damage
    std::vector<PlayerKey> Order = Value.ScoreboardOrder();
    // Team 1 (5 has a kill, 2 none), team 2 (9 no deaths before 7 with one death), unassigned last.
    CHECK((Order == std::vector<PlayerKey>{5, 2, 9, 7, 3}));
    // Equal stats fall back to the key.
    Ledger Tied;
    Tied.AddPlayer(8, 1);
    Tied.AddPlayer(4, 1);
    CHECK((Tied.ScoreboardOrder() == std::vector<PlayerKey>{4, 8}));
    CHECK(Ledger().ScoreboardOrder().empty());
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
        {"roster rules", RosterRules},
        {"enemy kill credits kill, death and headshot", EnemyKillCreditsKillDeathAndHeadshot},
        {"death observed before kill report counted once", DeathObservedBeforeKillReportIsCountedOnce},
        {"unattributed, team and self deaths", UnattributedTeamAndSelfDeaths},
        {"unassigned players are enemies", UnassignedPlayersAreEnemies},
        {"assists need threshold and enemy", AssistsNeedThresholdAndEnemy},
        {"new life clears contributions", NewLifeClearsContributions},
        {"invalid reports ignored", InvalidReportsAreIgnored},
        {"removed players stop contributing", RemovedPlayersStopContributing},
        {"reset keeps roster and life", ResetKeepsRosterAndLife},
        {"scoreboard order total and stable", ScoreboardOrderIsTotalAndStable},
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
    std::printf("Stats ledger: %d/%d cases passed, %d checks, %d failures\n", Passed, Total, Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
