#include "VSCharacterPresentationComponent.h"

#include "../VSCharacter.h"
#include "../Combat/VSHealthComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

const FName UVSCharacterPresentationComponent::RifleGripBone(TEXT("hand_r"));
// grip_derivation in docs/assets/meshy-pilot-integration/character-import-report.json.
const FTransform UVSCharacterPresentationComponent::RifleGripOffset(
    FQuat(-0.814922, 0.579475, 0.00887, 0.00564).GetNormalized(), FVector(4.1008, -13.5549, -6.1893));

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
}

bool UVSCharacterPresentationComponent::Initialize(USkeletalMeshComponent* InBody, TArrayView<UPrimitiveComponent* const> PrototypeVisuals)
{
    bActive = false;
    SetComponentTickEnabled(false);
    if (!InBody || InBody->GetOwner() != GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) { return false; }
    USkeletalMesh* Mesh = MeshAsset.LoadSynchronous();
    Hold = HoldAsset.LoadSynchronous();
    Walk = WalkAsset.LoadSynchronous();
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
    bWalkPlaying = false;
    bShown = true;
    InBody->PlayAnimation(Hold, true);
    SetComponentTickEnabled(true);
    return true;
}

void UVSCharacterPresentationComponent::PlayLocomotion(bool bMoving, float PlayRate)
{
    USkeletalMeshComponent* Mesh = Body.Get();
    if (!Mesh) { return; }
    if (bMoving != bWalkPlaying && (!bMoving || Walk))
    {
        Mesh->PlayAnimation(bMoving ? Walk.Get() : Hold.Get(), true);
        bWalkPlaying = bMoving;
    }
    Mesh->SetPlayRate(bWalkPlaying ? PlayRate : 1.0f);
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

    // Simulated proxies receive this velocity through CharacterMovement replication.
    const float Speed = Pawn->GetVelocity().Size2D();
    const float PlayRate = Speed > WalkClipSpeed ? FMath::Min(Speed / WalkClipSpeed, MaxWalkPlayRate) : 1.0f;
    PlayLocomotion(Speed > 20.0f, PlayRate);
}
