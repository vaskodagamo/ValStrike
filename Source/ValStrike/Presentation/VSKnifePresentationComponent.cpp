#include "VSKnifePresentationComponent.h"

#include "VSCharacterPresentationComponent.h"
#include "../VSCharacter.h"
#include "../Combat/VSHealthComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogVSKnifePresentation, Log, All);

namespace
{
    const TCHAR* KnifePresentationArmsRoot = TEXT("/Game/Art/Characters/FPArms03");

    template <typename T>
    TSoftObjectPtr<T> KnifePresentationAsset(const TCHAR* Root, const TCHAR* Name, const TCHAR* Folder = TEXT(""))
    {
        return TSoftObjectPtr<T>(FSoftObjectPath(FString::Printf(TEXT("%s%s/%s.%s"), Root, Folder, Name, Name)));
    }

    const TCHAR* KnifePresentationCueName(EVSKnifeCue Cue)
    {
        switch (Cue)
        {
        case EVSKnifeCue::Equip: return TEXT("Equip");
        case EVSKnifeCue::Holster: return TEXT("Holster");
        case EVSKnifeCue::Slash: return TEXT("Slash");
        case EVSKnifeCue::Stab: return TEXT("Stab");
        case EVSKnifeCue::Impact: return TEXT("Impact");
        case EVSKnifeCue::Cancel: return TEXT("Cancel");
        }
        return TEXT("?");
    }
}

const FName UVSKnifePresentationComponent::ArmsKnifeBone(TEXT("weapon_socket_r"));
// first_person.offset_on_weapon_socket_r: constant over every frame of all five clips.
const FTransform UVSKnifePresentationComponent::FirstPersonKnifeOffset(
    FQuat(0.0, -0.948683, -0.316228, 0.0).GetNormalized(), FVector(-1.5, -6.88, -1.16));
const FName UVSKnifePresentationComponent::WorldKnifeBone(TEXT("hand_r"));
// third_person.offset_on_hand_r: ART-27 Knife_Hold frame 1 on the native UrbanEntry skeleton.
const FTransform UVSKnifePresentationComponent::WorldKnifeOffset(
    FQuat(-0.565402, -0.404277, 0.316081, -0.645735).GetNormalized(), FVector(2.109, -12.1092, 0.6281));

UVSKnifePresentationComponent::UVSKnifePresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    SetIsReplicatedByDefault(false);
    ArmsAsset = KnifePresentationAsset<USkeletalMesh>(KnifePresentationArmsRoot, TEXT("SK_FPArms03_KnifePilot"));
    KnifeAsset = KnifePresentationAsset<UStaticMesh>(TEXT("/Game/Art/Weapons/Knife01"), TEXT("SM_VS_Knife01"), TEXT("/Meshes"));
    ArmsEquipAsset = KnifePresentationAsset<UAnimSequence>(KnifePresentationArmsRoot, TEXT("A_FPArms03_KnifeEquip"), TEXT("/Animations"));
    ArmsIdleAsset = KnifePresentationAsset<UAnimSequence>(KnifePresentationArmsRoot, TEXT("A_FPArms03_KnifeIdle"), TEXT("/Animations"));
    ArmsLocomotionAsset = KnifePresentationAsset<UAnimSequence>(KnifePresentationArmsRoot, TEXT("A_FPArms03_KnifeLocomotion"), TEXT("/Animations"));
    ArmsSlashAsset = KnifePresentationAsset<UAnimSequence>(KnifePresentationArmsRoot, TEXT("A_FPArms03_KnifeSlash"), TEXT("/Animations"));
    ArmsStabAsset = KnifePresentationAsset<UAnimSequence>(KnifePresentationArmsRoot, TEXT("A_FPArms03_KnifeStab"), TEXT("/Animations"));
}

bool UVSKnifePresentationComponent::Initialize(AVSCharacter* InCharacter, USceneComponent* FirstPersonAnchor, USkeletalMeshComponent* Body)
{
    Uninitialize();
    AActor* Owner = GetOwner();
    if (!InCharacter || InCharacter != Owner || !FirstPersonAnchor || FirstPersonAnchor->GetOwner() != Owner) { return false; }
    if (Owner->GetNetMode() == NM_DedicatedServer) { return false; }
    USkeletalMesh* ArmsMesh = ArmsAsset.LoadSynchronous();
    UStaticMesh* Knife = KnifeAsset.LoadSynchronous();
    ArmsEquip = ArmsEquipAsset.LoadSynchronous();
    ArmsIdle = ArmsIdleAsset.LoadSynchronous();
    ArmsLocomotion = ArmsLocomotionAsset.LoadSynchronous();
    ArmsSlash = ArmsSlashAsset.LoadSynchronous();
    ArmsStab = ArmsStabAsset.LoadSynchronous();
    if (!ArmsMesh || !Knife || !ArmsEquip || !ArmsIdle || !ArmsLocomotion || !ArmsSlash || !ArmsStab) { return false; }
    if (!Events.Bind(InCharacter)) { return false; }
    Character = InCharacter;
    BodyPresentation = Owner->FindComponentByClass<UVSCharacterPresentationComponent>();

    Arms = NewObject<USkeletalMeshComponent>(Owner, MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), TEXT("K01FirstPersonArms")));
    Owner->AddInstanceComponent(Arms);
    Arms->SetSkeletalMesh(ArmsMesh);
    Arms->SetMobility(EComponentMobility::Movable);
    Arms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Arms->SetGenerateOverlapEvents(false);
    Arms->SetCanEverAffectNavigation(false);
    Arms->SetCastShadow(false);
    Arms->SetOnlyOwnerSee(true);
    Arms->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    // Posed even while hidden, so the first visible frame after equip is never the bind pose
    // (a 40-bone evaluation; skinning still only happens when rendered).
    Arms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Arms->SetupAttachment(FirstPersonAnchor);
    Arms->SetRelativeTransform(ArmsFromCamera);
    Arms->SetVisibility(false);
    Arms->RegisterComponent();

    auto MakeKnife = [&](const TCHAR* Name, USceneComponent* Parent, FName Bone, const FTransform& Offset, bool bFirstPerson)
    {
        UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(Owner, MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), Name));
        Owner->AddInstanceComponent(Visual);
        Visual->SetStaticMesh(Knife);
        Visual->SetMobility(EComponentMobility::Movable);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetGenerateOverlapEvents(false);
        Visual->SetCanEverAffectNavigation(false);
        Visual->SetCastShadow(!bFirstPerson);
        Visual->SetOnlyOwnerSee(bFirstPerson);
        Visual->SetOwnerNoSee(!bFirstPerson);
        Visual->SetupAttachment(Parent, Bone);
        Visual->SetRelativeTransform(Offset);
        Visual->SetVisibility(false);
        Visual->RegisterComponent();
        return Visual;
    };
    FirstPersonKnife = MakeKnife(TEXT("K01FirstPersonKnife"), Arms, ArmsKnifeBone, FirstPersonKnifeOffset, true);
    // Only the native UrbanEntry body has the measured hand_r; the prototype keeps no world knife.
    const UVSCharacterPresentationComponent* BodyView = BodyPresentation.Get();
    if (Body && Body->GetOwner() == Owner && BodyView && BodyView->HasKnifePose() && Body->GetBoneIndex(WorldKnifeBone) != INDEX_NONE)
    {
        WorldKnife = MakeKnife(TEXT("K01WorldKnife"), Body, WorldKnifeBone, WorldKnifeOffset, false);
    }
    Events.OnCue().AddUObject(this, &UVSKnifePresentationComponent::HandleCue);
    SetComponentTickEnabled(true);
    return true;
}

void UVSKnifePresentationComponent::Uninitialize()
{
    SetComponentTickEnabled(false);
    Events.OnCue().RemoveAll(this);
    Events.Unbind();
    for (UPrimitiveComponent* Visual : {static_cast<UPrimitiveComponent*>(FirstPersonKnife.Get()), static_cast<UPrimitiveComponent*>(WorldKnife.Get()),
        static_cast<UPrimitiveComponent*>(Arms.Get())})
    {
        if (Visual)
        {
            GetOwner()->RemoveInstanceComponent(Visual);
            Visual->DestroyComponent();
        }
    }
    Arms = nullptr;
    FirstPersonKnife = WorldKnife = nullptr;
    ArmsCurrent = nullptr;
    ArmsActionRemaining = 0.0f;
    Character.Reset();
    BodyPresentation.Reset();
}

void UVSKnifePresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Uninitialize();
    Super::EndPlay(Reason);
}

bool UVSKnifePresentationComponent::IsFirstPersonView() const
{
    const AVSCharacter* Pawn = Character.Get();
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    return Pawn && Pawn->IsLocallyControlled() && PC && PC->GetViewTarget() == Pawn;
}

void UVSKnifePresentationComponent::PlayArms(UAnimSequence* Clip, bool bLoop)
{
    if (!Arms || !Clip) { return; }
    // One-shots always restart; loops only change when the clip changes.
    if (!bLoop || Clip != ArmsCurrent)
    {
        Arms->PlayAnimation(Clip, bLoop);
        ArmsCurrent = bLoop ? Clip : nullptr;
    }
}

void UVSKnifePresentationComponent::HandleCue(const FVSKnifeCueEvent& Event)
{
    UVSCharacterPresentationComponent* Body = BodyPresentation.Get();
    UAnimSequence* Clip = nullptr;
    switch (Event.Cue)
    {
    case EVSKnifeCue::Equip:
        Clip = ArmsEquip;
        if (Body) { Body->PlayKnifeAction(EVSKnifeBodyAction::Equip); }
        break;
    case EVSKnifeCue::Slash:
    case EVSKnifeCue::Stab:
        // The authored impact frame is at the cue's Duration (0.15 s / 0.30 s).
        Clip = Event.Cue == EVSKnifeCue::Slash ? ArmsSlash : ArmsStab;
        if (Body) { Body->PlayKnifeAction(Event.Cue == EVSKnifeCue::Slash ? EVSKnifeBodyAction::Slash : EVSKnifeBodyAction::Stab); }
        break;
    case EVSKnifeCue::Holster:
    case EVSKnifeCue::Cancel:
        ArmsActionRemaining = 0.0f;
        ArmsCurrent = nullptr;
        if (Body) { Body->PlayKnifeAction(EVSKnifeBodyAction::Stop); }
        break;
    case EVSKnifeCue::Impact:
        break;
    }
    if (Clip)
    {
        PlayArms(Clip, false);
        ArmsActionRemaining = Clip->GetPlayLength();
    }
    UE_LOG(LogVSKnifePresentation, Log, TEXT("%s cue=%s local=%d result=%d duration=%.2f view=%s"), *GetNameSafe(GetOwner()),
        KnifePresentationCueName(Event.Cue), Event.bLocal ? 1 : 0, static_cast<int32>(Event.Result), Event.Duration,
        IsFirstPersonView() ? TEXT("first_person") : TEXT("third_person"));
}

void UVSKnifePresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    const AVSCharacter* Pawn = Character.Get();
    if (!Pawn) { Uninitialize(); return; }
    Events.Tick(); // Equip/Holster from this machine's loadout view.
    const UVSHealthComponent* Health = Pawn->GetHealthComponent();
    const bool bShown = Events.IsKnifeShown() && Health && Health->IsAlive();
    const bool bFirstPerson = IsFirstPersonView();
    if (Arms)
    {
        Arms->SetVisibility(bShown && bFirstPerson, false);
        FirstPersonKnife->SetVisibility(bShown && bFirstPerson, false);
    }
    if (WorldKnife) { WorldKnife->SetVisibility(bShown && !bFirstPerson, false); }
    if (!bShown) { ArmsActionRemaining = 0.0f; return; }
    if (ArmsActionRemaining > 0.0f)
    {
        ArmsActionRemaining -= DeltaTime;
        if (ArmsActionRemaining > 0.0f) { return; }
        ArmsCurrent = nullptr;
    }
    const bool bMoving = Pawn->GetVelocity().Size2D() > 20.0f;
    PlayArms(bMoving ? ArmsLocomotion.Get() : ArmsIdle.Get(), true);
}

// ---- third-person knife body ------------------------------------------------------

struct FVSKnifeBodyAnimProxy : public FAnimInstanceProxy
{
    explicit FVSKnifeBodyAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

protected:
    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
    {
        FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
        const UVSKnifeBodyAnimInstance* Source = CastChecked<UVSKnifeBodyAnimInstance>(InAnimInstance);
        Base = Source->Base;
        Action = Source->Action;
        BaseTime = Source->BaseTime;
        ActionTime = Source->ActionTime;
        ActionAlpha = Source->ActionAlpha;
    }

    virtual bool Evaluate(FPoseContext& Output) override
    {
        if (!Base)
        {
            Output.ResetToRefPose();
            return true;
        }
        FAnimationPoseData BaseData(Output);
        Base->GetAnimationPose(BaseData, FAnimExtractContext(static_cast<double>(BaseTime)));
        if (!Action || ActionAlpha <= 0.0f) { return true; }
        FPoseContext Upper(this);
        FAnimationPoseData UpperData(Upper);
        Action->GetAnimationPose(UpperData, FAnimExtractContext(static_cast<double>(ActionTime)));
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        const FReferenceSkeleton& Reference = Bones.GetReferenceSkeleton();
        const int32 Spine = Reference.FindBoneIndex(TEXT("spine_01"));
        if (Spine == INDEX_NONE) { return true; }
        for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
        {
            const int32 MeshIndex = Bones.MakeMeshPoseIndex(Index).GetInt();
            if (MeshIndex == Spine || Reference.BoneIsChildOf(MeshIndex, Spine))
            {
                FTransform Blended;
                Blended.Blend(Output.Pose[Index], Upper.Pose[Index], ActionAlpha);
                Output.Pose[Index] = Blended;
            }
        }
        return true;
    }

private:
    const UAnimSequence* Base = nullptr;
    const UAnimSequence* Action = nullptr;
    float BaseTime = 0.0f;
    float ActionTime = 0.0f;
    float ActionAlpha = 0.0f;
};

void UVSKnifeBodyAnimInstance::SetBase(UAnimSequence* Clip, float PlayRate)
{
    BaseRate = PlayRate;
    if (Clip == Base) { return; }
    // Keep the normalized gait phase when switching between hold/walk/run.
    const float OldLength = Base ? Base->GetPlayLength() : 0.0f;
    const float NewLength = Clip ? Clip->GetPlayLength() : 0.0f;
    BaseTime = OldLength > 0.0f && NewLength > 0.0f ? BaseTime / OldLength * NewLength : 0.0f;
    Base = Clip;
}

void UVSKnifeBodyAnimInstance::PlayAction(UAnimSequence* Clip)
{
    if (!Clip) { return; }
    Action = Clip;
    ActionTime = 0.0f;
    bActionStopping = false;
}

void UVSKnifeBodyAnimInstance::StopAction()
{
    if (Action) { bActionStopping = true; }
}

void UVSKnifeBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    constexpr float BlendIn = 0.05f;  // Full weight well before the 0.15 s slash impact.
    constexpr float BlendOut = 0.1f;
    if (Base)
    {
        const float Length = Base->GetPlayLength();
        BaseTime = Length > 0.0f ? FMath::Fmod(BaseTime + DeltaSeconds * BaseRate, Length) : 0.0f;
    }
    if (!Action) { return; }
    const float Length = Action->GetPlayLength();
    ActionTime = FMath::Min(ActionTime + DeltaSeconds, Length);
    if (bActionStopping || ActionTime >= Length)
    {
        ActionAlpha -= DeltaSeconds / BlendOut;
        if (ActionAlpha <= 0.0f)
        {
            ActionAlpha = 0.0f;
            Action = nullptr;
            bActionStopping = false;
        }
    }
    else
    {
        ActionAlpha = FMath::Min(1.0f, ActionAlpha + DeltaSeconds / BlendIn);
    }
}

FAnimInstanceProxy* UVSKnifeBodyAnimInstance::CreateAnimInstanceProxy()
{
    return new FVSKnifeBodyAnimProxy(this);
}

void UVSKnifeBodyAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
    delete static_cast<FVSKnifeBodyAnimProxy*>(InProxy);
}
