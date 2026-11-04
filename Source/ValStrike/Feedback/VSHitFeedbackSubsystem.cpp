#include "VSHitFeedbackSubsystem.h"

#include "../Damage/VSDamageSubsystem.h"
#include "../VSPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

bool UVSHitFeedbackSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVSHitFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UVSDamageSubsystem* Damage = Collection.InitializeDependency<UVSDamageSubsystem>())
    {
        // The damage subsystem only broadcasts in server worlds; clients never route.
        DamageSource = Damage;
        DamageHandle = Damage->OnDamage().AddUObject(this, &UVSHitFeedbackSubsystem::HandleDamage);
    }
}

void UVSHitFeedbackSubsystem::Deinitialize()
{
    if (UVSDamageSubsystem* Damage = DamageSource.Get()) { Damage->OnDamage().Remove(DamageHandle); }
    DamageSource.Reset();
    DamageHandle.Reset();
    Super::Deinitialize();
}

void UVSHitFeedbackSubsystem::HandleDamage(const FVSDamageEvent& Event)
{
    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client || !IsValid(Event.Victim))
    {
        return;
    }
    AVSPlayerController* Shooter = Cast<AVSPlayerController>(Event.Instigator);
    AVSPlayerController* Victim = Cast<AVSPlayerController>(Event.VictimController);
    APawn* ShooterPawn = IsValid(Event.InstigatorPawn) ? Event.InstigatorPawn : nullptr;
    // Self-damage is shown as damage taken only, never as a hit marker.
    if (Shooter && Shooter != Victim && ShooterPawn)
    {
        Shooter->SendHitConfirmed(ShooterPawn, Event.IsHeadshot(), Event.bKilled, Event.AppliedDamage);
    }
    if (Victim)
    {
        Victim->SendDamageTaken(Event.Victim, ShooterPawn, Event.Instigator ? Event.Instigator->PlayerState.Get() : nullptr,
            static_cast<uint8>(Event.Weapon), Event.IsHeadshot(), Event.bKilled, Event.AppliedDamage);
    }
}
