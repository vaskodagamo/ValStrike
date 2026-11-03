#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSHealthComponent.generated.h"

/** Health changes are made on the server and observed through replication. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVSHealthComponent();

    float GetHealth() const { return Health; }
    float GetMaxHealth() const { return MaxHealth; }
    bool IsAlive() const { return Health > 0.0f; }

    // Native server entry point, never a client RPC. Returns damage actually applied.
    float ApplyPointDamage(float Damage);

    // Broadcast after health changes on the server and in the replication callback.
    FSimpleMulticastDelegate OnLifeStateChanged;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Explicit local development opt-in and authority are both required.
    bool DevelopmentReset();
#endif

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void OnRep_Health();

    UPROPERTY(EditDefaultsOnly, Replicated, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing = OnRep_Health)
    float Health = 100.0f;
};
