#include "VSWeaponLoadoutComponent.h"

#include "VSWeaponLoadoutRules.h"

#include "VSHealthComponent.h"
#include "VSPistolComponent.h"
#include "VSRifleComponent.h"
#include "../VSCharacter.h"
#include "../Match/VSMatchGameMode.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

UVSWeaponLoadoutComponent::UVSWeaponLoadoutComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

AVSCharacter* UVSWeaponLoadoutComponent::Character() const { return Cast<AVSCharacter>(GetOwner()); }

void UVSWeaponLoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    // Clamped on every instance: clients use EquipDuration for presentation timing.
    EquipDuration = FMath::IsFinite(EquipDuration) ? FMath::Clamp(EquipDuration, 0.0f, 5.0f) : 0.5f;
    MinSwitchInterval = FMath::IsFinite(MinSwitchInterval) ? FMath::Clamp(MinSwitchInterval, 0.0f, 2.0f) : 0.1f;
}

void UVSWeaponLoadoutComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(EquipTimer); }
    Super::EndPlay(Reason);
}

void UVSWeaponLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UVSWeaponLoadoutComponent, State);
}

UVSWeaponComponent* UVSWeaponLoadoutComponent::GetWeapon(EVSWeaponSlot Slot) const
{
    const AVSCharacter* Pawn = Character();
    if (!Pawn) { return nullptr; }
    switch (Slot)
    {
    case EVSWeaponSlot::Rifle: return Pawn->GetRifleComponent();
    case EVSWeaponSlot::Pistol: return Pawn->GetPistolComponent();
    }
    return nullptr;
}

EVSWeaponReadiness UVSWeaponLoadoutComponent::GetReadiness(const UVSWeaponComponent* Weapon) const
{
    if (!Weapon || Weapon != GetEquippedWeapon()) { return EVSWeaponReadiness::Holstered; }
    const bool bReady = GetOwner()->HasAuthority()
        ? VSWeaponLoadoutRules::IsReady(true, GetWorld()->GetTimeSeconds(), EquipReadyTime)
        : !State.bEquipping;
    return bReady ? EVSWeaponReadiness::Ready : EVSWeaponReadiness::Equipping;
}

bool UVSWeaponLoadoutComponent::HasValidOwner() const
{
    const AVSCharacter* Pawn = Character();
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    // Same possession rule as weapon requests; the actor channel checks ownership.
    return Pawn && Pawn->HasAuthority() && PC && PC->GetPawn() == Pawn && Pawn->GetOwner() == PC;
}

void UVSWeaponLoadoutComponent::RequestEquip(EVSEquipRequest Request)
{
    AVSCharacter* Pawn = Character();
    if (!Pawn || !Pawn->IsLocallyControlled() || !Pawn->CanMatchCombat()) { return; }
    const EVSWeaponSlot Target = Request == EVSEquipRequest::Last ? State.Previous : static_cast<EVSWeaponSlot>(Request);
    // Mirror the server's duplicate and rate checks so a normal client does not
    // send requests that can only be rejected.
    const double Now = GetWorld()->GetTimeSeconds();
    if (Target == State.Equipped || Now - LastLocalRequestTime < MinSwitchInterval) { return; }
    LastLocalRequestTime = Now;
    Pawn->StopCombatInput();
    ServerRequestEquip(static_cast<uint8>(Request));
}

void UVSWeaponLoadoutComponent::Reject(FName Reason)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++Rejections.FindOrAdd(Reason);
#endif
}

void UVSWeaponLoadoutComponent::ServerRequestEquip_Implementation(uint8 Request)
{
    if (!HasValidOwner()) { Reject(TEXT("owner")); return; }
    namespace Rules = VSWeaponLoadoutRules;
    AVSCharacter* Pawn = Character();
    Rules::Request In;
    // Raw bytes from the network are not trusted to be a valid enumerator.
    In.raw = Request;
    In.alive = Pawn->GetHealthComponent()->IsAlive();
    // The live match gate first processes expired deadlines, as for firing.
    In.combatAllowed = In.alive && AVSMatchGameMode::AllowsCombat(Pawn) && Pawn->CanMatchCombat();
    In.equipped = static_cast<Rules::Slot>(State.Equipped);
    In.previous = static_cast<Rules::Slot>(State.Previous);
    In.now = GetWorld()->GetTimeSeconds();
    In.lastSwitch = LastSwitchTime;
    In.minInterval = MinSwitchInterval;
    const Rules::Result Out = Rules::Evaluate(In);
    switch (Out.verdict)
    {
    case Rules::Verdict::Accept: break;
    case Rules::Verdict::Invalid: Reject(TEXT("invalid")); return;
    case Rules::Verdict::Dead: Reject(TEXT("dead")); return;
    case Rules::Verdict::Phase: Reject(TEXT("phase")); return;
    case Rules::Verdict::Duplicate: Reject(TEXT("duplicate")); return;
    case Rules::Verdict::Spam: Reject(TEXT("spam")); return;
    }
    const EVSWeaponSlot Target = static_cast<EVSWeaponSlot>(Out.target);
    if (!GetWeapon(Target)) { Reject(TEXT("invalid")); return; }
    Equip(Target);
}

void UVSWeaponLoadoutComponent::Equip(EVSWeaponSlot Slot)
{
    const double Now = GetWorld()->GetTimeSeconds();
    // Holstering stops fire and cancels an active reload without ammo transfer.
    if (UVSWeaponComponent* Outgoing = GetEquippedWeapon()) { Outgoing->Holster(); }
    State.Previous = State.Equipped;
    State.Equipped = Slot;
    ++State.EquipCount;
    LastSwitchTime = Now;
    EquipReadyTime = Now + EquipDuration;
    State.bEquipping = EquipDuration > 0.0f;
    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (State.bEquipping) { Timers.SetTimer(EquipTimer, this, &UVSWeaponLoadoutComponent::FinishEquip, EquipDuration, false); }
    else { Timers.ClearTimer(EquipTimer); }
    OnRep_State();
    GetOwner()->ForceNetUpdate();
}

void UVSWeaponLoadoutComponent::FinishEquip()
{
    // Readiness for requests uses EquipReadyTime; this only publishes the state.
    State.bEquipping = false;
    OnRep_State();
    GetOwner()->ForceNetUpdate();
}

void UVSWeaponLoadoutComponent::OnRep_State()
{
    // A stowed weapon never keeps a held local fire timer.
    for (const EVSWeaponSlot Slot : {EVSWeaponSlot::Rifle, EVSWeaponSlot::Pistol})
    {
        if (Slot != State.Equipped)
        {
            if (UVSWeaponComponent* Weapon = GetWeapon(Slot)) { Weapon->StopFire(); }
        }
    }
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void UVSWeaponLoadoutComponent::DevelopmentRequestEquip(uint8 RawRequest)
{
    FString Role;
    if (FParse::Value(FCommandLine::Get(), TEXT("VSWeaponSmoke="), Role)
        && (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        ServerRequestEquip(RawRequest);
    }
}

bool UVSWeaponLoadoutComponent::DevelopmentReset()
{
    FString Role;
    const bool bOptIn = (FParse::Value(FCommandLine::Get(), TEXT("VSWeaponSmoke="), Role) && Role == TEXT("server"))
        || (FParse::Value(FCommandLine::Get(), TEXT("VSCombatSmoke="), Role) && Role == TEXT("server"))
        || FParse::Param(FCommandLine::Get(), TEXT("VSCombatDev"));
    if (!GetOwner()->HasAuthority() || !bOptIn) { return false; }
    GetWorld()->GetTimerManager().ClearTimer(EquipTimer);
    const uint32 EquipCount = State.EquipCount;
    State = FVSLoadoutState();
    State.EquipCount = EquipCount;
    EquipReadyTime = 0.0;
    LastSwitchTime = -1.0e9;
    OnRep_State();
    GetOwner()->ForceNetUpdate();
    return true;
}
#endif
