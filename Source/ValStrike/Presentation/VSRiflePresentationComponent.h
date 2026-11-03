#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSRiflePresentationState.h"
#include "VSRiflePresentationComponent.generated.h"

class AVSCharacter;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** Cosmetic only. Polls public replicated state; never sends gameplay requests. */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSRiflePresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVSRiflePresentationComponent();

    // Call after Super::BeginPlay, or when attaching to an already live pawn.
    // Both anchors must belong to this component's owning character. Units: cm.
    bool Initialize(AVSCharacter* Character, USceneComponent* FirstPersonAnchor, USceneComponent* WorldAnchor);
    void Uninitialize();
    bool IsInitialized() const { return Character.IsValid(); }
    uint32 GetShotPulseCount() const { return ShotPulseCount; }
    uint32 GetReloadStartCount() const { return ReloadStartCount; }
    bool IsFlashActive() const { return FlashRemaining > 0.0f; }
    bool IsReloadPoseActive() const { return bReloading; }
    const UStaticMeshComponent* GetFirstPersonBody() const { return FirstPersonBody; }
    const UStaticMeshComponent* GetWorldBody() const { return WorldBody; }
    const UStaticMeshComponent* GetFirstPersonMagazine() const { return FirstPersonMagazine; }
    const UStaticMeshComponent* GetWorldMagazine() const { return WorldMagazine; }
    const UStaticMeshComponent* GetFirstPersonFlash() const { return FirstPersonFlash; }
    const UStaticMeshComponent* GetWorldFlash() const { return WorldFlash; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
    VSRiflePresentation::Snapshot ReadSnapshot() const;
    UStaticMeshComponent* MakeVisual(FName Name, UStaticMesh* Mesh, USceneComponent* Parent, FName Socket, bool bFirstPerson);
    void ApplyPose(float DeltaTime);

    UPROPERTY(EditDefaultsOnly, Category = "Rifle Presentation")
    TObjectPtr<UStaticMesh> BodyAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle Presentation")
    TObjectPtr<UStaticMesh> MagazineAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle Presentation")
    TObjectPtr<UStaticMesh> FlashAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle Presentation")
    TObjectPtr<UMaterialInterface> FlashMaterial;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle Presentation")
    FTransform FirstPersonGrip = FTransform(FRotator(-3, -2, 0), FVector(35, 17, -20));
    UPROPERTY(EditDefaultsOnly, Category = "Rifle Presentation")
    FTransform WorldGrip = FTransform(FRotator::ZeroRotator, FVector(35, 20, 112));

    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonBody;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonMagazine;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonFlash;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldBody;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldMagazine;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldFlash;
    TWeakObjectPtr<AVSCharacter> Character;
    VSRiflePresentation::State State;
    float FlashRemaining = 0;
    float KickRemaining = 0;
    float ReloadAge = 0;
    bool bVisible = false;
    bool bReloading = false;
    uint32 ShotPulseCount = 0;
    uint32 ReloadStartCount = 0;
};
