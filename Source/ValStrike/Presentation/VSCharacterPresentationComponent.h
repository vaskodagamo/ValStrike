#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSCharacterPresentationComponent.generated.h"

class UAnimSequence;
class UPrimitiveComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * Cosmetic third-person body for the Meshy Urban Entry pilot character (INT-ART-01).
 * Puts the native skeletal mesh on the character's existing mesh component and
 * drives an in-place idle/walk from the pawn's replicated velocity. Root motion is
 * never used; CharacterMovement stays the only source of displacement. Capsule,
 * camera, hit zones and authority are untouched. When the native assets are
 * missing, or on a dedicated server, nothing changes and the prototype visuals stay.
 *
 * With the pistol equipped the body holds ART-23's stationary pistol pose, also
 * while moving (no pistol locomotion clip). Temporary pilot fallbacks (no approved
 * clips): every rifle movement direction plays the forward rifle walk, crouch keeps
 * the standing pose, and the body is hidden while dead. No jump/fire/reload/death.
 */
UCLASS(ClassGroup = (Presentation))
class VALSTRIKE_API UVSCharacterPresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVSCharacterPresentationComponent();

    // Call from BeginPlay. Body must belong to this component's owner. The prototype
    // visuals stop rendering but keep their transforms, so existing bounds checks hold.
    bool Initialize(USkeletalMeshComponent* Body, TArrayView<UPrimitiveComponent* const> PrototypeVisuals);
    bool IsNativeCharacterActive() const { return bActive; }

    // R02 world model on hand_r in ART-15's rifle hold, converted to the native
    // skeleton (measured: FBX import negates Y). UE 5.8 Python cannot author a named
    // skeletal socket, so it lives here; see weapon-replacement02/weapons-import-report.json.
    static const FName RifleGripBone;
    static const FTransform RifleGripOffset;
    // P02 world model on hand_r in ART-23's pistol hold (measured, INT-ART-04
    // weapons-import-report.json). Use only when HasPistolPose().
    static const FTransform PistolGripOffset;
    bool HasPistolPose() const { return bActive && PistolHold != nullptr; }

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<USkeletalMesh> MeshAsset;
    // Hold while still, forward rifle walk while moving (a direct switch; a
    // Python-authored blend space left the body in its bind pose at runtime).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> HoldAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> WalkAsset;
    // Stationary pistol ready hold, played whenever the pistol is equipped.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> PistolHoldAsset;
    // Ground speed of the in-place walk clip, measured from its stance foot (cm/s).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "10"))
    float WalkClipSpeed = 145.0f;
    // Faster movement speeds the walk up to this rate; any remainder is foot slide.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "1"))
    float MaxWalkPlayRate = 1.8f;

private:
    void PlayClip(UAnimSequence* Clip, float PlayRate);

    TWeakObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Hold;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> PistolHold;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Current;
    bool bActive = false;
    bool bShown = true;
};
