#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSRifleComponent.generated.h"

class AVSCharacter;

USTRUCT()
struct FVSRifleState
{
    GENERATED_BODY()
    UPROPERTY() int32 Magazine = 30;
    UPROPERTY() int32 Reserve = 90;
    UPROPERTY() bool bReloading = false;
    UPROPERTY() uint32 ShotsFired = 0;
};

/** One fixed rifle. RPCs request actions; only the server changes ammo or traces hits. */
UCLASS(ClassGroup = (Combat))
class VALSTRIKE_API UVSRifleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVSRifleComponent();
    void StartFire();
    void StopFire();
    void Reload();
    void OwnerDied();
    const FVSRifleState& GetState() const { return State; }
    int32 GetMagazineCapacity() const { return MagazineCapacity; }
    float GetFireInterval() const { return FireInterval; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bool DevelopmentReset();
    void DevelopmentRequest(uint32 ShotId, FVector Origin, FVector Direction);
    uint32 DevelopmentLastRequest() const { return LastRequestId; }
    uint32 DevelopmentRejections(const FName Reason) const { return Rejections.FindRef(Reason); }
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    AVSCharacter* Character() const;
    bool HasValidOwner() const;
    void SendShot();
    void FinishReload();
    void Reject(FName Reason);

    UFUNCTION(Server, Reliable)
    void ServerRequestFire(uint32 ShotId, FVector Origin, FVector Direction);
    UFUNCTION(Server, Reliable)
    void ServerRequestReload();

    UPROPERTY(Replicated) FVSRifleState State;
    UPROPERTY(EditDefaultsOnly, Replicated, Category = "Rifle", meta = (ClampMin = "1"))
    int32 MagazineCapacity = 30;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle", meta = (ClampMin = "0"))
    int32 InitialReserve = 90;
    UPROPERTY(EditDefaultsOnly, Replicated, Category = "Rifle", meta = (ClampMin = "0.05"))
    float FireInterval = 0.12f;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle", meta = (ClampMin = "0.1"))
    float ReloadDuration = 2.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle", meta = (ClampMin = "1.0"))
    float Range = 10000.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle", meta = (ClampMin = "0.0"))
    float BodyDamage = 30.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Rifle", meta = (ClampMin = "0.0"))
    float HeadDamage = 100.0f;

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
