#include "VSKnifeComponent.h"

#include "VSHealthComponent.h"
#include "VSWeaponLoadoutComponent.h"
#include "../VSCharacter.h"
#include "../Damage/VSDamageEvents.h"
#include "../Damage/VSDamageSubsystem.h"
#include "../Match/VSMatchGameMode.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#endif

namespace VSKnifeDetail
{
// Candidates examined per impact; the match has four pawns.
constexpr std::size_t MaxCandidates = 8;

static_assert(static_cast<uint8>(EVSKnifeAttack::Slash) == VSKnifeRules::EncodeAttack(VSKnifeRules::Attack::Slash)
    && static_cast<uint8>(EVSKnifeAttack::Stab) == VSKnifeRules::EncodeAttack(VSKnifeRules::Attack::Stab), "attack bytes");
static_assert(static_cast<uint8>(EVSKnifeImpact::Wall) == static_cast<uint8>(VSKnifeRules::ImpactResult::Wall)
    && static_cast<uint8>(EVSKnifeImpact::Player) == static_cast<uint8>(VSKnifeRules::ImpactResult::Player), "impact values");

EVSKnifeAttack ToAttack(VSKnifeRules::Attack Kind)
{
    return Kind == VSKnifeRules::Attack::Slash ? EVSKnifeAttack::Slash : EVSKnifeAttack::Stab;
}

EVSKnifeImpact ToImpact(VSKnifeRules::ImpactResult Result) { return static_cast<EVSKnifeImpact>(Result); }
}

UVSKnifeComponent::UVSKnifeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

AVSCharacter* UVSKnifeComponent::Character() const { return Cast<AVSCharacter>(GetOwner()); }

void UVSKnifeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UVSKnifeComponent, State);
}

void UVSKnifeComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority())
    {
        if (UVSHealthComponent* Health = GetOwner()->FindComponentByClass<UVSHealthComponent>())
        {
            Health->OnLifeStateChanged.AddUObject(this, &UVSKnifeComponent::HandleLifeStateChanged);
        }
    }
}

void UVSKnifeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    // A destroyed pawn or a round reset never resolves a delayed impact.
    Actions.CancelAll();
    ClearImpactTimers();
    if (UVSHealthComponent* Health = GetOwner() ? GetOwner()->FindComponentByClass<UVSHealthComponent>() : nullptr)
    {
        Health->OnLifeStateChanged.RemoveAll(this);
    }
    KnifeEvent.Clear();
    Super::EndPlay(Reason);
}

void UVSKnifeComponent::ClearImpactTimers()
{
    if (UWorld* World = GetWorld())
    {
        for (FTimerHandle& Timer : ImpactTimers) { World->GetTimerManager().ClearTimer(Timer); }
    }
}

bool UVSKnifeComponent::HasValidOwner() const
{
    const AVSCharacter* Pawn = Character();
    const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    // Same possession rule as weapon and loadout requests.
    return Pawn && Pawn->HasAuthority() && PC && PC->GetPawn() == Pawn && Pawn->GetOwner() == PC;
}

void UVSKnifeComponent::Reject(FName Reason)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    ++Rejections.FindOrAdd(Reason);
#endif
}

void UVSKnifeComponent::RequestAttack(EVSKnifeAttack Kind)
{
    AVSCharacter* Pawn = Character();
    if (!Pawn || !Pawn->IsLocallyControlled() || !Pawn->CanMatchCombat()) { return; }
    const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent();
    // Mirror the server's slot, equip and cooldown checks so a normal client does
    // not send requests that can only be rejected.
    if (!Loadout || Loadout->GetKnifeReadiness() != EVSWeaponReadiness::Ready) { return; }
    VSKnifeRules::Attack Attack = VSKnifeRules::Attack::Slash;
    if (!VSKnifeRules::DecodeAttack(static_cast<uint8>(Kind), Attack)) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < NextLocalAttackTime) { return; }
    NextLocalAttackTime = Now + VSKnifeRules::TuningFor(Attack).nextAttack;
    if (++NextLocalSequence == 0) { ++NextLocalSequence; }
    // Cosmetic only: the swing animation may start now; damage waits for the server.
    KnifeEvent.Broadcast(EVSKnifeEvent::LocalStart, Kind, EVSKnifeImpact::None);
    ServerRequestKnifeAttack(VSKnifeRules::EncodeAttack(Attack), NextLocalSequence);
}

void UVSKnifeComponent::ServerRequestKnifeAttack_Implementation(uint8 Kind, uint32 Sequence)
{
    namespace Rules = VSKnifeRules;
    AVSCharacter* Pawn = Character();
    const UVSWeaponLoadoutComponent* Loadout = Pawn ? Pawn->GetLoadoutComponent() : nullptr;
    Rules::Request In;
    In.validOwner = HasValidOwner() && Loadout;
    In.sequence = Sequence;
    In.lastSequence = LastRequestId;
    // Raw bytes from the network are not trusted to be a valid enumerator.
    In.rawKind = Kind;
    In.alive = In.validOwner && Pawn->GetHealthComponent()->IsAlive();
    // The live match gate first processes expired deadlines, as for firing.
    In.combatAllowed = In.alive && AVSMatchGameMode::AllowsCombat(Pawn) && Pawn->CanMatchCombat();
    In.knifeEquipped = Loadout && Loadout->GetState().Equipped == EVSWeaponSlot::Knife;
    In.equipReady = Loadout && Loadout->GetKnifeReadiness() == EVSWeaponReadiness::Ready;
    In.now = GetWorld()->GetTimeSeconds();
    In.nextAttackTime = NextAttackTime;
    In.pending = Actions.Count();
    Rules::Attack Attack = Rules::Attack::Slash;
    const Rules::Verdict Outcome = Rules::Evaluate(In, Attack);
    if (Rules::ConsumesSequence(Outcome)) { LastRequestId = Sequence; }
    switch (Outcome)
    {
    case Rules::Verdict::Accept: break;
    case Rules::Verdict::Owner: Reject(TEXT("owner")); return;
    case Rules::Verdict::Sequence: Reject(TEXT("sequence")); return;
    case Rules::Verdict::Invalid: Reject(TEXT("invalid")); return;
    case Rules::Verdict::Dead: Reject(TEXT("dead")); return;
    case Rules::Verdict::Phase: Reject(TEXT("phase")); return;
    case Rules::Verdict::NotEquipped: Reject(TEXT("holstered")); return;
    case Rules::Verdict::Equipping: Reject(TEXT("equip")); return;
    case Rules::Verdict::Cooldown: Reject(TEXT("cooldown")); return;
    case Rules::Verdict::Busy: Reject(TEXT("busy")); return;
    }
    const int32 Slot = Actions.Start(Sequence, Attack, In.now);
    if (Slot < 0) { Reject(TEXT("busy")); return; }
    NextAttackTime = Rules::NextAttackTime(In.now, NextAttackTime, Attack);
    GetWorld()->GetTimerManager().SetTimer(ImpactTimers[Slot],
        FTimerDelegate::CreateUObject(this, &UVSKnifeComponent::ResolveImpact, Slot),
        static_cast<float>(Rules::TuningFor(Attack).impactTime), false);
    ++State.AttackCount;
    State.LastAttack = VSKnifeDetail::ToAttack(Attack);
    GetOwner()->ForceNetUpdate();
}

void UVSKnifeComponent::ResolveImpact(int32 Slot)
{
    namespace Rules = VSKnifeRules;
    UWorld* World = GetWorld();
    AVSCharacter* Pawn = Character();
    Rules::Pending Action;
    if (!World || !Pawn || !Actions.Take(Slot, World->GetTimeSeconds(), Action)) { return; }
    // Re-check every lifecycle gate at impact time: a delayed action never hits
    // after death, a switch, a reset or the combat phase closing.
    const UVSWeaponLoadoutComponent* Loadout = Pawn->GetLoadoutComponent();
    if (!HasValidOwner() || !Pawn->GetHealthComponent()->IsAlive() || !AVSMatchGameMode::AllowsCombat(Pawn)
        || !Pawn->CanMatchCombat() || !Loadout || Loadout->GetState().Equipped != EVSWeaponSlot::Knife)
    {
        Reject(TEXT("cancelled"));
        CancelActions();
        return;
    }

    const Rules::Tuning& Values = Rules::TuningFor(Action.kind);
    // Authoritative view at impact time; the client supplied only the kind.
    const FVector Eye = Pawn->GetPawnViewLocation();
    const FVector Aim = Pawn->GetBaseAimRotation().Vector();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(VSKnifeTrace), false, Pawn);

    std::array<Rules::Candidate, VSKnifeDetail::MaxCandidates> Candidates{};
    AVSCharacter* Victims[VSKnifeDetail::MaxCandidates] = {};
    const UPrimitiveComponent* Components[VSKnifeDetail::MaxCandidates] = {};
    float Distances[VSKnifeDetail::MaxCandidates] = {};
    std::size_t Count = 0;
    for (TActorIterator<AVSCharacter> It(World); It && Count < VSKnifeDetail::MaxCandidates; ++It)
    {
        AVSCharacter* Other = *It;
        if (Other == Pawn || !IsValid(Other) || !Other->GetHealthComponent() || !Other->GetHealthComponent()->IsAlive()) { continue; }
        const UCapsuleComponent* Capsule = Other->GetCapsuleComponent();
        const float Radius = Capsule->GetScaledCapsuleRadius();
        const float AxisHalf = FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - Radius);
        // Closest point of the capsule axis at the attacker's eye height.
        FVector Target = Capsule->GetComponentLocation();
        Target.Z = FMath::Clamp(Eye.Z, Target.Z - AxisHalf, Target.Z + AxisHalf);
        const FVector ToTarget = Target - Eye;
        const double Length = ToTarget.Size();
        const FVector Direction = Length > UE_KINDA_SMALL_NUMBER ? ToTarget / Length : Aim;
        const double CosAngle = FVector::DotProduct(Aim, Direction);
        const double Surface = FMath::Max(0.0, Length - Radius);
        if (!Rules::InReachAndCone(Values, Surface, CosAngle)) { continue; }
        // Hit zones block ECC_Visibility, as for firearm traces.
        FHitResult Hit;
        Rules::Probe Sight = Rules::Probe::Missed;
        if (World->LineTraceSingleByChannel(Hit, Eye, Target + Direction * Radius, ECC_Visibility, Query))
        {
            Sight = Hit.GetActor() == Other ? Rules::Probe::Clear
                : Cast<APawn>(Hit.GetActor()) ? Rules::Probe::Other : Rules::Probe::World;
        }
        const double Distance = Sight == Rules::Probe::Clear ? static_cast<double>(Hit.Distance) : Surface;
        Candidates[Count] = {static_cast<uint32>(Count + 1), Distance, CosAngle, Sight};
        Victims[Count] = Other;
        Components[Count] = Hit.GetComponent();
        Distances[Count] = Hit.Distance;
        ++Count;
    }
    // A blade that meets a wall straight ahead sounds like it, even with no target.
    FHitResult Forward;
    const bool bForwardWall = World->LineTraceSingleByChannel(Forward, Eye, Eye + Aim * Values.reachCm, ECC_Visibility, Query)
        && !Cast<APawn>(Forward.GetActor());
    const Rules::Selection Picked = Rules::Select(Values, Candidates, Count, bForwardWall);

    UVSDamageSubsystem* DamageService = UVSDamageSubsystem::Get(this);
    for (int32 Index = 0; Index < Picked.count && DamageService; ++Index)
    {
        const std::size_t Victim = static_cast<std::size_t>(Picked.ids[static_cast<std::size_t>(Index)] - 1);
        // Flat damage by attack kind through the single authoritative damage path
        // (team and match gates, attribution, stats, kill reward).
        FVSWeaponHit Hit;
        Hit.Shooter = Pawn;
        Hit.Victim = Victims[Victim];
        Hit.HitComponent = Components[Victim];
        Hit.Weapon = ValStrike::Damage::WeaponClass::Knife;
        Hit.DistanceCm = Distances[Victim];
        Hit.AttackDamage = static_cast<float>(Values.damage);
        DamageService->ApplyWeaponHit(Hit);
    }
    ++State.ImpactCount;
    State.LastImpact = VSKnifeDetail::ToImpact(Picked.result);
    State.LastImpactAttack = VSKnifeDetail::ToAttack(Action.kind);
    GetOwner()->ForceNetUpdate();
    KnifeEvent.Broadcast(EVSKnifeEvent::Impact, VSKnifeDetail::ToAttack(Action.kind), State.LastImpact);
}

void UVSKnifeComponent::CancelActions()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
    const std::size_t Cancelled = Actions.CancelAll();
    ClearImpactTimers();
    if (Cancelled == 0) { return; }
    ++State.CancelCount;
    GetOwner()->ForceNetUpdate();
    KnifeEvent.Broadcast(EVSKnifeEvent::Cancel, State.LastAttack, EVSKnifeImpact::None);
}

void UVSKnifeComponent::HandleLifeStateChanged()
{
    const AVSCharacter* Pawn = Character();
    if (Pawn && !Pawn->GetHealthComponent()->IsAlive()) { CancelActions(); }
}

void UVSKnifeComponent::OnRep_State()
{
    // First sight (join, respawn, relevancy return) adopts the counters so old
    // attacks are never replayed.
    if (!bSeenState)
    {
        bSeenState = true;
        SeenState = State;
        return;
    }
    const AVSCharacter* Pawn = Character();
    // The owner already started its swing locally (LocalStart).
    if (State.AttackCount != SeenState.AttackCount && !(Pawn && Pawn->IsLocallyControlled()))
    {
        KnifeEvent.Broadcast(EVSKnifeEvent::Start, State.LastAttack, EVSKnifeImpact::None);
    }
    if (State.ImpactCount != SeenState.ImpactCount)
    {
        KnifeEvent.Broadcast(EVSKnifeEvent::Impact, State.LastImpactAttack, State.LastImpact);
    }
    if (State.CancelCount != SeenState.CancelCount)
    {
        KnifeEvent.Broadcast(EVSKnifeEvent::Cancel, State.LastAttack, EVSKnifeImpact::None);
    }
    SeenState = State;
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void UVSKnifeComponent::DevelopmentRequestAttack(uint8 RawKind, uint32 Sequence)
{
    FString Role;
    if (FParse::Value(FCommandLine::Get(), TEXT("VSWeaponSmoke="), Role)
        && (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        if (Sequence == 0 && ++NextLocalSequence == 0) { ++NextLocalSequence; }
        ServerRequestKnifeAttack(RawKind, Sequence == 0 ? NextLocalSequence : Sequence);
    }
}
#endif
