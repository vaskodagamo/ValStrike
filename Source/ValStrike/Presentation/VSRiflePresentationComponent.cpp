#include "VSRiflePresentationComponent.h"

#include "../VSCharacter.h"
#include "../Combat/VSRifleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UVSRiflePresentationComponent::UVSRiflePresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    SetIsReplicatedByDefault(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Game/Art/Weapons/Rifle01/Meshes/SM_VS_Rifle01_Body.SM_VS_Rifle01_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Magazine(TEXT("/Game/Art/Weapons/Rifle01/Meshes/SM_VS_Rifle01_Magazine.SM_VS_Rifle01_Magazine"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Flash(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashSurface(TEXT("/Game/Presentation/Rifle01/M_VSRifleFlash.M_VSRifleFlash"));
    BodyAsset = Body.Object;
    MagazineAsset = Magazine.Object;
    FlashAsset = Flash.Object;
    // This fixed required asset participates in startup shader preparation and
    // cooking alongside the body/magazine, rather than first loading mid-game.
    FlashMaterial = FlashSurface.Object;
}

VSRiflePresentation::Snapshot UVSRiflePresentationComponent::ReadSnapshot() const
{
    const AVSCharacter* Pawn = Character.Get();
    if (!Pawn || !Pawn->GetRifleComponent()) { return {}; }
    const FVSPlayerHUDData HUD = Pawn->GetHUDData();
    return {HUD.bHasHealth && HUD.bHasWeapon, HUD.bAlive, HUD.bReloading, Pawn->GetRifleComponent()->GetState().ShotsFired};
}

UStaticMeshComponent* UVSRiflePresentationComponent::MakeVisual(FName Name, UStaticMesh* Mesh,
    USceneComponent* Parent, FName Socket, bool bFirstPerson)
{
    UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), Name));
    GetOwner()->AddInstanceComponent(Visual);
    Visual->SetStaticMesh(Mesh);
    Visual->SetMobility(EComponentMobility::Movable);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetGenerateOverlapEvents(false);
    Visual->SetCanEverAffectNavigation(false);
    Visual->SetCastShadow(!bFirstPerson);
    Visual->SetOnlyOwnerSee(bFirstPerson);
    Visual->SetOwnerNoSee(!bFirstPerson);
    Visual->SetupAttachment(Parent, Socket);
    Visual->RegisterComponent();
    return Visual;
}

bool UVSRiflePresentationComponent::Initialize(AVSCharacter* Pawn, USceneComponent* FirstPersonAnchor, USceneComponent* WorldAnchor)
{
    Uninitialize();
    if (!Pawn || GetOwner() != Pawn || !FirstPersonAnchor || !WorldAnchor
        || FirstPersonAnchor->GetOwner() != Pawn || WorldAnchor->GetOwner() != Pawn) { return false; }
    if (Pawn->GetNetMode() == NM_DedicatedServer) { return false; } // No visual work on the server.
    UMaterialInterface* Material = FlashMaterial;
    if (!BodyAsset || !MagazineAsset || !FlashAsset || !Material) { return false; }
    // UE's uncooked on-demand shader path can otherwise leave a first, brief
    // pulse using the grey fallback indefinitely. Prepare this one required
    // surface at attachment, never in the per-frame/fire path. The engine's
    // completion work is editor-only; packaged builds use cooked shaders.
    Material->EnsureIsComplete();
    for (FName Socket : {FName(TEXT("Grip")), FName(TEXT("Muzzle")), FName(TEXT("SupportHand")), FName(TEXT("Magazine"))})
    {
        if (!BodyAsset->FindSocket(Socket)) { return false; }
    }
    Character = Pawn;
    const VSRiflePresentation::Update Initial = State.Initialize(ReadSnapshot());
    bVisible = Initial.visible;
    bReloading = Initial.reloading;
    // A late attachment in an ongoing reload holds the withdrawn pose; it does
    // not replay an old reload-start animation or invent an authoritative timer.
    ReloadAge = bReloading ? 0.4f : 0.0f;
    FirstPersonBody = MakeVisual(TEXT("R01FirstPersonBody"), BodyAsset, FirstPersonAnchor, NAME_None, true);
    FirstPersonMagazine = MakeVisual(TEXT("R01FirstPersonMagazine"), MagazineAsset, FirstPersonBody, TEXT("Magazine"), true);
    FirstPersonFlash = MakeVisual(TEXT("R01FirstPersonFlash"), FlashAsset, FirstPersonBody, TEXT("Muzzle"), true);
    WorldBody = MakeVisual(TEXT("R01WorldBody"), BodyAsset, WorldAnchor, NAME_None, false);
    WorldMagazine = MakeVisual(TEXT("R01WorldMagazine"), MagazineAsset, WorldBody, TEXT("Magazine"), false);
    WorldFlash = MakeVisual(TEXT("R01WorldFlash"), FlashAsset, WorldBody, TEXT("Muzzle"), false);
    for (UStaticMeshComponent* Flash : {FirstPersonFlash.Get(), WorldFlash.Get()})
    {
        Flash->SetMaterial(0, Material);
        Flash->SetRelativeLocation(FVector(4, 0, 0));
        Flash->SetRelativeScale3D(FVector(0.13f, 0.045f, 0.045f));
        Flash->SetCastShadow(false);
    }
    ApplyPose(0);
    SetComponentTickEnabled(true);
    return true;
}

void UVSRiflePresentationComponent::Uninitialize()
{
    SetComponentTickEnabled(false);
    for (UStaticMeshComponent* Visual : {FirstPersonFlash.Get(), FirstPersonMagazine.Get(), FirstPersonBody.Get(),
        WorldFlash.Get(), WorldMagazine.Get(), WorldBody.Get()})
    {
        if (Visual)
        {
            GetOwner()->RemoveInstanceComponent(Visual);
            Visual->DestroyComponent();
        }
    }
    FirstPersonBody = FirstPersonMagazine = FirstPersonFlash = nullptr;
    WorldBody = WorldMagazine = WorldFlash = nullptr;
    Character.Reset();
    State = VSRiflePresentation::State();
    FlashRemaining = KickRemaining = ReloadAge = 0;
    bVisible = bReloading = false;
    ShotPulseCount = ReloadStartCount = 0;
}

void UVSRiflePresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Uninitialize();
    Super::EndPlay(Reason);
}

void UVSRiflePresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!Character.IsValid()) { Uninitialize(); return; }
    FlashRemaining = FMath::Max(0.0f, FlashRemaining - DeltaTime);
    KickRemaining = FMath::Max(0.0f, KickRemaining - DeltaTime * 9.0f);
    const VSRiflePresentation::Update Update = State.Observe(ReadSnapshot());
    bVisible = Update.visible;
    bReloading = Update.reloading;
    if (!bVisible)
    {
        FlashRemaining = KickRemaining = ReloadAge = 0;
    }
    else
    {
        if (Update.shotPulse)
        {
            ++ShotPulseCount;
            FlashRemaining = 0.065f;
            KickRemaining = 1.0f;
        }
        if (Update.reloadStarted) { ++ReloadStartCount; ReloadAge = 0; }
        if (bReloading) { ReloadAge += DeltaTime; FlashRemaining = KickRemaining = 0; }
        else { ReloadAge = 0; }
    }
    ApplyPose(DeltaTime);
}

void UVSRiflePresentationComponent::ApplyPose(float DeltaTime)
{
    const AVSCharacter* Pawn = Character.Get();
    if (!Pawn || !FirstPersonBody || !WorldBody) { return; }
    const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
    const bool bFirstPerson = Pawn->IsLocallyControlled() && PC && PC->GetViewTarget() == Pawn;
    const float ReloadBlend = bReloading ? FMath::Clamp(ReloadAge / 0.3f, 0.0f, 1.0f) : 0;
    // All motion is relative mesh presentation. Camera/control/trace aim is untouched.
    for (UStaticMeshComponent* Body : {FirstPersonBody.Get(), WorldBody.Get()})
    {
        const bool bFP = Body == FirstPersonBody;
        FTransform Pose = bFP ? FirstPersonGrip : WorldGrip;
        Pose.AddToTranslation(FVector(-2.0f * KickRemaining, 0, -3.0f * ReloadBlend));
        Pose.SetRotation(Pose.GetRotation() * FRotator(1.8f * KickRemaining - 12.0f * ReloadBlend, 0, -8.0f * ReloadBlend).Quaternion());
        Body->SetRelativeTransform(Pose);
        Body->SetVisibility(bVisible && (bFP ? bFirstPerson : !bFirstPerson), false);
    }
    for (UStaticMeshComponent* Magazine : {FirstPersonMagazine.Get(), WorldMagazine.Get()})
    {
        Magazine->SetRelativeLocation(FVector(-3.0f * ReloadBlend, 0, -13.0f * ReloadBlend));
        Magazine->SetRelativeRotation(FRotator(0, 0, 8.0f * ReloadBlend));
        Magazine->SetVisibility(bVisible && (Magazine == FirstPersonMagazine ? bFirstPerson : !bFirstPerson), false);
    }
    FirstPersonFlash->SetVisibility(bVisible && bFirstPerson && FlashRemaining > 0, false);
    WorldFlash->SetVisibility(bVisible && !bFirstPerson && FlashRemaining > 0, false);
}
