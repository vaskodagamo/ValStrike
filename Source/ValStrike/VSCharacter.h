#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VSHUDData.h"
#include "VSCharacter.generated.h"

class UCameraComponent;
class UBoxComponent;
class USphereComponent;
class UPrimitiveComponent;
class UVSRifleComponent;
class UVSPistolComponent;
class UVSWeaponComponent;
class UVSWeaponLoadoutComponent;
class UVSHealthComponent;
class UVSRiflePresentationComponent;
class UVSPistolPresentationComponent;
class UVSWeaponPresentationComponent;
enum class EVSWeaponSlot : uint8;
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

    FVSPlayerHUDData GetHUDData() const;
    // Release held combat input when a menu takes focus; gameplay stays server-owned.
    void StopCombatInput();
    UVSRifleComponent* GetRifleComponent() const { return Rifle; }
    UVSPistolComponent* GetPistolComponent() const { return Pistol; }
    UVSWeaponLoadoutComponent* GetLoadoutComponent() const { return Loadout; }
    // Fixed loadout: every slot always has its weapon; null only for invalid slots.
    UVSWeaponComponent* GetWeapon(EVSWeaponSlot Slot) const;
    UVSWeaponComponent* GetEquippedWeapon() const;
    EVSWeaponSlot GetEquippedWeaponSlot() const;
    // Read-only server-accepted shots by one weapon (replicated). Each weapon's
    // counter only advances while it is equipped; cosmetic consumers such as
    // tracers can watch both counters independently.
    uint32 GetAcceptedShotCount(EVSWeaponSlot Slot) const;
    // Cosmetic weapon views (initialized only on non-dedicated instances).
    UVSWeaponPresentationComponent* GetWeaponPresentation(EVSWeaponSlot Slot) const;
    UVSHealthComponent* GetHealthComponent() const { return Health; }
    // Practice defaults stay open. Match mode drives these gates on the server;
    // replicated gates also suppress local prediction/input while locked.
    bool CanMatchMove() const;
    bool CanMatchCombat() const;
    void SetMatchGates(bool bMove, bool bCombat);
    bool IsHeadHit(const UPrimitiveComponent* Component) const;
    virtual FVector GetPawnViewLocation() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CreateInputMappings();
    void BeginFire();
    void BeginReload();
    void EquipRifle();
    void EquipPistol();
    void EquipLast();
    void HandleHealthChanged();
    void BeginJump();
    void Move(const FInputActionValue& Value);
    void LookYaw(const FInputActionValue& Value);
    void LookPitch(const FInputActionValue& Value);
    void BeginCrouch();
    void EndCrouch();
    void BeginWalk();
    void EndWalk();
    void UpdatePrototypeVisuals();

    UFUNCTION()
    void OnRep_MatchGates();

    UPROPERTY(ReplicatedUsing = OnRep_MatchGates)
    bool bMatchMovementAllowed = true;

    UPROPERTY(ReplicatedUsing = OnRep_MatchGates)
    bool bMatchCombatAllowed = true;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UVSRifleComponent> Rifle;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UVSPistolComponent> Pistol;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UVSWeaponLoadoutComponent> Loadout;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UVSHealthComponent> Health;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UBoxComponent> BodyHitRegion;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<USphereComponent> HeadHitRegion;

    UPROPERTY(VisibleAnywhere, Category = "View")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    // Cosmetic R01/P01 views. Initialized only on non-dedicated instances.
    UPROPERTY(VisibleAnywhere, Category = "Presentation")
    TObjectPtr<UVSRiflePresentationComponent> RiflePresentation;

    UPROPERTY(VisibleAnywhere, Category = "Presentation")
    TObjectPtr<UVSPistolPresentationComponent> PistolPresentation;

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

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> FireAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ReloadAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipRifleAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipPistolAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipLastAction;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    bool bWalkHeld = false;
};
