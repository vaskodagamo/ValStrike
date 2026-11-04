#pragma once

#include <cstdint>

namespace VSShotEffects
{
/** What one observation of a shooter's accepted-shot counter should show. */
struct Decision
{
    bool spawn = false;        // Show exactly one tracer/impact for this observation.
    std::uint32_t shots = 0;   // Accepted shots this effect represents (>1 means coalesced).
    bool baseline = false;     // Counter adopted without effects (first sight, reset, unknown gap).
};

/**
 * Turns one weapon's replicated accepted-shot counter into cosmetic events.
 * Engine-free so the replay/coalescing rules can be tested without Unreal.
 * It never writes gameplay state; the server alone increments the counter.
 */
class ShotCounter
{
public:
    // Slack absorbs replication jitter between consecutive updates.
    static constexpr std::uint32_t BatchSlack = 3;

    bool IsInitialized() const { return initialized; }
    std::uint32_t Last() const { return last; }

    Decision Observe(std::uint32_t shots, double now, double fireInterval)
    {
        Decision result;
        if (!initialized)
        {
            // First sight (join, respawn, relevancy return): adopt the current
            // count so old shots are never replayed.
            Adopt(shots, now);
            result.baseline = true;
            return result;
        }
        if (shots == last)
        {
            return result;
        }
        if (shots < last)
        {
            // A reset on the same pawn (or wrap-around) proves nothing about
            // when those shots happened.
            Adopt(shots, now);
            result.baseline = true;
            return result;
        }
        const std::uint32_t delta = shots - last;
        // The server spaces accepted shots by about one fire interval (it
        // tolerates a small early arrival). A larger jump than the time since
        // the last change allows is stale history, for example a first
        // replication that arrived after a default-valued snapshot.
        const double interval = (fireInterval > 0.05 ? fireInterval : 0.05) * 0.8;
        const double elapsed = now > lastChange ? now - lastChange : 0.0;
        const double possible = elapsed / interval;
        const double allowed = (possible < 1.0e6 ? possible : 1.0e6) + BatchSlack;
        if (static_cast<double>(delta) > allowed)
        {
            Adopt(shots, now);
            result.baseline = true;
            return result;
        }
        // A replicated batch becomes one effect at the latest known view;
        // older aims inside the batch are unknown on this client.
        Adopt(shots, now);
        result.spawn = true;
        result.shots = delta;
        return result;
    }

private:
    void Adopt(std::uint32_t shots, double now)
    {
        initialized = true;
        last = shots;
        lastChange = now;
    }

    bool initialized = false;
    std::uint32_t last = 0;
    double lastChange = 0.0;
};

/** Where one effect's cosmetic trace direction came from. */
enum class DirectionSource : std::uint8_t
{
    BaseAim,   // Pawn base aim now: no recoil/spread known on this machine.
    Predicted, // Owning client's predicted spread direction for the accepted shot.
    Server,    // Server-resolved direction (this machine is the server, e.g. a listen host).
};

/**
 * Predicted is used only for the local shooter, and only when a prediction
 * paired with this accepted shot; Server only when this machine resolved a
 * shot that no earlier effect used. Otherwise base aim, as before spread.
 */
inline DirectionSource ChooseDirection(bool localShooter, bool havePrediction, bool haveFreshServerShot)
{
    if (localShooter && havePrediction) { return DirectionSource::Predicted; }
    if (haveFreshServerShot) { return DirectionSource::Server; }
    return DirectionSource::BaseAim;
}

/**
 * Owning client only: pairs locally predicted shots with the accepted-shot
 * counter. The weapon exposes only its latest prediction, so it is noted once
 * per frame; skipped ids become unknown entries to keep the order. Accepted
 * shots consume entries oldest first. A shot the server rejected leaves its
 * entry behind until it expires, so a mismatch lasts at most one burst.
 */
template <typename Payload>
class PredictedShots
{
public:
    static constexpr int Capacity = 32;

    int Size() const { return count; }

    void Note(std::uint32_t shotId, const Payload& payload, double now)
    {
        if (shotId == 0 || (noted && shotId == lastId)) { return; }
        if (noted)
        {
            std::uint32_t gap = shotId - lastId;
            // Ids skip 0 on wrap-around; that slot was never a shot.
            if (shotId < lastId && gap > 1) { --gap; }
            if (gap > static_cast<std::uint32_t>(Capacity)) { Clear(); }
            else { for (std::uint32_t i = 1; i < gap; ++i) { Push(Entry{false, Payload(), now}); } }
        }
        noted = true;
        lastId = shotId;
        Push(Entry{true, payload, now});
    }

    // Drops predictions whose acceptance can no longer be expected.
    void Expire(double now, double horizon)
    {
        while (count > 0 && entries[head].time < now - horizon) { Pop(); }
    }

    // Consumes the entries for `shots` accepted shots. True with the latest
    // one's payload when that shot's direction is known.
    bool Take(std::uint32_t shots, Payload& out)
    {
        bool known = false;
        for (std::uint32_t i = 0; i < shots && count > 0; ++i)
        {
            const Entry& entry = entries[head];
            known = entry.known;
            if (known) { out = entry.payload; }
            Pop();
        }
        return known;
    }

    void Clear() { head = 0; count = 0; }

private:
    struct Entry
    {
        bool known = false;
        Payload payload = Payload();
        double time = 0.0;
    };

    void Push(const Entry& entry)
    {
        if (count == Capacity) { Pop(); }
        entries[(head + count) % Capacity] = entry;
        ++count;
    }

    void Pop()
    {
        head = (head + 1) % Capacity;
        --count;
    }

    Entry entries[Capacity];
    int head = 0;
    int count = 0;
    std::uint32_t lastId = 0;
    bool noted = false;
};

/** Linear hold-then-fade opacity for an impact mark; 0 means expired. */
inline float MarkOpacity(double age, double hold, double fade)
{
    if (age < 0.0) { return 1.0f; }
    if (age <= hold) { return 1.0f; }
    if (fade <= 0.0) { return 0.0f; }
    const double t = (age - hold) / fade;
    return t >= 1.0 ? 0.0f : static_cast<float>(1.0 - t);
}

/** Chooses a pool slot: a free slot, else grow below the cap, else recycle the oldest. */
struct SlotChoice
{
    int index = -1;
    bool grow = false;
    bool recycled = false;
};

template <typename IsActive, typename SpawnTime>
SlotChoice ChooseSlot(int count, int cap, IsActive isActive, SpawnTime spawnTime)
{
    SlotChoice choice;
    for (int i = 0; i < count; ++i)
    {
        if (!isActive(i)) { choice.index = i; return choice; }
    }
    if (count < cap)
    {
        choice.index = count;
        choice.grow = true;
        return choice;
    }
    int oldest = -1;
    for (int i = 0; i < count; ++i)
    {
        if (oldest < 0 || spawnTime(i) < spawnTime(oldest)) { oldest = i; }
    }
    choice.index = oldest;
    choice.recycled = oldest >= 0;
    return choice;
}
}
