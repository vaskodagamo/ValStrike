#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSWeaponLoadoutRules.h"
#include "VSWeaponLoadoutComponent.generated.h"

class AVSCharacter;
class UVSWeaponComponent;

/** Fixed loadout slots. Every character carries both weapons. Values match VSWeaponLoadoutRules::Slot. */
UENUM()
enum class EVSWeaponSlot : uint8
{
    Rifle = 0,
    Pistol = 1,
};

/** Equip input; values are the raw request bytes in VSWeaponLoadoutRules. Last is resolved by the server. */
UENUM()
enum class EVSEquipRequest : uint8
{
    Rifle = 0,
    Pistol = 1,
    Last = 2,
};

enum class EVSWeaponReadiness : uint8
{
    Ready,
    Holstered,
    Equipping,
};

USTRUCT()
struct FVSLoadoutState
{
    GENERATED_BODY()
    UPROPERTY() EVSWeaponSlot Equipped = EVSWeaponSlot::Rifle;
    UPROPERTY() EVSWeaponSlot Previous = EVSWeaponSlot::Pistol;
    // True from an accepted switch until its equip time has elapsed on the server.
    UPROPERTY() bool bEquipping = false;
    // Accepted switches during this pawn's life; observers detect switches with it.
    UPROPERTY() uint32 EquipCount = 0;
    // Latest sequenced request the server processed, accepted or rejected. Only the
    // owning client reads it, to reconcile its predicted switch.
    UPROPERTY() uint32 AckSequence = 0;
};

/**
 * Server-authoritative weapon selection. A local request is validated on the
 * server (owner, alive, live combat gate, duplicate and rate limit); an accepted
 * switch holsters the previous weapon (canceling its reload) and blocks the new
 * one for EquipDuration. Fresh pawns, including every match round reset, start
 * with full weapons and the rifle ready.
 *
 * The owning client predicts its switch: the slot and equip timer change on the
 * key press, and each replicated state is reconciled by ack sequence
 * (VSWeaponLoadoutRules::Predictor). Observers and the server use only server
 * state, and the server alone decides whether a weapon may fire.
 */
UCLASS(ClassGroup = (Combat))
class VALSTRIKE_API UVSWeaponLoadoutComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVSWeaponLoadoutComponent();

    // Local input entry point. A remote owner predicts the switch at once and
    // sends a sequenced request; a listen-server host applies it directly.
    void RequestEquip(EVSEquipRequest Request);
    // Replicated server state, never the owner's prediction.
    const FVSLoadoutState& GetState() const { return State; }
    // This machine's view: the prediction on the owning client, server state elsewhere.
    EVSWeaponSlot GetEquippedSlot() const;
    bool IsEquipping() const;
    bool IsPredicting() const { return Prediction.active; }
    float GetEquipDuration() const { return EquipDuration; }
    UVSWeaponComponent* GetWeapon(EVSWeaponSlot Slot) const;
    UVSWeaponComponent* GetEquippedWeapon() const { return GetWeapon(GetEquippedSlot()); }
    // Fire/reload gate. The server uses its own clock; the owning client uses its
    // predicted slot and timer; observers use replicated state.
    EVSWeaponReadiness GetReadiness(const UVSWeaponComponent* Weapon) const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Raw RPC with an arbitrary byte, bypassing local checks (weapon smoke only).
    void DevelopmentRequestEquip(uint8 RawRequest);
    // Server: rifle equipped and ready, as on a fresh pawn (weapon/combat smoke only).
    bool DevelopmentReset();
    uint32 DevelopmentRejections(const FName Reason) const { return Rejections.FindRef(Reason); }
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    AVSCharacter* Character() const;
    bool HasValidOwner() const;
    void Equip(EVSWeaponSlot Slot);
    void FinishEquip();
    void Reject(FName Reason);
    VSWeaponLoadoutRules::ServerView GetServerView() const;
    // Owning client: sends the queued predicted request or schedules it, then reconciles.
    void SendPredictedRequest();
    void ReconcilePrediction();
    void StopStowedWeapons();

    // Sequence 0 is unsequenced (listen-server host and raw development requests).
    UFUNCTION(Server, Reliable)
    void ServerRequestEquip(uint8 Request, uint32 Sequence);
    UFUNCTION()
    void OnRep_State();

    UPROPERTY(ReplicatedUsing = OnRep_State)
    FVSLoadoutState State;

    // Seconds after an accepted switch before the new weapon may fire or reload.
    UPROPERTY(EditDefaultsOnly, Category = "Loadout", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float EquipDuration = 0.5f;
    // Accepted switches closer together than this are rejected as spam.
    UPROPERTY(EditDefaultsOnly, Category = "Loadout", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float MinSwitchInterval = 0.1f;

    FTimerHandle EquipTimer;
    FTimerHandle SendTimer;   // Owning client: queued predicted request.
    VSWeaponLoadoutRules::Predictor Prediction;
    double EquipReadyTime = 0.0;      // Server clock.
    double LastSwitchTime = -1.0e9;   // Server clock.
    double LastLocalRequestTime = -1.0e9; // Listen-server host only.
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TMap<FName, uint32> Rejections;
#endif
};
