#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Damage/VSDamageModel.h"
#include "VSWeaponComponent.generated.h"

class AVSCharacter;

/** Replicated per-weapon state. The name predates the second weapon; every weapon uses it. */
USTRUCT()
struct FVSRifleState
{
    GENERATED_BODY()
    UPROPERTY() int32 Magazine = 30;
    UPROPERTY() int32 Reserve = 90;
    UPROPERTY() bool bReloading = false;
    // Server-accepted shots by this weapon during this pawn's life.
    UPROPERTY() uint32 ShotsFired = 0;
};

/**
 * One server-authoritative hitscan weapon. RPCs request actions; only the server
 * changes ammo or traces hits. Each weapon keeps its own magazine, reserve, shot
 * sequence and cadence. Concrete weapons differ only in tuning defaults; the
 * owning character's loadout decides which weapon may fire.
 */
UCLASS(Abstract, ClassGroup = (Combat))
class VALSTRIKE_API UVSWeaponComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVSWeaponComponent();
    void StartFire();
    void StopFire();
    void Reload();
    void OwnerDied();
    // Switching away: stops local fire and, on the server, cancels an active
    // reload without transferring ammo.
    void Holster();
    const FVSRifleState& GetState() const { return State; }
    int32 GetMagazineCapacity() const { return MagazineCapacity; }
    float GetFireInterval() const { return FireInterval; }
    // Row of the damage table (Damage/VSDamageModel) this weapon's hits use.
    ValStrike::Damage::WeaponClass GetDamageClass() const { return DamageClass; }
    bool IsAutomatic() const { return bAutomatic; }
    const FString& GetDisplayName() const { return DisplayName; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bool DevelopmentReset();
    void DevelopmentRequest(uint32 ShotId, FVector Origin, FVector Direction);
    // Match server smoke probes execute the real reload entry/completion paths.
    void DevelopmentRequestReload();
    void DevelopmentFinishReload();
    uint32 DevelopmentLastRequest() const { return LastRequestId; }
    uint32 DevelopmentRejections(const FName Reason) const { return Rejections.FindRef(Reason); }
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    // Tuning. Concrete weapon constructors set these defaults; values are
    // clamped on the server at BeginPlay.
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    FString DisplayName = TEXT("WEAPON");
    // Automatic weapons repeat while fire is held; others fire once per press.
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    bool bAutomatic = true;
    UPROPERTY(EditDefaultsOnly, Replicated, Category = "Weapon", meta = (ClampMin = "1"))
    int32 MagazineCapacity = 30;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0"))
    int32 InitialReserve = 90;
    UPROPERTY(EditDefaultsOnly, Replicated, Category = "Weapon", meta = (ClampMin = "0.05"))
    float FireInterval = 0.12f;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "0.1"))
    float ReloadDuration = 2.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (ClampMin = "1.0"))
    float Range = 10000.0f;
    // Damage per hit zone comes from the shared table in Damage/VSDamageModel.cpp.
    ValStrike::Damage::WeaponClass DamageClass = ValStrike::Damage::WeaponClass::Rifle;

private:
    AVSCharacter* Character() const;
    bool HasValidOwner() const;
    // Local view of whether the owner's loadout lets this weapon act now.
    bool IsReadyLocally() const;
    void SendShot();
    void FinishReload();
    void CancelActions();
    void Reject(FName Reason);

    UFUNCTION(Server, Reliable)
    void ServerRequestFire(uint32 ShotId, FVector Origin, FVector Direction);
    UFUNCTION(Server, Reliable)
    void ServerRequestReload();

    UPROPERTY(Replicated) FVSRifleState State;

    FTimerHandle LocalFireTimer;
    FTimerHandle ReloadTimer;
    uint32 NextLocalShotId = 0;
    uint32 LastRequestId = 0;
    double NextServerFireTime = 0.0;
    bool bFireHeld = false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TMap<FName, uint32> Rejections;
#endif
};
