#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace VSHitFeedback
{
// Shooter marker variants in priority order: a stronger marker is never hidden
// by a weaker one that arrives while it is still showing.
enum class MarkerKind : std::uint8_t { None, Hit, Headshot, Kill };

// Bits of the one-byte flags field each client RPC carries.
enum Flag : std::uint8_t
{
    FlagHeadshot = 1u << 0,
    FlagKilled = 1u << 1,
    FlagHasSource = 1u << 2, // Victim only: the instigator location is meaningful.
};

constexpr double HitSeconds = 0.25;
constexpr double HeadshotSeconds = 0.35;
constexpr double KillSeconds = 0.6;
constexpr double IndicatorSeconds = 1.6;
constexpr std::size_t MaxIndicators = 4;
// A new hit from within this arc refreshes the existing indicator instead of stacking.
constexpr float IndicatorMergeDegrees = 20.0f;
// Sequences older than the newest seen by more than this are dropped as stale.
constexpr std::uint32_t SequenceWindow = 64;

/** A server-confirmed event addressed to this client, as unpacked from its RPC. */
struct Event
{
    std::uint32_t sequence = 0;  // Per-receiver server counter; 0 is never sent.
    std::uint8_t flags = 0;
    float damage = 0.0f;
    float sourceYawDeg = 0.0f;   // Victim: world yaw from the victim towards the instigator.
    std::uint8_t weapon = 0;     // Victim kill: ValStrike::Damage::WeaponClass value.
};

enum class Outcome : std::uint8_t { Shown, Duplicate, Stale, Ignored };

struct Marker
{
    MarkerKind kind = MarkerKind::None;
    float alpha = 0.0f;
    float damage = 0.0f;
};

struct Indicator
{
    float sourceYawDeg = 0.0f;
    float alpha = 0.0f;
};

struct Death
{
    bool eliminated = false;
    bool headshot = false;
    std::uint8_t weapon = 0;
};

/** Hold for 60% of the lifetime, then fade linearly; 0 once expired. */
inline float FadeAlpha(double age, double lifetime)
{
    if (lifetime <= 0.0 || age >= lifetime) { return 0.0f; }
    if (age <= lifetime * 0.6) { return 1.0f; }
    return static_cast<float>((lifetime - age) / (lifetime * 0.4));
}

inline double MarkerSeconds(MarkerKind kind)
{
    switch (kind)
    {
    case MarkerKind::Hit: return HitSeconds;
    case MarkerKind::Headshot: return HeadshotSeconds;
    case MarkerKind::Kill: return KillSeconds;
    default: return 0.0;
    }
}

inline MarkerKind MarkerFor(std::uint8_t flags)
{
    if (flags & FlagKilled) { return MarkerKind::Kill; }
    return (flags & FlagHeadshot) ? MarkerKind::Headshot : MarkerKind::Hit;
}

/** Shortest signed difference to - from in degrees, in [-180, 180). */
inline float DeltaDegrees(float from, float to)
{
    float delta = std::fmod(to - from + 180.0f, 360.0f);
    if (delta < 0.0f) { delta += 360.0f; }
    return delta - 180.0f;
}

/**
 * Client-side feedback for one local player. Engine-free so the dedup, priority
 * and respawn rules can be tested without Unreal. It only reacts to events the
 * server confirmed and never feeds anything back into gameplay.
 */
class State
{
public:
    // Respawn or a new pawn: drop everything on screen. Seen sequences are kept,
    // so an event that arrives again later is still not replayed.
    void BeginLife()
    {
        marker = {};
        markerStart = 0.0;
        for (Slot& slot : indicators) { slot = {}; }
        death = {};
    }

    Outcome OnHitConfirmed(const Event& event, double now)
    {
        const Outcome seen = Accept(event.sequence);
        if (seen != Outcome::Shown) { return seen; }
        const MarkerKind kind = MarkerFor(event.flags);
        const Marker current = CurrentMarker(now);
        if (current.kind != MarkerKind::None && kind < current.kind) { return Outcome::Ignored; }
        marker.kind = kind;
        marker.damage = std::isfinite(event.damage) && event.damage > 0.0f ? event.damage : 0.0f;
        markerStart = now;
        return Outcome::Shown;
    }

    Outcome OnDamageTaken(const Event& event, double now)
    {
        const Outcome seen = Accept(event.sequence);
        if (seen != Outcome::Shown) { return seen; }
        // After the death in this life nothing else stacks on the death screen.
        if (death.eliminated) { return Outcome::Ignored; }
        if (event.flags & FlagHasSource) { AddIndicator(event.sourceYawDeg, now); }
        if (event.flags & FlagKilled)
        {
            death.eliminated = true;
            death.headshot = (event.flags & FlagHeadshot) != 0;
            death.weapon = event.weapon;
        }
        return Outcome::Shown;
    }

    Marker CurrentMarker(double now) const
    {
        Marker result = marker;
        result.alpha = FadeAlpha(now - markerStart, MarkerSeconds(marker.kind));
        if (result.alpha <= 0.0f) { return {}; }
        return result;
    }

    // Writes the live indicators; returns how many were written.
    std::size_t CurrentIndicators(double now, Indicator* out, std::size_t capacity) const
    {
        std::size_t count = 0;
        for (const Slot& slot : indicators)
        {
            const float alpha = slot.active ? FadeAlpha(now - slot.start, IndicatorSeconds) : 0.0f;
            if (alpha > 0.0f && count < capacity) { out[count++] = {slot.yaw, alpha}; }
        }
        return count;
    }

    const Death& CurrentDeath() const { return death; }
    std::uint32_t NewestSequence() const { return newest; }

private:
    struct Slot
    {
        bool active = false;
        float yaw = 0.0f;
        double start = 0.0;
    };

    // Sliding window: a reliable kill retransmitted after a later unreliable hit
    // is still shown once; anything seen before, or far older, is dropped.
    Outcome Accept(std::uint32_t sequence)
    {
        if (sequence == 0) { return Outcome::Stale; }
        if (sequence > newest)
        {
            const std::uint32_t shift = sequence - newest;
            seenBits = shift >= SequenceWindow ? 0 : seenBits << shift;
            seenBits |= 1;
            newest = sequence;
            return Outcome::Shown;
        }
        const std::uint32_t age = newest - sequence;
        if (age >= SequenceWindow) { return Outcome::Stale; }
        const std::uint64_t bit = std::uint64_t{1} << age;
        if (seenBits & bit) { return Outcome::Duplicate; }
        seenBits |= bit;
        return Outcome::Shown;
    }

    void AddIndicator(float yaw, double now)
    {
        if (!std::isfinite(yaw)) { return; }
        Slot* target = nullptr;
        for (Slot& slot : indicators)
        {
            const bool live = slot.active && now - slot.start < IndicatorSeconds;
            if (live && std::fabs(DeltaDegrees(slot.yaw, yaw)) <= IndicatorMergeDegrees) { target = &slot; break; }
        }
        for (std::size_t i = 0; !target && i < MaxIndicators; ++i)
        {
            if (!indicators[i].active || now - indicators[i].start >= IndicatorSeconds) { target = &indicators[i]; }
        }
        if (!target)
        {
            target = &indicators[0];
            for (Slot& slot : indicators)
            {
                if (slot.start < target->start) { target = &slot; }
            }
        }
        target->active = true;
        target->yaw = yaw;
        target->start = now;
    }

    Marker marker;
    double markerStart = 0.0;
    Slot indicators[MaxIndicators];
    Death death;
    std::uint32_t newest = 0;
    std::uint64_t seenBits = 0;
};
}
