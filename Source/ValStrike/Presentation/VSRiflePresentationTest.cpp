#include "VSRiflePresentationTest.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "VSRiflePresentationComponent.h"
#include "../VSPlayerController.h"
#include "../Combat/VSRifleComponent.h"
#include "../Combat/VSHealthComponent.h"
#include "../ValStrike.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "UnrealClient.h"

namespace
{
bool IsPresentationTest()
{
    FString Role;
    return FParse::Value(FCommandLine::Get(), TEXT("VSRiflePresentationTest="), Role)
        && (Role == TEXT("server") || Role == TEXT("owner") || Role == TEXT("observer"));
}

class FPresentationSmoke
{
public:
    FPresentationSmoke()
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSRiflePresentationTest="), Role);
        FParse::Value(FCommandLine::Get(), TEXT("VSRiflePresentationOutput="), Output);
        FParse::Value(FCommandLine::Get(), TEXT("VSRiflePresentationStart="), Start);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FPresentationSmoke::Tick);
    }
    ~FPresentationSmoke() { FWorldDelegates::OnWorldPostActorTick.Remove(Delegate); }
private:
    bool Once(int32 Step, double Time, double At)
    {
        if (Time < At || Steps.Contains(Step)) { return false; }
        Steps.Add(Step); return true;
    }
    void Check(const TCHAR* Name, bool bPassed, const FString& Detail)
    {
        Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"), Name,
            bPassed ? TEXT("true") : TEXT("false"), *Detail));
        UE_LOG(LogValStrike, Display, TEXT("VSPRESENT %s %s %s %s"), *Role, Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    }
    void Finish(const TCHAR* Error = nullptr)
    {
        bFinished = true;
        const FString Report = FString::Printf(TEXT("{\"schema_version\":1,\"role\":\"%s\",\"completed\":true,\"checks\":[%s],\"error\":%s}\n"),
            *Role, *FString::Join(Checks, TEXT(",")), Error ? *FString::Printf(TEXT("\"%s\""), Error) : TEXT("null"));
        if (FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp"))))
            IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
    }
    void Capture(const TCHAR* Name) const
    {
        FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Output, TEXT("")) + TEXT("_") + Name + TEXT(".png"), true, false);
    }
    static void Key(APlayerController* PC, FKey Name, bool Wanted, bool& Held)
    {
        if (Held == Wanted) { return; }
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, Wanted ? IE_Pressed : IE_Released, Wanted ? 1 : 0));
        Held = Wanted;
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
        Pawn->ForceNetUpdate(); return true;
    }
    static void Lighting(UWorld* World, bool bDim)
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
            It->GetLightComponent()->SetIntensity(bDim ? 0.35f : 3.0f);
        for (TActorIterator<ASkyLight> It(World); It; ++It)
            It->GetLightComponent()->SetIntensity(bDim ? 0.25f : 1.0f);
    }
    void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        if (bFinished || !World || !World->IsGameWorld() || !World->GetGameState()) { return; }
        const bool bServer = Role == TEXT("server");
        if ((bServer && World->GetNetMode() != NM_DedicatedServer) || (!bServer && World->GetNetMode() != NM_Client)) { return; }
        const double Time = World->GetGameState()->GetServerWorldTimeSeconds() - Start;
        if (!bServer && !bNamed)
        {
            if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->PlayerState)
            {
                PC->SetName(Role == TEXT("owner") ? TEXT("VSPresentationOwner") : TEXT("VSPresentationObserver"));
                bNamed = true;
            }
        }
        AVSCharacter* Owner = nullptr;
        AVSCharacter* Observer = nullptr;
        for (TActorIterator<AVSCharacter> It(World); It; ++It)
        {
            if (const APlayerState* PS = It->GetPlayerState())
            {
                if (PS->GetPlayerName() == TEXT("VSPresentationOwner")) { Owner = *It; }
                if (PS->GetPlayerName() == TEXT("VSPresentationObserver")) { Observer = *It; }
            }
        }
        AVSCharacter* Local = Role == TEXT("owner") ? Owner : Observer;
        AVSPlayerController* PC = Local ? Cast<AVSPlayerController>(Local->GetController()) : nullptr;
        if (!Owner || !Observer || (!bServer && (!PC || !Local->InputComponent)))
        {
            if (Time > 1 && !(Time > 21 && Time < 25)) { Finish(TEXT("players_not_ready")); }
            return;
        }
        if (!bReady)
        {
            bReady = true;
            if (bServer)
            {
                const bool Placed = Place(Owner, FVector(-1000, -1000, 90), FRotator::ZeroRotator)
                    && Place(Observer, FVector(-850, -1200, 90), FRotator(-10, 126.87, 0));
                Check(TEXT("two_players"), Placed && Owner != Observer && !Owner->FindComponentByClass<UVSRiflePresentationComponent>(), TEXT("two possessed pawns; no server visual components"));
                if (!Placed) { Finish(TEXT("placement_failed")); return; }
                FirstPawnId = Owner->GetUniqueID();
            }
        }
        UVSRiflePresentationComponent* Visual = nullptr;
        if (!bServer)
        {
            Visual = Owner->FindComponentByClass<UVSRiflePresentationComponent>();
            if (!Visual)
            {
                Visual = NewObject<UVSRiflePresentationComponent>(Owner);
                Owner->AddInstanceComponent(Visual);
                Visual->RegisterComponent();
                if (!Visual->Initialize(Owner, Owner->FindComponentByClass<UCameraComponent>(), Owner->GetMesh()))
                { Finish(TEXT("visual_attachment_failed")); return; }
                if (FirstPawnId == 0) { FirstPawnId = Owner->GetUniqueID(); }
            }
        }
        const auto& Gun = Owner->GetRifleComponent()->GetState();
        const auto Details = [&]() { return FString::Printf(TEXT("shots=%u pulses=%u ammo=%d reload=%d alive=%d"),
            Gun.ShotsFired, Visual ? Visual->GetShotPulseCount() : 0, Gun.Magazine, Gun.bReloading, Owner->GetHUDData().bAlive); };
        if (Role == TEXT("owner"))
        {
            Key(PC, EKeys::LeftMouseButton, (Time >= 3 && Time < 3.05) || (Time >= 10 && Time < 12.1) || (Time >= 16 && Time < 16.05), bFireHeld);
            Key(PC, EKeys::R, (Time >= 5 && Time < 5.1) || (Time >= 18 && Time < 18.1), bReloadHeld);
            Key(PC, EKeys::Escape, Time >= 10.6 && Time < 10.7, bEscapeHeld);
            if (Once(100, Time, 12.2)) { PC->SetSessionMenuOpen(false); }
        }
        if (!bServer)
        {
            if (Once(0, Time, 0.6))
            {
                const UStaticMeshComponent* FP = Visual->GetFirstPersonBody();
                const UStaticMeshComponent* WP = Visual->GetWorldBody();
                const UMaterialInterface* FlashMaterial = Visual->GetFirstPersonFlash()->GetMaterial(0);
                Check(TEXT("flash_material"), FlashMaterial && FlashMaterial == Visual->GetWorldFlash()->GetMaterial(0)
                    && FlashMaterial->GetPathName() == TEXT("/Game/Presentation/Rifle01/M_VSRifleFlash.M_VSRifleFlash")
                    && FlashMaterial->GetShadingModels().HasShadingModel(MSM_Unlit),
                    FlashMaterial ? FlashMaterial->GetPathName() : TEXT("missing"));
                Check(TEXT("attached_assets"), FP && WP && FP->GetStaticMesh()->GetName() == TEXT("SM_VS_Rifle01_Body")
                    && Visual->GetFirstPersonMagazine()->GetAttachSocketName() == TEXT("Magazine")
                    && FP->DoesSocketExist(TEXT("Grip")) && FP->DoesSocketExist(TEXT("Muzzle")) && FP->DoesSocketExist(TEXT("SupportHand")), Details());
                Check(TEXT("no_collision"), FP->GetCollisionEnabled() == ECollisionEnabled::NoCollision
                    && WP->GetCollisionEnabled() == ECollisionEnabled::NoCollision
                    && Visual->GetFirstPersonMagazine()->GetCollisionEnabled() == ECollisionEnabled::NoCollision
                    && Visual->GetWorldMagazine()->GetCollisionEnabled() == ECollisionEnabled::NoCollision, Details());
                Check(TEXT("visibility"), FP->IsVisible() == (Role == TEXT("owner")) && WP->IsVisible() == (Role == TEXT("observer"))
                    && FP->bOnlyOwnerSee && WP->bOwnerNoSee, Details());
                Check(TEXT("neutral_view"), Visual->IsInitialized() && !Visual->IsFlashActive(), Details());
                AimBefore = PC->GetControlRotation();
                Capture(TEXT("neutral"));
            }
            if (Once(1, Time, 1.2)) { Lighting(World, true); }
            if (Once(2, Time, 2.0)) { Check(TEXT("dim_view"), Visual->IsInitialized(), Details()); Capture(TEXT("dim")); }
            if (Once(3, Time, 2.6)) { Lighting(World, false); }
            if (Gun.ShotsFired == 1 && Visual->IsFlashActive() && !bCapturedMuzzle)
            {
                bCapturedMuzzle = true;
                const UMaterialInterface* FlashMaterial = Visual->GetFirstPersonFlash()->GetMaterial(0);
                const FMaterialResource* Shader = FlashMaterial && World->Scene
                    ? FlashMaterial->GetMaterialResource(World->Scene->GetShaderPlatform()) : nullptr;
                Check(TEXT("flash_shader_ready"), Shader && Shader->GetGameThreadShaderMap()
                    && Shader->IsGameThreadShaderMapComplete() && Shader->GetCompileErrors().IsEmpty(),
                    FString::Printf(TEXT("resource=%d map=%d complete=%d errors=%d"), Shader != nullptr, Shader && Shader->GetGameThreadShaderMap(),
                        Shader && Shader->IsGameThreadShaderMapComplete(), Shader ? Shader->GetCompileErrors().Num() : -1));
                Capture(TEXT("muzzle"));
            }
            if (Once(4, Time, 4.2))
            {
                Check(TEXT("accepted_shot_once"), Gun.ShotsFired == 1 && Visual->GetShotPulseCount() == 1 && bCapturedMuzzle, Details());
                Check(TEXT("aim_unchanged"), PC->GetControlRotation().Equals(AimBefore, 0.001), Details());
            }
            if (Once(5, Time, 4.7)) { Check(TEXT("duplicate_snapshot_quiet"), Visual->GetShotPulseCount() == 1 && !Visual->IsFlashActive(), Details()); }
            if (Once(6, Time, 5.8))
            {
                const UStaticMeshComponent* Mag = Role == TEXT("owner") ? Visual->GetFirstPersonMagazine() : Visual->GetWorldMagazine();
                Check(TEXT("reload_pose"), Gun.bReloading && Visual->IsReloadPoseActive() && Mag->GetRelativeLocation().Z < -10, Details()); Capture(TEXT("reload"));
            }
        }
        if (Once(7, Time, 7.7)) { Check(TEXT("reload_completed"), !Gun.bReloading && Gun.Magazine == 30 && Gun.Reserve == 89
            && (bServer || (!Visual->IsReloadPoseActive() && Visual->GetFirstPersonMagazine()->GetRelativeLocation().IsNearlyZero())), Details()); }
        if (bServer && Once(8, Time, 4.2)) { Check(TEXT("authoritative_shot"), Gun.ShotsFired == 1 && Gun.Magazine == 29, Details()); }
        if (Once(9, Time, 11.1))
        {
            MenuShots = Gun.ShotsFired;
            MenuPulses = Visual ? Visual->GetShotPulseCount() : 0;
            if (Role == TEXT("owner")) { Check(TEXT("menu_visible"), PC->IsSessionMenuOpen(), Details()); Capture(TEXT("menu")); }
        }
        if (Once(10, Time, 12)) { Check(TEXT("menu_release"), MenuShots > 1 && Gun.ShotsFired == MenuShots
            && (bServer || (Visual->GetShotPulseCount() == MenuPulses && !Visual->IsFlashActive())), Details()); }
        if (!bServer && Once(11, Time, 14))
        {
            if (!Visual->Initialize(Owner, Owner->FindComponentByClass<UCameraComponent>(), Owner->GetMesh()))
            { Finish(TEXT("reattachment_failed")); return; }
        }
        if (!bServer && Once(12, Time, 15)) { Check(TEXT("late_attach_no_replay"), Gun.ShotsFired > 1 && Visual->GetShotPulseCount() == 0
            && Visual->GetReloadStartCount() == 0 && !Visual->IsFlashActive(), Details()); }
        if (bServer && Once(13, Time, 18.5))
        {
            bDiedDuringReload = Gun.bReloading;
            Owner->GetHealthComponent()->ApplyPointDamage(100);
        }
        if (Once(14, Time, 20.5))
        {
            if (bServer) { Check(TEXT("death"), bDiedDuringReload && !Owner->GetHUDData().bAlive && !Gun.bReloading, Details()); }
            else
            {
                Check(TEXT("death_cleanup"), !Owner->GetHUDData().bAlive && !Visual->GetFirstPersonBody()->IsVisible()
                    && !Visual->GetWorldBody()->IsVisible() && !Visual->IsFlashActive() && !Visual->IsReloadPoseActive(), Details());
                Capture(TEXT("death"));
            }
        }
        if (bServer && Once(15, Time, 22))
        {
            AController* Controller = Owner->GetController();
            Controller->UnPossess();
            Owner->Destroy();
            World->GetAuthGameMode()->RestartPlayer(Controller);
            if (!Place(Cast<AVSCharacter>(Controller->GetPawn()), FVector(-1000, -1000, 90), FRotator::ZeroRotator))
            { Finish(TEXT("respawn_placement_failed")); return; }
        }
        if (Once(16, Time, 24.5)) { Check(TEXT("new_pawn"), Owner->GetUniqueID() != FirstPawnId && Owner->GetHUDData().bAlive && Gun.ShotsFired == 0
            && Gun.Magazine == 30 && (bServer || (Visual->GetShotPulseCount() == 0 && !Visual->IsReloadPoseActive() && !Visual->IsFlashActive())), Details()); }
        if (Time >= 26) { Finish(); }
    }
    FString Role, Output;
    float Start = 30;
    FDelegateHandle Delegate;
    TArray<FString> Checks;
    TSet<int32> Steps;
    uint32 FirstPawnId = 0;
    FRotator AimBefore;
    uint32 MenuShots = 0, MenuPulses = 0;
    bool bNamed = false, bReady = false, bFinished = false, bFireHeld = false, bReloadHeld = false, bEscapeHeld = false;
    bool bCapturedMuzzle = false, bDiedDuringReload = false;
};
TUniquePtr<FPresentationSmoke> PresentationSmoke;
}
#endif

AVSRiflePresentationTestGameMode::AVSRiflePresentationTestGameMode()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (IsPresentationTest()) { DefaultPawnClass = AVSRiflePresentationTestCharacter::StaticClass(); }
#endif
}

void AVSRiflePresentationTestGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!IsPresentationTest()) { ErrorMessage = TEXT("Presentation fixture requires explicit development opt-in."); }
#else
    ErrorMessage = TEXT("Presentation fixture is unavailable in Shipping/Test.");
#endif
}

void AVSRiflePresentationTestCharacter::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (IsPresentationTest() && !PresentationSmoke) { PresentationSmoke = MakeUnique<FPresentationSmoke>(); }
#endif
}
