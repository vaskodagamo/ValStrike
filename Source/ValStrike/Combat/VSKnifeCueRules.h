#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Engine-free client cue sequencing for the replicated knife state (KNIFE-01-C1),
// shared by UVSKnifeComponent::OnRep_State and Tests/Weapons/knife_rules_test.cpp.
// Cosmetic only: nothing here can apply damage.
namespace VSKnifeCueRules
{
// The replicated counters as one client received them.
struct Counters
{
    std::uint32_t attacks = 0;      // Server-accepted attacks this life; attack N is index N.
    std::uint32_t impacts = 0;      // Resolved impacts this life.
    std::uint32_t cancels = 0;      // Lifecycle cancellations this life.
    std::uint32_t impactAttack = 0; // Index of the attack the latest impact resolved.
    std::uint32_t cancelAttack = 0; // Attacks accepted when the latest cancellation happened.
};

enum class Cue : std::uint8_t { Start, Impact, Cancel };

struct Step
{
    Cue cue = Cue::Start;
    std::uint32_t attack = 0;
};

struct Steps
{
    std::array<Step, 3> items{};
    int count = 0;
};

// Baseline versus live updates. Adopt() is the baseline: the state a client
// first receives with the pawn (spawn, late join, relevancy return) is history
// and never plays. Every later change is live and plays once, in attack order.
class Watch
{
public:
    void Adopt(const Counters& Value)
    {
        Seen = Value;
        bBaselined = true;
    }

    bool IsBaselined() const { return bBaselined; }

    // Cues for a live update, ordered by attack index (start before its own
    // impact or cancel). A start that the same update already shows cancelled
    // never plays, so nothing presents after a lifecycle cancellation. Several
    // starts in one update play the latest once, as before.
    Steps Observe(const Counters& Now)
    {
        Steps Out;
        if (!bBaselined)
        {
            Adopt(Now);
            return Out;
        }
        const bool bStart = Now.attacks != Seen.attacks;
        const bool bImpact = Now.impacts != Seen.impacts;
        const bool bCancel = Now.cancels != Seen.cancels;
        const bool bStartCancelled = bStart && bCancel && Now.cancelAttack >= Now.attacks;
        Step Pending[3];
        int Count = 0;
        if (bImpact) { Pending[Count++] = {Cue::Impact, Now.impactAttack}; }
        if (bCancel) { Pending[Count++] = {Cue::Cancel, Now.cancelAttack}; }
        if (bStart && !bStartCancelled) { Pending[Count++] = {Cue::Start, Now.attacks}; }
        // Insertion sort by (attack, Start < Impact < Cancel).
        for (int Index = 0; Index < Count; ++Index)
        {
            for (int Other = Index + 1; Other < Count; ++Other)
            {
                if (Before(Pending[Other], Pending[Index]))
                {
                    const Step Swap = Pending[Index];
                    Pending[Index] = Pending[Other];
                    Pending[Other] = Swap;
                }
            }
        }
        for (int Index = 0; Index < Count; ++Index) { Out.items[static_cast<std::size_t>(Index)] = Pending[Index]; }
        Out.count = Count;
        Seen = Now;
        return Out;
    }

private:
    static bool Before(const Step& A, const Step& B)
    {
        return A.attack != B.attack ? A.attack < B.attack : static_cast<int>(A.cue) < static_cast<int>(B.cue);
    }

    Counters Seen;
    bool bBaselined = false;
};
}
