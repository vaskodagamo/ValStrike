#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

// Engine-free rules for client gameplay audio: which replicated observations
// produce a cue (once only), footstep cadence, the device beep curve and the
// PLACEHOLDER waveform generator. Nothing here writes gameplay state.
namespace VSGameplayAudio
{
/** One slot per cue. Values index the subsystem's art slots and stats. */
enum class Cue : std::uint8_t
{
    RifleShot,
    PistolShot,
    Footstep,
    Land,
    Hurt,
    Death,
    PlantStart,
    DefuseStart,
    DeviceBeep,
    DefuseComplete,
    Count
};
constexpr int CueCount = static_cast<int>(Cue::Count);

/**
 * Footsteps, CS-style: running is audible, walking and crouching are silent.
 * Steps come from distance travelled on the ground, so cadence follows speed.
 */
struct FootstepConfig
{
    double audibleSpeedRatio = 0.6; // Of the run speed; the 0.45 walk scale stays below it.
    double strideCm = 140.0;        // Ground distance between two steps at run speed.
};

class FootstepCadence
{
public:
    /** Advances one frame; true means play one footstep now. */
    bool Step(double dt, double groundSpeed, double runSpeed, bool onGround, bool crouched,
        const FootstepConfig& config = FootstepConfig())
    {
        if (!(dt > 0.0) || !(groundSpeed >= 0.0) || !(runSpeed > 0.0) || !onGround || crouched ||
            groundSpeed < runSpeed * config.audibleSpeedRatio)
        {
            // Quiet movement never banks distance toward a later audible step.
            // Half a stride means the first audible step comes quickly.
            travelled = config.strideCm * 0.5;
            return false;
        }
        travelled += groundSpeed * (dt < 0.25 ? dt : 0.25);
        if (travelled < config.strideCm)
        {
            return false;
        }
        travelled = std::fmod(travelled, config.strideCm);
        return true;
    }

private:
    double travelled = FootstepConfig().strideCm * 0.5;
};

/** Seconds between steps at a constant audible speed; 0 when silent. */
inline double FootstepInterval(double groundSpeed, double runSpeed, bool crouched,
    const FootstepConfig& config = FootstepConfig())
{
    if (crouched || !(runSpeed > 0.0) || !(groundSpeed >= runSpeed * config.audibleSpeedRatio))
    {
        return 0.0;
    }
    return config.strideCm / groundSpeed;
}

/** A landing after a real fall; small step-downs and the first sight are quiet. */
class LandingDetector
{
public:
    static constexpr double MinImpactSpeed = 300.0; // cm/s downward at touchdown.

    bool Observe(bool falling, double verticalSpeed)
    {
        if (!initialized)
        {
            initialized = true;
            wasFalling = falling;
            peakDown = 0.0;
            return false;
        }
        bool land = false;
        if (falling)
        {
            const double down = -verticalSpeed;
            if (down > peakDown) { peakDown = down; }
        }
        else if (wasFalling)
        {
            land = peakDown >= MinImpactSpeed;
            peakDown = 0.0;
        }
        wasFalling = falling;
        return land;
    }

private:
    bool initialized = false;
    bool wasFalling = false;
    double peakDown = 0.0;
};

/** Own damage and death from one replicated health value. */
struct LifeDecision
{
    bool hurt = false;
    bool death = false;
    bool baseline = false;
};

class LifeWatch
{
public:
    LifeDecision Observe(double health)
    {
        LifeDecision result;
        if (!initialized || !(health == health))
        {
            // First sight (spawn, rejoin, new view target) adopts the value.
            initialized = health == health;
            last = health;
            result.baseline = true;
            return result;
        }
        if (health < last)
        {
            if (health <= 0.0 && last > 0.0) { result.death = true; }
            else if (health > 0.0) { result.hurt = true; }
        }
        // Heals and resets are silent re-baselines.
        last = health;
        return result;
    }

private:
    bool initialized = false;
    double last = 0.0;
};

/** Objective cue decisions from the replicated match wire state. */
struct ObjectiveDecision
{
    bool plantStart = false;
    bool defuseStart = false;
    bool defuseComplete = false;
    bool baseline = false;
};

class ObjectiveWatch
{
public:
    // Values match ValStrike::Rules::DeviceState (Rules/VSMatchRules.h).
    static constexpr std::uint8_t Planted = 4;
    static constexpr std::uint8_t Defused = 5;

    ObjectiveDecision Observe(std::uint64_t roundId, std::uint8_t deviceState, std::uint64_t interactionToken)
    {
        ObjectiveDecision result;
        if (!initialized)
        {
            // Joining mid-plant or after a defuse must not replay those cues.
            Adopt(roundId, deviceState, interactionToken);
            result.baseline = true;
            return result;
        }
        if (interactionToken != 0 && interactionToken != lastToken)
        {
            // An interaction on a planted device can only be a defuse.
            if (deviceState == Planted) { result.defuseStart = true; }
            else { result.plantStart = true; }
        }
        if (roundId == lastRound && lastState == Planted && deviceState == Defused)
        {
            result.defuseComplete = true;
        }
        Adopt(roundId, deviceState, interactionToken);
        return result;
    }

private:
    void Adopt(std::uint64_t roundId, std::uint8_t deviceState, std::uint64_t interactionToken)
    {
        initialized = true;
        lastRound = roundId;
        lastState = deviceState;
        lastToken = interactionToken;
    }

    bool initialized = false;
    std::uint64_t lastRound = 0;
    std::uint8_t lastState = 0;
    std::uint64_t lastToken = 0;
};

/** Device beep spacing: 1 s at plant, quadratically down to 0.12 s at detonation. */
inline double BeepInterval(double remaining, double total)
{
    constexpr double Slowest = 1.0;
    constexpr double Fastest = 0.12;
    if (!(total > 0.0) || !(remaining == remaining)) { return Slowest; }
    double t = remaining / total;
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    return Fastest + (Slowest - Fastest) * t * t;
}

/** Emits device beeps against server time while the device is planted. */
class BeepScheduler
{
public:
    static constexpr double DefaultFuseSeconds = 30.0; // Rules::Config::DetonationMs.

    bool Observe(bool planted, double serverNow, double deadline, double fuse = DefaultFuseSeconds)
    {
        if (!planted || !(deadline > serverNow) || !(serverNow == serverNow))
        {
            next = -1.0;
            return false;
        }
        // A live device is current state, not history, so first sight may beep.
        // A server clock correction backwards restarts the cadence.
        if (next < 0.0 || next > serverNow + BeepInterval(fuse, fuse))
        {
            next = serverNow;
        }
        if (serverNow < next) { return false; }
        next = serverNow + BeepInterval(deadline - serverNow, fuse);
        return true;
    }

private:
    double next = -1.0;
};

// ---------------------------------------------------------------------------
// PLACEHOLDER SOUND GENERATOR. Synthesized clicks, noise bursts and tones that
// stand in until Codex delivers real cue assets through the subsystem's art
// slots. Output is mono, deterministic per (cue, variant) and within [-1, 1].
// ---------------------------------------------------------------------------
namespace Placeholder
{
constexpr int FootstepVariants = 4;

class Noise
{
public:
    explicit Noise(std::uint32_t seed) : state(seed ? seed : 0x9E3779B9u) {}
    double Next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<double>(state) / 2147483647.5 - 1.0;
    }

private:
    std::uint32_t state;
};

constexpr double Pi = 3.14159265358979323846;

inline double Clamp1(double v) { return v > 1.0 ? 1.0 : (v < -1.0 ? -1.0 : v); }

/** Noise burst plus decaying sine thump; the shared shape of shots and steps. */
inline void Burst(std::vector<float>& out, int rate, double seconds, std::uint32_t seed, double noiseGain,
    double noiseDecay, double lowpass, double toneHz, double toneGain, double toneDecay)
{
    Noise noise(seed);
    const int n = static_cast<int>(seconds * rate);
    out.resize(static_cast<std::size_t>(n));
    const double alpha = 1.0 - std::exp(-2.0 * Pi * lowpass / rate);
    double filtered = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / rate;
        filtered += alpha * (noise.Next() - filtered);
        const double attack = t < 0.002 ? t / 0.002 : 1.0;
        const double v = attack * (noiseGain * filtered * std::exp(-t / noiseDecay) +
            toneGain * std::sin(2.0 * Pi * toneHz * t) * std::exp(-t / toneDecay));
        out[static_cast<std::size_t>(i)] = static_cast<float>(Clamp1(v));
    }
}

/** Sequence of sine blips (Hz, seconds each) with short gaps. */
inline void Blips(std::vector<float>& out, int rate, const double* hz, int count, double each, double gap, double gain)
{
    const int per = static_cast<int>((each + gap) * rate);
    out.assign(static_cast<std::size_t>(per * count), 0.0f);
    for (int b = 0; b < count; ++b)
    {
        for (int i = 0; i < static_cast<int>(each * rate); ++i)
        {
            const double t = static_cast<double>(i) / rate;
            const double env = (t < 0.004 ? t / 0.004 : 1.0) * std::exp(-t / (each * 0.5));
            out[static_cast<std::size_t>(b * per + i)] = static_cast<float>(Clamp1(gain * env * std::sin(2.0 * Pi * hz[b] * t)));
        }
    }
}

/** Downward pitch sweep with a little grit, for hurt and death. */
inline void Sweep(std::vector<float>& out, int rate, double seconds, double fromHz, double toHz, double gain, std::uint32_t seed)
{
    Noise noise(seed);
    const int n = static_cast<int>(seconds * rate);
    out.resize(static_cast<std::size_t>(n));
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / rate;
        const double hz = fromHz + (toHz - fromHz) * (t / seconds);
        phase += 2.0 * Pi * hz / rate;
        const double env = (t < 0.005 ? t / 0.005 : 1.0) * (1.0 - t / seconds);
        // Soft-clipped sine reads as a buzz without aliasing harshly.
        const double v = std::tanh(2.5 * std::sin(phase)) * 0.8 + 0.2 * noise.Next();
        out[static_cast<std::size_t>(i)] = static_cast<float>(Clamp1(gain * env * v));
    }
}

/** Fills `out` with the cue's mono samples at `rate` Hz. Unknown cues give silence. */
inline void Generate(Cue cue, int variant, int rate, std::vector<float>& out)
{
    out.clear();
    if (rate < 8000 || rate > 192000) { return; }
    const std::uint32_t seed = 0x5EED0000u + static_cast<std::uint32_t>(cue) * 131u +
        static_cast<std::uint32_t>(variant < 0 ? 0 : variant);
    switch (cue)
    {
    case Cue::RifleShot:
        Burst(out, rate, 0.24, seed, 0.95, 0.045, 3200.0, 85.0, 0.6, 0.06);
        break;
    case Cue::PistolShot:
        Burst(out, rate, 0.16, seed, 0.9, 0.025, 6500.0, 170.0, 0.4, 0.03);
        break;
    case Cue::Footstep:
        Burst(out, rate, 0.09, seed, 0.7, 0.015, 900.0 + 150.0 * (variant % FootstepVariants),
            70.0 + 6.0 * (variant % FootstepVariants), 0.45, 0.02);
        break;
    case Cue::Land:
        Burst(out, rate, 0.2, seed, 0.75, 0.03, 700.0, 55.0, 0.7, 0.05);
        break;
    case Cue::Hurt:
        Sweep(out, rate, 0.15, 620.0, 340.0, 0.6, seed);
        break;
    case Cue::Death:
        Sweep(out, rate, 0.6, 420.0, 110.0, 0.7, seed);
        break;
    case Cue::PlantStart:
    {
        const double hz[] = {660.0, 880.0};
        Blips(out, rate, hz, 2, 0.08, 0.04, 0.6);
        break;
    }
    case Cue::DefuseStart:
    {
        const double hz[] = {880.0, 660.0};
        Blips(out, rate, hz, 2, 0.08, 0.04, 0.6);
        break;
    }
    case Cue::DeviceBeep:
    {
        const double hz[] = {1760.0};
        Blips(out, rate, hz, 1, 0.07, 0.0, 0.55);
        break;
    }
    case Cue::DefuseComplete:
    {
        const double hz[] = {523.25, 659.25, 783.99};
        Blips(out, rate, hz, 3, 0.12, 0.03, 0.6);
        break;
    }
    case Cue::Count:
        break;
    }
}

inline int VariantCount(Cue cue) { return cue == Cue::Footstep ? FootstepVariants : 1; }
}
}
