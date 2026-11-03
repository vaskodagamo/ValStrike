#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VSDamageEvents.h"
#include "VSDamageModel.h"
#include "VSDamageSubsystem.generated.h"

/**
 * Per-world damage service.
 * - Gives every ACharacter a UVSHitZoneSet (Shadow mode) unless it already has one.
 * - ApplyWeaponHit: the single server entry point that turns a trace hit into
 *   zone damage through the victim's UVSHealthComponent (keeping its match gates).
 * - OnDamage / OnKill: server-only hooks for stats, economy and feedback.
 */
UCLASS()
class VALSTRIKE_API UVSDamageSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    static UVSDamageSubsystem* Get(const UObject* WorldContext);

    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // Server only. Returns the health actually removed (0 when rejected: client
    // world, unknown component, invalid weapon, dead victim or a closed match gate).
    float ApplyWeaponHit(const FVSWeaponHit& Hit);

    // Server only. For damage applied elsewhere that should still reach stats and
    // economy. Ignored on clients or without positive applied damage.
    void PublishDamage(const FVSDamageEvent& Event);

    // Every attributed hit with applied damage, kills included.
    FVSDamageEventSignature& OnDamage() { return DamageDelegate; }
    // Only hits that took the victim from alive to dead; fires after OnDamage.
    FVSDamageEventSignature& OnKill() { return KillDelegate; }

    const ValStrike::Damage::DamageTable& GetDamageTable() const { return Table; }

private:
    void HandleActorSpawned(AActor* Actor);
    bool IsServerWorld() const;

    ValStrike::Damage::DamageTable Table;
    FVSDamageEventSignature DamageDelegate;
    FVSDamageEventSignature KillDelegate;
    FDelegateHandle SpawnHandle;
};
