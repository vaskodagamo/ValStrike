#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSKnifeRules.h"
#include "VSKnifeComponent.generated.h"

class AVSCharacter;

/** Knife attack kinds; values are the request bytes of VSKnifeRules::Attack. */
UENUM()
enum class EVSKnifeAttack : uint8
{
    Slash = 0, // Primary (LMB).
    Stab = 1,  // Secondary (RMB).
};

/** Cosmetic impact result; values match VSKnifeRules::ImpactResult. */
UENUM()
enum class EVSKnifeImpact : uint8
{
    None = 0,
    Miss = 1,
    Wall = 2,
    Player = 3,
};

/**
 * Replicated cosmetic knife state. Counters only grow during the pawn's life;
 * clients turn their changes into events. Nothing here can apply damage.
 */
USTRUCT()
struct FVSKnifeState
{
    GENERATED_BODY()
    // Server-accepted attacks and the latest kind.
    UPROPERTY() uint32 AttackCount = 0;
    UPROPERTY() EVSKnifeAttack LastAttack = EVSKnifeAttack::Slash;
    // Resolved impacts, the latest result and the attack it belongs to.
    UPROPERTY() uint32 ImpactCount = 0;
    UPROPERTY() EVSKnifeImpact LastImpact = EVSKnifeImpact::None;
    UPROPERTY() EVSKnifeAttack LastImpactAttack = EVSKnifeAttack::Slash;
    // Lifecycle transitions that cancelled at least one pending impact.
    UPROPERTY() uint32 CancelCount = 0;
};

/** Cosmetic knife events for presentation and audio. */
enum class EVSKnifeEvent : uint8
{
    LocalStart, // Owning client: a request was sent (prediction; may still be rejected).
    Start,      // Remote pawn: the server accepted an attack.
    Impact,     // The server resolved an impact (all machines).
    Cancel,     // Pending impacts were cancelled (all machines).
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FVSKnifeEventSignature, EVSKnifeEvent, EVSKnifeAttack, EVSKnifeImpact);

/**
 * Server-authoritative knife (KNIFE-01). The owning client requests a slash or
 * stab with a sequence number; the server validates owner, life, combat gate,
 * equipped and ready knife, cooldown, sequence and the pending bound. At the
 * impact time it reads the authoritative view, selects victims by reach, cone
 * and an obstruction trace (VSKnifeRules::Select) and applies flat Knife damage
 * through UVSDamageSubsystem. A switch away, death, end of play, development
 * reset or a closed combat phase cancels pending impacts.
 *
 * Phase B only creates the component on AVSCharacter and routes LMB/RMB to
 * RequestAttack while UVSWeaponLoadoutComponent::IsKnifeEquipped().
 */
UCLASS(ClassGroup = (Combat))
class VALSTRIKE_API UVSKnifeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UVSKnifeComponent();

    // Owning client input. Sends a request only when it could be accepted.
    void RequestAttack(EVSKnifeAttack Kind);
    // Server: drops every pending impact. Other machines ignore it.
    void CancelActions();
    const FVSKnifeState& GetState() const { return State; }
    FVSKnifeEventSignature& OnKnifeEvent() { return KnifeEvent; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Raw RPC with an arbitrary kind byte, bypassing local checks (weapon smoke
    // only). Sequence 0 takes the next local sequence; any other value is sent as is.
    void DevelopmentRequestAttack(uint8 RawKind, uint32 Sequence = 0);
    uint32 DevelopmentLastSequence() const { return NextLocalSequence; }
    uint32 DevelopmentRejections(const FName Reason) const { return Rejections.FindRef(Reason); }
    uint32 DevelopmentPendingCount() const { return static_cast<uint32>(Actions.Count()); }
#endif

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    AVSCharacter* Character() const;
    bool HasValidOwner() const;
    void Reject(FName Reason);
    void ResolveImpact(int32 Slot);
    void HandleLifeStateChanged();
    void ClearImpactTimers();

    UFUNCTION(Server, Reliable)
    void ServerRequestKnifeAttack(uint8 Kind, uint32 Sequence);
    UFUNCTION()
    void OnRep_State();

    UPROPERTY(ReplicatedUsing = OnRep_State)
    FVSKnifeState State;

    FVSKnifeEventSignature KnifeEvent;
    VSKnifeRules::Actions Actions;                       // Server.
    FTimerHandle ImpactTimers[VSKnifeRules::MaxPending]; // Server, by action slot.
    double NextAttackTime = 0.0;                         // Server clock.
    uint32 LastRequestId = 0;                            // Server.
    uint32 NextLocalSequence = 0;                        // Owning client.
    double NextLocalAttackTime = 0.0;                    // Owning client clock.
    FVSKnifeState SeenState;                             // Clients: last state turned into events.
    bool bSeenState = false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TMap<FName, uint32> Rejections;
#endif
};
