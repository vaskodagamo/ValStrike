#include "VSHitZoneSet.h"

#include "../Combat/VSHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"

namespace
{
namespace VSD = ValStrike::Damage;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
TAutoConsoleVariable<int32> GVSHitZoneDrawCVar(
    TEXT("vs.Damage.DrawHitZones"), 0,
    TEXT("Development builds only. 1 draws the hidden hit zones of every other player (your own pawn stays hidden)."),
    ECVF_Cheat);
#endif

FColor VSHitZoneDebugColor(VSD::HitZone Zone)
{
    switch (Zone)
    {
    case VSD::HitZone::Head: return FColor(255, 48, 48);
    case VSD::HitZone::UpperTorso: return FColor(255, 150, 30);
    case VSD::HitZone::LowerTorso: return FColor(255, 230, 40);
    case VSD::HitZone::UpperArm: return FColor(40, 220, 255);
    case VSD::HitZone::Forearm: return FColor(60, 110, 255);
    case VSD::HitZone::Thigh: return FColor(60, 230, 90);
    case VSD::HitZone::LowerLeg: return FColor(170, 255, 60);
    case VSD::HitZone::Foot: return FColor(240, 70, 230);
    default: return FColor::White;
    }
}

FVector VSHitZoneVector(const std::array<double, 3>& Value) { return FVector(Value[0], Value[1], Value[2]); }
}

UVSHitZoneSet::UVSHitZoneSet()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    // After movement and network move processing have resized the capsule.
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    bAutoActivate = true;
    SetIsReplicatedByDefault(false);
    SetHiddenInGame(true);
    CurrentLayout = VSD::BuildLayout(88.0);
}

UVSHitZoneSet* UVSHitZoneSet::FindOn(const AActor* Actor)
{
    return Actor ? Actor->FindComponentByClass<UVSHitZoneSet>() : nullptr;
}

UVSHitZoneSet* UVSHitZoneSet::AddTo(ACharacter* Character, EVSHitZoneTraceMode Mode)
{
    if (!IsValid(Character) || !Character->GetCapsuleComponent())
    {
        return nullptr;
    }
    if (UVSHitZoneSet* Existing = FindOn(Character))
    {
        return Existing;
    }
    UVSHitZoneSet* Set = NewObject<UVSHitZoneSet>(Character,
        MakeUniqueObjectName(Character, StaticClass(), TEXT("VSHitZones")), RF_Transient);
    Set->TraceMode = Mode;
    Set->SetupAttachment(Character->GetCapsuleComponent());
    Character->AddInstanceComponent(Set);
    Set->RegisterComponent();
    return Set;
}

UCapsuleComponent* UVSHitZoneSet::FindCapsule() const
{
    if (UCapsuleComponent* Parent = Cast<UCapsuleComponent>(GetAttachParent()))
    {
        return Parent;
    }
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    return Character ? Character->GetCapsuleComponent() : nullptr;
}

float UVSHitZoneSet::ReadHalfHeight() const
{
    const UCapsuleComponent* Capsule = FindCapsule();
    return Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : AppliedHalfHeight;
}

void UVSHitZoneSet::BeginPlay()
{
    Super::BeginPlay();
    CreateShapes();
    if (UVSHealthComponent* Found = GetOwner() ? GetOwner()->FindComponentByClass<UVSHealthComponent>() : nullptr)
    {
        Health = Found;
        LifeHandle = Found->OnLifeStateChanged.AddUObject(this, &UVSHitZoneSet::HandleLifeStateChanged);
    }
    HandleLifeStateChanged();
    RefreshLayout();
}

void UVSHitZoneSet::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Health.IsValid() && LifeHandle.IsValid())
    {
        Health->OnLifeStateChanged.Remove(LifeHandle);
    }
    LifeHandle.Reset();
    Super::EndPlay(EndPlayReason);
}

void UVSHitZoneSet::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    if (!bDestroyingHierarchy)
    {
        for (UShapeComponent* Shape : Shapes)
        {
            if (IsValid(Shape)) { Shape->DestroyComponent(); }
        }
    }
    Shapes.Reset();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UVSHitZoneSet::CreateShapes()
{
    AActor* Owner = GetOwner();
    if (!Owner || Shapes.Num() > 0)
    {
        return;
    }
    const VSD::Layout Initial = VSD::BuildLayout(FMath::Max(ReadHalfHeight(), 17.0f));
    CurrentLayout = Initial.Valid ? Initial : VSD::BuildLayout(88.0);
    for (const VSD::ZoneVolume& Volume : CurrentLayout.Volumes)
    {
        const FString BaseName = FString(TEXT("VSHitZone_")) + UTF8_TO_TCHAR(Volume.Name);
        UShapeComponent* Shape = nullptr;
        if (Volume.Shape == VSD::ZoneShape::Sphere)
        {
            USphereComponent* Sphere = NewObject<USphereComponent>(Owner,
                MakeUniqueObjectName(Owner, USphereComponent::StaticClass(), FName(*BaseName)), RF_Transient);
            Sphere->InitSphereRadius(static_cast<float>(Volume.Radius));
            Shape = Sphere;
        }
        else
        {
            UBoxComponent* Box = NewObject<UBoxComponent>(Owner,
                MakeUniqueObjectName(Owner, UBoxComponent::StaticClass(), FName(*BaseName)), RF_Transient);
            Box->InitBoxExtent(VSHitZoneVector(Volume.HalfExtent));
            Shape = Box;
        }
        Shape->SetupAttachment(this);
        Shape->SetRelativeLocation(VSHitZoneVector(Volume.Center));
        Shape->SetMobility(EComponentMobility::Movable);
        Shape->SetCollisionObjectType(ECC_WorldDynamic);
        Shape->SetGenerateOverlapEvents(false);
        Shape->SetCanEverAffectNavigation(false);
        Shape->CanCharacterStepUpOn = ECB_No;
        Shape->bReturnMaterialOnMove = false;
        Shape->SetSimulatePhysics(false);
        Shape->SetEnableGravity(false);
        Shape->ShapeColor = VSHitZoneDebugColor(Volume.Zone);
        Shape->SetLineThickness(1.5f);
        // Never rendered as geometry; vs.Damage.DrawHitZones draws debug lines instead.
        Shape->SetHiddenInGame(true);
        Shape->SetOwnerNoSee(true);
        Shape->SetCastShadow(false);
        Shapes.Add(Shape);
    }
    ApplyCollision();
    for (UShapeComponent* Shape : Shapes)
    {
        Owner->AddInstanceComponent(Shape);
        Shape->RegisterComponent();
    }
    AppliedHalfHeight = static_cast<float>(CurrentLayout.CapsuleHalfHeight);
}

void UVSHitZoneSet::ApplyCollision()
{
    const ECollisionResponse Response = TraceMode == EVSHitZoneTraceMode::Authoritative ? ECR_Block : ECR_Overlap;
    for (UShapeComponent* Shape : Shapes)
    {
        if (!Shape) { continue; }
        Shape->SetCollisionResponseToAllChannels(ECR_Ignore);
        Shape->SetCollisionResponseToChannel(TraceChannel, Response);
        Shape->SetCollisionEnabled(bZonesEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
}

void UVSHitZoneSet::SetTraceMode(EVSHitZoneTraceMode Mode)
{
    if (TraceMode != Mode)
    {
        TraceMode = Mode;
        ApplyCollision();
    }
}

void UVSHitZoneSet::HandleLifeStateChanged()
{
    const bool bAlive = !Health.IsValid() || Health->IsAlive();
    if (bAlive != bZonesEnabled)
    {
        bZonesEnabled = bAlive;
        ApplyCollision();
    }
}

void UVSHitZoneSet::RefreshLayout()
{
    if (Shapes.Num() != static_cast<int32>(VSD::VolumeCount))
    {
        return;
    }
    const float HalfHeight = ReadHalfHeight();
    const VSD::Layout Next = VSD::BuildLayout(HalfHeight);
    if (!Next.Valid)
    {
        return;
    }
    CurrentLayout = Next;
    AppliedHalfHeight = HalfHeight;
    for (int32 Index = 0; Index < Shapes.Num(); ++Index)
    {
        const VSD::ZoneVolume& Volume = CurrentLayout.Volumes[Index];
        UShapeComponent* Shape = Shapes[Index];
        if (!Shape) { continue; }
        if (UBoxComponent* Box = Cast<UBoxComponent>(Shape))
        {
            Box->SetBoxExtent(VSHitZoneVector(Volume.HalfExtent), false);
        }
        else if (USphereComponent* Sphere = Cast<USphereComponent>(Shape))
        {
            Sphere->SetSphereRadius(static_cast<float>(Volume.Radius), false);
        }
        Shape->SetRelativeLocation(VSHitZoneVector(Volume.Center));
    }
}

void UVSHitZoneSet::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    const float HalfHeight = ReadHalfHeight();
    if (HalfHeight != AppliedHalfHeight)
    {
        RefreshLayout();
    }
    DrawDebugZones();
}

bool UVSHitZoneSet::IsDebugDrawRequested()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return GVSHitZoneDrawCVar.GetValueOnGameThread() != 0;
#else
    return false;
#endif
}

void UVSHitZoneSet::DrawDebugZones()
{
    bDebugDrawn = false;
#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    UWorld* World = GetWorld();
    const APawn* Pawn = Cast<APawn>(GetOwner());
    // Like OwnerNoSee: the local player's own zones would surround its camera.
    if (!IsDebugDrawRequested() || !World || World->GetNetMode() == NM_DedicatedServer || (Pawn && Pawn->IsLocallyControlled()))
    {
        return;
    }
    // Drawn 0.5 cm outside each shape so lines do not z-fight with the prototype meshes.
    constexpr float Inflate = 0.5f;
    for (int32 Index = 0; Index < Shapes.Num(); ++Index)
    {
        const UShapeComponent* Shape = Shapes[Index];
        if (!Shape) { continue; }
        const FColor Color = bZonesEnabled ? Shape->ShapeColor : FColor(110, 110, 110);
        if (const UBoxComponent* Box = Cast<UBoxComponent>(Shape))
        {
            DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent() + FVector(Inflate),
                Box->GetComponentQuat(), Color, false, -1.0f, SDPG_World, 1.25f);
        }
        else if (const USphereComponent* Sphere = Cast<USphereComponent>(Shape))
        {
            DrawDebugSphere(World, Sphere->GetComponentLocation(), Sphere->GetScaledSphereRadius() + Inflate, 16,
                Color, false, -1.0f, SDPG_World, 1.25f);
        }
    }
    bDebugDrawn = true;
#endif
}

int32 UVSHitZoneSet::FindShapeIndex(const UPrimitiveComponent* Component) const
{
    if (!Component) { return INDEX_NONE; }
    return Shapes.IndexOfByPredicate([Component](const TObjectPtr<UShapeComponent>& Shape) { return Shape.Get() == Component; });
}

UShapeComponent* UVSHitZoneSet::GetShape(int32 Index) const
{
    return Shapes.IsValidIndex(Index) ? Shapes[Index].Get() : nullptr;
}

VSD::HitZone UVSHitZoneSet::ResolveZone(const UPrimitiveComponent* Component) const
{
    const int32 Index = FindShapeIndex(Component);
    return Index != INDEX_NONE && Index < static_cast<int32>(VSD::VolumeCount) ? CurrentLayout.Volumes[Index].Zone : VSD::HitZone::None;
}
