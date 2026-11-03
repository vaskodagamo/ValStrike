#include "VSRifleComponent.h"

#include "VSHealthComponent.h"
#include "../VSCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

UVSRifleComponent::UVSRifleComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

AVSCharacter* UVSRifleComponent::Character() const { return Cast<AVSCharacter>(GetOwner()); }

void UVSRifleComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority())
    {
        MagazineCapacity = FMath::Clamp(MagazineCapacity, 1, 100);
        InitialReserve = FMath::Clamp(InitialReserve, 0, 1000);
        FireInterval = FMath::IsFinite(FireInterval) ? FMath::Clamp(FireInterval, 0.05f, 5.0f) : 0.12f;
        ReloadDuration = FMath::IsFinite(ReloadDuration) ? FMath::Clamp(ReloadDuration, 0.1f, 20.0f) : 2.0f;
        Range = FMath::IsFinite(Range) ? FMath::Clamp(Range, 1.0f, 50000.0f) : 10000.0f;
        BodyDamage = FMath::IsFinite(BodyDamage) ? FMath::Clamp(BodyDamage, 0.0f, 1000.0f) : 30.0f;
        HeadDamage = FMath::IsFinite(HeadDamage) ? FMath::Clamp(HeadDamage, 0.0f, 1000.0f) : 100.0f;
        State.Magazine = MagazineCapacity;
        State.Reserve = InitialReserve;
    }
}

void UVSRifleComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopFire();
    GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    Super::EndPlay(Reason);
}

void UVSRifleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UVSRifleComponent, State);
    DOREPLIFETIME(UVSRifleComponent, MagazineCapacity);
    DOREPLIFETIME(UVSRifleComponent, FireInterval);
}

bool UVSRifleComponent::HasValidOwner() const
{
    const AVSCharacter* Pawn = Character();
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    // Unreal's actor channel enforces the sending connection's ownership before
    // delivering a component Server RPC. Also require current possession here.
    return Pawn && Pawn->HasAuthority() && PC && PC->GetPawn() == Pawn && Pawn->GetOwner() == PC;
}

void UVSRifleComponent::StartFire()
{
    AVSCharacter* Pawn = Character();
    if (bFireHeld || !Pawn || !Pawn->IsLocallyControlled() || !Pawn->GetHealthComponent()->IsAlive()) { return; }
    bFireHeld = true;
    SendShot();
    GetWorld()->GetTimerManager().SetTimer(LocalFireTimer, this, &UVSRifleComponent::SendShot, FireInterval, true);
}

void UVSRifleComponent::StopFire()
{
    bFireHeld = false;
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(LocalFireTimer); }
}

void UVSRifleComponent::SendShot()
{
    AVSCharacter* Pawn = Character();
    if (!bFireHeld || !Pawn || !Pawn->IsLocallyControlled() || !Pawn->GetHealthComponent()->IsAlive())
    {
        StopFire();
        return;
    }
    if (State.bReloading || State.Magazine <= 0) { return; }
    // Zero is reserved; sequence never resets during the pawn's lifetime.
    if (++NextLocalShotId == 0) { ++NextLocalShotId; }
    ServerRequestFire(NextLocalShotId, Pawn->GetPawnViewLocation(), Pawn->GetBaseAimRotation().Vector());
}

void UVSRifleComponent::Reject(FName Reason)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++Rejections.FindOrAdd(Reason);
#endif
}

void UVSRifleComponent::ServerRequestFire_Implementation(uint32 ShotId, FVector Origin, FVector Direction)
{
    if (!HasValidOwner()) { Reject(TEXT("owner")); return; }
    // Reliable ordered requests and a bounded positive serial delta prevent
    // duplicates/replays, including wrap-around. Consume rejected IDs as well.
    const uint32 Delta = ShotId - LastRequestId;
    if (ShotId == 0 || Delta == 0 || Delta > 1024) { Reject(TEXT("sequence")); return; }
    LastRequestId = ShotId;
    AVSCharacter* Pawn = Character();
    if (!Pawn->GetHealthComponent()->IsAlive()) { Reject(TEXT("dead")); return; }
    if (State.bReloading) { Reject(TEXT("reload")); return; }
    if (State.Magazine <= 0) { Reject(TEXT("empty")); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    // Absorb one 60Hz scheduling frame without granting sustained rate credit.
    const double ArrivalTolerance = FMath::Min(0.02, static_cast<double>(FireInterval) * 0.2);
    if (Now + ArrivalTolerance < NextServerFireTime) { Reject(TEXT("cadence")); return; }
    if (Origin.ContainsNaN() || Direction.ContainsNaN()) { Reject(TEXT("nonfinite")); return; }
    const FVector ServerOrigin = Pawn->GetPawnViewLocation();
    if (FVector::DistSquared(Origin, ServerOrigin) > FMath::Square(100.0)) { Reject(TEXT("origin")); return; }
    if (!FMath::IsNearlyEqual(Direction.SizeSquared(), 1.0, 0.01)
        || FVector::DotProduct(Direction, Pawn->GetBaseAimRotation().Vector()) < 0.9396926)
    {
        Reject(TEXT("direction")); return;
    }

    // RULE-01 integration must add phase eligibility here and in reload.
    // Current-position hit registration: no client victim, damage or rewind.
    NextServerFireTime = FMath::Max(Now, NextServerFireTime) + FireInterval;
    --State.Magazine;
    ++State.ShotsFired;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSRifleTrace), false, Pawn);
    if (GetWorld()->LineTraceSingleByChannel(Hit, ServerOrigin,
        ServerOrigin + Direction.GetSafeNormal() * Range, ECC_Visibility, Query))
    {
        if (AVSCharacter* Target = Cast<AVSCharacter>(Hit.GetActor()))
        {
            Target->GetHealthComponent()->ApplyPointDamage(Target->IsHeadHit(Hit.GetComponent()) ? HeadDamage : BodyDamage);
        }
    }
    Pawn->ForceNetUpdate();
}

void UVSRifleComponent::Reload()
{
    AVSCharacter* Pawn = Character();
    if (Pawn && Pawn->IsLocallyControlled()) { ServerRequestReload(); }
}

void UVSRifleComponent::ServerRequestReload_Implementation()
{
    if (!HasValidOwner() || !Character()->GetHealthComponent()->IsAlive()
        || State.bReloading || State.Magazine >= MagazineCapacity || State.Reserve <= 0) { return; }
    State.bReloading = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UVSRifleComponent::FinishReload, ReloadDuration, false);
    Character()->ForceNetUpdate();
}

void UVSRifleComponent::FinishReload()
{
    if (!HasValidOwner() || !Character()->GetHealthComponent()->IsAlive() || !State.bReloading)
    {
        State.bReloading = false;
        GetOwner()->ForceNetUpdate();
        return;
    }
    const int32 Transfer = FMath::Min(MagazineCapacity - State.Magazine, State.Reserve);
    State.Magazine += Transfer;
    State.Reserve -= Transfer;
    State.bReloading = false;
    GetOwner()->ForceNetUpdate();
}

void UVSRifleComponent::OwnerDied()
{
    StopFire();
    if (GetOwner()->HasAuthority())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
        State.bReloading = false;
        GetOwner()->ForceNetUpdate();
    }
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool UVSRifleComponent::DevelopmentReset()
{
    FString Role;
    const bool bOptIn = (FParse::Value(FCommandLine::Get(), TEXT("VSCombatSmoke="), Role) && Role == TEXT("server"))
        || FParse::Param(FCommandLine::Get(), TEXT("VSCombatDev"));
    if (!GetOwner()->HasAuthority() || !bOptIn) { return false; }
    StopFire();
    GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    State = FVSRifleState();
    State.Magazine = MagazineCapacity;
    State.Reserve = InitialReserve;
    NextServerFireTime = 0.0;
    // Preserve request history: old packets must not become valid after reset.
    GetOwner()->ForceNetUpdate();
    return true;
}

void UVSRifleComponent::DevelopmentRequest(uint32 ShotId, FVector Origin, FVector Direction)
{
    FString Role;
    if (FParse::Value(FCommandLine::Get(), TEXT("VSCombatSmoke="), Role)
        && (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        NextLocalShotId = ShotId;
        ServerRequestFire(ShotId, Origin, Direction);
    }
}
#endif
