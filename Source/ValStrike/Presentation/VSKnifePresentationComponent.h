#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Components/ActorComponent.h"
#include "VSKnifePresentationEvents.h"
#include "VSKnifePresentationComponent.generated.h"

class UAnimSequence;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVSCharacterPresentationComponent;
struct FVSKnifeBodyAnimProxy;

/**
 * Cosmetic K01 knife views (INT-ART-03). Consumes the frozen gameplay cue API
 * (FVSKnifePresentationEvents) and never sends requests or changes gameplay state.
 *
 * First person (owner, own view): the ART-26-C1 gloved FPArms03 pair on the camera
 * with K01 on weapon_socket_r; equip, idle/locomotion, slash and stab clips start
 * on their cues, so the authored impact frames land at the cue's impact time.
 * Third person (everyone else): K01 on the UrbanEntry hand_r; the body's ART-27
 * knife hold/locomotion and upper-body actions are driven through
 * UVSCharacterPresentationComponent. Holster, death and Cancel stop or hide them.
 * Both views stay hidden while the knife is not equipped, and on a dedicated server
 * nothing is created. Offsets are measured on the native skeletons (see
 * docs/assets/knife-integration/knife-import-report.json).
 */
UCLASS(ClassGroup = (Presentation))
class VALSTRIKE_API UVSKnifePresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVSKnifePresentationComponent();

    // Call from BeginPlay after the character presentation. FirstPersonAnchor is the
    // first-person camera; Body is the third-person mesh (K01 attaches to its hand_r
    // only when it carries the native UrbanEntry skeleton).
    bool Initialize(AVSCharacter* Character, USceneComponent* FirstPersonAnchor, USkeletalMeshComponent* Body);
    void Uninitialize();
    bool IsInitialized() const { return Character.IsValid(); }
    bool IsKnifeShown() const { return Events.IsKnifeShown(); }
    const USkeletalMeshComponent* GetFirstPersonArms() const { return Arms; }
    const UStaticMeshComponent* GetFirstPersonKnife() const { return FirstPersonKnife; }
    const UStaticMeshComponent* GetWorldKnife() const { return WorldKnife; }

    // Measured attachments (centimetres; knife-import-report.json).
    static const FName ArmsKnifeBone;
    static const FTransform FirstPersonKnifeOffset;
    static const FName WorldKnifeBone;
    static const FTransform WorldKnifeOffset;

protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<USkeletalMesh> ArmsAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<UStaticMesh> KnifeAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<UAnimSequence> ArmsEquipAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<UAnimSequence> ArmsIdleAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<UAnimSequence> ArmsLocomotionAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<UAnimSequence> ArmsSlashAsset;
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    TSoftObjectPtr<UAnimSequence> ArmsStabAsset;
    // ART-26 source framing: the arm component sits at (+20, 0, -5) cm from the
    // camera (source camera (-20, 0, 5) looking +X, 90 degree horizontal FOV).
    UPROPERTY(EditDefaultsOnly, Category = "Knife Presentation")
    FTransform ArmsFromCamera = FTransform(FVector(20.0f, 0.0f, -5.0f));

private:
    void HandleCue(const FVSKnifeCueEvent& Event);
    bool IsFirstPersonView() const;
    void PlayArms(UAnimSequence* Clip, bool bLoop);

    FVSKnifePresentationEvents Events;
    TWeakObjectPtr<AVSCharacter> Character;
    TWeakObjectPtr<UVSCharacterPresentationComponent> BodyPresentation;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Arms;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonKnife;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> WorldKnife;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmsEquip;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmsIdle;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmsLocomotion;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmsSlash;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmsStab;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> ArmsCurrent;
    // Seconds left of a one-shot first-person clip (equip/attack) before idle/locomotion.
    float ArmsActionRemaining = 0.0f;
};

/**
 * Native (graph-less) third-person knife body: a looping in-place base clip for
 * the whole body, with an optional one-shot action layered in local space on
 * spine_01 and its children (ART-27 contract), so attacks keep the moving legs.
 */
UCLASS(Transient, NotBlueprintable)
class VALSTRIKE_API UVSKnifeBodyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    // Gait phase carries over when the base clip changes.
    void SetBase(UAnimSequence* Clip, float PlayRate);
    // Restarts the action from its first frame.
    void PlayAction(UAnimSequence* Clip);
    // Blends the action out (cancel/holster).
    void StopAction();
    const UAnimSequence* GetAction() const { return Action; }

protected:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
    friend struct FVSKnifeBodyAnimProxy;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Base;
    UPROPERTY(Transient) TObjectPtr<UAnimSequence> Action;
    float BaseRate = 1.0f;
    float BaseTime = 0.0f;
    float ActionTime = 0.0f;
    float ActionAlpha = 0.0f;
    bool bActionStopping = false;
};
