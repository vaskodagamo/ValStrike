#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "VSDamageModel.h"

class AController;
class APawn;
class UPrimitiveComponent;

/** One authoritative weapon hit as the server resolved it, for UVSDamageSubsystem::ApplyWeaponHit. */
struct FVSWeaponHit
{
    APawn* Shooter = nullptr;                     // Pawn that fired; its controller is the instigator.
    APawn* Victim = nullptr;                      // Pawn owning the hit component.
    const UPrimitiveComponent* HitComponent = nullptr; // FHitResult::GetComponent() of the server trace.
    ValStrike::Damage::WeaponClass Weapon = ValStrike::Damage::WeaponClass::Rifle;
    float DistanceCm = 0.0f;                      // FHitResult::Distance of the server trace.
    float AttackDamage = 0.0f;                    // Melee attack damage (knife); 0 for firearms.
};

/**
 * Server-only notification of health removed by an attributed hit. Broadcast
 * after the victim's UVSHealthComponent applied the damage, so the victim's
 * OnLifeStateChanged has already fired for a kill. Pointers are valid only
 * during the broadcast; copy weak pointers if you keep them.
 */
struct FVSDamageEvent
{
    AController* Instigator = nullptr;
    APawn* InstigatorPawn = nullptr;
    AController* VictimController = nullptr;
    APawn* Victim = nullptr;
    ValStrike::Damage::HitZone Zone = ValStrike::Damage::HitZone::None;
    ValStrike::Damage::WeaponClass Weapon = ValStrike::Damage::WeaponClass::Rifle;
    float DistanceCm = 0.0f;
    float RequestedDamage = 0.0f; // Damage model output (LethalDamage for a lethal head hit).
    float AppliedDamage = 0.0f;   // Health actually removed after the server clamp.
    bool bLethalRule = false;     // The weapon's lethal-head rule fired.
    bool bKilled = false;         // This hit took the victim from alive to dead.

    // Never true for the knife (flat damage on every zone).
    bool IsHeadshot() const { return ValStrike::Damage::IsHeadshotHit(Weapon, Zone); }
};

DECLARE_MULTICAST_DELEGATE_OneParam(FVSDamageEventSignature, const FVSDamageEvent&);
