#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Combat/VSWeaponLoadoutComponent.h"
#include "VSRiflePresentationState.h"
#include "VSWeaponPresentationComponent.generated.h"

class AVSCharacter;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Cosmetic first-person and observer views of one loadout weapon. Polls public
 * replicated state; never sends gameplay requests. Visible only while its slot is
 * equipped and the owner is alive. Accepted shots before attachment, while
 * stowed or before a revival are never replayed. No collision or navigation.
 */
UCLASS(Abstract, ClassGroup = (Presentation))
class VALSTRIKE_API UVSWeaponPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVSWeaponPresentationComponent();

    // Call after Super::BeginPlay, or when attaching to an already live pawn.
    // Both anchors must belong to this component's owning character. Units: cm.
    bool Initialize(AVSCharacter* Character, USceneComponent* FirstPersonAnchor, USceneComponent* WorldAnchor);
    void Uninitialize();
    bool IsInitialized() const { return Character.IsValid(); }
    EVSWeaponSlot GetSlot() const { return Slot; }
    // Equipped, available and alive; the matching first-person/world view renders.
    bool IsShown() const { return bVisible; }
    // 0 = fully lowered at the start of an equip, 1 = ready pose.
    float GetEquipBlend() const { return EquipBlend; }
    uint32 GetShotPulseCount() const { return ShotPulseCount; }
    uint32 GetReloadStartCount() const { return ReloadStartCount; }
    bool IsFlashActive() const { return FlashRemaining > 0.0f; }
    bool IsReloadPoseActive() const { return bReloading; }
    // World-space Muzzle socket of the view this instance renders (first person
    // for the local viewer, world model otherwise). False while hidden.
    bool GetMuzzleTransform(FTransform& Out) const;
    const UStaticMeshComponent* GetFirstPersonBody() const { return FirstPersonBody; }
    const UStaticMeshComponent* GetWorldBody() const { return WorldBody; }
    const UStaticMeshComponent* GetFirstPersonMagazine() const { return FirstPersonMagazine; }
    const UStaticMeshComponent* GetWorldMagazine() const { return WorldMagazine; }
    const UStaticMeshComponent* GetFirstPersonFlash() const { return FirstPersonFlash; }
    const UStaticMeshComponent* GetWorldFlash() const { return WorldFlash; }
    const UStaticMeshComponent* GetFirstPersonSlide() const { return FirstPersonSlide; }
    const UStaticMeshComponent* GetWorldSlide() const { return WorldSlide; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

    // Per-weapon configuration, set by concrete constructors.
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    EVSWeaponSlot Slot = EVSWeaponSlot::Rifle;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    TObjectPtr<UStaticMesh> BodyAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    TObjectPtr<UStaticMesh> MagazineAsset;
    // Optional; attached to the body's Slide socket and kicked back per shot.
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    TObjectPtr<UStaticMesh> SlideAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    TObjectPtr<UStaticMesh> FlashAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    TObjectPtr<UMaterialInterface> FlashMaterial;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    TArray<FName> RequiredSockets;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    FTransform FirstPersonGrip;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    FTransform WorldGrip;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    FVector FlashScale = FVector(0.13f, 0.045f, 0.045f);
    // Magazine offset at full reload pose, relative to its socket.
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    FVector ReloadMagazineOffset = FVector(-3.0f, 0.0f, -13.0f);
    UPROPERTY(EditDefaultsOnly, Category = "Weapon Presentation")
    float SlideKickDistance = 0.0f;
    // Short prefix for created component names, e.g. R01.
    FString VisualPrefix = TEXT("Weapon");

private:
    VSRiflePresentation::Snapshot ReadSnapshot() const;
    bool IsEquippingThisSlot() const;
    bool IsFirstPersonView() const;
    UStaticMeshComponent* MakeVisual(const TCHAR* Name, UStaticMesh* Mesh, USceneComponent* Parent, FName Socket, bool bFirstPerson);
    void ApplyPose();

    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonBody;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonMagazine;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonSlide;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonFlash;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldBody;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldMagazine;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldSlide;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldFlash;
    TWeakObjectPtr<AVSCharacter> Character;
    VSRiflePresentation::State State;
    float FlashRemaining = 0;
    float KickRemaining = 0;
    float ReloadAge = 0;
    float EquipAge = 0;
    float EquipBlend = 1;
    bool bVisible = false;
    bool bReloading = false;
    uint32 ShotPulseCount = 0;
    uint32 ReloadStartCount = 0;
};
