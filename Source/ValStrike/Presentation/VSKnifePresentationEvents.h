#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "../VSCharacter.h"
#include "../Combat/VSKnifeComponent.h"
#include "../Combat/VSKnifeRules.h"
#include "../Combat/VSWeaponLoadoutComponent.h"

/**
 * Gameplay-owned cosmetic knife event API (KNIFE-01), frozen for native binding
 * (INT-ART-03). A native presentation component owns one FVSKnifePresentationEvents,
 * binds it to its character, calls Tick() once per frame and plays first-person
 * or third-person animation, effects and hand visibility from OnCue().
 *
 * Every cue is cosmetic. Damage and hit results come only from the server
 * (UVSKnifeComponent); nothing here sends requests or changes gameplay state.
 *
 * Cues, in the order a viewer sees them:
 * - Equip:   the knife slot became equipped in this machine's view (the owning
 *            client's prediction, server state elsewhere). Duration = equip time.
 * - Holster: the knife slot was left (switch, round reset). Stop knife animations.
 * - Slash / Stab: an attack started. bLocal for the owning client's own press
 *            (it may still be rejected); otherwise the server accepted it.
 *            Duration = seconds until the impact marker; Recovery = until the next
 *            attack. Animations must place their impact at Duration.
 * - Impact:  the server resolved an impact; Result is Miss, Wall or Player.
 * - Cancel:  pending impacts were cancelled (switch, death, reset, closed phase).
 *            Stop the attack before its impact.
 */
enum class EVSKnifeCue : uint8
{
    Equip,
    Holster,
    Slash,
    Stab,
    Impact,
    Cancel,
};

struct FVSKnifeCueEvent
{
    EVSKnifeCue Cue = EVSKnifeCue::Equip;
    EVSKnifeImpact Result = EVSKnifeImpact::None; // Impact only.
    bool bLocal = false;      // Owning client's own prediction (Equip, Slash, Stab).
    float Duration = 0.0f;    // Equip: equip time. Slash/Stab: seconds to impact.
    float Recovery = 0.0f;    // Slash/Stab: seconds to the next attack.
};

class FVSKnifePresentationEvents
{
public:
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnCue, const FVSKnifeCueEvent&);

    ~FVSKnifePresentationEvents() { Unbind(); }

    FOnCue& OnCue() { return Cue; }

    // Binds to a character's knife. A knife already equipped at bind time (late
    // join, relevancy return) emits Equip on the next Tick with no attack replay.
    bool Bind(AVSCharacter* InCharacter)
    {
        Unbind();
        UVSKnifeComponent* KnifeComponent = InCharacter ? InCharacter->GetKnifeComponent() : nullptr;
        if (!KnifeComponent || !InCharacter->GetLoadoutComponent()) { return false; }
        Character = InCharacter;
        Knife = KnifeComponent;
        bKnifeShown = false;
        Handle = KnifeComponent->OnKnifeEvent().AddRaw(this, &FVSKnifePresentationEvents::HandleKnifeEvent);
        return true;
    }

    void Unbind()
    {
        if (UVSKnifeComponent* KnifeComponent = Knife.Get()) { KnifeComponent->OnKnifeEvent().Remove(Handle); }
        Handle.Reset();
        Knife.Reset();
        Character.Reset();
        bKnifeShown = false;
    }

    // Emits Equip/Holster from the loadout view. Call once per frame.
    void Tick()
    {
        const AVSCharacter* Pawn = Character.Get();
        const UVSWeaponLoadoutComponent* Loadout = Pawn ? Pawn->GetLoadoutComponent() : nullptr;
        const bool bShown = Loadout && Loadout->IsKnifeEquipped();
        if (bShown == bKnifeShown) { return; }
        bKnifeShown = bShown;
        FVSKnifeCueEvent Event;
        Event.Cue = bShown ? EVSKnifeCue::Equip : EVSKnifeCue::Holster;
        Event.bLocal = Pawn && Pawn->IsLocallyControlled();
        Event.Duration = bShown ? Loadout->GetEquipDuration() : 0.0f;
        Cue.Broadcast(Event);
    }

    bool IsKnifeShown() const { return bKnifeShown; }

private:
    void HandleKnifeEvent(EVSKnifeEvent Kind, EVSKnifeAttack Attack, EVSKnifeImpact Result)
    {
        FVSKnifeCueEvent Event;
        switch (Kind)
        {
        case EVSKnifeEvent::LocalStart:
        case EVSKnifeEvent::Start:
        {
            const VSKnifeRules::Tuning& Values = Attack == EVSKnifeAttack::Slash ? VSKnifeRules::Slash : VSKnifeRules::Stab;
            Event.Cue = Attack == EVSKnifeAttack::Slash ? EVSKnifeCue::Slash : EVSKnifeCue::Stab;
            Event.bLocal = Kind == EVSKnifeEvent::LocalStart;
            Event.Duration = static_cast<float>(Values.impactTime);
            Event.Recovery = static_cast<float>(Values.nextAttack);
            break;
        }
        case EVSKnifeEvent::Impact:
            Event.Cue = EVSKnifeCue::Impact;
            Event.Result = Result;
            break;
        case EVSKnifeEvent::Cancel:
            Event.Cue = EVSKnifeCue::Cancel;
            break;
        }
        Cue.Broadcast(Event);
    }

    FOnCue Cue;
    TWeakObjectPtr<AVSCharacter> Character;
    TWeakObjectPtr<UVSKnifeComponent> Knife;
    FDelegateHandle Handle;
    bool bKnifeShown = false;
};
