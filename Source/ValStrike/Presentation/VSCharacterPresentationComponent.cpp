#include "VSCharacterPresentationComponent.h"

#include "../VSCharacter.h"
#include "../Combat/VSHealthComponent.h"
#include "../Combat/VSWeaponLoadoutComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

const FName UVSCharacterPresentationComponent::RifleGripBone(TEXT("hand_r"));
// R02 on the ART-15 rifle hold: grips.R02_on_rifle_hold in
// docs/assets/weapon-replacement02/weapons-import-report.json (ART-17 hand-relative fit;
// R01's former offset was (4.1008,-13.5549,-6.1893), quat (-0.814922,0.579475,0.00887,0.00564)).
const FTransform UVSCharacterPresentationComponent::RifleGripOffset(
    FQuat(-0.772515, 0.63491, 0.009241, 0.005007).GetNormalized(), FVector(2.3021, -15.0518, -5.7134));

// P02 on ART-23's pistol hold: grips.P02_on_pistol_hold in the same report (P02 at
// (38,3,134); P01's former offset was (4.2582,-12.9002,-5.2861), same rotation).
const FTransform UVSCharacterPresentationComponent::PistolGripOffset(
    FQuat(-0.70704, 0.707095, 0.009685, 0.004084).GetNormalized(), FVector(4.2820, -12.9586, -2.2868));

namespace
{
    const TCHAR* CharacterRoot = TEXT("/Game/Art/Characters/UrbanEntry01");
}

UVSCharacterPresentationComponent::UVSCharacterPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(false);
    MeshAsset = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(FString::Printf(TEXT("%s/SK_VS_UrbanEntry01.SK_VS_UrbanEntry01"), CharacterRoot)));
    HoldAsset = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("%s/Animations/A_UE01_Pilot_RifleHold.A_UE01_Pilot_RifleHold"), CharacterRoot)));
    WalkAsset = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("%s/Animations/A_UE01_Pilot_Walk_RifleHold.A_UE01_Pilot_Walk_RifleHold"), CharacterRoot)));
    PistolHoldAsset = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(FString::Printf(TEXT("%s/Animations/A_UE01_Pilot_PistolHold.A_UE01_Pilot_PistolHold"), CharacterRoot)));
}

bool UVSCharacterPresentationComponent::Initialize(USkeletalMeshComponent* InBody, TArrayView<UPrimitiveComponent* const> PrototypeVisuals)
{
    bActive = false;
    SetComponentTickEnabled(false);
    if (!InBody || InBody->GetOwner() != GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) { return false; }
    USkeletalMesh* Mesh = MeshAsset.LoadSynchronous();
    Hold = HoldAsset.LoadSynchronous();
    Walk = WalkAsset.LoadSynchronous();
    PistolHold = PistolHoldAsset.LoadSynchronous();
    // The hold is the minimum usable pose; without it keep the prototype rather
    // than showing an unanimated bind pose.
    if (!Mesh || !Hold) { return false; }

    Body = InBody;
    InBody->SetSkeletalMesh(Mesh);
    InBody->SetOwnerNoSee(true);
    InBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    // Owner never sees its own body, so it need not pose it.
    InBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    for (UPrimitiveComponent* Visual : PrototypeVisuals)
    {
        if (Visual) { Visual->SetVisibility(false); }
    }
    bActive = true;
    bShown = true;
    Current = nullptr;
    PlayClip(Hold, 1.0f);
    SetComponentTickEnabled(true);
    return true;
}

void UVSCharacterPresentationComponent::PlayClip(UAnimSequence* Clip, float PlayRate)
{
    USkeletalMeshComponent* Mesh = Body.Get();
    if (!Mesh || !Clip) { return; }
    if (Clip != Current)
    {
        Mesh->PlayAnimation(Clip, true);
        Current = Clip;
    }
    Mesh->SetPlayRate(PlayRate);
}

void UVSCharacterPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    USkeletalMeshComponent* Mesh = Body.Get();
    const AVSCharacter* Pawn = Cast<AVSCharacter>(GetOwner());
    if (!bActive || !Mesh || !Pawn) { return; }

    const bool bAlive = Pawn->GetHealthComponent() && Pawn->GetHealthComponent()->IsAlive();
    if (bAlive != bShown)
    {
        Mesh->SetVisibility(bAlive);
        bShown = bAlive;
    }
    if (!bAlive) { return; }

    // Same slot source as the weapon views, so pose and visible weapon agree.
    if (PistolHold && Pawn->GetEquippedWeaponSlot() == EVSWeaponSlot::Pistol)
    {
        PlayClip(PistolHold, 1.0f);
        return;
    }
    // Simulated proxies receive this velocity through CharacterMovement replication.
    const float Speed = Pawn->GetVelocity().Size2D();
    const bool bMoving = Speed > 20.0f && Walk;
    const float PlayRate = Speed > WalkClipSpeed ? FMath::Min(Speed / WalkClipSpeed, MaxWalkPlayRate) : 1.0f;
    PlayClip(bMoving ? Walk.Get() : Hold.Get(), bMoving ? PlayRate : 1.0f);
}
