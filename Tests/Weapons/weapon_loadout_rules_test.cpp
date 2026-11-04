// Standalone checks for Combat/VSWeaponLoadoutRules.h (no Unreal process).
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Weapons/weapon_loadout_rules_test.cpp \
//   -o Saved/Tests/Weapons/weapon_loadout_rules_test && Saved/Tests/Weapons/weapon_loadout_rules_test
#include "../../Source/ValStrike/Combat/VSWeaponLoadoutRules.h"

#include <cstdio>
#include <deque>
#include <vector>

namespace
{
using namespace VSWeaponLoadoutRules;
int Failures = 0;
int Checks = 0;

void Expect(bool Condition, const char* Name)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::printf("FAIL %s\n", Name);
    }
}

Request Base()
{
    Request In;
    In.now = 10.0;
    In.lastSwitch = 5.0;
    return In;
}

bool Is(const Result& Out, Verdict V, Slot S) { return Out.verdict == V && Out.target == S; }

constexpr double Equip = 0.5;
constexpr EquipTimes Times{Equip, 0.4};
constexpr double Interval = 0.1;

// Owning client and server joined by a fixed one-way delay, stepping 10 ms. The
// server mirrors ServerRequestEquip/Equip/FinishEquip; snapshots replicate after
// every server change.
struct Network
{
    struct Message { double arrival; std::uint8_t raw; std::uint32_t sequence; };
    struct Snapshot { double arrival; ServerView view; };
    double delay = 0.06;
    double now = 0.0;
    bool combatAllowed = true;
    Predictor client;
    ServerView server;
    double lastSwitch = -1.0e9;
    double readyAt = 0.0;
    std::deque<Message> toServer;
    std::deque<Snapshot> toClient;
    std::vector<Outcome> outcomes;
    std::vector<Slot> viewHistory;
    std::vector<std::uint8_t> sent;
    int viewChangesWithoutPress = 0;

    void Press(std::uint8_t Raw)
    {
        client.Press(Raw, server, now, Times);
        Flush();
    }

    void Flush()
    {
        std::uint32_t Sequence = 0;
        Slot Target = Slot::Rifle;
        if (client.TakeSend(now, Interval, Sequence, Target))
        {
            toServer.push_back({now + delay, EncodeRequest(Target), Sequence});
            sent.push_back(EncodeRequest(Target));
        }
    }

    void Publish() { toClient.push_back({now + delay, server}); }

    void Serve(const Message& In)
    {
        Request R;
        R.raw = In.raw;
        R.combatAllowed = combatAllowed;
        R.equipped = server.equipped;
        R.previous = server.previous;
        R.now = now;
        R.lastSwitch = lastSwitch;
        R.minInterval = Interval;
        const Result Out = Evaluate(R);
        if (In.sequence != 0) { server.ack = In.sequence; }
        if (Out.verdict == Verdict::Accept)
        {
            server.previous = server.equipped;
            server.equipped = Out.target;
            server.equipping = true;
            ++server.equipCount;
            lastSwitch = now;
            readyAt = now + Times.For(Out.target);
        }
        Publish();
    }

    void Run(double Seconds)
    {
        const double End = now + Seconds;
        while (now < End - 1.0e-9)
        {
            now += 0.01;
            Flush();
            while (!toServer.empty() && toServer.front().arrival <= now + 1.0e-9) { Serve(toServer.front()); toServer.pop_front(); }
            if (server.equipping && now >= readyAt) { server.equipping = false; Publish(); }
            while (!toClient.empty() && toClient.front().arrival <= now + 1.0e-9)
            {
                const Slot Before = View();
                outcomes.push_back(client.Reconcile(toClient.front().view, now));
                toClient.pop_front();
                if (View() != Before) { ++viewChangesWithoutPress; viewHistory.push_back(View()); }
            }
        }
    }

    Slot View() const { return client.active ? client.equipped : server.equipped; }
    bool Has(Outcome O) const
    {
        for (const Outcome Each : outcomes) { if (Each == O) { return true; } }
        return false;
    }
};
}

int main()
{
    {
        Request In = Base();
        In.raw = RequestPistol;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Pistol), "pistol accepted from rifle");
        In.raw = RequestRifle;
        Expect(Is(Evaluate(In), Verdict::Duplicate, Slot::Rifle), "rifle while rifle equipped is duplicate");
        In.raw = RequestLast;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Pistol), "last resolves to server previous slot");
        In.equipped = Slot::Pistol;
        In.previous = Slot::Rifle;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Rifle), "last toggles back to rifle");
        In.raw = RequestPistol;
        Expect(Is(Evaluate(In), Verdict::Duplicate, Slot::Pistol), "pistol while pistol equipped is duplicate");
    }
    {
        bool bAllInvalid = true;
        for (int Raw = RequestKnife + 1; Raw <= 255; ++Raw)
        {
            Request In = Base();
            In.raw = static_cast<std::uint8_t>(Raw);
            bAllInvalid = bAllInvalid && Is(Evaluate(In), Verdict::Invalid, Slot::Rifle);
        }
        Expect(bAllInvalid, "every byte above Knife is invalid and keeps the equipped slot");
    }
    {
        // Precedence: each earlier failure masks every later one.
        Request In = Base();
        In.raw = 9;
        In.alive = false;
        In.combatAllowed = false;
        In.lastSwitch = In.now;
        Expect(Evaluate(In).verdict == Verdict::Invalid, "invalid before dead");
        In.raw = RequestRifle;
        Expect(Evaluate(In).verdict == Verdict::Dead, "dead before phase and duplicate");
        In.alive = true;
        Expect(Evaluate(In).verdict == Verdict::Phase, "phase before duplicate");
        In.combatAllowed = true;
        Expect(Evaluate(In).verdict == Verdict::Duplicate, "duplicate before spam");
        In.raw = RequestPistol;
        Expect(Evaluate(In).verdict == Verdict::Spam, "spam within interval");
    }
    {
        // Rate limit boundary: exactly MinInterval later is allowed (binary-exact values).
        Request In = Base();
        In.raw = RequestPistol;
        In.minInterval = 0.125;
        In.lastSwitch = 9.9375;
        Expect(Evaluate(In).verdict == Verdict::Spam, "0.0625 s after a switch is spam");
        In.lastSwitch = 9.875;
        Expect(Evaluate(In).verdict == Verdict::Accept, "exactly the interval is accepted");
        In.minInterval = 0.0;
        In.lastSwitch = In.now;
        Expect(Evaluate(In).verdict == Verdict::Accept, "zero interval disables the rate limit");
    }
    {
        Request In = Base();
        In.raw = RequestPistol;
        In.alive = false;
        Expect(Is(Evaluate(In), Verdict::Dead, Slot::Rifle), "dead owner keeps the equipped slot");
        In.alive = true;
        In.combatAllowed = false;
        Expect(Is(Evaluate(In), Verdict::Phase, Slot::Rifle), "closed gate keeps the equipped slot");
    }
    Expect(!IsReady(false, 10.0, 0.0), "holstered weapon is never ready");
    Expect(!IsReady(true, 10.0, 10.5), "equipping weapon is not ready");
    Expect(IsReady(true, 10.5, 10.5), "ready exactly at equip completion");
    Expect(IsReady(true, 11.0, 10.5), "ready after equip completion");
    {
        Expect(IsSequenceNewer(2, 1) && !IsSequenceNewer(1, 2) && !IsSequenceNewer(5, 5), "sequence order");
        Expect(IsSequenceNewer(1, 0xFFFFFFFFu) && !IsSequenceNewer(0xFFFFFFFFu, 1), "sequence order across wrap-around");
        Predictor P;
        P.sentSequence = 0xFFFFFFFFu;
        ServerView S;
        P.Press(RequestPistol, S, 0.0, Times);
        std::uint32_t Sequence = 7;
        Slot Target = Slot::Rifle;
        Expect(P.TakeSend(0.0, Interval, Sequence, Target) && Sequence == 1 && Target == Slot::Pistol, "wrapped sequence skips unsequenced 0");
    }
    {
        // Local press: immediate slot and timer, explicit-slot request, local duplicate/invalid filtering.
        Predictor P;
        ServerView S;
        Expect(!P.Press(RequestRifle, S, 1.0, Times) && !P.active, "pressing the equipped slot predicts nothing");
        Expect(!P.Press(9, S, 1.0, Times) && !P.active, "invalid press predicts nothing");
        Expect(P.Press(RequestLast, S, 1.0, Times) && P.equipped == Slot::Pistol && P.previous == Slot::Rifle, "last predicts the previous slot at once");
        Expect(P.IsEquipping(S, 1.49) && !P.IsEquipping(S, 1.5), "predicted equip timer starts at the press");
        std::uint32_t Sequence = 0;
        Slot Target = Slot::Rifle;
        Expect(P.TakeSend(1.0, Interval, Sequence, Target) && Sequence == 1 && Target == Slot::Pistol, "last is sent as an explicit slot");
        Expect(!P.TakeSend(1.0, Interval, Sequence, Target), "one request per press");
        Expect(P.Press(RequestRifle, S, 1.05, Times) && P.queued, "press inside the spacing window queues");
        Expect(!P.TakeSend(1.14, Interval, Sequence, Target), "queued request waits for interval plus margin");
        Expect(P.TakeSend(1.16, Interval, Sequence, Target) && Sequence == 2 && Target == Slot::Rifle, "queued request sends when the window opens");
    }
    {
        // Reconciliation by ack sequence.
        Predictor P;
        ServerView S;
        P.Press(RequestPistol, S, 0.0, Times);
        std::uint32_t Sequence = 0;
        Slot Target = Slot::Rifle;
        P.TakeSend(0.0, Interval, Sequence, Target);
        Expect(P.Reconcile(S, 0.05) == Outcome::Keep && P.equipped == Slot::Pistol, "stale snapshot keeps the prediction");
        ServerView Accepted{Slot::Pistol, Slot::Rifle, true, 1, 1};
        Expect(P.Reconcile(Accepted, 0.12) == Outcome::Confirm && P.equipped == Slot::Pistol, "ack with agreement confirms");
        Expect(P.IsEquipping(Accepted, 0.49) && !P.IsEquipping(Accepted, 0.5), "confirmed switch keeps the local timer, not the server flag");
        Expect(P.Reconcile(Accepted, 0.55) == Outcome::Confirm, "local timer done but server still equipping holds");
        ServerView Done = Accepted;
        Done.equipping = false;
        Expect(P.Reconcile(Done, 0.62) == Outcome::Settle && !P.active, "settles once both sides finished");
        Expect(P.Reconcile(Done, 0.7) == Outcome::Mirror && P.equipped == Slot::Pistol, "mirrors the server afterwards");
    }
    {
        Predictor P;
        ServerView S;
        P.Press(RequestPistol, S, 0.0, Times);
        std::uint32_t Sequence = 0;
        Slot Target = Slot::Rifle;
        P.TakeSend(0.0, Interval, Sequence, Target);
        S.ack = 1; // Rejected: state unchanged, ack advanced.
        Expect(P.Reconcile(S, 0.12) == Outcome::Snap && !P.active && P.equipped == Slot::Rifle, "rejected latest request snaps back");
        Expect(!P.IsEquipping(S, 0.2), "snapped view uses the server equip flag");
        Expect(P.Press(RequestPistol, S, 0.3, Times) && P.queued, "can request again after a snap-back");
    }
    {
        // Server-driven change after confirmation (not caused by this client's requests).
        Predictor P;
        ServerView S;
        P.Press(RequestPistol, S, 0.0, Times);
        std::uint32_t Sequence = 0;
        Slot Target = Slot::Rifle;
        P.TakeSend(0.0, Interval, Sequence, Target);
        ServerView Accepted{Slot::Pistol, Slot::Rifle, true, 1, 1};
        P.Reconcile(Accepted, 0.12);
        ServerView Other{Slot::Pistol, Slot::Rifle, true, 3, 1};
        Expect(P.Reconcile(Other, 0.2) == Outcome::Snap && !P.active, "unacknowledged server switches replace a confirmed prediction");
    }
    {
        // Dropping a queued request (combat gate closed) reconciles against the server.
        Predictor P;
        ServerView S;
        P.Press(RequestPistol, S, 0.0, Times);
        P.DropQueued();
        Expect(P.Reconcile(S, 0.0) == Outcome::Snap && P.equipped == Slot::Rifle, "dropped request snaps to the server");
    }
    {
        // Accepted switch over a 120 ms round trip: no flicker, ready one RTT/2 before the server.
        Network N;
        N.Press(RequestPistol);
        Expect(N.View() == Slot::Pistol && N.client.IsEquipping(N.server, N.now), "switch is visible on the press");
        N.Run(1.0);
        Expect(N.View() == Slot::Pistol && N.server.equipped == Slot::Pistol && N.viewChangesWithoutPress == 0, "accepted switch never flickers");
        Expect(N.Has(Outcome::Confirm) && N.Has(Outcome::Settle) && !N.Has(Outcome::Snap), "accepted switch confirms then settles");
    }
    {
        // Rapid 2/1/2 then 1 within one interval settle on the last press with matching server state.
        Network N;
        N.Press(RequestPistol);
        N.Run(0.02);
        N.Press(RequestRifle);
        N.Run(0.02);
        N.Press(RequestPistol);
        N.Run(0.02);
        N.Press(RequestRifle);
        Expect(N.View() == Slot::Rifle, "view follows the newest press");
        N.Run(1.5);
        Expect(N.View() == Slot::Rifle && N.server.equipped == Slot::Rifle && !N.client.active, "rapid presses settle on the last request");
        Expect(N.viewChangesWithoutPress == 0 && !N.Has(Outcome::Snap), "rapid accepted presses never snap");
        Expect(N.server.equipCount == 2, "coalesced presses send only the needed switches");
    }
    {
        // Pressing back to the in-flight target cancels the queued request.
        Network N;
        N.Press(RequestPistol);
        N.Run(0.02);
        N.Press(RequestRifle);
        N.Press(RequestPistol);
        N.Run(1.0);
        Expect(N.View() == Slot::Pistol && N.server.equipCount == 1 && N.server.ack == 1, "press back to in-flight target sends nothing more");
        Expect(N.viewChangesWithoutPress == 0, "cancelled request never flickers");
    }
    {
        // Server rejects (combat gate closed on the server only): one snap, then agreement.
        Network N;
        N.combatAllowed = false;
        N.Press(RequestPistol);
        N.Run(1.0);
        Expect(N.View() == Slot::Rifle && N.server.equipped == Slot::Rifle && !N.client.active, "server rejection snaps back to the rifle");
        Expect(N.Has(Outcome::Snap) && N.viewChangesWithoutPress == 1, "rejection snaps exactly once");
    }
    {
        // Slot and request domains: byte 2 stays Last; the knife (slot 2) is request 3.
        Expect(RequestRifle == 0 && RequestPistol == 1 && RequestLast == 2 && RequestKnife == 3, "request bytes are stable");
        Expect(static_cast<int>(Slot::Knife) == 2 && SlotCount == 3, "knife is slot 2");
        bool bRoundTrip = true;
        for (const Slot Each : {Slot::Rifle, Slot::Pistol, Slot::Knife})
        {
            const DecodedRequest Back = DecodeRequest(EncodeRequest(Each));
            bRoundTrip = bRoundTrip && Back.kind == RequestKind::Equip && Back.slot == Each;
        }
        Expect(bRoundTrip, "every slot round-trips through its request byte");
        Expect(EncodeRequest(Slot::Knife) == RequestKnife && EncodeRequest(Slot::Knife) != RequestLast, "knife never encodes as Last");
        Expect(DecodeRequest(RequestLast).kind == RequestKind::Last, "byte 2 decodes as Last");
        bool bInvalid = true;
        for (int Raw = 4; Raw <= 255; ++Raw) { bInvalid = bInvalid && DecodeRequest(static_cast<std::uint8_t>(Raw)).kind == RequestKind::Invalid; }
        Expect(bInvalid, "bytes above 3 decode as invalid");
        Expect(Times.For(Slot::Knife) == 0.4 && Times.For(Slot::Rifle) == 0.5 && Times.For(Slot::Pistol) == 0.5, "per-slot equip times");
    }
    {
        // Server resolution across three slots; Last follows server history.
        Request In = Base();
        In.raw = RequestKnife;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Knife), "knife request accepted from rifle");
        In.equipped = Slot::Knife;
        In.previous = Slot::Rifle;
        Expect(Is(Evaluate(In), Verdict::Duplicate, Slot::Knife), "knife while knife equipped is duplicate");
        In.raw = RequestLast;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Rifle), "last from knife returns to rifle");
        In.equipped = Slot::Pistol;
        In.previous = Slot::Knife;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Knife), "last from pistol returns to knife");
        In.equipped = Slot::Rifle;
        In.previous = Slot::Pistol;
        Expect(Is(Evaluate(In), Verdict::Accept, Slot::Pistol), "byte 2 is never the knife without knife history");
    }
    {
        // Prediction: 3 switches at once with the knife equip time and sends request byte 3.
        Predictor P;
        ServerView S;
        Expect(P.Press(RequestKnife, S, 2.0, Times) && P.equipped == Slot::Knife && P.previous == Slot::Rifle, "knife press predicts slot 2");
        Expect(P.IsEquipping(S, 2.39) && !P.IsEquipping(S, 2.4), "predicted knife equip is 0.40 s");
        std::uint32_t Sequence = 0;
        Slot Target = Slot::Rifle;
        Expect(P.TakeSend(2.0, Interval, Sequence, Target) && Target == Slot::Knife && EncodeRequest(Target) == RequestKnife,
            "queued knife target is sent as request 3");
        Expect(P.Press(RequestLast, S, 2.2, Times) && P.equipped == Slot::Rifle && P.previous == Slot::Knife, "predicted Q from knife returns to rifle");
        Expect(P.Press(RequestLast, S, 2.3, Times) && P.equipped == Slot::Knife, "predicted Q toggles back to knife");
        Expect(!P.queued, "pressing back to the in-flight knife cancels the queued rifle");
    }
    {
        // 1/2/3/Q over the simulated network: client and server agree at every rest.
        Network N;
        N.Press(RequestKnife);
        N.Run(1.0);
        Expect(N.View() == Slot::Knife && N.server.equipped == Slot::Knife && N.server.previous == Slot::Rifle && !N.client.active,
            "knife equip settles on both sides");
        N.Press(RequestPistol);
        N.Run(1.0);
        N.Press(RequestLast);
        N.Run(1.0);
        Expect(N.View() == Slot::Knife && N.server.equipped == Slot::Knife && N.server.previous == Slot::Pistol, "Q from pistol returns to knife");
        N.Press(RequestLast);
        N.Run(1.0);
        Expect(N.View() == Slot::Pistol && N.server.equipped == Slot::Pistol && N.viewChangesWithoutPress == 0 && !N.Has(Outcome::Snap),
            "three-slot history never flickers or snaps");
        bool bNoLast = true;
        for (const std::uint8_t Byte : N.sent) { bNoLast = bNoLast && Byte != RequestLast; }
        Expect(bNoLast, "the predicting client only sends explicit slots");
    }
    {
        // Rapid 3/1/3/2 settles on the last press.
        Network N;
        N.Press(RequestKnife);
        N.Run(0.02);
        N.Press(RequestRifle);
        N.Run(0.02);
        N.Press(RequestKnife);
        N.Run(0.02);
        N.Press(RequestPistol);
        N.Run(1.5);
        Expect(N.View() == Slot::Pistol && N.server.equipped == Slot::Pistol && !N.client.active && !N.Has(Outcome::Snap),
            "rapid presses across three slots settle on the last request");
    }
    {
        // A rejected knife switch snaps back once.
        Network N;
        N.combatAllowed = false;
        N.Press(RequestKnife);
        N.Run(1.0);
        Expect(N.View() == Slot::Rifle && N.Has(Outcome::Snap) && N.viewChangesWithoutPress == 1, "rejected knife switch snaps back");
    }
    std::printf("%d/%d weapon loadout rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
