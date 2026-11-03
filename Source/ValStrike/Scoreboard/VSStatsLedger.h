#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Engine-independent per-player match statistics. The server feeds it observed
// lives, deaths and attributed hits; clients only ever see the replicated copy.
namespace ValStrike::Stats
{
using PlayerKey = std::int32_t; // Positive server-issued identity (APlayerState player id).
constexpr std::size_t MaxPlayers = 32; // Sanity bound well above the 2v2 roster.
// Proposal: an enemy who removed at least this much health from a victim's
// current life, without the kill, earns an assist when that life is killed.
constexpr double DefaultAssistDamage = 40.0;

struct PlayerLine
{
    PlayerKey Key = 0;
    std::uint8_t Team = 0; // 0 = unassigned (practice mode); never a teammate of anyone.
    std::int32_t Kills = 0;
    std::int32_t Deaths = 0;
    std::int32_t Assists = 0;
    std::int32_t HeadshotKills = 0;
    double DamageDealt = 0.0; // Health actually removed from enemies.
    std::uint32_t Life = 0;   // Increments on every BeginLife.
    bool Alive = false;
    bool KillResolved = false; // The current life's death has had its kill credit decided.
};

struct HitReport
{
    PlayerKey Attacker = 0; // 0 when the source has no player.
    PlayerKey Victim = 0;
    double AppliedDamage = 0.0; // Health actually removed by this hit.
    bool Headshot = false;
    bool Killed = false; // This hit ended the victim's life.
};

enum class HitOutcome : std::uint8_t
{
    Ignored,       // Unknown victim, invalid amount, or a repeated kill report.
    Damage,        // Recorded without ending the life.
    Kill,          // Enemy kill credited.
    TeamKill,      // Teammate kill: death counted, no credit.
    SelfKill,      // Death counted, no credit.
    Unattributed,  // Death counted, attacker unknown.
};

class Ledger
{
public:
    explicit Ledger(double AssistDamage = DefaultAssistDamage);

    bool AddPlayer(PlayerKey Key, std::uint8_t Team);
    bool RemovePlayer(PlayerKey Key);
    bool SetTeam(PlayerKey Key, std::uint8_t Team);
    // A fresh pawn starts a new life: alive, with no damage taken yet.
    bool BeginLife(PlayerKey Key);
    // Counts at most one death per life. Returns true when this call counted it.
    bool RecordDeath(PlayerKey Key);
    HitOutcome RecordHit(const HitReport& Report);
    // New match: zero every counter, keep the roster and current life state.
    void ResetStats();

    const PlayerLine* Find(PlayerKey Key) const;
    const std::vector<PlayerLine>& Lines() const { return Players; }
    bool AreEnemies(PlayerKey A, PlayerKey B) const;
    // Team 1, team 2, then unassigned; kills desc, deaths asc, assists desc,
    // damage desc, key asc. Total order, so every client lists the same rows.
    std::vector<PlayerKey> ScoreboardOrder() const;

private:
    struct Contribution
    {
        PlayerKey Attacker = 0;
        PlayerKey Victim = 0;
        std::uint32_t VictimLife = 0;
        double Damage = 0.0;
    };

    PlayerLine* FindMutable(PlayerKey Key);
    void ForgetVictim(PlayerKey Victim);

    std::vector<PlayerLine> Players;
    std::vector<Contribution> Received;
    double AssistThreshold = DefaultAssistDamage;
};
}
