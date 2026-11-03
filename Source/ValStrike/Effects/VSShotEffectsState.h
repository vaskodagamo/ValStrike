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
