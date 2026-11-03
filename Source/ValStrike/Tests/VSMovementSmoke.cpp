#include "VSMovementSmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"

namespace
{
class FVSMovementSmoke
{
public:
    explicit FVSMovementSmoke(FString InRole) : Role(MoveTemp(InRole))
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSMovementSmokeStart="), StartTime);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSMovementSmoke::Tick);
    }

    ~FVSMovementSmoke()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(Delegate);
        ReleaseKeys();
    }

private:
    void Event(const TCHAR* EventName, double Time, const TCHAR* Detail = TEXT("")) const
    {
        // All string fields here are fixed harness values, never user-provided JSON.
        UE_LOG(LogValStrike, Display, TEXT("VSMOVE_JSON {\"event\":\"%s\",\"role\":\"%s\",\"t\":%.3f,\"detail\":\"%s\"}"),
            EventName, *Role, Time, Detail);
    }

    void Fail(double Time, const TCHAR* Reason)
    {
        Event(TEXT("error"), Time, Reason);
        bFinished = true;
        ReleaseKeys();
    }

    static void Key(APlayerController* PC, FKey KeyName, bool Wanted, bool& Held)
    {
        if (PC && Wanted != Held)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(
                KeyName, Wanted ? IE_Pressed : IE_Released, Wanted ? 1.0f : 0.0f));
            Held = Wanted;
        }
    }

    void ReleaseKeys()
    {
        Key(LocalController.Get(), EKeys::W, false, bForward);
        Key(LocalController.Get(), EKeys::LeftShift, false, bWalk);
        Key(LocalController.Get(), EKeys::LeftControl, false, bCrouch);
        Key(LocalController.Get(), EKeys::SpaceBar, false, bJump);
    }

    bool Place(AVSCharacter* Character, const FVector& Location)
    {
        check(Character->HasAuthority());
        Character->GetCharacterMovement()->StopMovementImmediately();
        if (!Character->TeleportTo(Location, FRotator::ZeroRotator))
        {
            return false;
        }
        if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
        {
            PC->SetControlRotation(FRotator::ZeroRotator);
            PC->ClientSetLocation(Character->GetActorLocation(), FRotator::ZeroRotator);
            PC->ClientSetRotation(FRotator::ZeroRotator);
        }
        Character->ForceNetUpdate();
        return true;
    }

    void Sample(AVSCharacter* Driver, double Time) const
    {
        const FVector Position = Driver->GetActorLocation();
        const FVector Velocity = Driver->GetVelocity();
        FBox VisualBounds(ForceInit);
        TInlineComponentArray<UStaticMeshComponent*> Visuals(Driver);
        for (const UStaticMeshComponent* Visual : Visuals)
        {
            if (Visual->GetFName() == TEXT("BodyVisual") || Visual->GetFName() == TEXT("HeadVisual"))
            {
                VisualBounds += Visual->CalcBounds(Visual->GetComponentTransform()).GetBox();
            }
        }
        const UCameraComponent* Camera = Driver->FindComponentByClass<UCameraComponent>();
        const double EyeZ = Camera ? Camera->GetComponentLocation().Z : Position.Z;
        UE_LOG(LogValStrike, Display, TEXT("VSMOVE_JSON {\"event\":\"sample\",\"role\":\"%s\",\"t\":%.3f,\"player\":\"VSDriver\",\"player_id\":%d,\"authority\":%s,\"net_role\":%d,\"x\":%.3f,\"y\":%.3f,\"z\":%.3f,\"speed\":%.3f,\"vz\":%.3f,\"grounded\":%s,\"crouched\":%s,\"half_height\":%.3f,\"eye_z\":%.3f,\"body_min_z\":%.3f,\"body_max_z\":%.3f}"),
            *Role, Time, Driver->GetPlayerState()->GetPlayerId(), Driver->HasAuthority() ? TEXT("true") : TEXT("false"),
            static_cast<int32>(Driver->GetLocalRole()), Position.X, Position.Y, Position.Z,
            Velocity.Size2D(), Velocity.Z, Driver->GetCharacterMovement()->IsMovingOnGround() ? TEXT("true") : TEXT("false"),
            Driver->IsCrouched() ? TEXT("true") : TEXT("false"), Driver->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(),
            EyeZ, VisualBounds.IsValid ? VisualBounds.Min.Z : Position.Z, VisualBounds.IsValid ? VisualBounds.Max.Z : Position.Z);
    }

    void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        if (bFinished || !World || !World->IsGameWorld() || !World->GetGameState())
        {
            return;
        }
        const bool bServer = Role == TEXT("server");
        if ((bServer && World->GetNetMode() != NM_DedicatedServer)
            || (!bServer && World->GetNetMode() != NM_Client))
        {
            return;
        }
        const double Time = World->GetGameState()->GetServerWorldTimeSeconds() - StartTime;
        if (!bServer && !bNameRequested)
        {
            APlayerController* NameController = World->GetFirstPlayerController();
            if (NameController && NameController->IsLocalController() && NameController->PlayerState)
            {
                NameController->SetName(Role == TEXT("driver") ? TEXT("VSDriver") : TEXT("VSObserver"));
                bNameRequested = true;
            }
        }
        AVSCharacter* Driver = nullptr;
        AVSCharacter* Observer = nullptr;
        for (TActorIterator<AVSCharacter> It(World); It; ++It)
        {
            if (const APlayerState* Player = It->GetPlayerState())
            {
                if (Player->GetPlayerName() == TEXT("VSDriver")) { Driver = *It; }
                if (Player->GetPlayerName() == TEXT("VSObserver")) { Observer = *It; }
            }
        }
        AVSCharacter* LocalPawn = Role == TEXT("driver") ? Driver : Observer;
        APlayerController* PC = LocalPawn ? Cast<APlayerController>(LocalPawn->GetController()) : nullptr;
        if (!Driver || !Observer || (!bServer && (!PC || !PC->IsLocalController() || !LocalPawn->InputComponent)))
        {
            if (Time > 1.0) { Fail(Time, TEXT("two_players_not_ready_before_start_plus_one")); }
            return;
        }
        if (!bReady)
        {
            if (Time > 1.0) { Fail(Time, TEXT("first_ready_after_start_plus_one")); return; }
            bReady = true;
            LocalController = bServer ? nullptr : PC;
            UE_LOG(LogValStrike, Display, TEXT("VSMOVE_JSON {\"event\":\"ready\",\"role\":\"%s\",\"t\":%.3f,\"initial_spawn_distance\":%.3f}"),
                *Role, Time, FVector::Distance(Driver->GetActorLocation(), Observer->GetActorLocation()));
            if (bServer)
            {
                if (!Place(Driver, FVector(-1400.0, -1100.0, 90.0))
                    || !Place(Observer, FVector(-1200.0, -900.0, 90.0)))
                {
                    Fail(Time, TEXT("initial_placement_failed")); return;
                }
                Event(TEXT("setup"), Time, TEXT("initial"));
            }
        }
        if (bServer && Time >= 20.0 && !bStairsPlaced)
        {
            bStairsPlaced = true;
            if (!Place(Driver, FVector(-550.0, 1050.0, 90.0))) { Fail(Time, TEXT("stairs_placement_failed")); return; }
            Event(TEXT("setup"), Time, TEXT("stairs"));
        }
        if (bServer && Time >= 27.0 && !bCoverPlaced)
        {
            bCoverPlaced = true;
            if (!Place(Driver, FVector(-900.0, 0.0, 90.0))) { Fail(Time, TEXT("cover_placement_failed")); return; }
            Event(TEXT("setup"), Time, TEXT("cover"));
        }
        if (Role == TEXT("driver"))
        {
            PC->SetControlRotation(FRotator::ZeroRotator);
            Key(PC, EKeys::LeftShift, Time >= 3.0 && Time < 5.0, bWalk);
            Key(PC, EKeys::LeftControl, Time >= 5.5 && Time < 7.0, bCrouch);
            const bool bMove = (Time >= 1.0 && Time < 2.5) || (Time >= 3.0 && Time < 5.0)
                || (Time >= 5.5 && Time < 7.0) || (Time >= 11.0 && Time < 18.0)
                || (Time >= 21.0 && Time < 24.0) || (Time >= 28.0 && Time < 31.0);
            Key(PC, EKeys::W, bMove, bForward);
            Key(PC, EKeys::SpaceBar, Time >= 8.0 && Time < 8.15, bJump);
        }
        if (Time >= -1.0 && Time - LastSample >= 0.1)
        {
            LastSample = Time;
            Sample(Driver, Time);
        }
        if (Time >= 33.0)
        {
            Event(TEXT("done"), Time);
            bFinished = true;
            ReleaseKeys();
        }
    }

    FString Role;
    FDelegateHandle Delegate;
    TWeakObjectPtr<APlayerController> LocalController;
    float StartTime = 30.0f;
    double LastSample = -1000.0;
    bool bReady = false, bFinished = false, bStairsPlaced = false, bCoverPlaced = false, bNameRequested = false;
    bool bForward = false, bWalk = false, bCrouch = false, bJump = false;
};

TUniquePtr<FVSMovementSmoke> MovementSmoke;
}

void ValStrikeTests::StartMovementSmoke()
{
    FString Role;
    if (FParse::Value(FCommandLine::Get(), TEXT("VSMovementSmoke="), Role)
        && (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        MovementSmoke = MakeUnique<FVSMovementSmoke>(Role);
    }
}

void ValStrikeTests::StopMovementSmoke()
{
    MovementSmoke.Reset();
}

#else

void ValStrikeTests::StartMovementSmoke() {}
void ValStrikeTests::StopMovementSmoke() {}

#endif
