#include "VSDamageSubsystem.h"

#include "VSHitZoneSet.h"
#include "../Combat/VSHealthComponent.h"
#include "../ValStrike.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Tests/VSDamageRuntimeSmoke.h"

namespace
{
namespace VSD = ValStrike::Damage;
}

UVSDamageSubsystem* UVSDamageSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UVSDamageSubsystem>() : nullptr;
}

bool UVSDamageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVSDamageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Table = VSD::Sanitized(VSD::DefaultTable());
    if (UWorld* World = GetWorld())
    {
        SpawnHandle = World->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateUObject(this, &UVSDamageSubsystem::HandleActorSpawned));
    }
}

void UVSDamageSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->RemoveOnActorSpawnedHandler(SpawnHandle);
    }
    SpawnHandle.Reset();
    DamageDelegate.Clear();
    KillDelegate.Clear();
    Super::Deinitialize();
}

void UVSDamageSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    for (TActorIterator<ACharacter> It(&InWorld); It; ++It)
    {
        HandleActorSpawned(*It);
    }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ValStrikeDamageTests::StartRuntimeSmoke();
#endif
}

void UVSDamageSubsystem::HandleActorSpawned(AActor* Actor)
{
    // Every machine builds the same local geometry; only server traces decide damage.
    if (ACharacter* Character = Cast<ACharacter>(Actor))
    {
        UVSHitZoneSet::AddTo(Character, EVSHitZoneTraceMode::Shadow);
    }
}

bool UVSDamageSubsystem::IsServerWorld() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_Client;
}

float UVSDamageSubsystem::ApplyWeaponHit(const FVSWeaponHit& Hit)
{
    if (!IsServerWorld() || !IsValid(Hit.Victim) || !Hit.Victim->HasAuthority() || !Hit.HitComponent ||
        Hit.HitComponent->GetOwner() != Hit.Victim)
    {
        return 0.0f;
    }
    UVSHealthComponent* VictimHealth = Hit.Victim->FindComponentByClass<UVSHealthComponent>();
    const UVSHitZoneSet* Zones = UVSHitZoneSet::FindOn(Hit.Victim);
    if (!VictimHealth || !Zones)
    {
        return 0.0f;
    }
    VSD::HitRequest Request;
    Request.Weapon = Hit.Weapon;
    Request.Zone = Zones->ResolveZone(Hit.HitComponent);
    Request.DistanceCm = Hit.DistanceCm;
    const VSD::HitResult Result = VSD::Compute(Table, Request);
    if (Result.Status != VSD::HitStatus::Applied || Result.Damage <= 0.0f)
    {
        return 0.0f;
    }
    const bool bWasAlive = VictimHealth->IsAlive();
    const float Applied = VictimHealth->ApplyPointDamage(Result.Damage);
    if (Applied <= 0.0f)
    {
        return 0.0f;
    }
    FVSDamageEvent Event;
    Event.InstigatorPawn = IsValid(Hit.Shooter) ? Hit.Shooter : nullptr;
    Event.Instigator = Event.InstigatorPawn ? Event.InstigatorPawn->GetController() : nullptr;
    Event.Victim = Hit.Victim;
    Event.VictimController = Hit.Victim->GetController();
    Event.Zone = Request.Zone;
    Event.Weapon = Request.Weapon;
    Event.DistanceCm = Hit.DistanceCm;
    Event.RequestedDamage = Result.Damage;
    Event.AppliedDamage = Applied;
    Event.bLethalRule = Result.Lethal;
    Event.bKilled = bWasAlive && !VictimHealth->IsAlive();
    PublishDamage(Event);
    return Applied;
}

void UVSDamageSubsystem::PublishDamage(const FVSDamageEvent& Event)
{
    if (!IsServerWorld() || !IsValid(Event.Victim) || !FMath::IsFinite(Event.AppliedDamage) || Event.AppliedDamage <= 0.0f)
    {
        return;
    }
    UE_LOG(LogValStrike, Log, TEXT("VSDAMAGE hit shooter=%s victim=%s weapon=%hs zone=%hs applied=%.1f killed=%d"),
        Event.InstigatorPawn ? *Event.InstigatorPawn->GetName() : TEXT("none"), *Event.Victim->GetName(),
        VSD::WeaponName(Event.Weapon), VSD::ZoneName(Event.Zone), Event.AppliedDamage, Event.bKilled ? 1 : 0);
    DamageDelegate.Broadcast(Event);
    if (Event.bKilled)
    {
        // Kill rewards subscribe here (Economy binds OnKill from its own module).
        KillDelegate.Broadcast(Event);
    }
}
