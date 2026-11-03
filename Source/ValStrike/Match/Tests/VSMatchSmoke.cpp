#include "VSMatchSmoke.h"
#include "Net/UnrealNetwork.h"

AVSMatchSmokeState::AVSMatchSmokeState()
{
    bReplicates = true; bAlwaysRelevant = true; SetNetUpdateFrequency(30);
}
void AVSMatchSmokeState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSMatchSmokeState, MatchIndex);
    DOREPLIFETIME(AVSMatchSmokeState, Stage);
    DOREPLIFETIME(AVSMatchSmokeState, StageAt);
    DOREPLIFETIME(AVSMatchSmokeState, RoundId);
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "../VSMatchGameMode.h"
#include "../VSMatchGameState.h"
#include "../VSMatchPlayerState.h"
#include "../VSMatchPlayerController.h"
#include "../VSMatchObjective.h"
#include "../../VSCharacter.h"
#include "../../Combat/VSHealthComponent.h"
#include "../../Combat/VSRifleComponent.h"
#include "../../ValStrike.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

namespace
{
namespace R = ValStrike::Rules;
class FVSMatchSmoke
{
public:
    explicit FVSMatchSmoke(FString InRole) : Role(MoveTemp(InRole))
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSMatchOutput="), Output);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSMatchSmoke::Tick);
        PreDelegate = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FVSMatchSmoke::BeforeTick);
    }
    ~FVSMatchSmoke()
    { FWorldDelegates::OnWorldPostActorTick.Remove(Delegate); FWorldDelegates::OnWorldPreActorTick.Remove(PreDelegate); }
private:
    void Check(const TCHAR* Name, bool Passed, const FString& Detail)
    {
        if (Names.Contains(Name)) { return; }
        Names.Add(Name);
        FString Safe = Detail.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));
        Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"), Name, Passed ? TEXT("true") : TEXT("false"), *Safe));
        UE_LOG(LogValStrike, Display, TEXT("VSMATCHCHECK %s %s %s %s"), *Role, Name, Passed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    }
    void Finish(const TCHAR* Error = nullptr)
    {
        Finished = true;
        const FString Report = FString::Printf(TEXT("{\"schema_version\":1,\"role\":\"%s\",\"completed\":true,\"checks\":[%s],\"error\":%s}\n"),
            *Role, *FString::Join(Checks, TEXT(",")), Error ? *FString::Printf(TEXT("\"%s\""), Error) : TEXT("null"));
        if (FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        { IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true); }
    }
    AVSMatchPlayerController* Named(UWorld* World, const FString& Name) const
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            AVSMatchPlayerController* PC = Cast<AVSMatchPlayerController>(It->Get());
            if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Name) { return PC; }
        }
        return nullptr;
    }
    AVSMatchPlayerController* TeamController(UWorld* World, R::Team Team, bool IncludeLate = false) const
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            AVSMatchPlayerController* PC = Cast<AVSMatchPlayerController>(It->Get());
            const AVSMatchPlayerState* PS = PC ? PC->GetPlayerState<AVSMatchPlayerState>() : nullptr;
            if (PS && PS->TeamId == static_cast<uint8>(Team) && (IncludeLate || PS->GetPlayerName() != TEXT("VSMatchLate"))) { return PC; }
        }
        return nullptr;
    }
    bool Place(AVSMatchPlayerController* PC, FVector Where)
    {
        AVSCharacter* Pawn = PC ? Cast<AVSCharacter>(PC->GetPawn()) : nullptr;
        if (!Pawn) { return false; }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Where.Z = 90;
        if (!Pawn->TeleportTo(Where, FRotator::ZeroRotator)) { return false; }
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->ClientSetLocation(Pawn->GetActorLocation(), FRotator::ZeroRotator);
        Pawn->ForceNetUpdate();
        return true;
    }
    void Stage(AVSMatchSmokeState* Director, int32 Value, double Now)
    {
        Director->Stage = Value; Director->StageAt = Now + 0.25; Director->ForceNetUpdate();
    }
    static void Key(AVSMatchPlayerController* PC, FKey KeyName, bool Wanted, bool& Held)
    {
        if (PC && Wanted != Held)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(KeyName, Wanted ? IE_Pressed : IE_Released, Wanted ? 1.0f : 0.0f));
            Held = Wanted;
        }
    }
    void Client(UWorld* World, AVSMatchGameState* GS, AVSMatchSmokeState* Director)
    {
        AVSMatchPlayerController* PC = Cast<AVSMatchPlayerController>(World->GetFirstPlayerController());
        if (!PC || !PC->IsLocalController() || !PC->PlayerState) { return; }
        if (!NameRequested)
        {
            PC->SetName(Role == TEXT("late") ? TEXT("VSMatchLate") : Role == TEXT("driver") ? TEXT("VSMatchDriver") : TEXT("VSMatchObserver"));
            NameRequested = true;
        }
        AVSMatchPlayerState* PS = PC->GetPlayerState<AVSMatchPlayerState>();
        if (!PS || PS->ConnectionId == 0 || PS->TeamId == 0 || !Director || !GS->Match.bReady) { return; }
        const FVSMatchHUDData HUD = PC->GetMatchHUDData();
        const double Time = GS->GetServerWorldTimeSeconds() - Director->StageAt;
        const bool Attacker = PS->TeamId == GS->Match.AttackingTeam;
        const R::Phase Phase = static_cast<R::Phase>(GS->Match.Phase);
        if (LastClientPhase != GS->Match.Phase)
        { LastClientPhase = GS->Match.Phase; ClientPhaseAt = GS->GetServerWorldTimeSeconds(); }
        const double PhaseTime = GS->GetServerWorldTimeSeconds() - ClientPhaseAt;
        const bool Main = Role != TEXT("late");
        AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn());
        if (Main)
        {
            if (Pawn) { Check(TEXT("network_role"), Pawn->GetLocalRole() == ROLE_AutonomousProxy && !Pawn->HasAuthority(), Pawn->GetName()); }
            Check(TEXT("replicated_match"), HUD.bAvailable && PS->ConnectionId != 0 && PS->TeamId != 0,
                FString::Printf(TEXT("team=%d id=%llu round=%llu"), PS->TeamId, PS->ConnectionId, GS->Match.RoundId));
            if (Phase == R::Phase::Preparation && Pawn)
            { Check(TEXT("replicated_preparation"), !Pawn->CanMatchMove() && !Pawn->CanMatchCombat(), Pawn->GetName()); }
            if (Pawn && PS->bParticipating && Pawn->GetName() != LastPawn)
            {
                LastPawn = Pawn->GetName(); ++PawnCount;
                if (PawnCount >= 2)
                { Check(TEXT("fresh_pawn"), Pawn->GetHealthComponent()->GetHealth() == 100 && Pawn->GetRifleComponent()->GetState().Magazine == 30,
                    FString::Printf(TEXT("new=%s count=%d"), *LastPawn, PawnCount)); }
            }
            if (Phase == R::Phase::Planted)
            {
                bool Found = false;
                for (TActorIterator<AVSMatchObjective> It(World); It; ++It)
                { Found |= It->ObjectiveState == static_cast<uint8>(R::DeviceState::Planted) && !It->IsHidden(); }
                if (Found) { Check(TEXT("replicated_objective"), HUD.Objective == EVSMatchHUDObjective::Planted && HUD.bHasPhaseDeadline, TEXT("replicated armed actor and synchronized deadline")); }
            }
            if (Phase == R::Phase::Resolved && (!Pawn || !Pawn->CanMatchCombat()))
            { Check(TEXT("replicated_result"), GS->Match.ScoreOne + GS->Match.ScoreTwo == GS->Match.CompletedRounds && (!Pawn || !Pawn->CanMatchCombat()), TEXT("single score and locked pawn")); }
            if (Phase == R::Phase::MatchComplete)
            { Check(TEXT("replicated_match_complete"), GS->Match.CompletedRounds == 6 && HUD.bCanRequestRematch, TEXT("six rounds and rematch hook")); }
            if (Director->MatchIndex == 1 && Phase == R::Phase::Preparation)
            { Check(TEXT("replicated_rematch"), GS->Match.CompletedRounds == 0 && GS->Match.AttackingTeam == 1 && GS->Match.ScoreOne == 0 && GS->Match.ScoreTwo == 0, TEXT("scores and roles reset")); }
        }
        const bool NewStage = LastStage != Director->Stage || LastRound != Director->RoundId;
        if (NewStage)
        {
            Key(PC, EKeys::E, false, EHeld); Key(PC, EKeys::G, false, GHeld);
            LastStage = Director->Stage; LastRound = Director->RoundId;
            MenuOpened = false; MenuClosed = false;
        }
        bool E = false, G = false;
        if (Time >= 0 && PS->bAlive && PS->bParticipating)
        {
            switch (Director->Stage)
            {
            case 10: E = Main && Time < 0.2; break; // Defender's invalid attempt also traverses the real RPC.
            case 11: E = Main && Attacker; break;
            case 13: E = Main && Attacker; break;
            case 12: E = Main && !Attacker; break;
            case 20: G = Main && Attacker && Time < 0.2; break;
            case 21: E = !Main; break;
            case 23: E = Main && Attacker; break;
            case 24: E = Main && Attacker; break;
            case 60: E = Main && Attacker; break;
            default: break;
            }
        }
        Key(PC, EKeys::E, E, EHeld); Key(PC, EKeys::G, G, GHeld);
        if (Main && Attacker && Director->Stage == 11)
        {
            if (Time > 0.2 && !MenuOpened) { MenuOpened = true; PC->SetSessionMenuOpen(true); }
            if (Time > 0.5 && MenuOpened && !MenuClosed) { MenuClosed = true; PC->SetSessionMenuOpen(false); }
            if (MenuClosed && Time > 0.55 && Time < 0.85)
            { PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, IE_Repeat, 1.0f)); }
        }
        const bool LockedInput = Main && (Phase == R::Phase::Preparation || Phase == R::Phase::Resolved) && PhaseTime > 0.15 && PhaseTime < 0.45;
        Key(PC, EKeys::W, LockedInput, WHeld);
        Key(PC, EKeys::SpaceBar, LockedInput, JumpHeld);
        Key(PC, EKeys::LeftControl, LockedInput, CrouchHeld);
        const bool TimeoutProbe = Main && Attacker && Director->MatchIndex == 0 && Director->Stage == 30 && Phase == R::Phase::Active;
        const bool Fire = LockedInput || (Main && Attacker && Director->MatchIndex == 0 && GS->Match.CompletedRounds == 3 &&
            Director->Stage == 40 && Time >= 0.3 && Time < 1.3 && Phase == R::Phase::Active) || (TimeoutProbe && Time > 0.3 && Time < 0.55);
        Key(PC, EKeys::LeftMouseButton, Fire, FireHeld);
        const double Remaining = GS->Match.PhaseDeadlineSeconds - GS->GetServerWorldTimeSeconds();
        Key(PC, EKeys::R, TimeoutProbe && Remaining < 1.0 && Remaining > 0.2, ReloadHeld);
        if (!Main && Director->Stage == 22 && !LateTravelRequested)
        { LateTravelRequested = true; PC->DisconnectFromServer(); }
        if (Director->Stage == 99 && Director->MatchIndex == 0 && Role == TEXT("driver") && Time > 0.8 && !RematchRequested)
        { PC->RequestMatchRematch(); RematchRequested = true; }
        if (Main && Director->Stage == 100 && Phase == R::Phase::MatchComplete) { Finish(); }
    }

    void Server(UWorld* World, AVSMatchGameMode* Mode, AVSMatchSmokeState* Director)
    {
        const double Now = World->GetTimeSeconds();
        const R::Snapshot& S = Mode->ReadRules();
        AVSMatchPlayerController* Driver = Named(World, TEXT("VSMatchDriver"));
        AVSMatchPlayerController* Observer = Named(World, TEXT("VSMatchObserver"));
        if (!Driver || !Observer || !Mode->IsMapReady()) { return; }
        AVSMatchPlayerController* Attacker = TeamController(World, S.AttackingTeam);
        AVSMatchPlayerController* Defender = TeamController(World, S.DefendingTeam());
        AVSMatchPlayerController* Late = Named(World, TEXT("VSMatchLate"));
        Check(TEXT("map_contract"), Mode->IsMapReady() && Mode->GetObjectiveActor() != nullptr, TEXT("native tags, TriggerBox, recovery floor and capsule clearance"));
        if (!JoinRequested)
        { UE_LOG(LogValStrike, Display, TEXT("VSMATCH_REQUEST_LATE_JOIN")); JoinRequested = true; }
        if (Late && !LateSeen)
        {
            const AVSMatchPlayerState* PS = Late->GetPlayerState<AVSMatchPlayerState>();
            if (PS && S.FindPlayer(PS->ConnectionId))
            {
                LateSeen = true;
                Check(TEXT("late_join_spectator"), !PS->bParticipating && Cast<AVSCharacter>(Late->GetPawn()) == nullptr,
                    FString::Printf(TEXT("id=%llu phase=%d"), PS->ConnectionId, static_cast<int32>(S.CurrentPhase)));
            }
        }
        if (S.CurrentPhase == R::Phase::Preparation && S.RoundId != ServerRound)
        {
            if (FirstMatchComplete && S.CompletedRounds == 0 && Director->MatchIndex == 0)
            {
                Director->MatchIndex = 1;
                Check(TEXT("rematch"), S.Scores[0] == 0 && S.Scores[1] == 0 && S.AttackingTeam == R::Team::One, TEXT("real client rematch RPC"));
            }
            ServerRound = S.RoundId; ++RoundCount;
            Director->RoundId = S.RoundId; Stage(Director, 0, Now);
            bool Reset = true, Spawn = true, Gates = true;
            ProbePositions.Reset(); ProbeAmmo.Reset();
            for (AVSMatchPlayerController* PC : {Driver, Observer})
            {
                AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn());
                Reset &= Pawn && !PawnNames.Contains(Pawn->GetName()) && Pawn->GetHealthComponent()->GetHealth() == 100 &&
                    Pawn->GetRifleComponent()->GetState().Magazine == 30 && Pawn->GetRifleComponent()->GetState().Reserve == 90;
                if (Pawn)
                {
                    PawnNames.Add(Pawn->GetName());
                    ProbePositions.Add(PC->GetPlayerState<AVSMatchPlayerState>()->ConnectionId, Pawn->GetActorLocation());
                    ProbeAmmo.Add(PC->GetPlayerState<AVSMatchPlayerState>()->ConnectionId, Pawn->GetRifleComponent()->GetState().Magazine);
                    const bool IsAttacker = PC->GetPlayerState<AVSMatchPlayerState>()->TeamId == static_cast<uint8>(S.AttackingTeam);
                    bool NearStart = false;
                    for (TActorIterator<APlayerStart> It(World); It; ++It)
                    {
                        if (It->ActorHasTag(IsAttacker ? TEXT("VS_AttackerSpawn") : TEXT("VS_DefenderSpawn")))
                        { NearStart |= FVector::Dist(Pawn->GetActorLocation(), It->GetActorLocation()) < 50; }
                    }
                    Spawn &= NearStart;
                    const float Before = Pawn->GetHealthComponent()->GetHealth();
                    Gates &= !Pawn->CanMatchMove() && !AVSMatchGameMode::AllowsCombat(Pawn) &&
                        Pawn->GetHealthComponent()->ApplyPointDamage(20) == 0 && Pawn->GetHealthComponent()->GetHealth() == Before;
                }
            }
            AllResets &= Reset; AllSpawns &= Spawn;
            Check(TEXT("preparation_gates"), Gates, TEXT("actual damage entry returns zero; movement and rifle gate closed"));
            Check(TEXT("role_spawns"), Spawn, FString::Printf(TEXT("round=%d attackTeam=%d"), RoundCount, static_cast<int32>(S.AttackingTeam)));
            if (S.CompletedRounds == 3) { Check(TEXT("role_swap"), S.AttackingTeam == R::Team::Two && Spawn, TEXT("persistent teams use opposite role starts after three awards")); }
        }
        const bool PrepInputCheck = S.CurrentPhase == R::Phase::Preparation && Now - Director->StageAt > 1.0;
        const bool ResolvedInputCheck = S.CurrentPhase == R::Phase::Resolved && LastResolved == S.RoundId && Now - ResolvedAt > 0.55;
        if (PrepInputCheck || ResolvedInputCheck)
        {
            bool Locked = true;
            for (AVSMatchPlayerController* PC : {Driver, Observer})
            {
                AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn());
                const uint64 Id = PC->GetPlayerState<AVSMatchPlayerState>()->ConnectionId;
                Locked &= Pawn && ProbePositions.Contains(Id) && ProbeAmmo.Contains(Id) &&
                    Pawn->GetActorLocation().Equals(ProbePositions.FindRef(Id), 2.0) && !Pawn->bIsCrouched &&
                    Pawn->GetRifleComponent()->GetState().Magazine == ProbeAmmo.FindRef(Id) &&
                    Pawn->GetCharacterMovement()->Velocity.IsNearlyZero();
            }
            Check(PrepInputCheck ? TEXT("preparation_input_lock") : TEXT("resolved_input_lock"), Locked,
                TEXT("both clients sent W/jump/crouch/fire; server position, velocity and ammunition stayed fixed"));
        }
        if (S.CurrentPhase == R::Phase::Active && Director->Stage == 0 && (RoundCount != 1 || LateSeen))
        {
            const FVector Center = Mode->GetSiteCenter();
            Place(Attacker, Center + FVector(-100,0,0));
            Place(Defender, Center + FVector(600,0,0));
            if (Late && Late->GetPawn()) { Place(Late, Center + FVector(100,0,0)); }
            const int32 StageId = Director->MatchIndex == 1 ? (S.CompletedRounds == 0 ? 60 : 40) :
                S.CompletedRounds == 0 ? 10 : S.CompletedRounds == 1 ? 20 : S.CompletedRounds == 2 ? 30 : S.CompletedRounds == 5 ? 50 : 40;
            if (Director->MatchIndex == 0 && S.CompletedRounds == 3)
            {
                Place(Defender, Center + FVector(100,0,0));
                AVSCharacter* Shooter = Attacker ? Cast<AVSCharacter>(Attacker->GetPawn()) : nullptr;
                AVSCharacter* Target = Defender ? Cast<AVSCharacter>(Defender->GetPawn()) : nullptr;
                if (Shooter && Target)
                {
                    const FRotator Aim = (Target->GetActorLocation() + FVector(0,0,72) - Shooter->GetPawnViewLocation()).Rotation();
                    Attacker->SetControlRotation(Aim); Attacker->ClientSetRotation(Aim);
                }
            }
            if (StageId == 30 && Attacker)
            { Attacker->SetControlRotation(FRotator(0,180,0)); Attacker->ClientSetRotation(FRotator(0,180,0)); }
            Stage(Director, StageId, Now);
        }
        const double Time = Now - Director->StageAt;
        if (Director->Stage == 60 && S.CurrentPhase == R::Phase::Planted)
        {
            // Clear the planter's live hit regions from the defender's device sightline.
            Place(Attacker, Mode->GetSiteCenter() + FVector(600,0,0));
            Place(Defender, Mode->GetObjectiveActor()->GetActorLocation() + FVector(100,0,0));
            Stage(Director, 61, Now);
        }
        if (Director->Stage == 50 && S.CurrentInteraction.Kind == R::InteractionKind::Plant)
        { PlantTieObserved |= S.CurrentInteraction.CompletesAt == S.PhaseDeadline; }
        if (Director->Stage == 61 && S.CurrentInteraction.Kind == R::InteractionKind::Defuse)
        { DefuseTieObserved |= S.CurrentInteraction.CompletesAt == S.PhaseDeadline; }
        if (Director->Stage == 10 && S.CurrentInteraction.Kind == R::InteractionKind::Plant) { InterruptedToken = S.CurrentInteraction.Token; }
        if (Director->Stage == 10 && Time > 0.6)
        {
            Check(TEXT("plant_interrupted"), InterruptedToken != 0 && S.CurrentPhase == R::Phase::Active && S.CurrentInteraction.Kind == R::InteractionKind::None && S.Objective.State == R::DeviceState::Carried,
                TEXT("real E press/release shorter than plant duration resets progress"));
            Check(TEXT("invalid_objective_request"), Mode->GetRejectedCommands() > 0, TEXT("defender plant intent rejected by server"));
            Stage(Director, 11, Now);
        }
        if (Director->Stage == 11 && S.CurrentInteraction.Kind == R::InteractionKind::Plant) { MenuToken = S.CurrentInteraction.Token; }
        if (Director->Stage == 11 && Time > 0.9)
        {
            Check(TEXT("menu_cancels_interaction"), MenuToken != 0 && MenuToken != InterruptedToken &&
                S.CurrentPhase == R::Phase::Active && S.CurrentInteraction.Kind == R::InteractionKind::None,
                TEXT("menu opened while E held; repeated E events after focus return did not resume progress"));
            Stage(Director, 13, Now);
        }
        if (Director->Stage == 13 && S.CurrentPhase == R::Phase::Planted)
        {
            Check(TEXT("plant_completed"), S.Objective.State == R::DeviceState::Planted && S.Objective.Carrier == 0 &&
                Mode->GetObjectiveActor()->GetActorLocation().Equals(Mode->GetSiteCenter(), 220), TEXT("real held E; actor placed on validated site floor"));
            if (AVSCharacter* Pawn = Attacker ? Cast<AVSCharacter>(Attacker->GetPawn()) : nullptr)
            { Pawn->GetHealthComponent()->ApplyPointDamage(1000); }
            Place(Defender, Mode->GetObjectiveActor()->GetActorLocation() + FVector(100,0,0));
            Stage(Director, 12, Now);
        }
        if (Director->Stage == 12 && S.CurrentPhase == R::Phase::Planted && S.LivingCount(S.AttackingTeam) == 0)
        { Check(TEXT("postplant_attacker_death"), S.CompletedRounds == 0, TEXT("device remains live with zero living attackers")); }

        if (Director->Stage == 20 && S.Objective.State == R::DeviceState::Dropped)
        {
            Place(Late, Mode->GetObjectiveActor()->GetActorLocation() + FVector(80,0,0));
            Stage(Director, 21, Now);
        }
        if (Director->Stage == 21 && Late && S.Objective.Carrier == Late->GetPlayerState<AVSMatchPlayerState>()->ConnectionId)
        {
            Stage(Director, 22, Now);
        }
        if (Director->Stage == 22 && !Late && S.Objective.State == R::DeviceState::Dropped)
        {
            UE_LOG(LogValStrike, Display, TEXT("VSMATCH_REQUEST_LATE_LEAVE")); LeaveRequested = true;
            Check(TEXT("disconnect_drop"), S.CurrentPhase == R::Phase::Active && S.LivingCount(S.AttackingTeam) == 1,
                TEXT("real auxiliary connection closed while carrying; grounded drop acknowledged"));
            Place(Attacker, Mode->GetObjectiveActor()->GetActorLocation() + FVector(80,0,0));
            Stage(Director, 23, Now);
        }
        if (Director->Stage == 23 && Attacker && S.Objective.Carrier == Attacker->GetPlayerState<AVSMatchPlayerState>()->ConnectionId)
        {
            Place(Attacker, Mode->GetSiteCenter() + FVector(-100,0,0));
            Stage(Director, 24, Now);
        }
        if (Director->Stage == 40 && S.CurrentPhase == R::Phase::Active && Time > 0.7 &&
            (Director->MatchIndex == 1 || S.CompletedRounds != 3))
        {
            if (Director->MatchIndex == 1)
            {
                AVSMatchPlayerController* Victim = TeamController(World, R::Team::Two);
                if (AVSCharacter* Pawn = Victim ? Cast<AVSCharacter>(Victim->GetPawn()) : nullptr) { Pawn->GetHealthComponent()->ApplyPointDamage(1000); }
            }
            else
            {
                // Round four: attackers win; round five: simultaneous wipe; round six: defenders win.
                AVSMatchPlayerController* Victim = S.CompletedRounds == 3 ? Defender : Attacker;
                if (AVSCharacter* Pawn = Victim ? Cast<AVSCharacter>(Victim->GetPawn()) : nullptr) { Pawn->GetHealthComponent()->ApplyPointDamage(1000); }
                if (S.CompletedRounds == 4)
                { if (AVSCharacter* Pawn = Defender ? Cast<AVSCharacter>(Defender->GetPawn()) : nullptr) { Pawn->GetHealthComponent()->ApplyPointDamage(1000); } }
            }
            Stage(Director, 41, Now);
        }
        if ((S.CurrentPhase == R::Phase::Resolved || S.CurrentPhase == R::Phase::MatchComplete) && LastResolved != S.RoundId)
        {
            LastResolved = S.RoundId; ++Awards;
            ResolvedAt = Now; ProbePositions.Reset(); ProbeAmmo.Reset();
            for (AVSMatchPlayerController* PC : {Driver, Observer})
            {
                if (AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn()))
                {
                    const uint64 Id = PC->GetPlayerState<AVSMatchPlayerState>()->ConnectionId;
                    ProbePositions.Add(Id, Pawn->GetActorLocation());
                    ProbeAmmo.Add(Id, Pawn->GetRifleComponent()->GetState().Magazine);
                }
            }
            if (Director->MatchIndex == 0)
            {
                if (S.CompletedRounds == 1) { Check(TEXT("defuse_win"), S.Reason == R::WinReason::Defuse && S.RoundWinner == S.DefendingTeam(), TEXT("defender E hold after attacker death")); }
                if (S.CompletedRounds == 2) { Check(TEXT("detonation_win"), S.Reason == R::WinReason::Detonation, TEXT("planted actor clock expired")); }
                if (S.CompletedRounds == 3)
                {
                    Check(TEXT("timeout_win"), S.Reason == R::WinReason::Timeout, TEXT("unplanted round clock expired"));
                    AVSCharacter* Pawn = Attacker ? Cast<AVSCharacter>(Attacker->GetPawn()) : nullptr;
                    const float HP = Pawn ? Pawn->GetHealthComponent()->GetHealth() : 0;
                    const bool Blocked = Pawn && !AVSMatchGameMode::AllowsCombat(Pawn) && Pawn->GetHealthComponent()->ApplyPointDamage(100) == 0 && Pawn->GetHealthComponent()->GetHealth() == HP;
                    Check(TEXT("resolved_gate"), Blocked, TEXT("resolved phase rejects actual damage and combat"));
                }
                if (S.CompletedRounds == 4)
                {
                    AVSCharacter* Shooter = Attacker ? Cast<AVSCharacter>(Attacker->GetPawn()) : nullptr;
                    Check(TEXT("elimination_win"), S.Reason == R::WinReason::Elimination && S.RoundWinner == S.AttackingTeam &&
                        Shooter && Shooter->GetRifleComponent()->GetState().ShotsFired > 0, TEXT("real client fire RPC and authoritative rifle hit resolve attackers"));
                }
                if (S.CompletedRounds == 5) { Check(TEXT("simultaneous_elimination"), S.Reason == R::WinReason::Elimination && S.RoundWinner == S.DefendingTeam(), TEXT("two actual health deaths share sealed timestamp")); }
                if (S.CompletedRounds == 6) { Check(TEXT("plant_deadline_tie"), PlantTieObserved && S.Reason == R::WinReason::Timeout && S.Objective.State != R::DeviceState::Planted,
                    TEXT("server-scheduled validated plant completed exactly at hard deadline; timeout won")); }
            }
            else if (S.CompletedRounds == 1)
            { Check(TEXT("defuse_deadline_tie"), DefuseTieObserved && S.Reason == R::WinReason::Detonation,
                TEXT("server-scheduled validated defuse completed exactly at detonation; detonation won")); }
        }
        if (S.CurrentPhase == R::Phase::MatchComplete)
        {
            if (Director->MatchIndex == 0 && !FirstMatchComplete)
            {
                FirstMatchComplete = true;
                Check(TEXT("six_round_match"), S.CompletedRounds == 6 && S.Scores[0] == 3 && S.Scores[1] == 3 && S.Result == R::MatchResult::Draw,
                    FString::Printf(TEXT("score=%d:%d"), S.Scores[0], S.Scores[1]));
                Stage(Director, 99, Now);
            }
            else if (Director->MatchIndex == 1)
            {
                Check(TEXT("fresh_pawn_reset"), AllResets && PawnNames.Num() == 24 && AllSpawns, FString::Printf(TEXT("unique primary pawns=%d"), PawnNames.Num()));
                Check(TEXT("twelve_rounds"), RoundCount == 12 && Awards == 12 && S.Scores[0] == 6 && S.Scores[1] == 0 && LeaveRequested,
                    FString::Printf(TEXT("preparations=%d awards=%d final=%d:%d"), RoundCount, Awards, S.Scores[0], S.Scores[1]));
                if (Director->Stage != 100) { Stage(Director, 100, Now); }
                if (Now - Director->StageAt > 1.0) { Finish(); }
            }
        }
    }
    void BeforeTick(UWorld* World, ELevelTick Type, float Delta)
    {
        if (Finished || Role != TEXT("server") || !World || World->GetNetMode() != NM_DedicatedServer) { return; }
        AVSMatchGameMode* Mode = World->GetAuthGameMode<AVSMatchGameMode>();
        if (!Mode) { return; }
        const int64 Now = static_cast<int64>(FMath::FloorToDouble(World->GetTimeSeconds() * 1000.0));
        const int64 Deadline = Mode->ReadRules().PhaseDeadline;
        AVSMatchSmokeState* Director = nullptr;
        TActorIterator<AVSMatchSmokeState> DirectorIt(World);
        if (DirectorIt) { Director = *DirectorIt; }
        if (Director && Director->Stage == 50 && !PlantTieScheduled && Now >= Deadline - 600)
        { PlantTieScheduled = Mode->DevelopmentScheduleInteraction(TeamController(World, Mode->ReadRules().AttackingTeam), Deadline - 600, false); }
        if (Director && Director->Stage == 61 && !DefuseTieScheduled && Now >= Deadline - 700)
        { DefuseTieScheduled = Mode->DevelopmentScheduleInteraction(TeamController(World, Mode->ReadRules().DefendingTeam()), Deadline - 700, true); }
        if (Mode->ReadRules().CurrentPhase != R::Phase::Active || Mode->ReadRules().CompletedRounds != 2) { return; }
        if (Now < Deadline || Names.Contains(TEXT("deadline_gate"))) { return; }
        AVSMatchPlayerController* PC = TeamController(World, Mode->ReadRules().AttackingTeam);
        AVSCharacter* Pawn = PC ? Cast<AVSCharacter>(PC->GetPawn()) : nullptr;
        if (!Pawn) { return; }
        UVSRifleComponent* Gun = Pawn->GetRifleComponent();
        const FVSRifleState Before = Gun->GetState();
        const uint32 BeforeReloadRejections = Gun->DevelopmentRejections(TEXT("phase"));
        Gun->DevelopmentFinishReload();
        Gun->DevelopmentRequestReload();
        Check(TEXT("reload_deadline_gate"), Before.bReloading && Before.Magazine < Gun->GetMagazineCapacity() &&
            !Gun->GetState().bReloading && Gun->GetState().Magazine == Before.Magazine && Gun->GetState().Reserve == Before.Reserve &&
            Gun->DevelopmentRejections(TEXT("phase")) >= BeforeReloadRejections + 2,
            FString::Printf(TEXT("real client reload pending; expired completion/start rejected before ammo transfer; now=%lld deadline=%lld magazine=%d reserve=%d"),
                Now, Deadline, Before.Magazine, Before.Reserve));
        const uint32 Rejections = Gun->DevelopmentRejections(TEXT("phase"));
        const float HP = Pawn->GetHealthComponent()->GetHealth();
        Gun->DevelopmentRequest(Gun->DevelopmentLastRequest() + 1, Pawn->GetPawnViewLocation(), Pawn->GetBaseAimRotation().Vector());
        const bool Blocked = Gun->GetState().Magazine == Before.Magazine && Gun->GetState().ShotsFired == Before.ShotsFired &&
            Gun->DevelopmentRejections(TEXT("phase")) > Rejections && Pawn->GetHealthComponent()->ApplyPointDamage(100) == 0 &&
            Pawn->GetHealthComponent()->GetHealth() == HP;
        Check(TEXT("deadline_gate"), Blocked, FString::Printf(TEXT("model Active before pre-actor shot; now=%lld deadline=%lld; ammo=%d shots=%u"),
            Now, Deadline, Gun->GetState().Magazine, Gun->GetState().ShotsFired));
    }
    void Tick(UWorld* World, ELevelTick Type, float Delta)
    {
        if (Finished || !World || !World->IsGameWorld()) { return; }
        const bool ServerRole = Role == TEXT("server");
        if ((ServerRole && World->GetNetMode() != NM_DedicatedServer) || (!ServerRole && World->GetNetMode() != NM_Client)) { return; }
        AVSMatchGameState* GS = World->GetGameState<AVSMatchGameState>();
        if (!GS) { return; }
        AVSMatchSmokeState* Director = nullptr;
        TActorIterator<AVSMatchSmokeState> DirectorIt(World);
        if (DirectorIt) { Director = *DirectorIt; }
        if (ServerRole && !Director) { Director = World->SpawnActor<AVSMatchSmokeState>(); }
        if (ServerRole)
        {
            if (AVSMatchGameMode* Mode = World->GetAuthGameMode<AVSMatchGameMode>()) { Server(World, Mode, Director); }
        }
        else { Client(World, GS, Director); }
        if (World->GetTimeSeconds() > 220 && Role != TEXT("late")) { Finish(TEXT("native_scenario_timeout")); }
    }
    FString Role, Output, LastPawn;
    TArray<FString> Checks;
    TSet<FString> Names, PawnNames;
    TMap<uint64, FVector> ProbePositions;
    TMap<uint64, int32> ProbeAmmo;
    FDelegateHandle Delegate, PreDelegate;
    bool Finished = false, NameRequested = false, EHeld = false, GHeld = false, FireHeld = false, RematchRequested = false;
    bool JoinRequested = false, LeaveRequested = false, LateSeen = false, FirstMatchComplete = false;
    bool LateTravelRequested = false;
    bool MenuOpened = false, MenuClosed = false;
    bool WHeld = false, JumpHeld = false, CrouchHeld = false, ReloadHeld = false;
    int32 LastClientPhase = -1;
    double ClientPhaseAt = 0, ResolvedAt = 0;
    bool PlantTieScheduled = false, DefuseTieScheduled = false, PlantTieObserved = false, DefuseTieObserved = false;
    bool AllResets = true, AllSpawns = true;
    int32 LastStage = -1, PawnCount = 0, RoundCount = 0, Awards = 0;
    uint64 LastRound = 0, ServerRound = 0, LastResolved = 0, InterruptedToken = 0, MenuToken = 0;
};
TUniquePtr<FVSMatchSmoke> MatchSmoke;
}
namespace ValStrikeTests
{
void StartMatchSmoke()
{
    FString Role;
    if (!MatchSmoke && FParse::Value(FCommandLine::Get(), TEXT("VSMatchSmoke="), Role) &&
        (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer") || Role == TEXT("late")))
    { MatchSmoke = MakeUnique<FVSMatchSmoke>(Role); }
}
}
#else
namespace ValStrikeTests { void StartMatchSmoke() {} }
#endif
