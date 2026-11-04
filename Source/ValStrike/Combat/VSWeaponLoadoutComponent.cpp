#include "VSWeaponLoadoutComponent.h"

#include "VSWeaponLoadoutRules.h"

#include "VSHealthComponent.h"
#include "VSKnifeComponent.h"
#include "VSKnifeRules.h"
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

namespace VSLoadoutDetail
{
namespace Rules = VSWeaponLoadoutRules;
// Engine enums mirror the rules' slot and request domains value for value; the
// two domains only meet through EncodeRequest/DecodeRequest.
static_assert(static_cast<uint8>(EVSWeaponSlot::Rifle) == static_cast<uint8>(Rules::Slot::Rifle)
    && static_cast<uint8>(EVSWeaponSlot::Pistol) == static_cast<uint8>(Rules::Slot::Pistol)
    && static_cast<uint8>(EVSWeaponSlot::Knife) == static_cast<uint8>(Rules::Slot::Knife), "slot domain");
static_assert(static_cast<uint8>(EVSEquipRequest::Rifle) == Rules::RequestRifle
    && static_cast<uint8>(EVSEquipRequest::Pistol) == Rules::RequestPistol
    && static_cast<uint8>(EVSEquipRequest::Last) == Rules::RequestLast
    && static_cast<uint8>(EVSEquipRequest::Knife) == Rules::RequestKnife, "request domain");

Rules::Slot ToRulesSlot(EVSWeaponSlot Slot) { return static_cast<Rules::Slot>(Slot); }
EVSWeaponSlot FromRulesSlot(Rules::Slot Slot) { return static_cast<EVSWeaponSlot>(Slot); }
uint8 RequestByte(EVSEquipRequest Request) { return static_cast<uint8>(Request); }
}

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
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(EquipTimer);
        GetWorld()->GetTimerManager().ClearTimer(SendTimer);
    }
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
    case EVSWeaponSlot::Knife: return nullptr; // Not a firearm.
    }
    return nullptr;
}

UVSKnifeComponent* UVSWeaponLoadoutComponent::GetKnife() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UVSKnifeComponent>() : nullptr;
}

bool UVSWeaponLoadoutComponent::HasSlot(EVSWeaponSlot Slot) const
{
    return Slot == EVSWeaponSlot::Knife ? GetKnife() != nullptr : GetWeapon(Slot) != nullptr;
}

float UVSWeaponLoadoutComponent::EquipDurationFor(EVSWeaponSlot Slot) const
{
    return Slot == EVSWeaponSlot::Knife ? static_cast<float>(VSKnifeRules::EquipSeconds) : EquipDuration;
}

VSWeaponLoadoutRules::EquipTimes UVSWeaponLoadoutComponent::GetEquipTimes() const
{
    VSWeaponLoadoutRules::EquipTimes Times;
    Times.firearm = EquipDuration;
    Times.knife = VSKnifeRules::EquipSeconds;
    return Times;
}

EVSWeaponSlot UVSWeaponLoadoutComponent::GetEquippedSlot() const
{
    // Only the owning client ever activates the prediction.
    return Prediction.active ? VSLoadoutDetail::FromRulesSlot(Prediction.equipped) : State.Equipped;
}

bool UVSWeaponLoadoutComponent::IsEquipping() const
{
    return Prediction.active ? GetWorld()->GetTimeSeconds() < Prediction.readyTime : State.bEquipping;
}

VSWeaponLoadoutRules::ServerView UVSWeaponLoadoutComponent::GetServerView() const
{
    VSWeaponLoadoutRules::ServerView View;
    View.equipped = VSLoadoutDetail::ToRulesSlot(State.Equipped);
    View.previous = VSLoadoutDetail::ToRulesSlot(State.Previous);
    View.equipping = State.bEquipping;
    View.equipCount = State.EquipCount;
    View.ack = State.AckSequence;
    return View;
}

EVSWeaponReadiness UVSWeaponLoadoutComponent::GetReadiness(const UVSWeaponComponent* Weapon) const
{
    // A null equipped firearm (knife slot) never matches: firearms are holstered.
    if (!Weapon || Weapon != GetEquippedWeapon()) { return EVSWeaponReadiness::Holstered; }
    return SlotReadiness(GetEquippedSlot());
}

EVSWeaponReadiness UVSWeaponLoadoutComponent::GetKnifeReadiness() const
{
    if (!IsKnifeEquipped()) { return EVSWeaponReadiness::Holstered; }
    return SlotReadiness(EVSWeaponSlot::Knife);
}

EVSWeaponReadiness UVSWeaponLoadoutComponent::SlotReadiness(EVSWeaponSlot Slot) const
{
    if (Slot != GetEquippedSlot()) { return EVSWeaponReadiness::Holstered; }
    // The owner's predicted readiness only gates its own requests; the server
    // still rejects fire for a weapon that is holstered or equipping there.
    const bool bReady = GetOwner()->HasAuthority()
        ? VSWeaponLoadoutRules::IsReady(true, GetWorld()->GetTimeSeconds(), EquipReadyTime)
        : !IsEquipping();
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
    const double Now = GetWorld()->GetTimeSeconds();
    if (Pawn->HasAuthority())
    {
        // A listen-server host switches on the call itself; nothing to predict.
        // Mirror the server's duplicate and rate checks.
        VSWeaponLoadoutRules::Slot Target = VSWeaponLoadoutRules::Slot::Rifle;
        if (!VSWeaponLoadoutRules::ResolveRequest(VSLoadoutDetail::RequestByte(Request), VSLoadoutDetail::ToRulesSlot(State.Previous), Target)
            || VSLoadoutDetail::FromRulesSlot(Target) == State.Equipped || Now - LastLocalRequestTime < MinSwitchInterval)
        {
            return;
        }
        LastLocalRequestTime = Now;
        Pawn->StopCombatInput();
        ServerRequestEquip(VSLoadoutDetail::RequestByte(Request), 0);
        return;
    }
    // Switch the local view and start the equip timer now; the request follows
    // within the server's rate limit, coalesced to the newest press.
    if (!Prediction.Press(VSLoadoutDetail::RequestByte(Request), GetServerView(), Now, GetEquipTimes())) { return; }
    Pawn->StopCombatInput();
    SendPredictedRequest();
}

void UVSWeaponLoadoutComponent::SendPredictedRequest()
{
    UWorld* World = GetWorld();
    const AVSCharacter* Pawn = Character();
    if (!World || !Pawn) { return; }
    const double Now = World->GetTimeSeconds();
    // A queued press that can no longer be accepted falls back to server state.
    if (!Pawn->CanMatchCombat()) { Prediction.DropQueued(); }
    uint32 Sequence = 0;
    VSWeaponLoadoutRules::Slot Target = VSWeaponLoadoutRules::Slot::Rifle;
    if (Prediction.TakeSend(Now, MinSwitchInterval, Sequence, Target))
    {
        ServerRequestEquip(VSWeaponLoadoutRules::EncodeRequest(Target), Sequence);
    }
    else if (Prediction.queued)
    {
        const double Delay = FMath::Max(Prediction.NextSendTime(MinSwitchInterval) - Now, 0.001);
        World->GetTimerManager().SetTimer(SendTimer, this, &UVSWeaponLoadoutComponent::SendPredictedRequest, static_cast<float>(Delay), false);
        return;
    }
    // A cancelled or dropped request may already be settled by the last snapshot.
    ReconcilePrediction();
    StopStowedWeapons();
}

void UVSWeaponLoadoutComponent::ReconcilePrediction()
{
    if (Prediction.active) { Prediction.Reconcile(GetServerView(), GetWorld()->GetTimeSeconds()); }
}

void UVSWeaponLoadoutComponent::Reject(FName Reason)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++Rejections.FindOrAdd(Reason);
#endif
}

void UVSWeaponLoadoutComponent::ServerRequestEquip_Implementation(uint8 Request, uint32 Sequence)
{
    if (!HasValidOwner()) { Reject(TEXT("owner")); return; }
    // Ack every processed sequenced request, accepted or not, so the owner can
    // keep or snap back its prediction. Reliable ordering keeps acks monotonic.
    if (Sequence != 0)
    {
        State.AckSequence = Sequence;
        GetOwner()->ForceNetUpdate();
    }
    namespace Rules = VSWeaponLoadoutRules;
    AVSCharacter* Pawn = Character();
    Rules::Request In;
    // Raw bytes from the network are not trusted to be a valid enumerator.
    In.raw = Request;
    In.alive = Pawn->GetHealthComponent()->IsAlive();
    // The live match gate first processes expired deadlines, as for firing.
    In.combatAllowed = In.alive && AVSMatchGameMode::AllowsCombat(Pawn) && Pawn->CanMatchCombat();
    In.equipped = VSLoadoutDetail::ToRulesSlot(State.Equipped);
    In.previous = VSLoadoutDetail::ToRulesSlot(State.Previous);
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
    const EVSWeaponSlot Target = VSLoadoutDetail::FromRulesSlot(Out.target);
    if (!HasSlot(Target)) { Reject(TEXT("invalid")); return; }
    Equip(Target);
}

void UVSWeaponLoadoutComponent::Equip(EVSWeaponSlot Slot)
{
    const double Now = GetWorld()->GetTimeSeconds();
    HolsterSlot(State.Equipped);
    State.Previous = State.Equipped;
    State.Equipped = Slot;
    ++State.EquipCount;
    LastSwitchTime = Now;
    const float Duration = EquipDurationFor(Slot);
    EquipReadyTime = Now + Duration;
    State.bEquipping = Duration > 0.0f;
    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (State.bEquipping) { Timers.SetTimer(EquipTimer, this, &UVSWeaponLoadoutComponent::FinishEquip, Duration, false); }
    else { Timers.ClearTimer(EquipTimer); }
    OnRep_State();
    GetOwner()->ForceNetUpdate();
}

void UVSWeaponLoadoutComponent::HolsterSlot(EVSWeaponSlot Slot)
{
    // Holstering stops fire and cancels an active reload without ammo transfer;
    // a stowed knife never lands a pending impact.
    if (Slot == EVSWeaponSlot::Knife)
    {
        if (UVSKnifeComponent* Knife = GetKnife()) { Knife->CancelActions(); }
    }
    else if (UVSWeaponComponent* Outgoing = GetWeapon(Slot))
    {
        Outgoing->Holster();
    }
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
    ReconcilePrediction();
    StopStowedWeapons();
}

void UVSWeaponLoadoutComponent::StopStowedWeapons()
{
    // A stowed weapon never keeps a held local fire timer.
    const EVSWeaponSlot Equipped = GetEquippedSlot();
    for (const EVSWeaponSlot Slot : {EVSWeaponSlot::Rifle, EVSWeaponSlot::Pistol})
    {
        if (Slot != Equipped)
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
        ServerRequestEquip(RawRequest, 0);
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
    if (UVSKnifeComponent* Knife = GetKnife()) { Knife->CancelActions(); }
    const uint32 EquipCount = State.EquipCount;
    const uint32 AckSequence = State.AckSequence;
    State = FVSLoadoutState();
    State.EquipCount = EquipCount;
    State.AckSequence = AckSequence;
    EquipReadyTime = 0.0;
    LastSwitchTime = -1.0e9;
    OnRep_State();
    GetOwner()->ForceNetUpdate();
    return true;
}
#endif
