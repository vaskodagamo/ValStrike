#pragma once

#include <cstdint>

// Engine-free equip request validation, shared by the server component and the
// standalone test in Tests/Weapons. Times are server seconds.
namespace VSWeaponLoadoutRules
{
enum class Slot : std::uint8_t { Rifle = 0, Pistol = 1 };

// Raw request bytes from the network: rifle, pistol, last weapon.
constexpr std::uint8_t RequestRifle = 0;
constexpr std::uint8_t RequestPistol = 1;
constexpr std::uint8_t RequestLast = 2;

enum class Verdict : std::uint8_t { Accept, Invalid, Dead, Phase, Duplicate, Spam };

struct Request
{
    std::uint8_t raw = RequestRifle;
    bool alive = true;
    bool combatAllowed = true;
    Slot equipped = Slot::Rifle;
    Slot previous = Slot::Pistol;
    double now = 0.0;
    double lastSwitch = -1.0e9;
    double minInterval = 0.1;
};

struct Result
{
    Verdict verdict = Verdict::Invalid;
    Slot target = Slot::Rifle;
};

// Order: malformed byte, dead owner, closed combat gate, already equipped, rate
// limit. "Last" resolves from server state, never from the client's view.
inline Result Evaluate(const Request& In)
{
    if (In.raw > RequestLast) { return {Verdict::Invalid, In.equipped}; }
    if (!In.alive) { return {Verdict::Dead, In.equipped}; }
    if (!In.combatAllowed) { return {Verdict::Phase, In.equipped}; }
    const Slot Target = In.raw == RequestLast ? In.previous : static_cast<Slot>(In.raw);
    if (Target == In.equipped) { return {Verdict::Duplicate, Target}; }
    if (In.now - In.lastSwitch < In.minInterval) { return {Verdict::Spam, Target}; }
    return {Verdict::Accept, Target};
}

// Server readiness: the equipped weapon may act once its equip time has elapsed.
inline bool IsReady(bool bEquipped, double Now, double EquipReadyTime)
{
    return bEquipped && Now >= EquipReadyTime;
}
}
