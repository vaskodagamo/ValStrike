#include "VSShotEffectsTest.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "VSShotEffectsSubsystem.h"
#include "../Combat/VSHealthComponent.h"
#include "../Combat/VSRifleComponent.h"
#include "../Presentation/VSRiflePresentationComponent.h"
#include "../ValStrike.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SceneInterface.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

// File-local names are unique to this file: the unity build compiles every
// ValStrike .cpp into one translation unit.
namespace VSShotEffectsTestDetail
{
bool IsShotEffectsTest()
{
    FString Role;
    return FParse::Value(FCommandLine::Get(), TEXT("VSShotEffectsTest="), Role)
        && (Role == TEXT("server") || Role == TEXT("shooter") || Role == TEXT("observer"));
}

const TCHAR* const ShotFxShooterName = TEXT("VSShotFxShooter");
const TCHAR* const ShotFxObserverName = TEXT("VSShotFxObserver");
// VS_Graybox: floor top Z=0, west wall inner face X=-1950, Cover_3 west face X=-175.
const FVector ShotFxShooterSpot(-1000, -1000, 90);
const FRotator ShotFxShooterFacing(0, 180, 0);
const FVector ShotFxObserverSpot(-650, -1300, 90);
const FRotator ShotFxObserverView(-6, 159.4, 0);
const FVector ShotFxTargetSpot(-600, -1000, 90); // Observer stands here for the player shot.
const FRotator ShotFxTargetFacing(0, 180, 0);

struct FShotFxPlan
{
    const TCHAR* Label;
    double AimAt;
    double Press;
    FRotator Aim;
    const TCHAR* Capture;
};

// One click each; every accepted shot has its own window. Times are seconds
// after the shared start on the server clock.
const FShotFxPlan ShotFxPlan[] = {
    {TEXT("wall"), 2.0, 3.0, FRotator(0, 180, 0), TEXT("tracer_neutral")},
    {TEXT("floor"), 5.0, 6.0, FRotator(-30, 180, 0), nullptr},
    {TEXT("cover"), 8.0, 9.0, FRotator(0, 22.99, 0), nullptr},
    {TEXT("player"), 10.0, 11.0, FRotator(-5, 0, 0), TEXT("player")},
    {TEXT("wall_dim"), 12.6, 14.5, FRotator(0, 170, 0), TEXT("tracer_dim")},
    {TEXT("muzzle"), 20.0, 21.0, FRotator(0, 185, 0), TEXT("muzzle")},
    {TEXT("rejoin"), 42.0, 43.0, FRotator(0, 175, 0), TEXT("rejoin")},
};
constexpr int32 ShotFxPlanCount = UE_ARRAY_COUNT(ShotFxPlan);
constexpr double ShotFxWindowBefore = 0.5;
constexpr double ShotFxWindowAfter = 1.4;
constexpr double ShotFxBurstOpen = 22.9;
constexpr double ShotFxBurstClose = 32.0;
constexpr double ShotFxFinish = 45.5;

FString ShotFxVec(const FVector& V) { return FString::Printf(TEXT("[%.2f,%.2f,%.2f]"), V.X, V.Y, V.Z); }

const TCHAR* ShotFxKind(EVSShotImpactKind Kind)
{
    switch (Kind)
    {
    case EVSShotImpactKind::World: return TEXT("world");
    case EVSShotImpactKind::Character: return TEXT("character");
    case EVSShotImpactKind::Self: return TEXT("self");
    default: return TEXT("none");
    }
}

class FVSShotEffectsSmoke
{
public:
    FVSShotEffectsSmoke()
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSShotEffectsTest="), Role);
        FParse::Value(FCommandLine::Get(), TEXT("VSShotEffectsOutput="), Output);
        FParse::Value(FCommandLine::Get(), TEXT("VSShotEffectsStart="), Start);
        FParse::Value(FCommandLine::Get(), TEXT("VSShotEffectsPort="), Port);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSShotEffectsSmoke::Tick);
    }
    ~FVSShotEffectsSmoke() { FWorldDelegates::OnWorldPostActorTick.Remove(Delegate); }

private:
    struct FShotResult
    {
        int32 Effects = 0;
        bool bRecord = false;
        FVSShotEffectRecord Record;
        bool bTracerVisible = false;
        bool bExpectMuzzle = false;
        TWeakObjectPtr<const USceneComponent> ExpectedMuzzle;
        FVector ExpectedMuzzleLocation = FVector::ZeroVector;
        FString HitRole = TEXT("none");
        bool bMarkOnTarget = false;
        bool bCaptured = false;
        bool bChecked = false;
        uint32 ServerAccepted = 0;
        bool bServerHit = false;
        FVector ServerPoint = FVector::ZeroVector;
        FString ServerHitRole = TEXT("none");
    };

    struct FStatsMark
    {
        uint32 Effects = 0, Represented = 0, Tracers = 0, WorldMarks = 0, Shots = 0;
        bool bValid = false;
    };

    bool Once(int32 Step, double Time, double At)
    {
        if (Time < At || Steps.Contains(Step)) { return false; }
        Steps.Add(Step);
        return true;
    }
    bool First(int32 Step)
    {
        if (Steps.Contains(Step)) { return false; }
        Steps.Add(Step);
        return true;
    }
    void Check(const FString& Name, bool bPassed, const FString& Detail)
    {
        Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"), *Name,
            bPassed ? TEXT("true") : TEXT("false"), *Detail));
        UE_LOG(LogValStrike, Display, TEXT("VSSHOTFX %s %s %s %s"), *Role, *Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    }
    void Finish(const TCHAR* Error = nullptr)
    {
        bFinished = true;
        const FString Report = FString::Printf(
            TEXT("{\"schema_version\":1,\"role\":\"%s\",\"completed\":true,\"checks\":[%s],\"shots\":[%s],\"error\":%s}\n"),
            *Role, *FString::Join(Checks, TEXT(",")), *FString::Join(Shots, TEXT(",")),
            Error ? *FString::Printf(TEXT("\"%s\""), Error) : TEXT("null"));
        if (FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp"))))
        {
            IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
        }
    }
    void Capture(const TCHAR* Name) const
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Output, TEXT("")) + TEXT("_") + Name + TEXT(".png"), true, false);
    }
    static void Key(APlayerController* PC, FKey Name, bool bWanted, bool& bHeld)
    {
        if (bHeld == bWanted) { return; }
        UE_LOG(LogValStrike, Log, TEXT("VSSHOTFX key %s %s"), *Name.ToString(), bWanted ? TEXT("down") : TEXT("up"));
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, bWanted ? IE_Pressed : IE_Released, bWanted ? 1 : 0));
        bHeld = bWanted;
    }
    static bool Place(AVSCharacter* Pawn, FVector Location, FRotator Rotation)
    {
        if (!Pawn) { return false; }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        if (!Pawn->TeleportTo(Location, Rotation)) { return false; }
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            PC->SetControlRotation(Rotation);
            PC->ClientSetLocation(Location, Rotation);
            PC->ClientSetRotation(Rotation);
        }
        Pawn->ForceNetUpdate();
        return true;
    }
    static void Lighting(UWorld* World, bool bDim)
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It) { It->GetLightComponent()->SetIntensity(bDim ? 0.35f : 3.0f); }
        for (TActorIterator<ASkyLight> It(World); It; ++It) { It->GetLightComponent()->SetIntensity(bDim ? 0.25f : 1.0f); }
    }
    static int32 WindowAt(double Time)
    {
        for (int32 Index = 0; Index < ShotFxPlanCount; ++Index)
        {
            if (Time >= ShotFxPlan[Index].Press - ShotFxWindowBefore && Time < ShotFxPlan[Index].Press + ShotFxWindowAfter) { return Index; }
        }
        return INDEX_NONE;
    }
    static bool InBurst(double Time) { return Time >= ShotFxBurstOpen && Time < ShotFxBurstClose; }
    static FString RoleOf(const AActor* Actor, const AVSCharacter* Shooter, const AVSCharacter* Observer)
    {
        if (!Actor) { return TEXT("none"); }
        return Actor == Shooter ? TEXT("shooter") : Actor == Observer ? TEXT("observer") : TEXT("world");
    }
    static bool ShaderReady(const UMaterialInterface* Material, const UWorld* World, FString& Detail)
    {
        const FMaterialResource* Shader = Material && World->Scene
            ? Material->GetMaterialResource(World->Scene->GetShaderPlatform()) : nullptr;
        const bool bReady = Shader && Shader->GetGameThreadShaderMap() && Shader->IsGameThreadShaderMapComplete()
            && Shader->GetCompileErrors().IsEmpty();
        Detail += FString::Printf(TEXT("%s resource=%d map=%d complete=%d errors=%d; "), Material ? *Material->GetName() : TEXT("missing"),
            Shader != nullptr, Shader && Shader->GetGameThreadShaderMap(), Shader && Shader->IsGameThreadShaderMapComplete(),
            Shader ? Shader->GetCompileErrors().Num() : -1);
        return bReady;
    }
    static bool ServerHasNoEffects(UWorld* World, FString& Detail)
    {
        const bool bSubsystem = World->GetSubsystem<UVSShotEffectsSubsystem>() != nullptr;
        int32 Actors = 0, Components = 0;
        for (TActorIterator<AActor> It(World); It; ++It) { Actors += It->ActorHasTag(UVSShotEffectsSubsystem::EffectsActorTag) ? 1 : 0; }
        for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
        {
            if (It->GetWorld() == World && (It->GetName().StartsWith(TEXT("VSShotTracer")) || It->GetName().StartsWith(TEXT("VSImpactMark")))) { ++Components; }
        }
        Detail = FString::Printf(TEXT("dedicated=%d subsystem=%d effect_actors=%d effect_components=%d"),
            IsRunningDedicatedServer(), bSubsystem, Actors, Components);
        return IsRunningDedicatedServer() && !bSubsystem && Actors == 0 && Components == 0;
    }

    void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        if (bFinished || !World || !World->IsGameWorld() || !World->GetGameState()) { return; }
        const bool bServer = Role == TEXT("server");
        if ((bServer && World->GetNetMode() != NM_DedicatedServer) || (!bServer && World->GetNetMode() != NM_Client)) { return; }
        const AVSShotEffectsTestGameState* Clock = Cast<AVSShotEffectsTestGameState>(World->GetGameState());
        double Time = (bServer || !Clock ? World->GetGameState()->GetServerWorldTimeSeconds() : Clock->EstimateServerSeconds()) - Start;
        // Scripted steps must never re-trigger: the client timeline only moves forward.
        if (!bServer) { Time = FMath::Max(Time, LastClientTime); LastClientTime = Time; }
        if (!bServer) { IsolateFromDesktopInput(World); }
        if (!bServer && NamedWorld.Get() != World)
        {
            if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->PlayerState)
            {
                PC->SetName(Role == TEXT("shooter") ? ShotFxShooterName : ShotFxObserverName);
                NamedWorld = World;
            }
        }
        AVSCharacter* Shooter = nullptr;
        AVSCharacter* Observer = nullptr;
        for (TActorIterator<AVSCharacter> It(World); It; ++It)
        {
            if (const APlayerState* PS = It->GetPlayerState())
            {
                if (PS->GetPlayerName() == ShotFxShooterName) { Shooter = *It; }
                if (PS->GetPlayerName() == ShotFxObserverName) { Observer = *It; }
            }
        }
        AVSCharacter* Local = Role == TEXT("shooter") ? Shooter : Observer;
        APlayerController* PC = Local ? Cast<APlayerController>(Local->GetController()) : nullptr;
        // Respawn and rejoin legitimately remove a player for a moment.
        // Each process estimates the server clock; allow a few hundred ms of skew.
        const bool bTransition = (Time > 16.4 && Time < 19.6) || (Time > 34.4 && Time < 41.9);
        if (!Shooter || !Observer || (!bServer && (!PC || !Local->InputComponent)))
        {
            if (Time > 1 && !bTransition) { Finish(TEXT("players_not_ready")); }
            return;
        }
        if (bServer) { ServerTick(World, Time, Shooter, Observer); }
        else { ClientTick(World, Time, Shooter, Observer, PC); }
        if (!bFinished && Time >= ShotFxFinish) { Finish(); }
    }

    static void IsolateFromDesktopInput(UWorld* World)
    {
        // Rendered clients take window focus. Real keystrokes or mouse movement on
        // this desktop must not fire, reload, move or turn the scripted players.
        // Scripted keys use APlayerController::InputKey and bypass the viewport.
        // Possession re-applies game input mode, so this runs every tick.
        UGameViewportClient* Viewport = World->GetGameViewport();
        if (!Viewport) { return; }
        if (!Viewport->IgnoreInput()) { Viewport->SetIgnoreInput(true); }
        if (Viewport->GetMouseCaptureMode() != EMouseCaptureMode::NoCapture) { Viewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture); }
        if (Viewport->GetMouseLockMode() != EMouseLockMode::DoNotLock) { Viewport->SetMouseLockMode(EMouseLockMode::DoNotLock); }
    }

    void ServerTick(UWorld* World, double Time, AVSCharacter* Shooter, AVSCharacter* Observer)
    {
        if (!bReady)
        {
            bReady = true;
            const bool bPlaced = Place(Shooter, ShotFxShooterSpot, ShotFxShooterFacing)
                && Place(Observer, ShotFxObserverSpot, ShotFxObserverView);
            Check(TEXT("two_players"), bPlaced && Shooter != Observer, TEXT("two possessed pawns placed"));
            if (!bPlaced) { Finish(TEXT("placement_failed")); return; }
            FirstShooterId = Shooter->GetUniqueID();
            ObserverId = Observer->GetUniqueID();
        }
        if (Observer->GetUniqueID() != ObserverId)
        {
            // The observer reconnected: a fresh controller and pawn.
            ObserverId = Observer->GetUniqueID();
            ++ObserverRejoins;
            Place(Observer, ShotFxObserverSpot, ShotFxObserverView);
        }
        if (Once(1, Time, 0.5))
        {
            FString Detail;
            Check(TEXT("no_effects_on_server"), ServerHasNoEffects(World, Detail), Detail);
        }

        // Accepted shots and the server's own reference trace, taken in the same
        // frame as acceptance from the pawn's server view and base aim.
        const uint32 ShotsNow = Shooter->GetRifleComponent()->GetState().ShotsFired;
        if (Shooter->GetUniqueID() != TrackedShooterId) { TrackedShooterId = Shooter->GetUniqueID(); TrackedShots = ShotsNow; }
        else if (ShotsNow > TrackedShots)
        {
            const uint32 Delta = ShotsNow - TrackedShots;
            TrackedShots = ShotsNow;
            const int32 Window = WindowAt(Time);
            if (Window != INDEX_NONE)
            {
                FShotResult& Result = Results[Window];
                Result.ServerAccepted += Delta;
                const FVector Eye = Shooter->GetPawnViewLocation();
                FHitResult Hit;
                FCollisionQueryParams Query(SCENE_QUERY_STAT(VSShotFxReference), false, Shooter);
                Result.bServerHit = World->LineTraceSingleByChannel(Hit, Eye, Eye + Shooter->GetBaseAimRotation().Vector() * 10000.0,
                    ECC_Visibility, Query);
                Result.ServerPoint = Result.bServerHit ? FVector(Hit.ImpactPoint) : FVector::ZeroVector;
                Result.ServerHitRole = RoleOf(Hit.GetActor(), Shooter, Observer);
            }
            else if (InBurst(Time)) { BurstServerShots += Delta; }
            else
            {
                UnexpectedServerShots += Delta;
                UE_LOG(LogValStrike, Warning, TEXT("VSSHOTFX server unexpected accepted shot(s) at %.2f s: %u"), Time, Delta);
            }
        }

        if (Once(2, Time, 10.0)) { Place(Observer, ShotFxTargetSpot, ShotFxTargetFacing); }
        if (Once(3, Time, 12.3))
        {
            const float Health = Observer->GetHealthComponent()->GetHealth();
            Check(TEXT("player_damage"), FMath::IsNearlyEqual(Health, 70.0f), FString::Printf(TEXT("observer_health=%.1f"), Health));
        }
        if (Once(4, Time, 12.5)) { Place(Observer, ShotFxObserverSpot, ShotFxObserverView); }
        if (Once(5, Time, 17.0))
        {
            AController* Controller = Shooter->GetController();
            Controller->UnPossess();
            Shooter->Destroy();
            World->GetAuthGameMode()->RestartPlayer(Controller);
            if (!Place(Cast<AVSCharacter>(Controller->GetPawn()), ShotFxShooterSpot, ShotFxShooterFacing))
            {
                Finish(TEXT("respawn_placement_failed"));
            }
            return;
        }
        const auto& Gun = Shooter->GetRifleComponent()->GetState();
        if (Once(6, Time, 19.2))
        {
            Check(TEXT("respawn"), Shooter->GetUniqueID() != FirstShooterId && Shooter->GetHUDData().bAlive
                && Gun.ShotsFired == 0 && Gun.Magazine == 30,
                FString::Printf(TEXT("new_pawn=%d shots=%u ammo=%d"), Shooter->GetUniqueID() != FirstShooterId, Gun.ShotsFired, Gun.Magazine));
        }
        for (int32 Index = 0; Index < ShotFxPlanCount; ++Index)
        {
            FShotResult& Result = Results[Index];
            if (Result.bChecked || Time < ShotFxPlan[Index].Press + ShotFxWindowAfter) { continue; }
            Result.bChecked = true;
            const bool bPlayer = FCString::Strcmp(ShotFxPlan[Index].Label, TEXT("player")) == 0;
            Check(FString::Printf(TEXT("server_%s"), ShotFxPlan[Index].Label), Result.ServerAccepted == 1 && Result.bServerHit
                && Result.ServerHitRole == (bPlayer ? TEXT("observer") : TEXT("world")),
                FString::Printf(TEXT("accepted=%u hit=%d role=%s point=%s"), Result.ServerAccepted, Result.bServerHit,
                    *Result.ServerHitRole, *ShotFxVec(Result.ServerPoint)));
            Shots.Add(FString::Printf(TEXT("{\"label\":\"%s\",\"accepted\":%u,\"hit\":%s,\"role\":\"%s\",\"point\":%s}"),
                ShotFxPlan[Index].Label, Result.ServerAccepted, Result.bServerHit ? TEXT("true") : TEXT("false"),
                *Result.ServerHitRole, *ShotFxVec(Result.ServerPoint)));
        }
        if (Once(7, Time, 32.1))
        {
            Check(TEXT("burst_shots"), BurstServerShots > static_cast<uint32>(UVSShotEffectsSubsystem::MaxMarks),
                FString::Printf(TEXT("accepted=%u ammo=%d reserve=%d"), BurstServerShots, Gun.Magazine, Gun.Reserve));
        }
        if (Once(8, Time, 41.8))
        {
            Check(TEXT("observer_rejoined"), ObserverRejoins == 1, FString::Printf(TEXT("rejoins=%d"), ObserverRejoins));
        }
        if (Once(9, Time, 45.0))
        {
            FString Detail;
            Check(TEXT("no_effects_on_server_end"), ServerHasNoEffects(World, Detail), Detail);
            Check(TEXT("no_unexpected_server_shots"), UnexpectedServerShots == 0, FString::Printf(TEXT("unexpected=%u"), UnexpectedServerShots));
        }
    }

    void EnsureVisual(AVSCharacter* Shooter)
    {
        if (Shooter->FindComponentByClass<UVSRiflePresentationComponent>()) { return; }
        UVSRiflePresentationComponent* Visual = NewObject<UVSRiflePresentationComponent>(Shooter);
        Shooter->AddInstanceComponent(Visual);
        Visual->RegisterComponent();
        if (!Visual->Initialize(Shooter, Shooter->FindComponentByClass<UCameraComponent>(), Shooter->GetMesh()))
        {
            Finish(TEXT("visual_attachment_failed"));
        }
    }

    void DriveShooter(APlayerController* PC, double Time)
    {
        // One click per planned shot: press when the timeline crosses the press time,
        // release on the next tick. A click never depends on hitting a time window.
        for (int32 Index = 0; Index < ShotFxPlanCount; ++Index)
        {
            if (Once(100 + Index, Time, ShotFxPlan[Index].AimAt)) { PC->SetControlRotation(ShotFxPlan[Index].Aim); }
            if (Once(200 + Index, Time, ShotFxPlan[Index].Press)) { ClickTicks = 1; }
        }
        const bool bFire = ClickTicks > 0;
        ClickTicks = FMath::Max(ClickTicks - 1, 0);
        // Two held bursts sweep across the west wall: more marks than the pool holds.
        const bool bBurstA = Time >= 23.0 && Time < 26.8;
        const bool bBurstB = Time >= 29.6 && Time < 31.4;
        if (bBurstA) { PC->SetControlRotation(FRotator(0, FMath::Lerp(200.0, 160.0, FMath::Clamp((Time - 23.0) / 3.6, 0.0, 1.0)), 0)); }
        if (bBurstB) { PC->SetControlRotation(FRotator(0, FMath::Lerp(160.0, 190.0, FMath::Clamp((Time - 29.6) / 1.8, 0.0, 1.0)), 0)); }
        Key(PC, EKeys::LeftMouseButton, bFire || bBurstA || bBurstB, bFireHeld);
        if (Once(300, Time, 27.0)) { ReloadTicks = 1; }
        Key(PC, EKeys::R, ReloadTicks > 0, bReloadHeld);
        ReloadTicks = FMath::Max(ReloadTicks - 1, 0);
    }

    FStatsMark MarkStats(const UVSShotEffectsSubsystem& Fx, const AVSCharacter* Shooter) const
    {
        const FVSShotEffectsStats& S = Fx.GetStats();
        FStatsMark Mark;
        Mark.Effects = S.Effects;
        Mark.Represented = S.RepresentedShots;
        Mark.Tracers = S.Tracers;
        Mark.WorldMarks = S.WorldMarks;
        Mark.Shots = Shooter->GetRifleComponent()->GetState().ShotsFired;
        Mark.bValid = true;
        return Mark;
    }

    void ClientTick(UWorld* World, double Time, AVSCharacter* Shooter, AVSCharacter* Observer, APlayerController* PC)
    {
        UVSShotEffectsSubsystem* Fx = World->GetSubsystem<UVSShotEffectsSubsystem>();
        if (!Fx) { Finish(TEXT("no_effects_subsystem")); return; }
        const bool bShooter = Role == TEXT("shooter");
        const FVSShotEffectsStats& S = Fx->GetStats();
        if (Fx != CurrentFx.Get())
        {
            // A new world after rejoin has a new subsystem; count from its own start.
            CurrentFx = Fx;
            SeenEffects = 0;
        }
        if (Time >= 19.5) { EnsureVisual(Shooter); }
        if (bFinished) { return; }
        if (bShooter) { DriveShooter(PC, Time); }
        if (Once(10, Time, 13.5) || Once(12, Time, 32.5)) { Lighting(World, true); }
        if (Once(11, Time, 15.5) || Once(13, Time, 34.0)) { Lighting(World, false); }

        if (Once(14, Time, 0.5))
        {
            const UStaticMeshComponent* WarmTracer = Fx->GetTracerComponent(0);
            const UStaticMeshComponent* WarmMark = Fx->GetMarkComponent(0);
            Check(TEXT("subsystem_present"), Fx->IsTickable() && Fx->IsWarmedUp() && S.Effects == 0 && S.Baselines >= 2
                && Fx->GetActiveTracerCount() == 0 && Fx->GetActiveMarkCount() == 0
                && WarmTracer && !WarmTracer->IsVisible() && WarmMark && !WarmMark->IsVisible(),
                FString::Printf(TEXT("tickable=%d warmed=%d effects=%u baselines=%u pools=%d/%d"), Fx->IsTickable(), Fx->IsWarmedUp(),
                    S.Effects, S.Baselines, Fx->GetTracerPoolSize(), Fx->GetMarkPoolSize()));
        }

        // The frame that first draws an effect ends before the next tick, so measure
        // every frame from the detection tick for 1.5 s of real time.
        const double RealNow = FPlatformTime::Seconds();
        const double FrameSeconds = LastRealTick > 0.0 ? RealNow - LastRealTick : 0.0;
        LastRealTick = RealNow;
        if (FirstEffectRealTime > 0.0 && !Steps.Contains(16))
        {
            LongestFrameAfterFirstEffect = FMath::Max(LongestFrameAfterFirstEffect, FrameSeconds);
            if (RealNow - FirstEffectRealTime >= 1.5 && First(16))
            {
                Check(TEXT("first_effect_no_stall"), LongestFrameAfterFirstEffect < 0.5,
                    FString::Printf(TEXT("longest_frame_s=%.3f (includes a screenshot capture)"), LongestFrameAfterFirstEffect));
            }
        }
        // Prime the screenshot path before any effect so its one-time cost is not
        // attributed to the first effect; also documents the scene beforehand.
        if (Once(17, Time, 0.6)) { Capture(TEXT("start")); }

        // Attribute each new effect to the planned shot window it appeared in.
        if (S.Effects != SeenEffects)
        {
            const uint32 New = S.Effects - SeenEffects;
            SeenEffects = S.Effects;
            const FVSShotEffectRecord& Record = Fx->GetLastEffect();
            const int32 Window = WindowAt(Time);
            if (Window != INDEX_NONE)
            {
                FShotResult& Result = Results[Window];
                Result.Effects += New;
                Result.bRecord = true;
                Result.Record = Record;
                const UStaticMeshComponent* Tracer = Fx->GetTracerComponent(Record.TracerSlot);
                Result.bTracerVisible = Record.bTracer && Tracer && Tracer->IsVisible();
                const UVSRiflePresentationComponent* Visual = Shooter->FindComponentByClass<UVSRiflePresentationComponent>();
                Result.bExpectMuzzle = Visual && Visual->IsInitialized() && Shooter->GetHUDData().bAlive;
                const UStaticMeshComponent* Body = Result.bExpectMuzzle
                    ? (bShooter ? Visual->GetFirstPersonBody() : Visual->GetWorldBody()) : nullptr;
                Result.ExpectedMuzzle = Body;
                Result.ExpectedMuzzleLocation = Body ? Body->GetSocketLocation(UVSShotEffectsSubsystem::MuzzleSocket) : FVector::ZeroVector;
                Result.HitRole = RoleOf(Record.HitActor.Get(), Shooter, Observer);
                const UStaticMeshComponent* Mark = Fx->GetMarkComponent(Record.MarkSlot);
                Result.bMarkOnTarget = Mark && Mark->GetAttachParent() && Mark->GetAttachParent()->GetOwner() == Observer;
                if (ShotFxPlan[Window].Capture && !Result.bCaptured)
                {
                    Result.bCaptured = true;
                    Capture(ShotFxPlan[Window].Capture);
                }
                if (First(15))
                {
                    FirstEffectRealTime = RealNow;
                    FString Detail;
                    const bool bTracerReady = ShaderReady(Fx->GetTracerMaterial(), World, Detail);
                    const bool bMarkReady = ShaderReady(Fx->GetMarkMaterial(), World, Detail);
                    Check(TEXT("effect_shaders_ready"), bTracerReady && bMarkReady, Detail);
                    const UMaterialInterface* TracerMaterial = Tracer ? Tracer->GetMaterial(0) : nullptr;
                    const UMaterialInterface* MarkMaterial = Mark ? Mark->GetMaterial(0) : nullptr;
                    Check(TEXT("effect_materials"), TracerMaterial && MarkMaterial
                        && TracerMaterial->GetPathName() == TEXT("/Game/Effects/M_VSShotTracer.M_VSShotTracer")
                        && MarkMaterial->GetPathName() == TEXT("/Game/Effects/M_VSImpactMark.M_VSImpactMark")
                        && TracerMaterial->GetShadingModels().HasShadingModel(MSM_Unlit) && TracerMaterial->GetBlendMode() == BLEND_Additive
                        && MarkMaterial->GetShadingModels().HasShadingModel(MSM_Unlit) && MarkMaterial->GetBlendMode() == BLEND_Translucent,
                        FString::Printf(TEXT("tracer=%s mark=%s"), TracerMaterial ? *TracerMaterial->GetPathName() : TEXT("missing"),
                            MarkMaterial ? *MarkMaterial->GetPathName() : TEXT("missing")));
                }
            }
            else if (InBurst(Time)) { BurstEffects += New; }
            else
            {
                UnexpectedEffects += New;
                UE_LOG(LogValStrike, Warning, TEXT("VSSHOTFX %s unexpected effect at %.2f s: represented=%u kind=%s"), *Role, Time,
                    Record.RepresentedShots, ShotFxKind(Record.Impact));
            }
        }

        // Single-shot results once each window closes.
        for (int32 Index = 0; Index < ShotFxPlanCount; ++Index)
        {
            if (!Results[Index].bChecked && Time >= ShotFxPlan[Index].Press + ShotFxWindowAfter) { CheckShot(Index, bShooter); }
        }

        // Lifetime of the first (wall) shot's tracer and mark, on this client's clock.
        const FShotResult& WallShot = Results[0];
        if (WallShot.bRecord && CurrentFx.Get() == Fx && Time < 16.0)
        {
            const FVSShotEffectRecord& Record = WallShot.Record;
            const double Age = World->GetTimeSeconds() - Record.Time;
            const UStaticMeshComponent* Tracer = Fx->GetTracerComponent(Record.TracerSlot);
            const UStaticMeshComponent* Mark = Fx->GetMarkComponent(Record.MarkSlot);
            const float Opacity = Fx->GetMarkOpacity(Record.MarkSlot);
            const FString Detail = FString::Printf(TEXT("age=%.2f opacity=%.3f mark_visible=%d active_tracers=%d"),
                Age, Opacity, Mark && Mark->IsVisible(), Fx->GetActiveTracerCount());
            if (Age >= 0.25 && First(20))
            {
                Check(TEXT("tracer_short_lived"), WallShot.bTracerVisible && Tracer && !Tracer->IsVisible()
                    && Fx->GetActiveTracerCount() == 0, Detail);
            }
            if (Age >= 1.2 && First(21)) { Check(TEXT("mark_hold"), Mark && Mark->IsVisible() && FMath::IsNearlyEqual(Opacity, 1.0f), Detail); }
            if (Age >= 3.4 && First(22)) { Check(TEXT("mark_fading"), Mark && Mark->IsVisible() && Opacity > 0.05f && Opacity < 0.95f, Detail); }
            if (Age >= 4.4 && First(23))
            {
                Check(TEXT("mark_faded"), Mark && !Mark->IsVisible() && Opacity < 0.0f, Detail);
                Capture(TEXT("fade"));
            }
        }

        if (Once(24, Time, 16.0))
        {
            // Effects never collide, block traces, affect navigation or cast shadows.
            bool bInert = Fx->GetEffectsActor() && !Fx->GetEffectsActor()->GetActorEnableCollision()
                && !Fx->GetEffectsActor()->GetIsReplicated();
            int32 Components = 0;
            for (int32 Slot = 0; Slot < Fx->GetTracerPoolSize() + Fx->GetMarkPoolSize(); ++Slot)
            {
                const UStaticMeshComponent* C = Slot < Fx->GetTracerPoolSize() ? Fx->GetTracerComponent(Slot)
                    : Fx->GetMarkComponent(Slot - Fx->GetTracerPoolSize());
                bInert &= C && C->GetCollisionEnabled() == ECollisionEnabled::NoCollision && !C->CanEverAffectNavigation()
                    && !C->CastShadow && !C->GetGenerateOverlapEvents() && C->GetOwner() == Fx->GetEffectsActor();
                ++Components;
            }
            Check(TEXT("effects_inert"), bInert && Components > 0, FString::Printf(TEXT("components=%d"), Components));
            AfterSingles = MarkStats(*Fx, Shooter);
            FirstShooterId = Shooter->GetUniqueID();
        }
        if (Once(25, Time, 19.2))
        {
            const uint32 ShotsNow = Shooter->GetRifleComponent()->GetState().ShotsFired;
            Check(TEXT("respawn_no_replay"), Shooter->GetUniqueID() != FirstShooterId && ShotsNow == 0
                && S.Effects == AfterSingles.Effects && Fx->GetActiveTracerCount() == 0,
                FString::Printf(TEXT("new_pawn=%d shots=%u effects=%u/%u baselines=%u"), Shooter->GetUniqueID() != FirstShooterId,
                    ShotsNow, S.Effects, AfterSingles.Effects, S.Baselines));
        }
        if (Once(26, Time, 22.0))
        {
            Check(TEXT("long_mark_timing"), Fx->DevelopmentSetWorldMarkTiming(40.0f, 1.5f), TEXT("hold=40 fade=1.5 for the pool test"));
        }
        if (Once(27, Time, ShotFxBurstOpen)) { BeforeBurst = MarkStats(*Fx, Shooter); }
        if (Once(28, Time, ShotFxBurstClose + 0.1))
        {
            const FStatsMark After = MarkStats(*Fx, Shooter);
            const uint32 ShotsDelta = After.Shots - BeforeBurst.Shots;
            const uint32 EffectsDelta = After.Effects - BeforeBurst.Effects;
            Check(TEXT("burst_accounting"), BeforeBurst.bValid && ShotsDelta > static_cast<uint32>(UVSShotEffectsSubsystem::MaxMarks)
                && After.Represented - BeforeBurst.Represented == ShotsDelta && EffectsDelta == BurstEffects
                && EffectsDelta <= ShotsDelta && After.Tracers - BeforeBurst.Tracers == EffectsDelta
                && After.WorldMarks - BeforeBurst.WorldMarks == EffectsDelta,
                FString::Printf(TEXT("accepted=%u effects=%u represented=%u coalesced=%u tracers=%u marks=%u"), ShotsDelta, EffectsDelta,
                    After.Represented - BeforeBurst.Represented, After.Represented - BeforeBurst.Represented - EffectsDelta,
                    After.Tracers - BeforeBurst.Tracers, After.WorldMarks - BeforeBurst.WorldMarks));
            Check(TEXT("pool_cap"), EffectsDelta > static_cast<uint32>(UVSShotEffectsSubsystem::MaxMarks)
                && Fx->GetMarkPoolSize() == UVSShotEffectsSubsystem::MaxMarks && Fx->GetActiveMarkCount() == UVSShotEffectsSubsystem::MaxMarks
                && S.MaxActiveMarks == UVSShotEffectsSubsystem::MaxMarks && S.MarksRecycled > 0
                && Fx->GetTracerPoolSize() <= UVSShotEffectsSubsystem::MaxTracers,
                FString::Printf(TEXT("mark_pool=%d active=%d max_active=%d recycled=%u tracer_pool=%d tracers_recycled=%u"),
                    Fx->GetMarkPoolSize(), Fx->GetActiveMarkCount(), S.MaxActiveMarks, S.MarksRecycled, Fx->GetTracerPoolSize(), S.TracersRecycled));
        }
        if (Once(29, Time, 32.2)) { Capture(TEXT("marks_neutral")); }
        if (Once(30, Time, 33.3)) { Capture(TEXT("marks_dim")); }
        if (Once(31, Time, 34.0))
        {
            Fx->DevelopmentSetWorldMarkTiming(2.5f, 1.5f);
            BeforeRejoin = MarkStats(*Fx, Shooter);
        }
        if (!bShooter && Once(32, Time, 35.0))
        {
            // Disconnect and join again: a new world, subsystem and pawn copies while
            // the shooter's accepted-shot counter is far above zero.
            PreRejoinFx = Fx;
            PreRejoinWorld = World;
            bRejoinRequested = true;
            GEngine->SetClientTravel(World, *FString::Printf(TEXT("127.0.0.1:%d?Name=%s"), Port, ShotFxObserverName), TRAVEL_Absolute);
            return;
        }
        if (Once(33, Time, 42.0))
        {
            const uint32 ShotsNow = Shooter->GetRifleComponent()->GetState().ShotsFired;
            if (bShooter)
            {
                Check(TEXT("quiet_during_rejoin"), S.Effects == BeforeRejoin.Effects,
                    FString::Printf(TEXT("effects=%u/%u"), S.Effects, BeforeRejoin.Effects));
            }
            else
            {
                Check(TEXT("rejoin_no_replay"), bRejoinRequested && World != PreRejoinWorld.Get() && Fx != PreRejoinFx.Get()
                    && S.Effects == 0 && S.Tracers == 0 && S.WorldMarks == 0 && S.Baselines >= 2
                    && ShotsNow > static_cast<uint32>(UVSShotEffectsSubsystem::MaxMarks),
                    FString::Printf(TEXT("new_world=%d effects=%u baselines=%u shooter_shots=%u"), World != PreRejoinWorld.Get(),
                        S.Effects, S.Baselines, ShotsNow));
            }
        }
        if (Once(34, Time, 45.0))
        {
            Check(TEXT("no_unexpected_effects"), UnexpectedEffects == 0, FString::Printf(TEXT("unexpected=%u"), UnexpectedEffects));
        }
    }

    void CheckShot(int32 Index, bool bShooter)
    {
        FShotResult& Result = Results[Index];
        Result.bChecked = true;
        const FShotFxPlan& Plan = ShotFxPlan[Index];
        const FVSShotEffectRecord& Record = Result.Record;
        const bool bPlayer = FCString::Strcmp(Plan.Label, TEXT("player")) == 0;
        const EVSShotImpactKind Expected = bPlayer ? (bShooter ? EVSShotImpactKind::Character : EVSShotImpactKind::Self) : EVSShotImpactKind::World;
        const double MuzzleError = FVector::Dist(Record.TracerStart, Result.ExpectedMuzzleLocation);
        const bool bOrigin = Result.bExpectMuzzle
            ? Record.bMuzzleOrigin && Record.MuzzleComponent == Result.ExpectedMuzzle && MuzzleError < 6.0
            : !Record.bMuzzleOrigin && FVector::Dist(Record.TracerStart, Record.Eye) < 80.0;
        bool bTarget = Record.MarkSlot != INDEX_NONE && Result.HitRole == TEXT("world");
        if (bPlayer)
        {
            bTarget = Result.HitRole == TEXT("observer") && (bShooter ? Result.bMarkOnTarget
                : Record.MarkSlot == INDEX_NONE && FVector::Dist(Record.TracerEnd, Record.ImpactPoint) > 59.0);
        }
        Check(FString::Printf(TEXT("shot_%s"), Plan.Label), Result.bRecord && Result.Effects == 1 && Record.RepresentedShots == 1
            && Record.bTracer && Result.bTracerVisible && Record.Impact == Expected && bOrigin && bTarget,
            FString::Printf(TEXT("effects=%d represented=%u tracer=%d visible=%d kind=%s hit=%s muzzle=%d expected_muzzle=%d muzzle_error=%.2f point=%s"),
                Result.Effects, Record.RepresentedShots, Record.bTracer, Result.bTracerVisible, ShotFxKind(Record.Impact), *Result.HitRole,
                Record.bMuzzleOrigin, Result.bExpectMuzzle, Result.bExpectMuzzle ? MuzzleError : -1.0, *ShotFxVec(Record.ImpactPoint)));
        Shots.Add(FString::Printf(TEXT("{\"label\":\"%s\",\"effects\":%d,\"kind\":\"%s\",\"role\":\"%s\",\"point\":%s,\"eye\":%s,\"start\":%s,\"end\":%s,\"muzzle\":%s}"),
            Plan.Label, Result.Effects, ShotFxKind(Record.Impact), *Result.HitRole, *ShotFxVec(Record.ImpactPoint), *ShotFxVec(Record.Eye),
            *ShotFxVec(Record.TracerStart), *ShotFxVec(Record.TracerEnd), Record.bMuzzleOrigin ? TEXT("true") : TEXT("false")));
    }

    FString Role, Output;
    float Start = 45;
    int32 Port = 7871;
    FDelegateHandle Delegate;
    TArray<FString> Checks;
    TArray<FString> Shots;
    TSet<int32> Steps;
    FShotResult Results[ShotFxPlanCount];
    TWeakObjectPtr<UWorld> NamedWorld;
    TWeakObjectPtr<UVSShotEffectsSubsystem> CurrentFx;
    TWeakObjectPtr<UVSShotEffectsSubsystem> PreRejoinFx;
    TWeakObjectPtr<UWorld> PreRejoinWorld;
    FStatsMark AfterSingles, BeforeBurst, BeforeRejoin;
    uint32 SeenEffects = 0, BurstEffects = 0, UnexpectedEffects = 0;
    uint32 TrackedShots = 0, BurstServerShots = 0, UnexpectedServerShots = 0;
    uint32 FirstShooterId = 0, ObserverId = 0, TrackedShooterId = 0;
    int32 ObserverRejoins = 0;
    int32 ClickTicks = 0, ReloadTicks = 0;
    double LastClientTime = -1.0e9;
    double LastRealTick = 0.0, FirstEffectRealTime = 0.0, LongestFrameAfterFirstEffect = 0.0;
    bool bReady = false, bFinished = false, bFireHeld = false, bReloadHeld = false, bRejoinRequested = false;
};

TUniquePtr<FVSShotEffectsSmoke> ShotEffectsSmoke;
}
#endif

AVSShotEffectsTestGameMode::AVSShotEffectsTestGameMode()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (VSShotEffectsTestDetail::IsShotEffectsTest())
    {
        DefaultPawnClass = AVSShotEffectsTestCharacter::StaticClass();
        GameStateClass = AVSShotEffectsTestGameState::StaticClass();
    }
#endif
}

void AVSShotEffectsTestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!VSShotEffectsTestDetail::IsShotEffectsTest()) { ErrorMessage = TEXT("Shot effects fixture requires explicit development opt-in."); }
#else
    ErrorMessage = TEXT("Shot effects fixture is unavailable in Shipping/Test.");
#endif
}

void AVSShotEffectsTestCharacter::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    using namespace VSShotEffectsTestDetail;
    if (IsShotEffectsTest() && !ShotEffectsSmoke) { ShotEffectsSmoke = MakeUnique<FVSShotEffectsSmoke>(); }
#endif
}

double AVSShotEffectsTestGameState::EstimateServerSeconds() const
{
    if (ClockSamples.IsEmpty()) { return GetServerWorldTimeSeconds(); }
    double Offset = ClockSamples[0].Offset;
    for (const FClockSample& Sample : ClockSamples) { Offset = FMath::Min(Offset, Sample.Offset); }
    return FPlatformTime::Seconds() - Offset;
}

void AVSShotEffectsTestGameState::OnRep_ReplicatedWorldTimeSecondsDouble()
{
    Super::OnRep_ReplicatedWorldTimeSecondsDouble();
    const double Now = FPlatformTime::Seconds();
    ClockSamples.Add({Now - ReplicatedWorldTimeSecondsDouble, Now});
    ClockSamples.RemoveAll([Now](const FClockSample& Sample) { return Now - Sample.Received > 3.0; });
}
