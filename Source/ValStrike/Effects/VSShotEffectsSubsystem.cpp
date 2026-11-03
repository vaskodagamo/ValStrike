#include "VSShotEffectsSubsystem.h"

#include "../Combat/VSRifleComponent.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

const FName UVSShotEffectsSubsystem::MuzzleSocket(TEXT("Muzzle"));

namespace VSShotEffectsDetail
{
// UVSRifleComponent keeps Range private and unreplicated. The cosmetic trace
// reads the weapon's own reflected property and applies the server's clamp;
// this default is only used if that property is ever renamed.
constexpr float DefaultWeaponRange = 10000.0f;
constexpr float TracerLifetime = 0.075f;     // s
constexpr float TracerEndWidth = 0.55f;      // Width multiplier at the end of the tracer's life.
constexpr float TracerBaseRadius = 0.05f;    // cm; the material widens it with view distance.
constexpr float MinTracerLength = 20.0f;     // cm; skip point-blank segments behind the muzzle.
constexpr float SelfHitStandoff = 60.0f;     // cm; never draw a tracer into this client's camera.
constexpr float MarkSurfaceOffset = 0.4f;    // cm along the hit normal; avoids depth fighting.
constexpr float WorldMarkHeatSeconds = 0.18f;
constexpr float CharacterMarkHold = 0.12f;
constexpr float CharacterMarkFade = 0.3f;
// Camera-relative tracer origins when no visible mesh has a Muzzle socket.
const FVector FirstPersonFallbackOffset(40.0, 12.0, -14.0);
const FVector ObserverFallbackOffset(40.0, 16.0, -28.0);

float WeaponRange(const UActorComponent& Weapon)
{
    static const FName RangeName(TEXT("Range"));
    if (const FFloatProperty* Property = FindFProperty<FFloatProperty>(Weapon.GetClass(), RangeName))
    {
        const float Value = Property->GetPropertyValue_InContainer(&Weapon);
        if (FMath::IsFinite(Value))
        {
            return FMath::Clamp(Value, 1.0f, 50000.0f); // Same clamp as the server's BeginPlay.
        }
    }
    return DefaultWeaponRange;
}

void SetDataIfChanged(UStaticMeshComponent& Component, int32 Index, float Value, float& Cached)
{
    if (!FMath::IsNearlyEqual(Value, Cached, 0.002f))
    {
        Component.SetCustomPrimitiveDataFloat(Index, Value);
        Cached = Value;
    }
}
}

UVSShotEffectsSubsystem::UVSShotEffectsSubsystem()
{
    // Fixed references keep these assets in startup loading and cook dependency
    // tracking, the same pattern as the rifle presentation's flash material.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Tracer(TEXT("/Game/Effects/M_VSShotTracer.M_VSShotTracer"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mark(TEXT("/Game/Effects/M_VSImpactMark.M_VSImpactMark"));
    TracerMesh = Cylinder.Object;
    MarkMesh = Plane.Object;
    TracerMaterial = Tracer.Object;
    MarkMaterial = Mark.Object;
}

bool UVSShotEffectsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Purely cosmetic: a dedicated server never creates this module.
    return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UVSShotEffectsSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UVSShotEffectsSubsystem::IsTickable() const
{
    return !bDisabled && HasCalledBeginPlay();
}

TStatId UVSShotEffectsSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVSShotEffectsSubsystem, STATGROUP_Tickables);
}

bool UVSShotEffectsSubsystem::IsEffectsWorld() const
{
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

void UVSShotEffectsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    // Also covers in-process dedicated server worlds, such as multi-client PIE.
    bDisabled = !IsEffectsWorld();
    if (bDisabled || !FApp::CanEverRender())
    {
        return;
    }
    // Uncooked editor builds compile shaders on demand and can otherwise draw
    // the first, very short effect with a fallback material. Finish these two
    // small materials at world start, never in the shot path. Editor-only work
    // inside the engine; cooked builds ship compiled shaders.
    for (UMaterialInterface* Material : {TracerMaterial.Get(), MarkMaterial.Get()})
    {
        if (Material) { Material->EnsureIsComplete(); }
    }
}

void UVSShotEffectsSubsystem::Deinitialize()
{
    const UWorld* World = GetWorld();
    if (IsValid(EffectsActor) && World && !World->bIsTearingDown)
    {
        EffectsActor->Destroy();
    }
    EffectsActor = nullptr;
    Tracers.Reset();
    Marks.Reset();
    Watches.Reset();
    Super::Deinitialize();
}

void UVSShotEffectsSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!IsEffectsWorld())
    {
        bDisabled = true;
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    UpdateEffects(Now);
    ObserveCharacters(Now);
}

void UVSShotEffectsSubsystem::ObserveCharacters(double Now)
{
    for (TActorIterator<AVSCharacter> It(GetWorld()); It; ++It)
    {
        const AVSCharacter* Pawn = *It;
        if (!IsValid(Pawn) || !Pawn->HasActorBegunPlay()) { continue; }
        // Each weapon component has its own accepted-shot counter and cache, so a
        // new pawn, rejoin or relevancy return starts from a fresh baseline.
        // Pistol handoff: observe its component's counter the same way here.
        if (const UVSRifleComponent* Rifle = Pawn->GetRifleComponent())
        {
            FShotWatch& Watch = Watches.FindOrAdd(TWeakObjectPtr<const UActorComponent>(Rifle));
            const VSShotEffects::Decision Decision =
                Watch.Counter.Observe(Rifle->GetState().ShotsFired, Now, Rifle->GetFireInterval());
            if (Decision.baseline) { ++Stats.Baselines; }
            if (Decision.spawn) { SpawnShotEffect(*Pawn, *Rifle, Decision.shots, Now); }
        }
    }
    for (auto It = Watches.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
}

FVector UVSShotEffectsSubsystem::TracerOrigin(const AVSCharacter& Shooter, bool bFirstPersonView, const FVector& Eye,
    const FRotator& Aim, bool& bOutMuzzle, const USceneComponent*& OutMuzzle) const
{
    // Read-only lookup of whichever visible weapon mesh exposes a Muzzle socket
    // (the R01 presentation, later a pistol). Owner visibility flags select the
    // first-person or world mesh this client actually renders.
    TInlineComponentArray<UMeshComponent*> Meshes(&Shooter);
    for (const UMeshComponent* Mesh : Meshes)
    {
        if (!Mesh || !Mesh->IsRegistered() || !Mesh->IsVisible() || !Mesh->DoesSocketExist(MuzzleSocket)) { continue; }
        if (bFirstPersonView ? Mesh->bOwnerNoSee : Mesh->bOnlyOwnerSee) { continue; }
        bOutMuzzle = true;
        OutMuzzle = Mesh;
        return Mesh->GetSocketLocation(MuzzleSocket);
    }
    bOutMuzzle = false;
    OutMuzzle = nullptr;
    return Eye + Aim.RotateVector(bFirstPersonView
        ? VSShotEffectsDetail::FirstPersonFallbackOffset : VSShotEffectsDetail::ObserverFallbackOffset);
}

void UVSShotEffectsSubsystem::SpawnShotEffect(const AVSCharacter& Shooter, const UActorComponent& Weapon,
    uint32 Represented, double Now)
{
    using namespace VSShotEffectsDetail;
    UWorld* World = GetWorld();
    const APlayerController* LocalPC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
    const AActor* ViewTarget = LocalPC ? LocalPC->GetViewTarget() : nullptr;
    const bool bFirstPersonView = Shooter.IsLocallyControlled() && ViewTarget == &Shooter;

    // Mirror the server trace: pawn view location, base aim (local: camera view;
    // remote: replicated yaw and view pitch), weapon range, Visibility channel.
    const FVector Eye = Shooter.GetPawnViewLocation();
    const FRotator Aim = Shooter.GetBaseAimRotation();
    const FVector Direction = Aim.Vector();
    const FVector TraceEnd = Eye + Direction * WeaponRange(Weapon);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSShotEffectsTrace), false, &Shooter);
    const bool bHit = World->LineTraceSingleByChannel(Hit, Eye, TraceEnd, ECC_Visibility, Query);
    const AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
    const bool bSelf = HitActor && HitActor == ViewTarget;
    const bool bCharacter = Cast<AVSCharacter>(HitActor) != nullptr;

    FVSShotEffectRecord Record;
    Record.Sequence = ++Stats.Effects;
    Record.Shooter = &Shooter;
    Record.RepresentedShots = Represented;
    Record.Eye = Eye;
    Record.Direction = Direction;
    Record.Time = Now;
    Record.Impact = !bHit ? EVSShotImpactKind::None
        : bSelf ? EVSShotImpactKind::Self
        : bCharacter ? EVSShotImpactKind::Character : EVSShotImpactKind::World;
    if (bHit)
    {
        Record.ImpactPoint = Hit.ImpactPoint;
        Record.ImpactNormal = Hit.ImpactNormal;
        Record.HitActor = HitActor;
    }
    Stats.RepresentedShots += Represented;
    Stats.CoalescedShots += Represented > 0 ? Represented - 1 : 0;

    const USceneComponent* Muzzle = nullptr;
    const FVector Start = TracerOrigin(Shooter, bFirstPersonView, Eye, Aim, Record.bMuzzleOrigin, Muzzle);
    Record.MuzzleComponent = Muzzle;
    FVector End = bHit ? Hit.ImpactPoint : TraceEnd;
    if (bSelf) { End -= Direction * SelfHitStandoff; }
    const FVector Segment = End - Start;
    Record.TracerStart = Start;
    Record.TracerEnd = End;
    if (Segment.Size() >= MinTracerLength && FVector::DotProduct(Segment, Direction) > 0.0)
    {
        Record.TracerSlot = ShowTracer(Start, End, Now);
        Record.bTracer = Record.TracerSlot != INDEX_NONE;
    }
    if (bSelf)
    {
        ++Stats.SelfHitsSuppressed;
    }
    else if (bHit)
    {
        const FVector ViewLocation = LocalPC && LocalPC->PlayerCameraManager
            ? LocalPC->PlayerCameraManager->GetCameraLocation() : Eye;
        Record.MarkSlot = ShowMark(Hit, bCharacter, ViewLocation, Now);
    }
    LastEffect = Record;
}

bool UVSShotEffectsSubsystem::EnsureEffectsActor()
{
    if (IsValid(EffectsActor)) { return true; }
    if (!TracerMesh || !MarkMesh || !TracerMaterial || !MarkMaterial)
    {
        if (!bWarnedMissingAssets)
        {
            bWarnedMissingAssets = true;
            UE_LOG(LogValStrike, Warning, TEXT("Shot effects disabled: tracer/mark mesh or material missing under /Game/Effects."));
        }
        return false;
    }
    UWorld* World = GetWorld();
    FActorSpawnParameters Params;
    Params.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), TEXT("VSShotEffects"));
    Params.ObjectFlags |= RF_Transient;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
    if (!Actor) { return false; }
    Actor->SetActorEnableCollision(false);
    USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
    Actor->SetRootComponent(Root);
    Actor->AddInstanceComponent(Root);
    Root->RegisterComponent();
    // A replaced actor destroyed its old components; start new pools.
    Tracers.Reset();
    Marks.Reset();
    EffectsActor = Actor;
    UE_LOG(LogValStrike, Log, TEXT("Shot effects active (client-only; %d tracers, %d marks max)."), MaxTracers, MaxMarks);
    return true;
}

UStaticMeshComponent* UVSShotEffectsSubsystem::MakeEffectComponent(const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material)
{
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(EffectsActor,
        MakeUniqueObjectName(EffectsActor, UStaticMeshComponent::StaticClass(), Name));
    Component->SetStaticMesh(Mesh);
    Component->SetMaterial(0, Material);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->bReceivesDecals = false;
    Component->bVisibleInReflectionCaptures = false;
    Component->bVisibleInRayTracing = false;
    Component->SetVisibility(false);
    // Unattached: each effect keeps its own world transform.
    EffectsActor->AddInstanceComponent(Component);
    Component->RegisterComponent();
    return Component;
}

int32 UVSShotEffectsSubsystem::ShowTracer(const FVector& Start, const FVector& End, double Now)
{
    if (!EnsureEffectsActor()) { return INDEX_NONE; }
    const VSShotEffects::SlotChoice Choice = VSShotEffects::ChooseSlot(Tracers.Num(), MaxTracers,
        [this](int Index) { return Tracers[Index].bActive; }, [this](int Index) { return Tracers[Index].SpawnTime; });
    if (Choice.index < 0) { return INDEX_NONE; }
    if (Choice.grow)
    {
        Tracers.AddDefaulted_GetRef().Component = MakeEffectComponent(TEXT("VSShotTracer"), TracerMesh, TracerMaterial);
    }
    if (Choice.recycled) { ++Stats.TracersRecycled; }
    FVSShotTracerSlot& Slot = Tracers[Choice.index];
    UStaticMeshComponent* Component = Slot.Component;
    if (!IsValid(Component)) { return INDEX_NONE; }

    // Stretch the engine cylinder along Start->End using its own bounds, so the
    // placement holds whether its pivot is centered or at the base.
    const FVector Axis = End - Start;
    const double Length = Axis.Size();
    const FBoxSphereBounds Bounds = TracerMesh->GetBounds();
    const double Height = FMath::Max(2.0 * Bounds.BoxExtent.Z, 1.0);
    const double Radius = FMath::Max(Bounds.BoxExtent.X, 1.0);
    const FVector Scale(VSShotEffectsDetail::TracerBaseRadius / Radius, VSShotEffectsDetail::TracerBaseRadius / Radius, Length / Height);
    const FQuat Rotation = FRotationMatrix::MakeFromZ(Axis / Length).ToQuat();
    const FVector Base(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
    Component->SetWorldTransform(FTransform(Rotation, Start - Rotation.RotateVector(Base * Scale), Scale));
    Component->SetCustomPrimitiveDataFloat(0, 1.0f); // Intensity
    Component->SetCustomPrimitiveDataFloat(1, 1.0f); // Width
    Component->SetVisibility(true);
    Slot.SpawnTime = Now;
    Slot.bActive = true;
    ++Stats.Tracers;
    return Choice.index;
}

int32 UVSShotEffectsSubsystem::ShowMark(const FHitResult& Hit, bool bCharacter, const FVector& ViewLocation, double Now)
{
    using namespace VSShotEffectsDetail;
    if (!EnsureEffectsActor()) { return INDEX_NONE; }
    const VSShotEffects::SlotChoice Choice = VSShotEffects::ChooseSlot(Marks.Num(), MaxMarks,
        [this](int Index) { return Marks[Index].bActive; }, [this](int Index) { return Marks[Index].SpawnTime; });
    if (Choice.index < 0) { return INDEX_NONE; }
    if (Choice.grow)
    {
        Marks.AddDefaulted_GetRef().Component = MakeEffectComponent(TEXT("VSImpactMark"), MarkMesh, MarkMaterial);
    }
    FVSShotMarkSlot& Slot = Marks[Choice.index];
    if (Choice.recycled) { ++Stats.MarksRecycled; }
    if (Slot.bActive) { HideMark(Slot); }
    UStaticMeshComponent* Component = Slot.Component;
    if (!IsValid(Component)) { return INDEX_NONE; }

    FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero()) { Normal = -(Hit.TraceEnd - Hit.TraceStart).GetSafeNormal(); }
    // Slightly larger far away so distant marks stay readable on this client.
    const double Distance = FVector::Dist(ViewLocation, Hit.ImpactPoint);
    const double Diameter = FMath::Clamp(Distance * 0.004, 7.0, 16.0) * (bCharacter ? 1.2 : 1.0);
    const FBoxSphereBounds Bounds = MarkMesh->GetBounds();
    const double Half = FMath::Max(Bounds.BoxExtent.X, 1.0);
    const FVector Scale(Diameter / (2.0 * Half), Diameter / (2.0 * Half), 1.0);
    const FQuat Rotation = FRotationMatrix::MakeFromZ(Normal).ToQuat();
    const FVector Location = Hit.ImpactPoint + Normal * MarkSurfaceOffset - Rotation.RotateVector(Bounds.Origin * Scale);
    Component->SetWorldTransform(FTransform(Rotation, Location, Scale));
    if (bCharacter && Hit.GetComponent())
    {
        // Follow the struck character; the engine detaches it if that pawn is destroyed.
        Component->AttachToComponent(Hit.GetComponent(), FAttachmentTransformRules::KeepWorldTransform);
        Slot.bAttached = true;
    }
    Slot.bCharacter = bCharacter;
    Slot.Hold = bCharacter ? CharacterMarkHold : WorldMarkHold;
    Slot.Fade = bCharacter ? CharacterMarkFade : WorldMarkFade;
    Slot.SpawnTime = Now;
    Slot.LastOpacity = Slot.LastHeat = -1.0f;
    SetDataIfChanged(*Component, 0, 1.0f, Slot.LastOpacity); // Fade
    SetDataIfChanged(*Component, 1, 1.0f, Slot.LastHeat);    // Heat
    Component->SetVisibility(true);
    Slot.bActive = true;
    ++(bCharacter ? Stats.CharacterMarks : Stats.WorldMarks);
    Stats.MaxActiveMarks = FMath::Max(Stats.MaxActiveMarks, GetActiveMarkCount());
    return Choice.index;
}

void UVSShotEffectsSubsystem::HideMark(FVSShotMarkSlot& Mark)
{
    if (UStaticMeshComponent* Component = Mark.Component)
    {
        if (Mark.bAttached && Component->GetAttachParent())
        {
            Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        }
        Component->SetVisibility(false);
    }
    Mark.bActive = false;
    Mark.bAttached = false;
}

void UVSShotEffectsSubsystem::UpdateEffects(double Now)
{
    using namespace VSShotEffectsDetail;
    for (FVSShotTracerSlot& Slot : Tracers)
    {
        if (!Slot.bActive || !IsValid(Slot.Component)) { continue; }
        const float T = static_cast<float>((Now - Slot.SpawnTime) / TracerLifetime);
        if (T >= 1.0f)
        {
            Slot.Component->SetVisibility(false);
            Slot.bActive = false;
            continue;
        }
        Slot.Component->SetCustomPrimitiveDataFloat(0, 1.0f - T);
        Slot.Component->SetCustomPrimitiveDataFloat(1, FMath::Lerp(1.0f, TracerEndWidth, T));
    }
    for (FVSShotMarkSlot& Slot : Marks)
    {
        if (!Slot.bActive || !IsValid(Slot.Component)) { continue; }
        // A character mark whose pawn went away has nothing left to mark.
        if (Slot.bAttached && !Slot.Component->GetAttachParent()) { HideMark(Slot); continue; }
        const double Age = Now - Slot.SpawnTime;
        const float Opacity = VSShotEffects::MarkOpacity(Age, Slot.Hold, Slot.Fade);
        if (Opacity <= 0.0f) { HideMark(Slot); continue; }
        const float Heat = Slot.bCharacter ? 1.0f : FMath::Clamp(1.0f - static_cast<float>(Age / WorldMarkHeatSeconds), 0.0f, 1.0f);
        SetDataIfChanged(*Slot.Component, 0, Opacity, Slot.LastOpacity);
        SetDataIfChanged(*Slot.Component, 1, Heat, Slot.LastHeat);
    }
}

int32 UVSShotEffectsSubsystem::GetActiveTracerCount() const
{
    int32 Count = 0;
    for (const FVSShotTracerSlot& Slot : Tracers) { Count += Slot.bActive ? 1 : 0; }
    return Count;
}

int32 UVSShotEffectsSubsystem::GetActiveMarkCount() const
{
    int32 Count = 0;
    for (const FVSShotMarkSlot& Slot : Marks) { Count += Slot.bActive ? 1 : 0; }
    return Count;
}

const UStaticMeshComponent* UVSShotEffectsSubsystem::GetTracerComponent(int32 Slot) const
{
    return Tracers.IsValidIndex(Slot) ? Tracers[Slot].Component.Get() : nullptr;
}

const UStaticMeshComponent* UVSShotEffectsSubsystem::GetMarkComponent(int32 Slot) const
{
    return Marks.IsValidIndex(Slot) ? Marks[Slot].Component.Get() : nullptr;
}

float UVSShotEffectsSubsystem::GetMarkOpacity(int32 Slot) const
{
    const UWorld* World = GetWorld();
    if (!World || !Marks.IsValidIndex(Slot) || !Marks[Slot].bActive) { return -1.0f; }
    const FVSShotMarkSlot& Mark = Marks[Slot];
    return VSShotEffects::MarkOpacity(World->GetTimeSeconds() - Mark.SpawnTime, Mark.Hold, Mark.Fade);
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool UVSShotEffectsSubsystem::DevelopmentSetWorldMarkTiming(float HoldSeconds, float FadeSeconds)
{
    FString Role;
    if (!FParse::Value(FCommandLine::Get(), TEXT("VSShotEffectsTest="), Role)
        || !FMath::IsFinite(HoldSeconds) || !FMath::IsFinite(FadeSeconds))
    {
        return false;
    }
    WorldMarkHold = FMath::Clamp(HoldSeconds, 0.1f, 120.0f);
    WorldMarkFade = FMath::Clamp(FadeSeconds, 0.05f, 30.0f);
    return true;
}
#endif
