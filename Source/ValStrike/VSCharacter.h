#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VSHUDData.h"
#include "VSMovementRules.h"
#include "VSCharacter.generated.h"

class UCameraComponent;
class UPrimitiveComponent;
class UVSRifleComponent;
class UVSPistolComponent;
class UVSKnifeComponent;
class UVSWeaponComponent;
class UVSWeaponLoadoutComponent;
class UVSHealthComponent;
class UVSHitZoneSet;
class UVSRiflePresentationComponent;
class UVSPistolPresentationComponent;
class UVSCharacterPresentationComponent;
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
    explicit AVSCharacter(const FObjectInitializer& ObjectInitializer);

    FVSPlayerHUDData GetHUDData() const;
    // Release held combat input when a menu takes focus; gameplay stays server-owned.
    void StopCombatInput();
    UVSRifleComponent* GetRifleComponent() const { return Rifle; }
    UVSPistolComponent* GetPistolComponent() const { return Pistol; }
    UVSWeaponLoadoutComponent* GetLoadoutComponent() const { return Loadout; }
    // Always carried; LMB slashes and RMB stabs while the knife slot is equipped.
    UVSKnifeComponent* GetKnifeComponent() const { return Knife; }
    // Fixed loadout: every slot always has its weapon; null only for invalid slots.
    UVSWeaponComponent* GetWeapon(EVSWeaponSlot Slot) const;
    UVSWeaponComponent* GetEquippedWeapon() const;
    EVSWeaponSlot GetEquippedWeaponSlot() const;
    // Weapon weight for movement speed: this machine's equipped slot (the owner's
    // prediction, server state elsewhere), read inside every predicted move.
    VSMovementRules::WeaponKind GetMovementWeaponKind() const;
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
    // Hidden locational hit zones; the server's weapon traces resolve damage against them.
    UVSHitZoneSet* GetHitZones() const { return HitZones; }
    bool IsHeadHit(const UPrimitiveComponent* Component) const;
    virtual FVector GetPawnViewLocation() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void CheckJumpInput(float DeltaTime) override;

protected:
    // Jumping is allowed while crouched (CS2 Ctrl+Space crouch-jump).
    virtual bool CanJumpInternal_Implementation() const override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CreateInputMappings();
    void BeginFire();
    void BeginSecondary();
    void BeginReload();
    void EquipRifle();
    void EquipPistol();
    void EquipKnife();
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
    TObjectPtr<UVSKnifeComponent> Knife;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UVSHealthComponent> Health;

    UPROPERTY(VisibleAnywhere, Category = "Combat")
    TObjectPtr<UVSHitZoneSet> HitZones;

    UPROPERTY(VisibleAnywhere, Category = "View")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    // Cosmetic R01/P01 views. Initialized only on non-dedicated instances.
    UPROPERTY(VisibleAnywhere, Category = "Presentation")
    TObjectPtr<UVSRiflePresentationComponent> RiflePresentation;

    UPROPERTY(VisibleAnywhere, Category = "Presentation")
    TObjectPtr<UVSPistolPresentationComponent> PistolPresentation;

    // Native third-person pilot body on GetMesh(); cosmetic, non-dedicated only.
    UPROPERTY(VisibleAnywhere, Category = "Presentation")
    TObjectPtr<UVSCharacterPresentationComponent> CharacterPresentation;

    UPROPERTY(VisibleAnywhere, Category = "Prototype")
    TObjectPtr<UStaticMeshComponent> BodyVisual;

    UPROPERTY(VisibleAnywhere, Category = "Prototype")
    TObjectPtr<UStaticMeshComponent> HeadVisual;

    // Fallback only: an AVSPlayerController supplies the local player's saved
    // setting (VSUserSettings, same 0.12 default). Never replicated.
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
    TObjectPtr<UInputAction> SecondaryAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ReloadAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipRifleAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipPistolAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipKnifeAction;

    UPROPERTY(Transient)
    TObjectPtr<UInputAction> EquipLastAction;

    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> InputSubsystem;
    bool bWalkHeld = false;
};
