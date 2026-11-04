#include "VSWeaponComponent.h"

#include "VSFireRequestRules.h"
#include "VSHealthComponent.h"
#include "VSWeaponLoadoutComponent.h"
#include "../VSCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
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
    // Ticks only on the owning client while a view kick is recovering.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
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
    const FVector Aim = Pawn->GetBaseAimRotation().Vector();
    // Predict with the same rule the server applies; the request carries only the
    // unmodified aim, never an offset.
    const uint32 Index = VSRecoilRules::RegisterShot(LocalRecoil, RecoilTuning, GetWorld()->GetTimeSeconds());
    LastPredictedShot = ResolveShot(NextLocalShotId, Index, Aim);
    ServerRequestFire(NextLocalShotId, Pawn->GetPawnViewLocation(), Aim);
    // Once per locally sent shot; replicated state never kicks the view again.
    ApplyViewKick(NextLocalShotId);
}

VSRecoilRules::Stance UVSWeaponComponent::CurrentStance() const
{
    const AVSCharacter* Pawn = Character();
    const UCharacterMovementComponent* Movement = Pawn ? Pawn->GetCharacterMovement() : nullptr;
    if (!Movement) { return VSRecoilRules::Stance::Standing; }
    return VSRecoilRules::ClassifyStance(Movement->IsFalling(), Pawn->bIsCrouched, Pawn->GetVelocity().Size2D(), RecoilTuning);
}

FVSShotSpread UVSWeaponComponent::ResolveShot(uint32 ShotId, uint32 Index, const FVector& Aim) const
{
    const VSRecoilRules::Stance ShooterStance = CurrentStance();
    const uint32 Seed = VSRecoilRules::ShotSeed(ShotId, static_cast<uint32>(DamageClass));
    const VSRecoilRules::Offset Deviation = VSRecoilRules::ShotOffset(RecoilTuning, Index, ShooterStance, Seed);
    const FVector Forward = Aim.GetSafeNormal();
    const FRotationMatrix Basis(Forward.Rotation());
    FVSShotSpread Shot;
    Shot.ShotId = ShotId;
    Shot.Index = Index;
    Shot.SpreadDegrees = static_cast<float>(VSRecoilRules::SpreadDegrees(RecoilTuning, Index, ShooterStance));
    Shot.Direction = (Forward
        + Basis.GetScaledAxis(EAxis::Y) * FMath::Tan(FMath::DegreesToRadians(Deviation.yaw))
        + Basis.GetScaledAxis(EAxis::Z) * FMath::Tan(FMath::DegreesToRadians(Deviation.pitch))).GetSafeNormal();
    return Shot;
}

void UVSWeaponComponent::ApplyViewKick(uint32 ShotId)
{
    AVSCharacter* Pawn = Character();
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !PC->IsLocalController()) { return; }
    const VSRecoilRules::Offset Kick = VSRecoilRules::ViewKick(RecoilTuning,
        VSRecoilRules::ShotSeed(ShotId, static_cast<uint32>(DamageClass)));
    if (Kick.pitch == 0.0 && Kick.yaw == 0.0) { return; }
    PC->SetControlRotation(PC->GetControlRotation() + FRotator(Kick.pitch, Kick.yaw, 0.0));
    OutstandingViewKick.pitch += Kick.pitch;
    OutstandingViewKick.yaw += Kick.yaw;
    LastViewKickTime = GetWorld()->GetTimeSeconds();
    SetComponentTickEnabled(true);
}

void UVSWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AVSCharacter* Pawn = Character();
    APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    const VSRecoilRules::Offset Step = VSRecoilRules::RecoverView(OutstandingViewKick, RecoilTuning,
        GetWorld()->GetTimeSeconds() - LastViewKickTime, DeltaTime);
    // Remove only the kick still outstanding, so the player's own aim changes stay.
    if (PC && PC->IsLocalController() && (Step.pitch != 0.0 || Step.yaw != 0.0))
    {
        PC->SetControlRotation(PC->GetControlRotation() - FRotator(Step.pitch, Step.yaw, 0.0));
    }
    OutstandingViewKick.pitch -= Step.pitch;
    OutstandingViewKick.yaw -= Step.yaw;
    if (!PC || !PC->IsLocalController() || (OutstandingViewKick.pitch == 0.0 && OutstandingViewKick.yaw == 0.0))
    {
        OutstandingViewKick = VSRecoilRules::Offset();
        SetComponentTickEnabled(false);
    }
}

float UVSWeaponComponent::GetCurrentSpreadDegrees() const
{
    const AVSCharacter* Pawn = Character();
    const VSRecoilRules::State& Heat = Pawn && Pawn->IsLocallyControlled() ? LocalRecoil : ServerRecoil;
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    return static_cast<float>(VSRecoilRules::SpreadDegrees(RecoilTuning,
        VSRecoilRules::ShotIndexAt(Heat, RecoilTuning, Now), CurrentStance()));
}

void UVSWeaponComponent::Reject(FName Reason)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++Rejections.FindOrAdd(Reason);
#endif
}

namespace
{
VSFireRequestRules::Vec3 ToFireRulesVec(const FVector& V) { return {V.X, V.Y, V.Z}; }

VSFireRequestRules::Readiness FireRulesReadiness(const AVSCharacter* Pawn, const UVSWeaponComponent* Weapon)
{
    const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent();
    switch (Loadout ? Loadout->GetReadiness(Weapon) : EVSWeaponReadiness::Ready)
    {
    case EVSWeaponReadiness::Holstered: return VSFireRequestRules::Readiness::Holstered;
    case EVSWeaponReadiness::Equipping: return VSFireRequestRules::Readiness::Equipping;
    default: return VSFireRequestRules::Readiness::Ready;
    }
}
}

void UVSWeaponComponent::ServerRequestFire_Implementation(uint32 ShotId, FVector Origin, FVector Direction)
{
    // Ordered checks, thresholds and reason names live in Combat/VSFireRequestRules.h.
    // Reliable ordered requests and a bounded positive serial delta prevent
    // duplicates/replays, including wrap-around. Rejected IDs past the sequence
    // check are consumed as well.
    AVSCharacter* Pawn = Character();
    VSFireRequestRules::FireInput In;
    In.bValidOwner = HasValidOwner();
    In.ShotId = ShotId;
    In.LastRequestId = LastRequestId;
    In.Origin = ToFireRulesVec(Origin);
    In.Direction = ToFireRulesVec(Direction);
    const double Now = GetWorld()->GetTimeSeconds();
    FVector ServerOrigin = FVector::ZeroVector;
    if (In.bValidOwner)
    {
        ServerOrigin = Pawn->GetPawnViewLocation();
        In.bAlive = Pawn->GetHealthComponent()->IsAlive();
        In.WeaponReadiness = FireRulesReadiness(Pawn, this);
        In.bReloading = State.bReloading;
        In.Magazine = State.Magazine;
        In.Now = Now;
        In.NextServerFireTime = NextServerFireTime;
        In.FireInterval = static_cast<double>(FireInterval);
        In.ServerOrigin = ToFireRulesVec(ServerOrigin);
        In.BaseAim = ToFireRulesVec(Pawn->GetBaseAimRotation().Vector());
    }
    // The live server gate processes expired deadlines before any ammo or hit
    // effect, so it runs only after every other check passed. Replicated flags
    // alone may still describe the previous phase.
    const VSFireRequestRules::FireVerdict Verdict = VSFireRequestRules::EvaluateFire(In,
        [Pawn] { return AVSMatchGameMode::AllowsCombat(Pawn) && Pawn->CanMatchCombat(); });
    if (Verdict.bConsumesShotId) { LastRequestId = ShotId; }
    if (Verdict.Outcome != VSFireRequestRules::Result::Accept)
    {
        Reject(VSFireRequestRules::ReasonName(Verdict.Outcome));
        return;
    }

    // Current-position hit registration: no client victim, damage or rewind.
    NextServerFireTime = FMath::Max(Now, NextServerFireTime) + FireInterval;
    --State.Magazine;
    ++State.ShotsFired;
    // Recoil and spread come from server state, the shot id and the server's view of
    // the pawn's stance; the validated aim is the only client input.
    const uint32 Index = VSRecoilRules::RegisterShot(ServerRecoil, RecoilTuning, Now);
    LastServerShot = ResolveShot(ShotId, Index, Direction);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSWeaponTrace), false, Pawn);
    if (GetWorld()->LineTraceSingleByChannel(Hit, ServerOrigin,
        ServerOrigin + LastServerShot.Direction * Range, ECC_Visibility, Query))
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
    AVSCharacter* Pawn = Character();
    VSFireRequestRules::ReloadInput In;
    In.bValidOwner = HasValidOwner();
    if (In.bValidOwner)
    {
        In.bAlive = Pawn->GetHealthComponent()->IsAlive();
        In.WeaponReadiness = FireRulesReadiness(Pawn, this);
        In.bReloading = State.bReloading;
        In.Magazine = State.Magazine;
        In.MagazineCapacity = MagazineCapacity;
        In.Reserve = State.Reserve;
    }
    const VSFireRequestRules::Result Verdict = VSFireRequestRules::EvaluateReload(In,
        [Pawn] { return AVSMatchGameMode::AllowsCombat(Pawn) && Pawn->CanMatchCombat(); });
    if (Verdict == VSFireRequestRules::Result::Ignored) { return; }
    if (Verdict != VSFireRequestRules::Result::Accept)
    {
        Reject(VSFireRequestRules::ReasonName(Verdict));
        return;
    }
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
    ServerRecoil = VSRecoilRules::State();
    LastServerShot = FVSShotSpread();
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
