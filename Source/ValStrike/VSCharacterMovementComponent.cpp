#include "VSCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "VSCharacter.h"
#include "VSMovementRules.h"

float UVSCharacterMovementComponent::GetWeaponSpeedMultiplier() const
{
    const AVSCharacter* Character = Cast<AVSCharacter>(CharacterOwner);
    return static_cast<float>(VSMovementRules::SpeedMultiplier(
        Character ? Character->GetMovementWeaponKind() : VSMovementRules::WeaponKind::Unarmed));
}

float UVSCharacterMovementComponent::GetMaxSpeed() const
{
    const float Base = Super::GetMaxSpeed();
    switch (MovementMode)
    {
    case MOVE_Walking:
    case MOVE_NavWalking:
    case MOVE_Falling:
        return Base * GetWeaponSpeedMultiplier();
    default:
        return Base;
    }
}

bool UVSCharacterMovementComponent::CanAttemptJump() const
{
    // The engine refuses a jump while crouch is held; CS2 allows it (Ctrl+Space).
    return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}

void UVSCharacterMovementComponent::Crouch(bool bClientSimulation)
{
    // Simulated proxies follow replicated location; only predicted moves tuck.
    const bool bAirTuck = !bClientSimulation && IsFalling() && HasValidData();
    const float OldHalfHeight = bAirTuck ? CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.0f;
    Super::Crouch(bClientSimulation);
    if (!bAirTuck || !CharacterOwner->bIsCrouched)
    {
        return;
    }
    const float Shrink = OldHalfHeight - CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    const float Drop = static_cast<float>(VSMovementRules::AirTuckDrop(Shrink, VSMovementRules::CrouchJumpGainTarget))
        * CharacterOwner->GetCapsuleComponent()->GetShapeScale();
    if (Drop > 0.0f)
    {
        // Swept, so a tuck just above a floor or ledge stops on it.
        FHitResult Hit;
        SafeMoveUpdatedComponent(Drop * GetGravityDirection(), UpdatedComponent->GetComponentQuat(), true, Hit);
    }
}
