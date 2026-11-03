#include "VSWeaponComponent.h"

#include "VSHealthComponent.h"
#include "VSWeaponLoadoutComponent.h"
#include "../VSCharacter.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "../Damage/VSDamageSubsystem.h"
#include "../Match/VSMatchGameMode.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include <initializer_list>

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

UVSWeaponComponent::UVSWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

AVSCharacter* UVSWeaponComponent::Character() const { return Cast<AVSCharacter>(GetOwner()); }

void UVSWeaponComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority())
    {
        MagazineCapacity = FMath::Clamp(MagazineCapacity, 1, 100);
        InitialReserve = FMath::Clamp(InitialReserve, 0, 1000);
        FireInterval = FMath::IsFinite(FireInterval) ? FMath::Clamp(FireInterval, 0.05f, 5.0f) : 0.12f;
        ReloadDuration = FMath::IsFinite(ReloadDuration) ? FMath::Clamp(ReloadDuration, 0.1f, 20.0f) : 2.0f;
        Range = FMath::IsFinite(Range) ? FMath::Clamp(Range, 1.0f, 50000.0f) : 10000.0f;
        State.Magazine = MagazineCapacity;
        State.Reserve = InitialReserve;
    }
}

void UVSWeaponComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopFire();
    GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
    Super::EndPlay(Reason);
}

void UVSWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UVSWeaponComponent, State);
    DOREPLIFETIME(UVSWeaponComponent, MagazineCapacity);
    DOREPLIFETIME(UVSWeaponComponent, FireInterval);
}

bool UVSWeaponComponent::HasValidOwner() const
{
    const AVSCharacter* Pawn = Character();
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    // Unreal's actor channel enforces the sending connection's ownership before
    // delivering a component Server RPC. Also require current possession here.
    return Pawn && Pawn->HasAuthority() && PC && PC->GetPawn() == Pawn && Pawn->GetOwner() == PC;
}

bool UVSWeaponComponent::IsReadyLocally() const
{
    const AVSCharacter* Pawn = Character();
    const UVSWeaponLoadoutComponent* Loadout = Pawn ? Pawn->GetLoadoutComponent() : nullptr;
    return !Loadout || Loadout->GetReadiness(this) == EVSWeaponReadiness::Ready;
}

void UVSWeaponComponent::StartFire()
{
    AVSCharacter* Pawn = Character();
    if (bFireHeld || !Pawn || !Pawn->IsLocallyControlled() || !Pawn->CanMatchCombat()) { return; }
    bFireHeld = true;
    SendShot();
    // Semi-automatic weapons send one request per press; the press stays held
    // until release so a held button cannot fire again.
    if (bFireHeld && bAutomatic)
    {
        GetWorld()->GetTimerManager().SetTimer(LocalFireTimer, this, &UVSWeaponComponent::SendShot, FireInterval, true);
    }
}

void UVSWeaponComponent::StopFire()
{
    bFireHeld = false;
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(LocalFireTimer); }
}

void UVSWeaponComponent::SendShot()
{
    AVSCharacter* Pawn = Character();
    if (!bFireHeld || !Pawn || !Pawn->IsLocallyControlled() || !Pawn->CanMatchCombat())
    {
        StopFire();
        return;
    }
    // Held automatic fire waits through reloads and equip time; the server
    // independently rejects any request that arrives before it is ready.
    if (State.bReloading || State.Magazine <= 0 || !IsReadyLocally()) { return; }
    // Zero is reserved; sequence never resets during the pawn's lifetime.
    if (++NextLocalShotId == 0) { ++NextLocalShotId; }
    ServerRequestFire(NextLocalShotId, Pawn->GetPawnViewLocation(), Pawn->GetBaseAimRotation().Vector());
}

void UVSWeaponComponent::Reject(FName Reason)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++Rejections.FindOrAdd(Reason);
#endif
}

void UVSWeaponComponent::ServerRequestFire_Implementation(uint32 ShotId, FVector Origin, FVector Direction)
{
    if (!HasValidOwner()) { Reject(TEXT("owner")); return; }
    // Reliable ordered requests and a bounded positive serial delta prevent
    // duplicates/replays, including wrap-around. Consume rejected IDs as well.
    const uint32 Delta = ShotId - LastRequestId;
    if (ShotId == 0 || Delta == 0 || Delta > 1024) { Reject(TEXT("sequence")); return; }
    LastRequestId = ShotId;
    AVSCharacter* Pawn = Character();
    if (!Pawn->GetHealthComponent()->IsAlive()) { Reject(TEXT("dead")); return; }
    // Only the equipped weapon may fire, and only after its equip time.
    if (const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent())
    {
        const EVSWeaponReadiness Readiness = Loadout->GetReadiness(this);
        if (Readiness == EVSWeaponReadiness::Holstered) { Reject(TEXT("holstered")); return; }
        if (Readiness == EVSWeaponReadiness::Equipping) { Reject(TEXT("equip")); return; }
    }
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

    // The live server gate processes expired deadlines before any ammo or hit
    // effect. Replicated flags alone may still describe the previous phase.
    if (!AVSMatchGameMode::AllowsCombat(Pawn) || !Pawn->CanMatchCombat())
    {
        Reject(TEXT("phase"));
        return;
    }
    // Current-position hit registration: no client victim, damage or rewind.
    NextServerFireTime = FMath::Max(Now, NextServerFireTime) + FireInterval;
    --State.Magazine;
    ++State.ShotsFired;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSWeaponTrace), false, Pawn);
    if (GetWorld()->LineTraceSingleByChannel(Hit, ServerOrigin,
        ServerOrigin + Direction.GetSafeNormal() * Range, ECC_Visibility, Query))
    {
        AVSCharacter* Target = Cast<AVSCharacter>(Hit.GetActor());
        UVSDamageSubsystem* DamageService = UVSDamageSubsystem::Get(this);
        if (Target && DamageService)
        {
            // Zone lookup, the damage table, the health gate, attribution, stats and kill
            // credit all live behind this one server call.
            FVSWeaponHit Shot;
            Shot.Shooter = Pawn;
            Shot.Victim = Target;
            Shot.HitComponent = Hit.GetComponent();
            Shot.Weapon = DamageClass;
            Shot.DistanceCm = Hit.Distance;
            DamageService->ApplyWeaponHit(Shot);
        }
    }
    Pawn->ForceNetUpdate();
}

void UVSWeaponComponent::Reload()
{
    AVSCharacter* Pawn = Character();
    if (Pawn && Pawn->IsLocallyControlled() && Pawn->CanMatchCombat() && IsReadyLocally()) { ServerRequestReload(); }
}

void UVSWeaponComponent::ServerRequestReload_Implementation()
{
    if (!HasValidOwner() || !Character()->GetHealthComponent()->IsAlive()) { return; }
    AVSCharacter* Pawn = Character();
    if (!AVSMatchGameMode::AllowsCombat(Pawn) || !Pawn->CanMatchCombat())
    {
        Reject(TEXT("phase"));
        return;
    }
    if (const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent())
    {
        const EVSWeaponReadiness Readiness = Loadout->GetReadiness(this);
        if (Readiness == EVSWeaponReadiness::Holstered) { Reject(TEXT("holstered")); return; }
        if (Readiness == EVSWeaponReadiness::Equipping) { Reject(TEXT("equip")); return; }
    }
    if (State.bReloading || State.Magazine >= MagazineCapacity || State.Reserve <= 0) { return; }
    State.bReloading = true;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &UVSWeaponComponent::FinishReload, ReloadDuration, false);
    Character()->ForceNetUpdate();
}

void UVSWeaponComponent::FinishReload()
{
    if (!HasValidOwner() || !Character()->GetHealthComponent()->IsAlive() || !State.bReloading)
    {
        State.bReloading = false;
        GetOwner()->ForceNetUpdate();
        return;
    }
    AVSCharacter* Pawn = Character();
    if (!AVSMatchGameMode::AllowsCombat(Pawn) || !Pawn->CanMatchCombat())
    {
        Reject(TEXT("phase"));
        OwnerDied();
        return;
    }
    // Holster() clears this timer; never complete a reload for a stowed weapon.
    const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent();
    if (Loadout && Loadout->GetReadiness(this) == EVSWeaponReadiness::Holstered)
    {
        Reject(TEXT("holstered"));
        CancelActions();
        return;
    }
    const int32 Transfer = FMath::Min(MagazineCapacity - State.Magazine, State.Reserve);
    State.Magazine += Transfer;
    State.Reserve -= Transfer;
    State.bReloading = false;
    GetOwner()->ForceNetUpdate();
}

void UVSWeaponComponent::CancelActions()
{
    StopFire();
    if (GetOwner()->HasAuthority())
    {
        GetWorld()->GetTimerManager().ClearTimer(ReloadTimer);
        State.bReloading = false;
        GetOwner()->ForceNetUpdate();
    }
}

void UVSWeaponComponent::OwnerDied() { CancelActions(); }
void UVSWeaponComponent::Holster() { CancelActions(); }

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
namespace
{
bool HasWeaponDevelopmentRole(std::initializer_list<const TCHAR*> CombatRoles, bool bMatchServer)
{
    FString Role;
    for (const TCHAR* Switch : {TEXT("VSCombatSmoke="), TEXT("VSWeaponSmoke=")})
    {
        if (FParse::Value(FCommandLine::Get(), Switch, Role))
        {
            for (const TCHAR* Allowed : CombatRoles) { if (Role == Allowed) { return true; } }
        }
    }
    return bMatchServer && FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) && Role == TEXT("server");
}
}

bool UVSWeaponComponent::DevelopmentReset()
{
    const bool bOptIn = HasWeaponDevelopmentRole({TEXT("server")}, false)
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

void UVSWeaponComponent::DevelopmentRequest(uint32 ShotId, FVector Origin, FVector Direction)
{
    if (HasWeaponDevelopmentRole({TEXT("server"), TEXT("driver"), TEXT("observer")}, true))
    {
        NextLocalShotId = ShotId;
        ServerRequestFire(ShotId, Origin, Direction);
    }
}

void UVSWeaponComponent::DevelopmentRequestReload()
{
    FString Role;
    if (GetOwner() && GetOwner()->HasAuthority() &&
        FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) && Role == TEXT("server"))
    {
        ServerRequestReload();
    }
}

void UVSWeaponComponent::DevelopmentFinishReload()
{
    FString Role;
    if (GetOwner() && GetOwner()->HasAuthority() && State.bReloading &&
        FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) && Role == TEXT("server"))
    {
        FinishReload();
    }
}
#endif
