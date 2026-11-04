#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSCharacterPresentationComponent.generated.h"

class UAnimSequence;
class UBlendSpace1D;
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
 * Temporary pilot fallbacks (no approved clips): every movement direction plays the
 * forward rifle walk, crouch keeps the standing pose, and the body is hidden while
 * dead. No jump/fire/reload/death animation.
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

    // R01 world model on hand_r. The offset is ART-15's hold reference converted
    // to the native skeleton (measured: FBX import negates Y). UE 5.8 Python cannot
    // author a named socket, so it lives here; see character-import-report.json.
    static const FName RifleGripBone;
    static const FTransform RifleGripOffset;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<USkeletalMesh> MeshAsset;
    // Preferred: 1D blend space, hold at 0 and rifle walk at WalkClipSpeed.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UBlendSpace1D> LocomotionAsset;
    // Fallback when the blend space is missing: switch between the two clips.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> HoldAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> WalkAsset;
    // Ground speed of the in-place walk clip, measured from its stance foot (cm/s).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "10"))
    float WalkClipSpeed = 145.0f;
    // Faster movement speeds the walk up to this rate; any remainder is foot slide.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "1"))
    float MaxWalkPlayRate = 1.8f;

private:
    void PlayLocomotion(bool bMoving, float PlayRate);

    TWeakObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UBlendSpace1D> Locomotion;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Hold;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    bool bActive = false;
    bool bWalkPlaying = false;
    bool bShown = true;
};
