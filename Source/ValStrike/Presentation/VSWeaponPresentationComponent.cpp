#include "VSWeaponPresentationComponent.h"

#include "../VSCharacter.h"
#include "../Combat/VSHealthComponent.h"
#include "../Combat/VSWeaponComponent.h"
#include "../Combat/VSWeaponLoadoutComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UVSWeaponPresentationComponent::UVSWeaponPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    SetIsReplicatedByDefault(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Flash(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashSurface(TEXT("/Game/Presentation/Rifle01/M_VSRifleFlash.M_VSRifleFlash"));
    FlashAsset = Flash.Object;
    // This fixed required asset participates in startup shader preparation and
    // cooking alongside the weapon meshes, rather than first loading mid-game.
    FlashMaterial = FlashSurface.Object;
}

VSRiflePresentation::Snapshot UVSWeaponPresentationComponent::ReadSnapshot() const
{
    const AVSCharacter* Pawn = Character.Get();
    const UVSWeaponComponent* Weapon = Pawn ? Pawn->GetWeapon(Slot) : nullptr;
    if (!Weapon) { return {}; }
    const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent();
    const UVSHealthComponent* Health = Pawn->GetHealthComponent();
    // A stowed weapon is unavailable: its view hides, and equipping it again
    // starts a fresh baseline instead of replaying anything.
    const bool bEquipped = !Loadout || Loadout->GetEquippedSlot() == Slot;
    return {Health != nullptr && bEquipped, Health && Health->IsAlive(), Weapon->GetState().bReloading, Weapon->GetState().ShotsFired};
}

bool UVSWeaponPresentationComponent::IsEquippingThisSlot() const
{
    const AVSCharacter* Pawn = Character.Get();
    const UVSWeaponLoadoutComponent* Loadout = Pawn ? Pawn->GetLoadoutComponent() : nullptr;
    return Loadout && Loadout->GetEquippedSlot() == Slot && Loadout->IsEquipping();
}

bool UVSWeaponPresentationComponent::IsFirstPersonView() const
{
    const AVSCharacter* Pawn = Character.Get();
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    return Pawn && Pawn->IsLocallyControlled() && PC && PC->GetViewTarget() == Pawn;
}

UStaticMeshComponent* UVSWeaponPresentationComponent::MakeVisual(const TCHAR* Name, UStaticMesh* Mesh,
    USceneComponent* Parent, FName Socket, bool bFirstPerson)
{
    const FName BaseName(*(VisualPrefix + Name));
    UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(GetOwner(), MakeUniqueObjectName(GetOwner(), UStaticMeshComponent::StaticClass(), BaseName));
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

bool UVSWeaponPresentationComponent::Initialize(AVSCharacter* Pawn, USceneComponent* FirstPersonAnchor, USceneComponent* WorldAnchor)
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
    for (const FName& Socket : RequiredSockets)
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
    // A late attachment mid-equip raises over the full duration; the true start is unknown.
    EquipAge = 0;
    EquipBlend = 1;
    FirstPersonBody = MakeVisual(TEXT("FirstPersonBody"), BodyAsset, FirstPersonAnchor, NAME_None, true);
    FirstPersonMagazine = MakeVisual(TEXT("FirstPersonMagazine"), MagazineAsset, FirstPersonBody, TEXT("Magazine"), true);
    FirstPersonFlash = MakeVisual(TEXT("FirstPersonFlash"), FlashAsset, FirstPersonBody, TEXT("Muzzle"), true);
    WorldBody = MakeVisual(TEXT("WorldBody"), BodyAsset, WorldAnchor, NAME_None, false);
    WorldMagazine = MakeVisual(TEXT("WorldMagazine"), MagazineAsset, WorldBody, TEXT("Magazine"), false);
    WorldFlash = MakeVisual(TEXT("WorldFlash"), FlashAsset, WorldBody, TEXT("Muzzle"), false);
    if (SlideAsset)
    {
        FirstPersonSlide = MakeVisual(TEXT("FirstPersonSlide"), SlideAsset, FirstPersonBody, TEXT("Slide"), true);
        WorldSlide = MakeVisual(TEXT("WorldSlide"), SlideAsset, WorldBody, TEXT("Slide"), false);
    }
    for (UStaticMeshComponent* Flash : {FirstPersonFlash.Get(), WorldFlash.Get()})
    {
        Flash->SetMaterial(0, Material);
        Flash->SetRelativeLocation(FVector(4, 0, 0));
        Flash->SetRelativeScale3D(FlashScale);
        Flash->SetCastShadow(false);
    }
    ApplyPose();
    SetComponentTickEnabled(true);
    return true;
}

void UVSWeaponPresentationComponent::Uninitialize()
{
    SetComponentTickEnabled(false);
    for (UStaticMeshComponent* Visual : {FirstPersonFlash.Get(), FirstPersonSlide.Get(), FirstPersonMagazine.Get(), FirstPersonBody.Get(),
        WorldFlash.Get(), WorldSlide.Get(), WorldMagazine.Get(), WorldBody.Get()})
    {
        if (Visual)
        {
            GetOwner()->RemoveInstanceComponent(Visual);
            Visual->DestroyComponent();
        }
    }
    FirstPersonBody = FirstPersonMagazine = FirstPersonSlide = FirstPersonFlash = nullptr;
    WorldBody = WorldMagazine = WorldSlide = WorldFlash = nullptr;
    Character.Reset();
    State = VSRiflePresentation::State();
    FlashRemaining = KickRemaining = ReloadAge = EquipAge = 0;
    EquipBlend = 1;
    bVisible = bReloading = false;
    ShotPulseCount = ReloadStartCount = 0;
}

void UVSWeaponPresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    Uninitialize();
    Super::EndPlay(Reason);
}

void UVSWeaponPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime, TickType, TickFunction);
    if (!Character.IsValid()) { Uninitialize(); return; }
    FlashRemaining = FMath::Max(0.0f, FlashRemaining - DeltaTime);
    KickRemaining = FMath::Max(0.0f, KickRemaining - DeltaTime * 9.0f);
    const VSRiflePresentation::Update Update = State.Observe(ReadSnapshot());
    const bool bWasVisible = bVisible;
    bVisible = Update.visible;
    bReloading = Update.reloading;
    if (!bVisible)
    {
        FlashRemaining = KickRemaining = ReloadAge = EquipAge = 0;
        EquipBlend = 1;
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
        // Raise from a lowered pose over the equip time, timed from when this
        // view first observed the replicated equip (includes replication delay).
        if (IsEquippingThisSlot())
        {
            EquipAge = bWasVisible ? EquipAge + DeltaTime : 0.0f;
            const AVSCharacter* Pawn = Character.Get();
            const float Duration = Pawn && Pawn->GetLoadoutComponent() ? Pawn->GetLoadoutComponent()->GetEquipDuration() : 0.0f;
            EquipBlend = Duration > 0.0f ? FMath::Clamp(EquipAge / Duration, 0.0f, 1.0f) : 1.0f;
        }
        else
        {
            EquipAge = 0;
            EquipBlend = 1;
        }
    }
    ApplyPose();
}

void UVSWeaponPresentationComponent::ApplyPose()
{
    if (!Character.IsValid() || !FirstPersonBody || !WorldBody) { return; }
    const bool bFirstPerson = IsFirstPersonView();
    const float ReloadBlend = bReloading ? FMath::Clamp(ReloadAge / 0.3f, 0.0f, 1.0f) : 0;
    const float Lowered = 1.0f - FMath::SmoothStep(0.0f, 1.0f, EquipBlend);
    // All motion is relative mesh presentation. Camera/control/trace aim is untouched.
    for (UStaticMeshComponent* Body : {FirstPersonBody.Get(), WorldBody.Get()})
    {
        const bool bFP = Body == FirstPersonBody;
        FTransform Pose = bFP ? FirstPersonGrip : WorldGrip;
        Pose.AddToTranslation(FVector(-2.0f * KickRemaining, 0, -3.0f * ReloadBlend - 14.0f * Lowered));
        Pose.SetRotation(Pose.GetRotation() * FRotator(1.8f * KickRemaining - 12.0f * ReloadBlend - 30.0f * Lowered, 0, -8.0f * ReloadBlend).Quaternion());
        Body->SetRelativeTransform(Pose);
        Body->SetVisibility(bVisible && (bFP ? bFirstPerson : !bFirstPerson), false);
    }
    for (UStaticMeshComponent* Magazine : {FirstPersonMagazine.Get(), WorldMagazine.Get()})
    {
        Magazine->SetRelativeLocation(ReloadMagazineOffset * ReloadBlend);
        Magazine->SetRelativeRotation(FRotator(0, 0, 8.0f * ReloadBlend));
        Magazine->SetVisibility(bVisible && (Magazine == FirstPersonMagazine ? bFirstPerson : !bFirstPerson), false);
    }
    for (UStaticMeshComponent* Slide : {FirstPersonSlide.Get(), WorldSlide.Get()})
    {
        if (!Slide) { continue; }
        Slide->SetRelativeLocation(FVector(-SlideKickDistance * KickRemaining, 0, 0));
        Slide->SetVisibility(bVisible && (Slide == FirstPersonSlide ? bFirstPerson : !bFirstPerson), false);
    }
    FirstPersonFlash->SetVisibility(bVisible && bFirstPerson && FlashRemaining > 0, false);
    WorldFlash->SetVisibility(bVisible && !bFirstPerson && FlashRemaining > 0, false);
}

bool UVSWeaponPresentationComponent::GetMuzzleTransform(FTransform& Out) const
{
    if (!bVisible || !FirstPersonBody || !WorldBody) { return false; }
    const UStaticMeshComponent* Body = IsFirstPersonView() ? FirstPersonBody.Get() : WorldBody.Get();
    Out = Body->GetSocketTransform(TEXT("Muzzle"));
    return true;
}
