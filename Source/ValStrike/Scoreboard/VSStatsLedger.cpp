#include "VSStatsLedger.h"

#include <algorithm>
#include <cmath>

namespace ValStrike::Stats
{
namespace
{
int VSStatsLedgerTeamRank(std::uint8_t Team) { return Team == 1 ? 0 : Team == 2 ? 1 : 2; }
}

Ledger::Ledger(double AssistDamage)
    : AssistThreshold(std::isfinite(AssistDamage) && AssistDamage > 0.0 ? AssistDamage : DefaultAssistDamage)
{
}

PlayerLine* Ledger::FindMutable(PlayerKey Key)
{
    for (PlayerLine& Line : Players)
    {
        if (Line.Key == Key) { return &Line; }
    }
    return nullptr;
}

const PlayerLine* Ledger::Find(PlayerKey Key) const
{
    for (const PlayerLine& Line : Players)
    {
        if (Line.Key == Key) { return &Line; }
    }
    return nullptr;
}

bool Ledger::AddPlayer(PlayerKey Key, std::uint8_t Team)
{
    if (Key <= 0 || Find(Key) || Players.size() >= MaxPlayers) { return false; }
    PlayerLine Line;
    Line.Key = Key;
    Line.Team = Team;
    Players.push_back(Line);
    return true;
}

bool Ledger::RemovePlayer(PlayerKey Key)
{
    const auto It = std::find_if(Players.begin(), Players.end(), [Key](const PlayerLine& Line) { return Line.Key == Key; });
    if (It == Players.end()) { return false; }
    Players.erase(It);
    ForgetVictim(Key);
    Received.erase(std::remove_if(Received.begin(), Received.end(),
        [Key](const Contribution& Item) { return Item.Attacker == Key; }), Received.end());
    return true;
}

bool Ledger::SetTeam(PlayerKey Key, std::uint8_t Team)
{
    PlayerLine* Line = FindMutable(Key);
    if (!Line || Line->Team == Team) { return false; }
    Line->Team = Team;
    return true;
}

bool Ledger::BeginLife(PlayerKey Key)
{
    PlayerLine* Line = FindMutable(Key);
    if (!Line) { return false; }
    ++Line->Life;
    Line->Alive = true;
    Line->KillResolved = false;
    ForgetVictim(Key);
    return true;
}

bool Ledger::RecordDeath(PlayerKey Key)
{
    PlayerLine* Line = FindMutable(Key);
    if (!Line || !Line->Alive) { return false; }
    Line->Alive = false;
    ++Line->Deaths;
    return true;
}

bool Ledger::AreEnemies(PlayerKey A, PlayerKey B) const
{
    const PlayerLine* X = Find(A);
    const PlayerLine* Y = Find(B);
    if (!X || !Y || A == B) { return false; }
    return X->Team == 0 || Y->Team == 0 || X->Team != Y->Team;
}

void Ledger::ForgetVictim(PlayerKey Victim)
{
    Received.erase(std::remove_if(Received.begin(), Received.end(),
        [Victim](const Contribution& Item) { return Item.Victim == Victim; }), Received.end());
}

HitOutcome Ledger::RecordHit(const HitReport& Report)
{
    PlayerLine* Victim = FindMutable(Report.Victim);
    if (!Victim || !std::isfinite(Report.AppliedDamage) || Report.AppliedDamage <= 0.0) { return HitOutcome::Ignored; }
    // A kill report for a life whose credit was already decided is a duplicate.
    if (Report.Killed && Victim->KillResolved) { return HitOutcome::Ignored; }
    // Damage to an already dead life (a stale report) changes nothing.
    if (!Report.Killed && !Victim->Alive) { return HitOutcome::Ignored; }

    const bool Enemy = AreEnemies(Report.Attacker, Report.Victim);
    PlayerLine* Attacker = Enemy ? FindMutable(Report.Attacker) : nullptr;
    if (Attacker)
    {
        Attacker->DamageDealt += Report.AppliedDamage;
        bool Merged = false;
        for (Contribution& Item : Received)
        {
            if (Item.Attacker == Report.Attacker && Item.Victim == Report.Victim && Item.VictimLife == Victim->Life)
            {
                Item.Damage += Report.AppliedDamage;
                Merged = true;
            }
        }
        if (!Merged) { Received.push_back({Report.Attacker, Report.Victim, Victim->Life, Report.AppliedDamage}); }
    }
    if (!Report.Killed) { return HitOutcome::Damage; }

    // The death may already be counted by the health observer; never twice.
    RecordDeath(Report.Victim);
    Victim->KillResolved = true;
    HitOutcome Outcome = HitOutcome::Unattributed;
    if (Attacker)
    {
        ++Attacker->Kills;
        if (Report.Headshot) { ++Attacker->HeadshotKills; }
        for (const Contribution& Item : Received)
        {
            if (Item.Victim != Report.Victim || Item.VictimLife != Victim->Life || Item.Attacker == Report.Attacker ||
                Item.Damage < AssistThreshold || !AreEnemies(Item.Attacker, Report.Victim))
            {
                continue;
            }
            if (PlayerLine* Assister = FindMutable(Item.Attacker)) { ++Assister->Assists; }
        }
        Outcome = HitOutcome::Kill;
    }
    else if (Report.Attacker == Report.Victim)
    {
        Outcome = HitOutcome::SelfKill;
    }
    else if (Find(Report.Attacker))
    {
        Outcome = HitOutcome::TeamKill;
    }
    ForgetVictim(Report.Victim);
    return Outcome;
}

void Ledger::ResetStats()
{
    for (PlayerLine& Line : Players)
    {
        Line.Kills = Line.Deaths = Line.Assists = Line.HeadshotKills = 0;
        Line.DamageDealt = 0.0;
    }
    Received.clear();
}

std::vector<PlayerKey> Ledger::ScoreboardOrder() const
{
    std::vector<const PlayerLine*> Sorted;
    Sorted.reserve(Players.size());
    for (const PlayerLine& Line : Players) { Sorted.push_back(&Line); }
    std::sort(Sorted.begin(), Sorted.end(), [](const PlayerLine* A, const PlayerLine* B)
    {
        const int RankA = VSStatsLedgerTeamRank(A->Team);
        const int RankB = VSStatsLedgerTeamRank(B->Team);
        if (RankA != RankB) { return RankA < RankB; }
        if (A->Kills != B->Kills) { return A->Kills > B->Kills; }
        if (A->Deaths != B->Deaths) { return A->Deaths < B->Deaths; }
        if (A->Assists != B->Assists) { return A->Assists > B->Assists; }
        if (A->DamageDealt != B->DamageDealt) { return A->DamageDealt > B->DamageDealt; }
        return A->Key < B->Key;
    });
    std::vector<PlayerKey> Order;
    Order.reserve(Sorted.size());
    for (const PlayerLine* Line : Sorted) { Order.push_back(Line->Key); }
    return Order;
}
}
