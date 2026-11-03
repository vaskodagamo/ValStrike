#pragma once

#include "CoreMinimal.h"
#include "../VSGameMode.h"
#include "../Rules/VSMatchRules.h"
#include "VSMatchGameMode.generated.h"

class AVSCharacter;
class AVSMatchPlayerController;
class AVSMatchObjective;
class APlayerStart;
class ATriggerBox;
class ATargetPoint;
enum class EVSMatchCommand : uint8;

/** Site01 server adapter. Select explicitly with ?game=/Script/ValStrike.VSMatchGameMode. */
UCLASS()
class VALSTRIKE_API AVSMatchGameMode : public AVSGameMode
{
    GENERATED_BODY()
public:
    AVSMatchGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    static bool AllowsCombat(const AVSCharacter* Pawn);
    static bool AllowsDamage(const AVSCharacter* Pawn);
    static void ReportDeath(AVSCharacter* Pawn);
    void HandleCommand(AVSMatchPlayerController* PC, EVSMatchCommand Command,
        uint32 RequestId, uint64 RoundId, uint64 DeviceRevision);

    const ValStrike::Rules::Snapshot& ReadRules() const { return Rules.Read(); }
    AVSMatchObjective* GetObjectiveActor() const { return Objective; }
    FVector GetSiteCenter() const;
    bool IsMapReady() const { return bMapReady; }
    uint64 GetRejectedCommands() const { return RejectedCommands; }

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Test harness may arrange world geometry/positions, never mutate model winners/scores.
    void DevelopmentSealAt(int64 Time);
    bool DevelopmentScheduleInteraction(AVSMatchPlayerController* PC, int64 At, bool bDefuse);
#endif

private:
    struct FSession
    {
        TWeakObjectPtr<AVSMatchPlayerController> Controller;
        ValStrike::Rules::Team Team = ValStrike::Rules::Team::None;
        uint32 LastClientRequest = 0;
        uint64 DomainRequest = 0;
        bool bHeld = false;
        bool bLeaving = false;
        bool bHasSafeFloor = false;
        FVector SafeFloor = FVector::ZeroVector;
        FVector PlantFloor = FVector::ZeroVector;
        int64 HoldBegan = 0;
        int64 HoldEnded = 0;
        int64 LastCancelAt = -1;
        ValStrike::Rules::InteractionKind IntentKind = ValStrike::Rules::InteractionKind::None;
    };
    struct FPending
    {
        int64 Time = 0;
        ValStrike::Rules::Event Event;
    };

    int64 NowMs() const;
    uint64 ConnectionId(const AController* PC) const;
    AVSCharacter* PawnFor(uint64 Id) const;
    bool LiveGate(const AVSCharacter* Pawn, bool bForCombat);
    bool ValidateMap();
    bool GroundAt(const FVector& Candidate, FVector& Ground) const;
    void RememberSafeFloor(uint64 Id);
    bool NearDevice(const AVSCharacter* Pawn) const;
    bool InSite(const AVSCharacter* Pawn) const;
    bool InteractionEligible(uint64 Id, ValStrike::Rules::InteractionKind Kind) const;
    void AuditEligibility();
    void CancelIntent(uint64 Id, bool bQueueStop);
    void Queue(ValStrike::Rules::Event Event, int64 At);
    void FlushThrough(int64 Through);
    void ApplyFrame(int64 At, const TArray<ValStrike::Rules::Event>& Events);
    void ApplyEffects(const ValStrike::Rules::StepResult& Result);
    void Publish();
    void ResetPawns();
    void ClearPawns();
    void PlaceDevice(const ValStrike::Rules::Effect& Effect);
    ValStrike::Rules::Event MakeEvent(ValStrike::Rules::EventKind Kind, uint64 Id = 0);

    UFUNCTION()
    void OnMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);

    UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "1", ClampMax = "2"))
    int32 MinimumPerTeam = 1;
    UPROPERTY(EditDefaultsOnly, Category = "Match", meta = (ClampMin = "0.1"))
    float ResultDisplaySeconds = 3.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Match")
    TSubclassOf<AVSMatchObjective> ObjectiveClass;
    UPROPERTY(Transient) TObjectPtr<AVSMatchObjective> Objective;
    UPROPERTY(Transient) TObjectPtr<ATriggerBox> Site;
    UPROPERTY(Transient) TObjectPtr<ATargetPoint> Recovery;
    UPROPERTY(Transient) TObjectPtr<AActor> ArenaFloor;
    UPROPERTY(Transient) TArray<TObjectPtr<APlayerStart>> AttackerStarts;
    UPROPERTY(Transient) TArray<TObjectPtr<APlayerStart>> DefenderStarts;

    ValStrike::Rules::MatchRules Rules;
    TMap<uint64, FSession> Sessions;
    TArray<FPending> Pending;
    uint64 NextConnection = 0;
    uint64 Sequence = 0;
    uint64 LastEffect = 0;
    uint64 RejectedCommands = 0;
    int64 NextRoundAt = 0;
    int64 LastAuditAt = 0;
    bool bMapReady = false;
    bool bApplying = false;
    bool bHasFrame = false;
    bool bRematchQueued = false;
    FBox FloorBounds = FBox(ForceInit);
    double FloorHeight = 0;
    FString MapError;
};
