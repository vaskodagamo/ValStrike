#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VSCharacter.generated.h"

class UCameraComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class UStaticMeshComponent;
struct FInputActionValue;

/** First-person movement slice. CharacterMovement owns movement and replication. */
UCLASS()
class VALSTRIKE_API AVSCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AVSCharacter();

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CreateInputMappings();
    void Move(const FInputActionValue& Value);
    void LookYaw(const FInputActionValue& Value);
    void LookPitch(const FInputActionValue& Value);
    void BeginCrouch();
    void EndCrouch();
    void BeginWalk();
    void EndWalk();
    void UpdatePrototypeVisuals();

    UPROPERTY(VisibleAnywhere, Category = "View")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(VisibleAnywhere, Category = "Prototype")
    TObjectPtr<UStaticMeshComponent> BodyVisual;

    UPROPERTY(VisibleAnywhere, Category = "Prototype")
    TObjectPtr<UStaticMeshComponent> HeadVisual;

    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.01", ClampMax = "2.0"))
    float MouseSensitivity = 0.12f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float WalkInputScale = 0.45f;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> MovementMapping;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> LookYawAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> LookPitchAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> CrouchAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> WalkAction;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    bool bWalkHeld = false;
};
