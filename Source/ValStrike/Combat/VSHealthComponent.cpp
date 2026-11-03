#include "VSHealthComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

UVSHealthComponent::UVSHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UVSHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.0f, MaxHealth) : 100.0f;
        Health = MaxHealth;
    }
}

void UVSHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UVSHealthComponent, MaxHealth);
    DOREPLIFETIME(UVSHealthComponent, Health);
}

float UVSHealthComponent::ApplyPointDamage(float Damage)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !IsAlive() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousHealth = Health;
    Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
    const float AppliedDamage = PreviousHealth - Health;
    if (AppliedDamage > 0.0f)
    {
        Owner->ForceNetUpdate();
        OnLifeStateChanged.Broadcast();
    }
    return AppliedDamage;
}

void UVSHealthComponent::OnRep_Health()
{
    OnLifeStateChanged.Broadcast();
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool UVSHealthComponent::DevelopmentReset()
{
    AActor* Owner = GetOwner();
    FString SmokeRole;
    const bool bSmokeServer = FParse::Value(FCommandLine::Get(), TEXT("VSCombatSmoke="), SmokeRole)
        && SmokeRole == TEXT("server");
    if (!Owner || !Owner->HasAuthority()
        || (!bSmokeServer && !FParse::Param(FCommandLine::Get(), TEXT("VSCombatDev"))))
    {
        return false;
    }

    const bool bChanged = Health != MaxHealth;
    Health = MaxHealth;
    Owner->ForceNetUpdate();
    if (bChanged)
    {
        OnLifeStateChanged.Broadcast();
    }
    return true;
}
#endif
