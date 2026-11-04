#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSCharacterPresentationComponent.generated.h"

class UAnimSequence;
class UPrimitiveComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UVSKnifeBodyAnimInstance;

/** Third-person knife body actions, cued by UVSKnifePresentationComponent. */
enum class EVSKnifeBodyAction : uint8
{
    Equip,
    Slash,
    Stab,
    Stop,
};

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
 * clips): every rifle movement direction plays ART-28's forward R02 walk or run,
 * crouch keeps the standing pose, and the body is hidden while dead. No
 * jump/fire/reload/death.
 *
 * With the knife equipped (INT-ART-03) the body uses ART-27's in-place knife hold,
 * walk or run, and equip/slash/stab play on the upper body (spine_01 and children)
 * over that base, so moving attacks keep the legs (UVSKnifeBodyAnimInstance).
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

    // R02 world model on hand_r in ART-28's fitted rifle hold/walk/run, measured on the
    // native skeleton (FBX import negates Y). UE 5.8 Python cannot author a named
    // skeletal socket, so it lives here; see knife-rifle-acceptance/rifle02-fit-import-report.json.
    static const FName RifleGripBone;
    static const FTransform RifleGripOffset;
    // P02 world model on hand_r in ART-23's pistol hold (measured, INT-ART-04
    // weapons-import-report.json). Use only when HasPistolPose().
    static const FTransform PistolGripOffset;
    bool HasPistolPose() const { return bActive && PistolHold != nullptr; }
    // ART-27 knife body; requires hold, equip, slash and stab (walk/run optional).
    bool HasKnifePose() const { return bActive && KnifeHold != nullptr; }
    // Cosmetic; Stop blends an action out. Ignored unless the knife body is active.
    void PlayKnifeAction(EVSKnifeBodyAction Action);

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<USkeletalMesh> MeshAsset;
    // ART-28 R02 hold while still, walk/run while moving (a direct switch; a
    // Python-authored blend space left the body in its bind pose at runtime).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> HoldAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> WalkAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> RunAsset;
    // Stationary pistol ready hold, played whenever the pistol is equipped.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation")
    TSoftObjectPtr<UAnimSequence> PistolHoldAsset;
    // ART-27 knife clips (INT-ART-03 import, root motion off).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife")
    TSoftObjectPtr<UAnimSequence> KnifeHoldAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife")
    TSoftObjectPtr<UAnimSequence> KnifeWalkAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife")
    TSoftObjectPtr<UAnimSequence> KnifeRunAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife")
    TSoftObjectPtr<UAnimSequence> KnifeEquipAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife")
    TSoftObjectPtr<UAnimSequence> KnifeSlashAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife")
    TSoftObjectPtr<UAnimSequence> KnifeStabAsset;
    // Stance-foot ground speeds of the in-place knife walk/run clips (measured,
    // knife-import-report.json) and the speed above which the run plays (cm/s).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife", meta = (ClampMin = "10"))
    float KnifeWalkClipSpeed = 138.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife", meta = (ClampMin = "10"))
    float KnifeRunClipSpeed = 287.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation|Knife", meta = (ClampMin = "10"))
    float KnifeRunThreshold = 210.0f;
    // Ground speed of the in-place walk clip, measured from its stance foot (cm/s).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "10"))
    float WalkClipSpeed = 138.0f;
    // Stance-foot speed of the in-place R02 run (same lower body as A_UE01_Running) and
    // the speed above which it plays (cm/s; rifle-fit import report).
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "10"))
    float RunClipSpeed = 287.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "10"))
    float RunThreshold = 210.0f;
    // Faster movement speeds the walk up to this rate; any remainder is foot slide.
    UPROPERTY(EditDefaultsOnly, Category = "Character Presentation", meta = (ClampMin = "1"))
    float MaxWalkPlayRate = 1.8f;

private:
    void PlayClip(UAnimSequence* Clip, float PlayRate);
    bool EnterKnifeMode();

    TWeakObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Hold;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Walk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Run;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> PistolHold;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Current;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> KnifeHold;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> KnifeWalk;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> KnifeRun;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> KnifeEquip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> KnifeSlash;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> KnifeStab;
    TWeakObjectPtr<UVSKnifeBodyAnimInstance> KnifeBody;
    bool bKnifeMode = false;
    bool bActive = false;
    bool bShown = true;
};
