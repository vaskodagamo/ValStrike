#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "VSStatsLedger.h"

// Engine-independent kill feed. The server appends one entry when the stats
// ledger resolves a death; clients only choose which replicated entries to show.
namespace ValStrike::KillFeed
{
using Seq = std::uint32_t;                 // Monotonic for the whole server session; 0 = none.
constexpr std::size_t MaxEntries = 5;      // Replicated ring size and on-screen row count.
constexpr double DefaultLifetime = 5.0;    // Seconds an entry stays on screen.
constexpr double DefaultFade = 0.5;        // Final seconds of the lifetime spent fading out.

enum class Kind : std::uint8_t
{
    Kill,     // Enemy kill credited by the ledger.
    TeamKill, // Teammate kill: death counted, no credit.
    SelfKill, // Own damage.
    World,    // Attacker unknown to the ledger (no player source).
};

enum class Weapon : std::uint8_t { Rifle, Pistol, Unknown };

// Whether a ledger hit outcome is a resolved death, and which feed kind it is.
// Ignored and plain damage outcomes never produce an entry.
inline bool KindFromOutcome(Stats::HitOutcome Outcome, Kind& Out)
{
    switch (Outcome)
    {
    case Stats::HitOutcome::Kill: Out = Kind::Kill; return true;
    case Stats::HitOutcome::TeamKill: Out = Kind::TeamKill; return true;
    case Stats::HitOutcome::SelfKill: Out = Kind::SelfKill; return true;
    case Stats::HitOutcome::Unattributed: Out = Kind::World; return true;
    default: return false;
    }
}

// Plain text in place of icons (an icon slot for art can replace it later).
inline const char* WeaponLabel(Weapon Value)
{
    switch (Value)
    {
    case Weapon::Rifle: return "RIFLE";
    case Weapon::Pistol: return "PISTOL";
    default: return "";
    }
}

// Kind tag shown before the weapon text; empty for an ordinary kill.
inline const char* KindLabel(Kind Value)
{
    switch (Value)
    {
    case Kind::TeamKill: return "TEAMKILL";
    case Kind::SelfKill: return "SELF";
    case Kind::World: return "DIED";
    default: return "";
    }
}

// The local player's own kills and deaths are highlighted. Key 0 is nobody.
inline bool InvolvesPlayer(Stats::PlayerKey Killer, Stats::PlayerKey Victim, Stats::PlayerKey Local)
{
    return Local != 0 && (Killer == Local || Victim == Local);
}

// Server-side bounded ring. Payload is whatever the caller replicates (names,
// teams, weapon); the ring owns ordering, the cap and sequence numbers.
template <typename Payload>
class Ring
{
public:
    struct Item
    {
        Seq Number = 0;
        double TimeSeconds = 0.0; // Server world time of the death.
        Payload Data{};
    };

    explicit Ring(std::size_t InCapacity = MaxEntries) : Capacity(InCapacity == 0 ? 1 : InCapacity) {}

    // Appends the newest entry, dropping the oldest beyond the cap. Returns its sequence.
    Seq Append(double TimeSeconds, Payload Data)
    {
        Item Entry;
        Entry.Number = ++Last;
        Entry.TimeSeconds = std::isfinite(TimeSeconds) ? TimeSeconds : 0.0;
        Entry.Data = std::move(Data);
        Entries.push_back(std::move(Entry));
        if (Entries.size() > Capacity)
        {
            Entries.erase(Entries.begin(), Entries.begin() + static_cast<std::ptrdiff_t>(Entries.size() - Capacity));
        }
        return Last;
    }

    // Round reset: empty the ring. The sequence keeps counting, so an entry
    // from the previous round can never be mistaken for a new one.
    bool Clear()
    {
        const bool Changed = !Entries.empty();
        Entries.clear();
        return Changed;
    }

    const std::vector<Item>& Items() const { return Entries; } // Oldest first.
    Seq LastSeq() const { return Last; }

private:
    std::vector<Item> Entries;
    std::size_t Capacity;
    Seq Last = 0;
};

// What a client knows about each replicated entry.
struct Stamp
{
    Seq Number = 0;
    double TimeSeconds = 0.0;
};

struct Visible
{
    std::size_t Index = 0; // Into the stamps passed to Select.
    float Alpha = 1.0f;    // 1 until the fade, then down to 0 at expiry.
};

// Rows to draw, newest first: unexpired, one per sequence number (a repeated or
// reordered sequence is drawn once), at most MaxRows. Server time ahead of the
// local estimate counts as age 0. Late joiners only see entries still in their lifetime.
inline std::vector<Visible> Select(const std::vector<Stamp>& Stamps, double NowSeconds,
    double Lifetime = DefaultLifetime, double Fade = DefaultFade, std::size_t MaxRows = MaxEntries)
{
    std::vector<Visible> Rows;
    if (!std::isfinite(NowSeconds) || !(Lifetime > 0.0) || MaxRows == 0)
    {
        return Rows;
    }
    const double FadeTime = std::isfinite(Fade) ? std::clamp(Fade, 0.0, Lifetime) : 0.0;
    std::vector<std::size_t> Order;
    for (std::size_t Index = 0; Index < Stamps.size(); ++Index)
    {
        if (Stamps[Index].Number != 0 && std::isfinite(Stamps[Index].TimeSeconds)) { Order.push_back(Index); }
    }
    std::stable_sort(Order.begin(), Order.end(),
        [&Stamps](std::size_t A, std::size_t B) { return Stamps[A].Number > Stamps[B].Number; });
    Seq Previous = 0;
    for (const std::size_t Index : Order)
    {
        const Stamp& Entry = Stamps[Index];
        if (Entry.Number == Previous) { continue; }
        Previous = Entry.Number;
        const double Age = std::max(0.0, NowSeconds - Entry.TimeSeconds);
        if (Age >= Lifetime) { continue; }
        const double Remaining = Lifetime - Age;
        Visible Row;
        Row.Index = Index;
        Row.Alpha = FadeTime > 0.0 && Remaining < FadeTime ? static_cast<float>(Remaining / FadeTime) : 1.0f;
        Rows.push_back(Row);
        if (Rows.size() >= MaxRows) { break; }
    }
    return Rows;
}
}
