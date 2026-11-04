#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VSCharacterMovementComponent.generated.h"

/**
 * CharacterMovement with ValStrike movement feel (VSMovementRules):
 * - Weapon weight: walking and falling max speed scale with the equipped weapon.
 *   The server reads its loadout state and the owning client its predicted slot,
 *   so steady movement on one weapon never disagrees.
 * - Crouch is allowed when a jump starts; a jump from a ground crouch launches
 *   standing (AVSCharacter::CheckJumpInput) and tucks again in the air.
 * - In-air crouch keeps the engine's center tuck minus AirTuckDrop, so the feet
 *   clear CS2's scaled crouch-jump gain without any added velocity.
 * Everything runs inside the predicted move on client and server alike.
 */
UCLASS()
class VALSTRIKE_API UVSCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    virtual float GetMaxSpeed() const override;
    virtual bool CanAttemptJump() const override;
    virtual void Crouch(bool bClientSimulation = false) override;

    float GetWeaponSpeedMultiplier() const;
};
