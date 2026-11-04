#pragma once

#include <cstdint>

// Engine-free equip request validation and owning-client switch prediction,
// shared by the loadout component and the standalone test in Tests/Weapons.
// Validation times are server seconds.
namespace VSWeaponLoadoutRules
{
// Loadout slots. The knife is always carried, never bought and has no ammo.
enum class Slot : std::uint8_t { Rifle = 0, Pistol = 1, Knife = 2 };
constexpr int SlotCount = 3;

// Raw request bytes on the network. Slots and requests are separate domains:
// byte 2 is "last weapon", so the knife is request 3 although it is slot 2.
// Convert only through EncodeRequest/DecodeRequest.
constexpr std::uint8_t RequestRifle = 0;
constexpr std::uint8_t RequestPistol = 1;
constexpr std::uint8_t RequestLast = 2;
constexpr std::uint8_t RequestKnife = 3;

enum class RequestKind : std::uint8_t { Equip, Last, Invalid };

struct DecodedRequest
{
    RequestKind kind = RequestKind::Invalid;
    Slot slot = Slot::Rifle; // Meaningful only for Equip.
};

constexpr std::uint8_t EncodeRequest(Slot Target)
{
    switch (Target)
    {
    case Slot::Rifle: return RequestRifle;
    case Slot::Pistol: return RequestPistol;
    case Slot::Knife: return RequestKnife;
    }
    return RequestRifle;
}

// Any byte; everything above RequestKnife is invalid.
constexpr DecodedRequest DecodeRequest(std::uint8_t Raw)
{
    switch (Raw)
    {
    case RequestRifle: return {RequestKind::Equip, Slot::Rifle};
    case RequestPistol: return {RequestKind::Equip, Slot::Pistol};
    case RequestLast: return {RequestKind::Last, Slot::Rifle};
    case RequestKnife: return {RequestKind::Equip, Slot::Knife};
    default: return {RequestKind::Invalid, Slot::Rifle};
    }
}

// The slot a request resolves to against a view (server state or the owner's
// prediction); false for an invalid byte.
constexpr bool ResolveRequest(std::uint8_t Raw, Slot Previous, Slot& OutTarget)
{
    const DecodedRequest In = DecodeRequest(Raw);
    if (In.kind == RequestKind::Invalid) { return false; }
    OutTarget = In.kind == RequestKind::Last ? Previous : In.slot;
    return true;
}

// Seconds from an accepted switch until the new slot may act.
struct EquipTimes
{
    double firearm = 0.5;
    double knife = 0.4;
    double For(Slot Target) const { return Target == Slot::Knife ? knife : firearm; }
};

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
    Slot Target = In.equipped;
    if (!ResolveRequest(In.raw, In.previous, Target)) { return {Verdict::Invalid, In.equipped}; }
    if (!In.alive) { return {Verdict::Dead, In.equipped}; }
    if (!In.combatAllowed) { return {Verdict::Phase, In.equipped}; }
    if (Target == In.equipped) { return {Verdict::Duplicate, Target}; }
    if (In.now - In.lastSwitch < In.minInterval) { return {Verdict::Spam, Target}; }
    return {Verdict::Accept, Target};
}

// Server readiness: the equipped weapon may act once its equip time has elapsed.
inline bool IsReady(bool bEquipped, double Now, double EquipReadyTime)
{
    return bEquipped && Now >= EquipReadyTime;
}

// --- Owning-client prediction. Times are the client's clock. ---

// Request sequence 0 is unsequenced (raw development requests) and never acked.
// Sequences skip 0 on wrap-around; A is newer when it is ahead by less than half the range.
inline bool IsSequenceNewer(std::uint32_t A, std::uint32_t B)
{
    return A != B && A - B < 0x80000000u;
}

// Extra client spacing between sent requests so jitter cannot compress their
// server arrivals below the server's minimum switch interval.
constexpr double SendSpacingMargin = 0.05;

// Replicated loadout state as the owning client last received it.
struct ServerView
{
    Slot equipped = Slot::Rifle;
    Slot previous = Slot::Pistol;
    bool equipping = false;
    std::uint32_t equipCount = 0;
    std::uint32_t ack = 0; // Latest sequenced request the server processed, accepted or not.
};

enum class Outcome : std::uint8_t
{
    Mirror,  // Not predicting: the view is the server state.
    Keep,    // A request is queued or unacknowledged: hold the prediction.
    Confirm, // The server acked the latest request and agrees: keep the local equip timer.
    Snap,    // The server acked the latest request and disagrees, or switched on its own.
    Settle,  // Confirmed and the equip finished on both sides: back to mirroring.
};

// The owning client's view of its loadout. It switches the slot and starts the
// equip timer on the key press, sends one explicit-slot request per spacing
// window (later presses coalesce into the newest target) and reconciles each
// replicated snapshot by ack sequence. Observers and the server never use it.
struct Predictor
{
    bool active = false;
    Slot equipped = Slot::Rifle;
    Slot previous = Slot::Pistol;
    double readyTime = 0.0;
    bool queued = false;
    Slot queuedTarget = Slot::Rifle;
    // Slot the server holds once every sent request is accepted.
    Slot sentTarget = Slot::Rifle;
    std::uint32_t sentSequence = 0;
    double lastSend = -1.0e9;
    bool confirmed = false;
    std::uint32_t confirmedCount = 0;

    // Rifle/pistol/knife/last press. Last resolves from the predicted view, so
    // the request on the wire is always an explicit slot. False if nothing changes.
    bool Press(std::uint8_t Raw, const ServerView& Server, double Now, const EquipTimes& Times)
    {
        if (!active) { Adopt(Server); }
        Slot Target = equipped;
        if (!ResolveRequest(Raw, previous, Target) || Target == equipped) { return false; }
        previous = equipped;
        equipped = Target;
        readyTime = Now + Times.For(Target);
        active = true;
        confirmed = false;
        // Pressing back to the in-flight target cancels a queued request instead of
        // sending one the server would reject as a duplicate.
        queued = Target != sentTarget;
        queuedTarget = Target;
        return true;
    }

    // Earliest time the queued request may be sent.
    double NextSendTime(double MinInterval) const { return lastSend + MinInterval + SendSpacingMargin; }

    // Takes the queued request when its spacing window is open. Send
    // EncodeRequest(OutTarget), never the slot value itself.
    bool TakeSend(double Now, double MinInterval, std::uint32_t& OutSequence, Slot& OutTarget)
    {
        if (!queued || Now < NextSendTime(MinInterval)) { return false; }
        queued = false;
        if (++sentSequence == 0) { ++sentSequence; }
        sentTarget = queuedTarget;
        lastSend = Now;
        OutSequence = sentSequence;
        OutTarget = queuedTarget;
        return true;
    }

    // A queued request that can no longer be sent (combat gate closed).
    void DropQueued() { queued = false; }

    Outcome Reconcile(const ServerView& Server, double Now)
    {
        if (!active) { Adopt(Server); return Outcome::Mirror; }
        if (queued || IsSequenceNewer(sentSequence, Server.ack)) { return Outcome::Keep; }
        if (Server.equipped != equipped || (confirmed && Server.equipCount != confirmedCount))
        {
            Adopt(Server);
            return Outcome::Snap;
        }
        // "Last" follows the server's history from here on.
        previous = Server.previous;
        if (!confirmed)
        {
            confirmed = true;
            confirmedCount = Server.equipCount;
        }
        if (!Server.equipping && Now >= readyTime)
        {
            Adopt(Server);
            return Outcome::Settle;
        }
        return Outcome::Confirm;
    }

    // While predicting, the local timer replaces the replicated equipping flag.
    bool IsEquipping(const ServerView& Server, double Now) const { return active ? Now < readyTime : Server.equipping; }

private:
    void Adopt(const ServerView& Server)
    {
        active = false;
        confirmed = false;
        queued = false;
        equipped = Server.equipped;
        previous = Server.previous;
        sentTarget = Server.equipped;
    }
};
}
