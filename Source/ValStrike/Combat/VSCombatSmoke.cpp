#include "VSCombatSmoke.h"
#include "VSCombatLookSmoke.h"
#include "VSWeaponSmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "VSRifleComponent.h"
#include <limits>
#include "VSHealthComponent.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"

namespace
{
class FVSCombatSmoke
{
public:
    explicit FVSCombatSmoke(FString InRole) : Role(MoveTemp(InRole))
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSCombatSmokeStart="), StartTime);
        FParse::Value(FCommandLine::Get(), TEXT("VSCombatOutput="), Output);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSCombatSmoke::Tick);
    }
    ~FVSCombatSmoke() { FWorldDelegates::OnWorldPostActorTick.Remove(Delegate); }

private:
    void Check(const TCHAR* Name, bool bPassed, FString Detail)
    {
        Detail.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Detail.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"),
            Name, bPassed ? TEXT("true") : TEXT("false"), *Detail));
        UE_LOG(LogValStrike, Display, TEXT("VSCOMBAT %s %s: %s (%s)"), *Role, Name,
            bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    }

    void Finish(const TCHAR* Error = nullptr)
    {
        bFinished = true;
        const FString Report = FString::Printf(
            TEXT("{\"schema_version\":1,\"role\":\"%s\",\"completed\":true,\"checks\":[%s],\"error\":%s}\n"),
            *Role, *FString::Join(Checks, TEXT(",")), Error ? *FString::Printf(TEXT("\"%s\""), Error) : TEXT("null"));
        if (!Output.IsEmpty() && FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp"))))
        {
            IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
        }
    }

    static void Key(APlayerController* PC, FKey Name, bool Wanted, bool& Held)
    {
        if (PC && Held != Wanted)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, Wanted ? IE_Pressed : IE_Released, Wanted ? 1.0f : 0.0f));
            Held = Wanted;
        }
    }

    bool Once(int32 Id, double Time, double At)
    {
        if (Time < At || Steps.Contains(Id)) { return false; }
        Steps.Add(Id);
        return true;
    }

    bool Place(AVSCharacter* Pawn, FVector Location)
    {
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        if (!Pawn->TeleportTo(Location, FRotator::ZeroRotator)) { return false; }
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            PC->SetControlRotation(FRotator::ZeroRotator);
            PC->ClientSetLocation(Pawn->GetActorLocation(), FRotator::ZeroRotator);
            PC->ClientSetRotation(FRotator::ZeroRotator);
        }
        Pawn->ForceNetUpdate();
        return true;
    }

    FString StateDetail(const AVSCharacter* Driver, const AVSCharacter* Target) const
    {
        const FVSPlayerHUDData D = Driver->GetHUDData();
        const FVSPlayerHUDData T = Target->GetHUDData();
        return FString::Printf(TEXT("ammo=%d/%d reserve=%d reload=%d targetHealth=%.1f alive=%d shots=%u"),
            D.MagazineAmmo, D.MagazineCapacity, D.ReserveAmmo, D.bReloading, T.Health, T.bAlive,
            Driver->GetRifleComponent()->GetState().ShotsFired);
    }

    void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        if (bFinished || !World || !World->IsGameWorld() || !World->GetGameState()) { return; }
        const bool bServer = Role == TEXT("server");
        if ((bServer && World->GetNetMode() != NM_DedicatedServer) || (!bServer && World->GetNetMode() != NM_Client)) { return; }
        const double Time = World->GetGameState()->GetServerWorldTimeSeconds() - StartTime;
        if (!bServer && !bNameRequested)
        {
            APlayerController* PC = World->GetFirstPlayerController();
            if (PC && PC->IsLocalController() && PC->PlayerState)
            {
                PC->SetName(Role == TEXT("driver") ? TEXT("VSCombatDriver") : TEXT("VSCombatObserver"));
                bNameRequested = true;
            }
        }
        AVSCharacter* Driver = nullptr;
        AVSCharacter* Target = nullptr;
        for (TActorIterator<AVSCharacter> It(World); It; ++It)
        {
            const APlayerState* PS = It->GetPlayerState();
            if (PS && PS->GetPlayerName() == TEXT("VSCombatDriver")) { Driver = *It; }
            if (PS && PS->GetPlayerName() == TEXT("VSCombatObserver")) { Target = *It; }
        }
        AVSCharacter* Local = Role == TEXT("driver") ? Driver : Target;
        APlayerController* PC = Local ? Cast<APlayerController>(Local->GetController()) : nullptr;
        if (!Driver || !Target || (!bServer && (!PC || !PC->IsLocalController() || !Local->InputComponent)))
        {
            if (Time > 1.0) { Finish(TEXT("players_not_ready")); }
            return;
        }
        UVSRifleComponent* Gun = Driver->GetRifleComponent();
        const auto Snapshot = [&]() { return StateDetail(Driver, Target); };
        const auto Aim = [&](bool bHead)
        {
            // Body shots aim at the chest (upper-torso zone, full weapon damage); head at the head zone.
            return (Target->GetActorLocation() + FVector(0, 0, bHead ? 72 : 40) - Driver->GetPawnViewLocation()).GetSafeNormal();
        };
        if (!bReady)
        {
            bReady = true;
            Check(TEXT("network_role"), Driver->GetLocalRole() == (bServer ? ROLE_Authority : Role == TEXT("driver") ? ROLE_AutonomousProxy : ROLE_SimulatedProxy)
                && Driver->HasAuthority() == bServer, FString::FromInt(static_cast<int32>(Driver->GetLocalRole())));
            if (bServer && (!Place(Driver, FVector(-1400, -1100, 90)) || !Place(Target, FVector(-800, -1100, 90))))
            {
                Finish(TEXT("placement_failed")); return;
            }
        }

        // All ordinary shots/reloads go through PlayerController -> Enhanced Input.
        if (Role == TEXT("driver"))
        {
            PC->SetControlRotation(Aim((Time >= 5.9 && Time < 8.0) || Time >= 41.5).Rotation());
            const bool bFire = (Time >= 1.0 && Time < 1.05) || (Time >= 6.0 && Time < 6.05)
                || (Time >= 12 && Time < 12.05) || (Time >= 14 && Time < 16)
                || (Time >= 17 && Time < 23) || (Time >= 28 && Time < 28.27)
                || (Time >= 32 && Time < 34.3) || (Time >= 42.5 && Time < 42.55);
            Key(PC, EKeys::LeftMouseButton, bFire, bFireHeld);
            Key(PC, EKeys::R, (Time >= 3 && Time < 3.1) || (Time >= 25 && Time < 25.1)
                || (Time >= 29 && Time < 29.1), bReloadHeld);
            if (Once(100, Time, 3.8)) { Gun->DevelopmentRequest(10, Driver->GetPawnViewLocation(), Aim(false)); }
            if (Once(101, Time, 24)) { Gun->DevelopmentRequest(90, Driver->GetPawnViewLocation(), Aim(false)); }
            if (Once(102, Time, 32.5)) { Driver->StopCombatInput(); }
            if (Once(103, Time, 36))
            {
                Gun->DevelopmentRequest(100, Driver->GetPawnViewLocation(), Aim(false));
                Gun->DevelopmentRequest(100, Driver->GetPawnViewLocation(), Aim(false));
                Gun->DevelopmentRequest(99, Driver->GetPawnViewLocation(), Aim(false));
                Gun->DevelopmentRequest(101, Driver->GetPawnViewLocation(), Aim(false));
            }
            if (Once(104, Time, 37))
            {
                Gun->DevelopmentRequest(102, Driver->GetPawnViewLocation() + FVector(500, 0, 0), Aim(false));
                Gun->DevelopmentRequest(103, Driver->GetPawnViewLocation(), -Aim(false));
                Gun->DevelopmentRequest(104, Driver->GetPawnViewLocation(), FVector(0, 0, 0));
                // Raw FVector serialization preserves NaN for server validation.
                Gun->DevelopmentRequest(105, FVector(std::numeric_limits<double>::quiet_NaN(), 0, 0), Aim(false));
            }
            if (Once(105, Time, 38))
            {
                Target->GetRifleComponent()->DevelopmentRequest(100, Target->GetPawnViewLocation(), -Aim(false));
            }
            if (Once(106, Time, 39.5))
            {
                Check(TEXT("foreign_owner_request"), Target->GetRifleComponent()->GetState().ShotsFired == 0
                    && Driver->GetHealthComponent()->GetHealth() == 100, Snapshot());
            }
        }
        if (Role == TEXT("observer"))
        {
            // Keep the yaw Place() gives the target (facing away from the driver), so the
            // driver's body shots hit the chest zone from behind. Side-on, a chest-height
            // shot meets the upper-arm zone (24, not 30) by design.
            PC->SetControlRotation(FRotator::ZeroRotator);
            Key(PC, EKeys::W, Time >= 7.2 && Time < 8.1, bForwardHeld);
            Key(PC, EKeys::LeftMouseButton, (Time >= 7.3 && Time < 7.6) || (Time >= 41 && Time < 41.05), bFireHeld);
            Key(PC, EKeys::R, (Time >= 7.3 && Time < 7.6) || (Time >= 42 && Time < 42.1), bReloadHeld);
            if (Once(107, Time, 7.5))
            {
                Target->GetRifleComponent()->DevelopmentRequest(1, Target->GetPawnViewLocation(), FVector(-1, 0, 0));
            }
        }

        if (Once(0, Time, 0.6))
        {
            const auto D = Driver->GetHUDData();
            Check(TEXT("initial_state"), D.bHasHealth && D.bHasWeapon && D.bAlive && D.Health == 100 && D.MaxHealth == 100
                && D.MagazineAmmo == 30 && D.MagazineCapacity == 30 && D.ReserveAmmo == 90 && !D.bReloading, Snapshot());
        }
        if (Once(1, Time, 2)) { Check(TEXT("body_damage"), Gun->GetState().Magazine == 29 && Target->GetHealthComponent()->GetHealth() == 70, Snapshot()); }
        if (Once(2, Time, 3.5)) { Check(TEXT("reload_started"), Gun->GetState().bReloading && Gun->GetState().Magazine == 29 && Gun->GetState().Reserve == 90, Snapshot()); }
        if (Once(3, Time, 5.7)) { Check(TEXT("reload_completed"), !Gun->GetState().bReloading && Gun->GetState().Magazine == 30 && Gun->GetState().Reserve == 89 && Target->GetHealthComponent()->GetHealth() == 70, Snapshot()); }
        if (Once(4, Time, 7))
        {
            DeathLocation = Target->GetActorLocation();
            Check(TEXT("head_death"), !Target->GetHealthComponent()->IsAlive() && Gun->GetState().Magazine == 29
                && Target->GetCharacterMovement()->MovementMode == MOVE_None, Snapshot());
        }
        if (Once(5, Time, 8.5)) { Check(TEXT("dead_actions_blocked"), !Target->GetHealthComponent()->IsAlive()
            && Target->GetRifleComponent()->GetState().Magazine == 30 && !Target->GetRifleComponent()->GetState().bReloading
            && FVector::Dist(DeathLocation, Target->GetActorLocation()) < 1.0 && Driver->GetHealthComponent()->GetHealth() == 100, Snapshot()); }
        if (bServer && Once(6, Time, 9))
        {
            Driver->GetHealthComponent()->DevelopmentReset();
            Target->GetHealthComponent()->DevelopmentReset();
            Gun->DevelopmentReset();
            Target->GetRifleComponent()->DevelopmentReset();
        }
        if (Once(7, Time, 10)) { Check(TEXT("development_reset"), Target->GetHealthComponent()->GetHealth() == 100
            && Target->GetCharacterMovement()->MovementMode == MOVE_Walking && Gun->GetState().Magazine == 30
            && Gun->GetState().Reserve == 90 && !Gun->GetState().bReloading, Snapshot()); }
        if (bServer && Once(8, Time, 11))
        {
            AStaticMeshActor* Cover = World->SpawnActor<AStaticMeshActor>(FVector(-1100, -1100, 140), FRotator::ZeroRotator);
            if (!Cover) { Finish(TEXT("cover_spawn_failed")); return; }
            Cover->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
            if (!Cover->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))))
            {
                Finish(TEXT("cover_mesh_failed")); return;
            }
            Cover->GetStaticMeshComponent()->SetWorldScale3D(FVector(1, 2, 3));
            Cover->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
            TestCover = Cover;
        }
        if (Once(9, Time, 13)) { Check(TEXT("cover_blocks_damage"), Gun->GetState().Magazine == 29 && Target->GetHealthComponent()->GetHealth() == 100, Snapshot()); }
        if (Once(10, Time, 16.5))
        {
            const uint32 Shots = Gun->GetState().ShotsFired;
            Check(TEXT("automatic_fire_cadence"), Shots >= 15 && Shots <= 18 && Gun->GetState().Magazine == 30 - static_cast<int32>(Shots)
                && Target->GetHealthComponent()->GetHealth() == 100, Snapshot());
        }
        if (Once(11, Time, 23.5)) { Check(TEXT("empty_magazine"), Gun->GetState().Magazine == 0 && Gun->GetState().ShotsFired == 30
            && Gun->GetState().Reserve == 90 && Target->GetHealthComponent()->GetHealth() == 100, Snapshot()); }
        if (Once(12, Time, 28.8)) { BeforePartialReload = Gun->GetState().Magazine; }
        if (Once(13, Time, 31.5)) { Check(TEXT("partial_reload"), BeforePartialReload > 0 && BeforePartialReload < 30
            && Gun->GetState().Magazine == 30 && Gun->GetState().Reserve == 60 - (30 - BeforePartialReload)
            && !Gun->GetState().bReloading, Snapshot()); }
        if (Once(14, Time, 33)) { AfterStopShots = Gun->GetState().ShotsFired; }
        if (Once(15, Time, 34)) { Check(TEXT("stop_combat_input"), AfterStopShots > 30 && Gun->GetState().ShotsFired == AfterStopShots, Snapshot()); }
        if (bServer && Once(16, Time, 35)) { Gun->DevelopmentReset(); }
        if (Once(17, Time, 39.7))
        {
            Check(TEXT("validation_no_mutation"), Gun->GetState().Magazine == 29 && Gun->GetState().Reserve == 90
                && Gun->GetState().ShotsFired == 1 && Target->GetHealthComponent()->GetHealth() == 100
                && Target->GetRifleComponent()->GetState().ShotsFired == 0, Snapshot());
            if (bServer)
            {
                Check(TEXT("reject_duplicate"), Gun->DevelopmentRejections(TEXT("sequence")) >= 2, TEXT("sequence counter includes duplicate and stale RPC"));
                Check(TEXT("reject_stale"), Gun->DevelopmentRejections(TEXT("sequence")) >= 2, TEXT("ordered RPC IDs 100,100,99"));
                Check(TEXT("reject_cadence"), Gun->DevelopmentRejections(TEXT("cadence")) > 0, TEXT("back-to-back IDs 100 and 101"));
                Check(TEXT("reject_origin"), Gun->DevelopmentRejections(TEXT("origin")) > 0, TEXT("500cm forged origin"));
                Check(TEXT("reject_direction"), Gun->DevelopmentRejections(TEXT("direction")) >= 2, TEXT("opposite and zero direction"));
                Check(TEXT("reject_nonfinite"), Gun->DevelopmentRejections(TEXT("nonfinite")) > 0, TEXT("NaN origin"));
                Check(TEXT("reject_reload"), Gun->DevelopmentRejections(TEXT("reload")) > 0, TEXT("shot while reloading"));
                Check(TEXT("reject_dead"), Target->GetRifleComponent()->DevelopmentRejections(TEXT("dead")) > 0, TEXT("dead owner raw request"));
                Check(TEXT("reject_empty"), Gun->DevelopmentRejections(TEXT("empty")) > 0, TEXT("empty magazine raw request"));
            }
        }
        if (bServer && Once(18, Time, 40.5) && TestCover.IsValid()) { TestCover->Destroy(); }
        if (Once(19, Time, 42.3)) { bObservedTargetReload = Target->GetHUDData().bReloading; }
        if (Once(20, Time, 45))
        {
            const auto T = Target->GetHUDData();
            Check(TEXT("reload_interrupted"), bObservedTargetReload && !T.bAlive && !T.bReloading
                && T.MagazineAmmo == 29 && T.ReserveAmmo == 90, StateDetail(Target, Driver));
            Finish();
        }
    }

    FString Role, Output;
    FDelegateHandle Delegate;
    TArray<FString> Checks;
    TSet<int32> Steps;
    float StartTime = 30;
    bool bNameRequested = false, bReady = false, bFinished = false;
    bool bFireHeld = false, bReloadHeld = false, bForwardHeld = false;
    TWeakObjectPtr<AStaticMeshActor> TestCover;
    bool bObservedTargetReload = false;
    FVector DeathLocation = FVector::ZeroVector;
    int32 BeforePartialReload = -1;
    uint32 AfterStopShots = 0;
};
TUniquePtr<FVSCombatSmoke> Smoke;
}

void ValStrikeTests::StartCombatSmoke()
{
    ValStrikeTests::StartCombatLookSmoke();
    ValStrikeTests::StartWeaponSmoke();
    FString Role;
    if (!Smoke && FParse::Value(FCommandLine::Get(), TEXT("VSCombatSmoke="), Role)
        && (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        Smoke = MakeUnique<FVSCombatSmoke>(Role);
    }
}
#else
void ValStrikeTests::StartCombatSmoke() { ValStrikeTests::StartWeaponSmoke(); }
#endif
