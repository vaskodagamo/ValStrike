#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Engine-free knife rules (KNIFE-01), shared by UVSKnifeComponent and
// Tests/Weapons/knife_rules_test.cpp. Times are server seconds measured from the
// accepted action start. The tuning is a shared contract with the knife
// animations: change it only together with their impact markers.
namespace VSKnifeRules
{
// Wire values of ServerRequestKnifeAttack's Kind byte.
enum class Attack : std::uint8_t { Slash = 0, Stab = 1 };
constexpr std::uint8_t AttackCount = 2;

struct Tuning
{
    double damage;       // Flat damage on every hit zone.
    double reachCm;      // From the server eye to the victim's hit surface.
    double halfAngleDeg; // Cone about the server view at impact time.
    double impactTime;   // Seconds after the accepted start.
    double nextAttack;   // Seconds after the accepted start before another attack.
    int maxTargets;      // Distinct victims one swing may hit.
};

constexpr Tuning Slash{40.0, 125.0, 35.0, 0.15, 0.55, 2};
constexpr Tuning Stab{65.0, 150.0, 10.0, 0.30, 0.90, 1};
constexpr double EquipSeconds = 0.40;

constexpr bool DecodeAttack(std::uint8_t Raw, Attack& Out)
{
    if (Raw >= AttackCount) { return false; }
    Out = Raw == 0 ? Attack::Slash : Attack::Stab;
    return true;
}

constexpr std::uint8_t EncodeAttack(Attack Kind) { return Kind == Attack::Slash ? 0 : 1; }

constexpr const Tuning& TuningFor(Attack Kind) { return Kind == Attack::Slash ? Slash : Stab; }

// --- Server request validation ---

// At most this many accepted attacks may wait for their impact. The cooldown
// already exceeds every impact time, so one is the normal maximum.
constexpr std::size_t MaxPending = 2;
// Accepted request sequences advance by 1..SequenceWindow (reliable, ordered RPCs).
constexpr std::uint32_t SequenceWindow = 1024;
// Absorbs one 60 Hz frame of arrival jitter without granting sustained credit,
// like the firearm cadence tolerance.
constexpr double CooldownArrivalTolerance = 0.02;
// A server timer may fire a hair before its scheduled impact time.
constexpr double ImpactTimerSlack = 0.005;

enum class Verdict : std::uint8_t
{
    Accept,
    Owner,      // Not the possessing owner.
    Sequence,   // Zero, duplicate, replayed or out-of-window sequence.
    Invalid,    // Unknown attack kind.
    Dead,
    Phase,      // Closed match combat gate.
    NotEquipped,
    Equipping,  // Knife equip time not elapsed.
    Cooldown,   // Before the previous attack's next-attack time.
    Busy,       // Pending impact table full.
};

struct Request
{
    bool validOwner = true;
    std::uint32_t sequence = 0;
    std::uint32_t lastSequence = 0; // Last sequence the server consumed for this pawn.
    std::uint8_t rawKind = 0;
    bool alive = true;
    bool combatAllowed = true;
    bool knifeEquipped = true;
    bool equipReady = true;
    double now = 0.0;
    double nextAttackTime = 0.0;
    std::size_t pending = 0;
};

constexpr bool IsSequenceAccepted(std::uint32_t Sequence, std::uint32_t Last)
{
    const std::uint32_t Delta = Sequence - Last;
    return Sequence != 0 && Delta != 0 && Delta <= SequenceWindow;
}

// Order: owner, sequence (consumed even when later checks reject), kind, life,
// phase, slot, equip, cooldown, pending bound.
inline Verdict Evaluate(const Request& In, Attack& OutKind)
{
    if (!In.validOwner) { return Verdict::Owner; }
    if (!IsSequenceAccepted(In.sequence, In.lastSequence)) { return Verdict::Sequence; }
    if (!DecodeAttack(In.rawKind, OutKind)) { return Verdict::Invalid; }
    if (!In.alive) { return Verdict::Dead; }
    if (!In.combatAllowed) { return Verdict::Phase; }
    if (!In.knifeEquipped) { return Verdict::NotEquipped; }
    if (!In.equipReady) { return Verdict::Equipping; }
    if (In.now + CooldownArrivalTolerance < In.nextAttackTime) { return Verdict::Cooldown; }
    if (In.pending >= MaxPending) { return Verdict::Busy; }
    return Verdict::Accept;
}

// Next-attack time after accepting Kind; early arrivals inside the tolerance
// do not shorten the following cooldown.
inline double NextAttackTime(double Now, double PreviousNext, Attack Kind)
{
    return (Now > PreviousNext ? Now : PreviousNext) + TuningFor(Kind).nextAttack;
}

// Whether a verdict consumes the request's sequence (every verdict after the
// sequence check, so a replay of a rejected request is also rejected).
constexpr bool ConsumesSequence(Verdict V) { return V != Verdict::Owner && V != Verdict::Sequence; }

// --- Pending impacts and lifecycle cancellation ---

struct Pending
{
    bool used = false;
    std::uint32_t actionId = 0; // The accepted request sequence.
    Attack kind = Attack::Slash;
    double impactTime = 0.0;
    std::uint32_t epoch = 0;
};

// Bounded table of accepted attacks waiting for their impact. Every lifecycle
// transition (switch, death, end of play, reset, closed phase) calls CancelAll,
// which advances the epoch: an impact scheduled before it can never resolve.
class Actions
{
public:
    // Slot index of the new action, or -1 when full.
    int Start(std::uint32_t ActionId, Attack Kind, double Now)
    {
        for (std::size_t Index = 0; Index < MaxPending; ++Index)
        {
            Pending& Entry = Slots[Index];
            if (Entry.used) { continue; }
            Entry = Pending{true, ActionId, Kind, Now + TuningFor(Kind).impactTime, Epoch};
            return static_cast<int>(Index);
        }
        return -1;
    }

    // Removes the action in Index and returns whether it may still resolve: it
    // exists, belongs to the current epoch and its impact time has come.
    bool Take(int Index, double Now, Pending& Out)
    {
        if (Index < 0 || static_cast<std::size_t>(Index) >= MaxPending) { return false; }
        Pending& Entry = Slots[static_cast<std::size_t>(Index)];
        if (!Entry.used) { return false; }
        if (Entry.epoch != Epoch) { Entry = Pending(); return false; }
        if (Now + ImpactTimerSlack < Entry.impactTime) { return false; }
        Out = Entry;
        Entry = Pending();
        return true;
    }

    // Returns the number of actions cancelled.
    std::size_t CancelAll()
    {
        std::size_t Cancelled = 0;
        for (Pending& Entry : Slots)
        {
            if (Entry.used) { ++Cancelled; }
            Entry = Pending();
        }
        ++Epoch;
        return Cancelled;
    }

    std::size_t Count() const
    {
        std::size_t Used = 0;
        for (const Pending& Entry : Slots) { Used += Entry.used ? 1 : 0; }
        return Used;
    }

    std::uint32_t CurrentEpoch() const { return Epoch; }

private:
    std::array<Pending, MaxPending> Slots{};
    std::uint32_t Epoch = 0;
};

// --- Impact candidates ---

// Result of the server's line-of-sight probe from its eye toward a candidate.
enum class Probe : std::uint8_t
{
    Clear,    // The first blocking hit was the candidate itself.
    World,    // World geometry blocks the way.
    Other,    // Another pawn is in front of the candidate.
    Missed,   // The probe hit nothing (no hit zone on the line).
};

struct Candidate
{
    std::uint32_t id = 0;      // Caller's victim key; 0 is never a target.
    double distanceCm = 0.0;   // Server eye to the candidate's hit surface.
    double cosAngle = -1.0;    // Server view direction vs direction to the candidate.
    Probe probe = Probe::Missed;
};

inline double CosHalfAngle(const Tuning& Value)
{
    return std::cos(Value.halfAngleDeg * 3.14159265358979323846 / 180.0);
}

// Cheap pre-filter before the probe: within reach and inside the cone.
inline bool InReachAndCone(const Tuning& Value, double DistanceCm, double CosAngle)
{
    return std::isfinite(DistanceCm) && std::isfinite(CosAngle) && DistanceCm >= 0.0
        && DistanceCm <= Value.reachCm && CosAngle >= CosHalfAngle(Value);
}

enum class ImpactResult : std::uint8_t { None = 0, Miss = 1, Wall = 2, Player = 3 };

struct Selection
{
    std::array<std::uint32_t, 2> ids{}; // Victims to damage, nearest first.
    int count = 0;
    ImpactResult result = ImpactResult::Miss;
};
static_assert(Slash.maxTargets <= 2 && Stab.maxTargets <= 2, "Selection holds two victims");

// Victims for one swing: in reach and cone with a clear probe, each id once,
// nearest first, at most maxTargets. The cosmetic result is Player when anyone
// is hit, Wall when the blade met world geometry (a blocked candidate or the
// forward probe), otherwise Miss.
template <std::size_t N>
Selection Select(const Tuning& Value, const std::array<Candidate, N>& In, std::size_t Count, bool ForwardWall)
{
    Selection Out;
    bool Wall = ForwardWall;
    const std::size_t Limit = Count < N ? Count : N;
    for (int Pick = 0; Pick < Value.maxTargets && Pick < static_cast<int>(Out.ids.size()); ++Pick)
    {
        const Candidate* Best = nullptr;
        for (std::size_t Index = 0; Index < Limit; ++Index)
        {
            const Candidate& Each = In[Index];
            if (Each.id == 0 || !InReachAndCone(Value, Each.distanceCm, Each.cosAngle)) { continue; }
            if (Each.probe == Probe::World) { Wall = true; }
            if (Each.probe != Probe::Clear) { continue; }
            bool Taken = false;
            for (int Prior = 0; Prior < Out.count; ++Prior) { Taken = Taken || Out.ids[static_cast<std::size_t>(Prior)] == Each.id; }
            if (Taken) { continue; }
            if (!Best || Each.distanceCm < Best->distanceCm) { Best = &Each; }
        }
        if (!Best) { break; }
        Out.ids[static_cast<std::size_t>(Out.count++)] = Best->id;
    }
    Out.result = Out.count > 0 ? ImpactResult::Player : (Wall ? ImpactResult::Wall : ImpactResult::Miss);
    return Out;
}
}
