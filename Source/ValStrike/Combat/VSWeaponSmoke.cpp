#include "VSWeaponSmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "VSHealthComponent.h"
#include "VSKnifeComponent.h"
#include "VSTestServerClock.h"
#include "VSWeaponComponent.h"
#include "VSWeaponLoadoutComponent.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "../Match/VSMatchGameState.h"
#include "../Presentation/VSKnifePresentationEvents.h"
#include "../Presentation/VSWeaponPresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
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
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
/**
 * Opt-in server/driver/observer weapon switching harness.
 * practice: graybox, practice game mode; dense equip validation, ammo, damage and
 *   model visibility timeline, then the knife (KNIFE-01) from 13 s: equip 3/Q,
 *   slash/stab hit, miss, wall, raw rejections and lifecycle cancellation.
 *   Run with null-RHI clients (no rendering stalls).
 * visual: graybox, rendered clients; spaced first-person/observer captures.
 * match: Site01, match game mode; preparation gate, combat and a real round reset.
 * Ordinary actions go through PlayerController -> Enhanced Input; raw RPCs are
 * used only to prove server-side rejection of requests a normal client never sends.
 * visual/match use Eventually() checks and seconds of slack between actions, so a
 * rendered client's stall (screenshot readback, first-draw pipeline compiles)
 * delays the sequence instead of breaking it.
 */
class FVSWeaponSmoke
{
public:
    explicit FVSWeaponSmoke(FString InRole) : Role(MoveTemp(InRole))
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSWeaponSmokeStart="), StartTime);
        FParse::Value(FCommandLine::Get(), TEXT("VSWeaponOutput="), Output);
        FParse::Value(FCommandLine::Get(), TEXT("VSWeaponScenario="), Scenario);
        bMatch = Scenario == TEXT("match");
        if (Scenario != TEXT("match") && Scenario != TEXT("visual")) { Scenario = TEXT("practice"); }
        bCapture = FParse::Param(FCommandLine::Get(), TEXT("VSWeaponCapture"));
        // Open floor on each map. The observer stands front-left of the driver and
        // looks back at it, so the driver's held weapon is seen side-on.
        Base = bMatch ? FVector(850, -150, 100) : FVector(-1000, -1000, 90);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSWeaponSmoke::Tick);
    }
    ~FVSWeaponSmoke() { FWorldDelegates::OnWorldPostActorTick.Remove(Delegate); }

private:
    static constexpr uint8 RifleRequest = static_cast<uint8>(EVSEquipRequest::Rifle);
    static constexpr uint8 PistolRequest = static_cast<uint8>(EVSEquipRequest::Pistol);

    // Detail is read after every argument is evaluated, so a condition helper may fill it.
    void Check(const TCHAR* Name, bool bPassed, const FString& InDetail)
    {
        FString Detail = InDetail;
        Detail.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Detail.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"),
            Name, bPassed ? TEXT("true") : TEXT("false"), *Detail));
        UE_LOG(LogValStrike, Display, TEXT("VSWEAPON %s %s: %s (%s)"), *Role, Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    }

    void Finish(const TCHAR* Error = nullptr)
    {
        bFinished = true;
        KnifeCues.Unbind();
        // Frame pacing is diagnostic context for timing-sensitive failures.
        const FString Report = FString::Printf(
            TEXT("{\"schema_version\":1,\"role\":\"%s\",\"scenario\":\"%s\",\"completed\":true,\"frames\":{\"count\":%d,\"mean_ms\":%.1f,\"max_ms\":%.1f},\"checks\":[%s],\"error\":%s}\n"),
            *Role, *Scenario, FrameCount, FrameCount ? FrameSeconds * 1000.0 / FrameCount : 0.0,
            MaxFrameSeconds * 1000.0, *FString::Join(Checks, TEXT(",")),
            Error ? *FString::Printf(TEXT("\"%s\""), Error) : TEXT("null"));
        if (!Output.IsEmpty() && FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp"))))
        {
            IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
        }
    }

    void Capture(const TCHAR* Name) const
    {
        if (bCapture && Role != TEXT("server"))
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Output, TEXT("")) + TEXT("_") + Name + TEXT(".png"), true, false);
        }
    }

    static void Key(APlayerController* PC, FKey Name, bool bWanted, bool& bHeld)
    {
        if (PC && bHeld != bWanted)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, bWanted ? IE_Pressed : IE_Released, bWanted ? 1.0f : 0.0f));
            bHeld = bWanted;
        }
    }

    // One-frame press: the release is processed in the next frame's input pass,
    // before the automatic fire timer can tick, so a tap is exactly one request
    // however long the frames are.
    void Tap(APlayerController* PC, FKey Name)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, IE_Pressed, 1.0f));
        PendingRelease.Add(Name);
    }

    void ReleaseTaps(APlayerController* PC)
    {
        for (const FKey& Name : PendingRelease) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, IE_Released, 0.0f)); }
        PendingRelease.Reset();
    }

    bool Once(int32 Id, double Time, double At)
    {
        if (Time < At || Steps.Contains(Id)) { return false; }
        Steps.Add(Id);
        return true;
    }

    // Passes as soon as the condition holds at or after From; fails at Until.
    // Returns true only on the tick that recorded a pass.
    bool Eventually(int32 Id, const TCHAR* Name, double Time, double From, double Until, bool bCondition, const FString& Detail)
    {
        if (Time < From || Steps.Contains(Id) || (!bCondition && Time < Until)) { return false; }
        Steps.Add(Id);
        Check(Name, bCondition, Detail);
        return bCondition;
    }

    static bool Place(AVSCharacter* Pawn, FVector Location, FRotator Rotation)
    {
        if (!Pawn) { return false; }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        if (!Pawn->TeleportTo(Location, Rotation)) { return false; }
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            PC->SetControlRotation(Rotation);
            PC->ClientSetLocation(Pawn->GetActorLocation(), Rotation);
            PC->ClientSetRotation(Rotation);
        }
        Pawn->ForceNetUpdate();
        return true;
    }

    bool PlacePair(AVSCharacter* Driver, AVSCharacter* Target) const
    {
        return Place(Driver, Base, FRotator::ZeroRotator) && Place(Target, Base + FVector(150, -200, 0), ObserverView);
    }

    static FString Detail(const AVSCharacter* Driver, const AVSCharacter* Target)
    {
        const UVSWeaponComponent* R = Driver->GetWeapon(EVSWeaponSlot::Rifle);
        const UVSWeaponComponent* P = Driver->GetWeapon(EVSWeaponSlot::Pistol);
        const FVSLoadoutState& L = Driver->GetLoadoutComponent()->GetState();
        return FString::Printf(TEXT("rifle=%d/%d shots=%u reload=%d pistol=%d/%d shots=%u reload=%d equipped=%d equipping=%d switches=%u target=%.1f alive=%d"),
            R->GetState().Magazine, R->GetState().Reserve, R->GetState().ShotsFired, R->GetState().bReloading,
            P->GetState().Magazine, P->GetState().Reserve, P->GetState().ShotsFired, P->GetState().bReloading,
            static_cast<int32>(L.Equipped), L.bEquipping, L.EquipCount,
            Target ? Target->GetHealthComponent()->GetHealth() : -1.0f, Target ? Target->GetHealthComponent()->IsAlive() : 0);
    }

    static bool Full(const AVSCharacter* Pawn)
    {
        const FVSRifleState& R = Pawn->GetWeapon(EVSWeaponSlot::Rifle)->GetState();
        const FVSRifleState& P = Pawn->GetWeapon(EVSWeaponSlot::Pistol)->GetState();
        const FVSLoadoutState& L = Pawn->GetLoadoutComponent()->GetState();
        return R.Magazine == 30 && R.Reserve == 90 && R.ShotsFired == 0 && !R.bReloading
            && P.Magazine == 12 && P.Reserve == 36 && P.ShotsFired == 0 && !P.bReloading
            && L.Equipped == EVSWeaponSlot::Rifle && !L.bEquipping && Pawn->GetHealthComponent()->GetHealth() == 100;
    }

    static bool Equipped(const AVSCharacter* Pawn, EVSWeaponSlot Slot, bool bEquipping)
    {
        const FVSLoadoutState& L = Pawn->GetLoadoutComponent()->GetState();
        return L.Equipped == Slot && L.bEquipping == bEquipping;
    }

    static FString ViewDetail(const UVSWeaponPresentationComponent* View)
    {
        return FString::Printf(TEXT("slot=%d init=%d shown=%d fp=%d world=%d blend=%.2f pulses=%u"), static_cast<int32>(View->GetSlot()),
            View->IsInitialized(), View->IsShown(), View->GetFirstPersonBody() && View->GetFirstPersonBody()->IsVisible(),
            View->GetWorldBody() && View->GetWorldBody()->IsVisible(), View->GetEquipBlend(), View->GetShotPulseCount());
    }

    // The driver's equipped model is shown in the right view (first person on the
    // driver, world model elsewhere) and the stowed model is hidden in both.
    bool ModelsShow(const AVSCharacter* Driver, EVSWeaponSlot Slot, FString& Out) const
    {
        const bool bOwnerView = Role == TEXT("driver");
        bool bOk = true;
        Out.Reset();
        for (const EVSWeaponSlot Each : {EVSWeaponSlot::Rifle, EVSWeaponSlot::Pistol})
        {
            const UVSWeaponPresentationComponent* View = Driver->GetWeaponPresentation(Each);
            if (!View || !View->IsInitialized()) { Out += TEXT("missing "); bOk = false; continue; }
            const bool bShown = Each == Slot;
            const bool bFP = View->GetFirstPersonBody()->IsVisible();
            const bool bWorld = View->GetWorldBody()->IsVisible();
            bOk &= View->IsShown() == bShown && bFP == (bShown && bOwnerView) && bWorld == (bShown && !bOwnerView)
                && View->GetFirstPersonBody()->bOnlyOwnerSee && View->GetWorldBody()->bOwnerNoSee;
            Out += ViewDetail(View) + TEXT("; ");
        }
        return bOk;
    }

    static bool NoCollision(const UVSWeaponPresentationComponent* View)
    {
        for (const UStaticMeshComponent* Part : {View->GetFirstPersonBody(), View->GetWorldBody(), View->GetFirstPersonMagazine(),
            View->GetWorldMagazine(), View->GetFirstPersonFlash(), View->GetWorldFlash(), View->GetFirstPersonSlide(), View->GetWorldSlide()})
        {
            if (Part && (Part->GetCollisionEnabled() != ECollisionEnabled::NoCollision || Part->CanEverAffectNavigation())) { return false; }
        }
        return true;
    }

    void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        if (bFinished || !World || !World->IsGameWorld() || !World->GetGameState()) { return; }
        const bool bServer = Role == TEXT("server");
        if ((bServer && World->GetNetMode() != NM_DedicatedServer) || (!bServer && World->GetNetMode() != NM_Client)) { return; }
        const double Time = Clock.Now(World) - StartTime;
        ++FrameCount;
        FrameSeconds += DeltaSeconds;
        MaxFrameSeconds = FMath::Max(MaxFrameSeconds, static_cast<double>(DeltaSeconds));
        if (!bServer && !bNameRequested)
        {
            APlayerController* PC = World->GetFirstPlayerController();
            if (PC && PC->IsLocalController() && PC->PlayerState)
            {
                PC->SetName(Role == TEXT("driver") ? TEXT("VSWeaponDriver") : TEXT("VSWeaponObserver"));
                bNameRequested = true;
            }
        }
        AVSCharacter* Driver = nullptr;
        AVSCharacter* Target = nullptr;
        for (TActorIterator<AVSCharacter> It(World); It; ++It)
        {
            const APlayerState* PS = It->GetPlayerState();
            if (PS && PS->GetPlayerName() == TEXT("VSWeaponDriver")) { Driver = *It; }
            if (PS && PS->GetPlayerName() == TEXT("VSWeaponObserver")) { Target = *It; }
        }
        if (bMatch) { TickMatch(World, Time, Driver, Target); }
        else if (Scenario == TEXT("visual")) { TickVisual(World, Time, Driver, Target); }
        else { TickPractice(World, Time, Driver, Target); }
    }

    APlayerController* LocalController(AVSCharacter* Driver, AVSCharacter* Target)
    {
        AVSCharacter* Local = Role == TEXT("driver") ? Driver : Role == TEXT("observer") ? Target : nullptr;
        APlayerController* PC = Local ? Cast<APlayerController>(Local->GetController()) : nullptr;
        if (!PC || !PC->IsLocalController() || !Local->InputComponent) { return nullptr; }
        if (!bLookIsolated)
        {
            // Rendered test windows can take focus and receive real OS input (a
            // click fires, motion turns the view). Scripted clients ignore
            // viewport input; their keys go straight to the controller and
            // their aim is set directly.
            if (UGameViewportClient* Viewport = PC->GetWorld()->GetGameViewport()) { Viewport->SetIgnoreInput(true); }
            PC->SetIgnoreLookInput(true);
            bLookIsolated = true;
        }
        return PC;
    }

    FRotator AimAt(const AVSCharacter* Driver, const AVSCharacter* Target, bool bHead) const
    {
        // Body shots aim at the chest (upper-torso zone, full weapon damage); head at the head zone.
        return (Target->GetActorLocation() + FVector(0, 0, bHead ? 72 : 40) - Driver->GetPawnViewLocation()).Rotation();
    }

    void TickPractice(UWorld* World, double Time, AVSCharacter* Driver, AVSCharacter* Target)
    {
        const bool bServer = Role == TEXT("server");
        APlayerController* PC = LocalController(Driver, Target);
        // The target is respawned between 11.5 and 13; tolerate that gap.
        if (!Driver || !Target || (!bServer && !PC))
        {
            if (Time > 1.0 && !(Time > 11.4 && Time < 13.0)) { Finish(TEXT("players_not_ready")); }
            return;
        }
        UVSWeaponComponent* Rifle = Driver->GetWeapon(EVSWeaponSlot::Rifle);
        UVSWeaponComponent* Pistol = Driver->GetWeapon(EVSWeaponSlot::Pistol);
        UVSWeaponLoadoutComponent* Loadout = Driver->GetLoadoutComponent();
        const auto State = [&]() { return Detail(Driver, Target); };
        if (!bReady)
        {
            bReady = true;
            if (bServer && !PlacePair(Driver, Target)) { Finish(TEXT("placement_failed")); return; }
        }

        if (Role == TEXT("driver"))
        {
            const bool bAim = (Time >= 2.2 && Time < 3.1) || (Time >= 3.3 && Time < 4.2) || (Time >= 4.9 && Time < 6.3);
            PC->SetControlRotation(Time >= 13.0 ? KnifeAim(Driver, Target)
                : bAim ? AimAt(Driver, Target, false) : FRotator::ZeroRotator);
            ReleaseTaps(PC);
            if (Once(120, Time, 1.0)) { Tap(PC, EKeys::Two); }
            if (Once(121, Time, 1.2)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(122, Time, 3.6)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(123, Time, 3.9)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(124, Time, 4.5)) { Tap(PC, EKeys::R); }
            if (Once(125, Time, 5.0)) { Tap(PC, EKeys::One); }
            if (Once(126, Time, 6.0)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(127, Time, 7.0)) { Tap(PC, EKeys::Q); }
            if (Once(128, Time, 9.3)) { Tap(PC, EKeys::Two); }
            // Holds: a semi-automatic press held for 0.5 s, then automatic fire held during equip.
            Key(PC, EKeys::LeftMouseButton, (Time >= 2.5 && Time < 3.0) || (Time >= 5.1 && Time < 5.35), bFireHeld);
            if (Once(100, Time, 1.15))
            {
                // Raw requests during equip: the server must refuse both weapons.
                const FVector Forward = Driver->GetBaseAimRotation().Vector();
                Pistol->DevelopmentRequest(1, Driver->GetPawnViewLocation(), Forward);
                Rifle->DevelopmentRequest(1, Driver->GetPawnViewLocation(), Forward);
            }
            if (Once(101, Time, 7.8)) { Loadout->DevelopmentRequestEquip(PistolRequest); }
            if (Once(102, Time, 7.9))
            {
                Loadout->DevelopmentRequestEquip(RifleRequest);
                Loadout->DevelopmentRequestEquip(PistolRequest);
                Loadout->DevelopmentRequestEquip(7);
            }
            if (Once(103, Time, 9.3)) { Loadout->DevelopmentRequestEquip(PistolRequest); }
        }
        if (Role == TEXT("observer"))
        {
            PC->SetControlRotation(ObserverView);
            ReleaseTaps(PC);
            if (Once(129, Time, 10.7)) { Tap(PC, EKeys::Two); }
            if (Once(110, Time, 10.6)) { Target->GetLoadoutComponent()->DevelopmentRequestEquip(PistolRequest); }
        }

        if (Once(0, Time, 0.6))
        {
            const FVSPlayerHUDData D = Driver->GetHUDData();
            Check(TEXT("initial_loadout"), Full(Driver) && Full(Target), State());
            Check(TEXT("hud_rifle"), D.bHasWeapon && D.WeaponName == TEXT("RIFLE") && !D.bEquipping && D.MagazineAmmo == 30
                && D.MagazineCapacity == 30 && D.ReserveAmmo == 90, D.WeaponName);
            if (bServer)
            {
                bool bNoVisuals = true;
                for (const AVSCharacter* Pawn : {Driver, Target})
                {
                    for (const EVSWeaponSlot Slot : {EVSWeaponSlot::Rifle, EVSWeaponSlot::Pistol})
                    {
                        const UVSWeaponPresentationComponent* View = Pawn->GetWeaponPresentation(Slot);
                        bNoVisuals &= View && !View->IsInitialized() && !View->GetFirstPersonBody() && !View->GetWorldBody();
                    }
                }
                Check(TEXT("server_no_visuals"), bNoVisuals, TEXT("dedicated server keeps both weapon views uninitialized"));
            }
            else
            {
                FString Views;
                Check(TEXT("rifle_models"), ModelsShow(Driver, EVSWeaponSlot::Rifle, Views), Views);
                const UVSWeaponPresentationComponent* PistolView = Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol);
                Check(TEXT("pistol_assets"), PistolView && PistolView->GetFirstPersonBody()
                    && PistolView->GetFirstPersonBody()->GetStaticMesh()->GetName() == TEXT("SM_VS_Pistol02_Body")
                    && PistolView->GetFirstPersonSlide() && PistolView->GetFirstPersonSlide()->GetStaticMesh()->GetName() == TEXT("SM_VS_Pistol02_Slide")
                    && PistolView->GetFirstPersonSlide()->GetAttachSocketName() == TEXT("Slide")
                    && PistolView->GetFirstPersonMagazine()->GetStaticMesh()->GetName() == TEXT("SM_VS_Pistol02_Magazine")
                    && PistolView->GetFirstPersonMagazine()->GetAttachSocketName() == TEXT("Magazine")
                    && PistolView->GetFirstPersonFlash()->GetAttachSocketName() == TEXT("Muzzle"), ViewDetail(PistolView));
                Check(TEXT("no_collision"), NoCollision(Driver->GetWeaponPresentation(EVSWeaponSlot::Rifle)) && NoCollision(PistolView),
                    TEXT("weapon views use NoCollision and never affect navigation"));
            }
        }
        if (Once(2, Time, 1.3))
        {
            const FVSPlayerHUDData D = Driver->GetHUDData();
            Check(TEXT("equip_started"), Equipped(Driver, EVSWeaponSlot::Pistol, true) && Loadout->GetState().EquipCount == 1
                && Rifle->GetState().Magazine == 30 && Pistol->GetState().Magazine == 12, State());
            Check(TEXT("hud_pistol_equipping"), D.WeaponName == TEXT("PISTOL") && D.bEquipping && D.MagazineAmmo == 12
                && D.MagazineCapacity == 12 && D.ReserveAmmo == 36, D.WeaponName);
            if (!bServer)
            {
                FString Views;
                const UVSWeaponPresentationComponent* PistolView = Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol);
                Check(TEXT("pistol_raising"), ModelsShow(Driver, EVSWeaponSlot::Pistol, Views) && PistolView->GetEquipBlend() < 1.0f, Views);
            }
        }
        if (Once(3, Time, 1.9))
        {
            Check(TEXT("equip_blocks_fire"), Equipped(Driver, EVSWeaponSlot::Pistol, false) && Pistol->GetState().ShotsFired == 0
                && Pistol->GetState().Magazine == 12 && Rifle->GetState().ShotsFired == 0 && Target->GetHealthComponent()->GetHealth() == 100, State());
            if (bServer)
            {
                Check(TEXT("reject_equip_time"), Pistol->DevelopmentRejections(TEXT("equip")) >= 1, TEXT("raw pistol request during equip"));
                Check(TEXT("reject_holstered"), Rifle->DevelopmentRejections(TEXT("holstered")) >= 1, TEXT("raw rifle request after switching away"));
            }
            else
            {
                FString Views;
                Check(TEXT("pistol_models"), ModelsShow(Driver, EVSWeaponSlot::Pistol, Views)
                    && Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol)->GetEquipBlend() == 1.0f, Views);
            }
        }
        if (Once(5, Time, 3.2))
        {
            Check(TEXT("pistol_semi_auto"), Pistol->GetState().ShotsFired == 1 && Pistol->GetState().Magazine == 11
                && Pistol->GetState().Reserve == 36 && Target->GetHealthComponent()->GetHealth() == 80 && Rifle->GetState().ShotsFired == 0, State());
            if (!bServer)
            {
                const UVSWeaponPresentationComponent* View = Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol);
                Check(TEXT("pistol_shot_pulse"), View->GetShotPulseCount() == 1
                    && Driver->GetWeaponPresentation(EVSWeaponSlot::Rifle)->GetShotPulseCount() == 0, ViewDetail(View));
            }
        }
        if (Once(6, Time, 4.3))
        {
            Check(TEXT("pistol_damage"), Pistol->GetState().ShotsFired == 3 && Pistol->GetState().Magazine == 9
                && Target->GetHealthComponent()->GetHealth() == 40, State());
        }
        if (Once(7, Time, 4.8)) { Check(TEXT("pistol_reload_started"), Pistol->GetState().bReloading && Pistol->GetState().Magazine == 9, State()); }
        if (Once(9, Time, 5.4))
        {
            Check(TEXT("switch_cancels_reload"), Equipped(Driver, EVSWeaponSlot::Rifle, true) && !Pistol->GetState().bReloading
                && Pistol->GetState().Magazine == 9 && Pistol->GetState().Reserve == 36, State());
        }
        if (Once(10, Time, 6.6))
        {
            const FVSPlayerHUDData D = Driver->GetHUDData();
            // One rifle shot from the tap after equip; the held fire during equip sent nothing.
            Check(TEXT("per_weapon_ammo"), Equipped(Driver, EVSWeaponSlot::Rifle, false) && Rifle->GetState().ShotsFired == 1
                && Rifle->GetState().Magazine == 29 && Rifle->GetState().Reserve == 90 && Pistol->GetState().ShotsFired == 3
                && Pistol->GetState().Magazine == 9 && Pistol->GetState().Reserve == 36 && Target->GetHealthComponent()->GetHealth() == 10, State());
            Check(TEXT("hud_rifle_after_switch"), D.WeaponName == TEXT("RIFLE") && !D.bEquipping && D.MagazineAmmo == 29
                && D.MagazineCapacity == 30 && D.ReserveAmmo == 90, D.WeaponName);
            if (!bServer)
            {
                FString Views;
                Check(TEXT("rifle_models_after_switch"), ModelsShow(Driver, EVSWeaponSlot::Rifle, Views)
                    && Driver->GetWeaponPresentation(EVSWeaponSlot::Rifle)->GetShotPulseCount() == 1
                    && Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol)->GetShotPulseCount() == 3, Views);
            }
        }
        if (Once(12, Time, 7.4))
        {
            Check(TEXT("equip_last"), Equipped(Driver, EVSWeaponSlot::Pistol, true) && Loadout->GetState().EquipCount == 3
                && Pistol->GetState().Magazine == 9 && Pistol->GetState().Reserve == 36, State());
            if (!bServer)
            {
                const UVSWeaponPresentationComponent* View = Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol);
                Check(TEXT("no_replay_on_reequip"), View->IsShown() && View->GetShotPulseCount() == 3 && !View->IsFlashActive(), ViewDetail(View));
            }
        }
        if (Once(13, Time, 8.7))
        {
            Check(TEXT("raw_equip_validation"), Equipped(Driver, EVSWeaponSlot::Rifle, false) && Loadout->GetState().EquipCount == 4, State());
            if (bServer)
            {
                Check(TEXT("reject_duplicate"), Loadout->DevelopmentRejections(TEXT("duplicate")) >= 1, TEXT("pistol requested while equipped"));
                Check(TEXT("reject_spam"), Loadout->DevelopmentRejections(TEXT("spam")) >= 1, TEXT("second switch in the same frame"));
                Check(TEXT("reject_invalid"), Loadout->DevelopmentRejections(TEXT("invalid")) >= 1, TEXT("raw request byte 7"));
            }
        }
        if (bServer && Once(14, Time, 9.0)) { Driver->SetMatchGates(true, false); }
        if (Once(15, Time, 9.7))
        {
            Check(TEXT("gate_blocks_equip"), Equipped(Driver, EVSWeaponSlot::Rifle, false) && Loadout->GetState().EquipCount == 4
                && !Driver->CanMatchCombat(), State());
            if (bServer) { Check(TEXT("reject_phase"), Loadout->DevelopmentRejections(TEXT("phase")) >= 1, TEXT("raw request with combat gate closed")); }
        }
        if (bServer && Once(16, Time, 9.8)) { Driver->SetMatchGates(true, true); }
        if (bServer && Once(17, Time, 10.2)) { Target->GetHealthComponent()->ApplyPointDamage(1000); }
        if (Once(18, Time, 11.2))
        {
            Check(TEXT("dead_blocks_equip"), !Target->GetHealthComponent()->IsAlive() && Equipped(Target, EVSWeaponSlot::Rifle, false)
                && Target->GetLoadoutComponent()->GetState().EquipCount == 0, Detail(Target, Driver));
            if (bServer) { Check(TEXT("reject_dead"), Target->GetLoadoutComponent()->DevelopmentRejections(TEXT("dead")) >= 1, TEXT("raw request from a dead owner")); }
            if (!bServer)
            {
                bool bHidden = true;
                for (const EVSWeaponSlot Slot : {EVSWeaponSlot::Rifle, EVSWeaponSlot::Pistol})
                {
                    bHidden &= !Target->GetWeaponPresentation(Slot)->IsShown();
                }
                Check(TEXT("dead_models_hidden"), bHidden, TEXT("both weapon views hidden on death"));
            }
            OldTargetId = Target->GetUniqueID();
        }
        if (bServer && Once(19, Time, 11.5))
        {
            AController* Controller = Target->GetController();
            Controller->UnPossess();
            Target->Destroy();
            World->GetAuthGameMode()->RestartPlayer(Controller);
            if (!Place(Cast<AVSCharacter>(Controller->GetPawn()), Base + FVector(150, -200, 0), ObserverView))
            {
                Finish(TEXT("respawn_placement_failed")); return;
            }
        }
        if (Once(20, Time, 13.0))
        {
            Check(TEXT("fresh_pawn_loadout"), Target->GetUniqueID() != OldTargetId && Full(Target), Detail(Target, Driver));
        }
        if (Steps.Contains(20)) { TickKnife(World, Time, Driver, Target, PC); }
    }

    // Straight at the point the server's knife probe aims for: the target's
    // capsule axis at the driver's eye height (clamped to the axis top).
    static FRotator KnifeAim(const AVSCharacter* Driver, const AVSCharacter* Target)
    {
        return (Target->GetActorLocation() + FVector(0, 0, 54) - Driver->GetPawnViewLocation()).Rotation();
    }

    static FString KnifeDetail(const AVSCharacter* Driver, const AVSCharacter* Target)
    {
        const FVSKnifeState& K = Driver->GetKnifeComponent()->GetState();
        const FVSLoadoutState& L = Driver->GetLoadoutComponent()->GetState();
        return FString::Printf(TEXT("attacks=%u impacts=%u last=%d cancels=%u equipped=%d previous=%d equipping=%d switches=%u driver=%.1f target=%.1f rifle_shots=%u pistol_shots=%u"),
            K.AttackCount, K.ImpactCount, static_cast<int32>(K.LastImpact), K.CancelCount, static_cast<int32>(L.Equipped),
            static_cast<int32>(L.Previous), L.bEquipping, L.EquipCount, Driver->GetHealthComponent()->GetHealth(),
            Target->GetHealthComponent()->GetHealth(), Driver->GetAcceptedShotCount(EVSWeaponSlot::Rifle),
            Driver->GetAcceptedShotCount(EVSWeaponSlot::Pistol));
    }

    // Server only: a thin blocking slab between the driver and a target 100 cm ahead.
    bool SpawnWall(UWorld* World)
    {
        UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Slab = Cube ? World->SpawnActor<AStaticMeshActor>(Base + FVector(55, 0, 0), FRotator::ZeroRotator, Params) : nullptr;
        if (!Slab) { return false; }
        UStaticMeshComponent* Mesh = Slab->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        Slab->SetActorScale3D(FVector(0.1, 2.0, 3.0));
        Wall = Slab;
        return true;
    }

    void BindKnifeCues(AVSCharacter* Driver)
    {
        if (bKnifeCuesBound || !KnifeCues.Bind(Driver)) { return; }
        bKnifeCuesBound = true;
        KnifeCues.OnCue().AddLambda([this](const FVSKnifeCueEvent& Event)
        {
            ++KnifeCueCounts[static_cast<int32>(Event.Cue)];
            if (Event.bLocal) { ++LocalKnifeCues; }
        });
    }

    // KNIFE-01 after the firearm timeline. Knife counters: 8 accepted attacks
    // (3 slashes, 5 stabs), 4 impacts (hit, miss, wall, killing stab) and 4
    // cancellations (switch, phase, reset, death). The server triggers each
    // lifecycle transition on the tick it sees the stab accepted, so the stab's
    // impact is always still pending.
    void TickKnife(UWorld* World, double Time, AVSCharacter* Driver, AVSCharacter* Target, APlayerController* PC)
    {
        const bool bServer = Role == TEXT("server");
        UVSKnifeComponent* Knife = Driver->GetKnifeComponent();
        UVSWeaponLoadoutComponent* Loadout = Driver->GetLoadoutComponent();
        if (!Knife || !Loadout) { Finish(TEXT("knife_missing")); return; }
        const FVSKnifeState& K = Knife->GetState();
        const FVSLoadoutState& L = Loadout->GetState();
        const auto State = [&]() { return KnifeDetail(Driver, Target); };
        const float TargetHealth = Target->GetHealthComponent()->GetHealth();
        const FVector Front = Base + FVector(100, 0, 0);
        const FRotator FacingDriver(0, 180, 0);
        if (!bServer)
        {
            BindKnifeCues(Driver);
            KnifeCues.Tick();
        }

        if (bServer)
        {
            if (Once(400, Time, 13.2) && !(Place(Driver, Base, FRotator::ZeroRotator) && Place(Target, Front, FacingDriver)))
            {
                Finish(TEXT("knife_placement_failed")); return;
            }
            if (Once(401, Time, 15.4) && !Place(Target, Base + FVector(300, 0, 0), FacingDriver)) { Finish(TEXT("knife_placement_failed")); return; }
            if (Once(402, Time, 16.6) && !(Place(Target, Front, FacingDriver) && SpawnWall(World))) { Finish(TEXT("knife_wall_failed")); return; }
            if (Once(403, Time, 17.7) && Wall.IsValid()) { Wall->Destroy(); }
            if (K.AttackCount == 4 && Once(404, Time, 18.0)) { Loadout->DevelopmentRequestEquip(RifleRequest); }
            if (K.AttackCount == 5 && Once(405, Time, 19.8)) { Driver->SetMatchGates(true, false); }
            if (Steps.Contains(405) && Once(406, Time, 20.4)) { Driver->SetMatchGates(true, true); }
            if (K.AttackCount == 6 && Once(407, Time, 20.9)) { Loadout->DevelopmentReset(); }
            if (K.AttackCount == 8 && Once(408, Time, 23.6)) { Driver->GetHealthComponent()->ApplyPointDamage(1000); }
        }
        if (Role == TEXT("driver"))
        {
            if (Once(300, Time, 13.5)) { Tap(PC, EKeys::Three); }
            if (Once(301, Time, 13.6)) { Knife->DevelopmentRequestAttack(0); }
            if (Once(302, Time, 14.4)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(303, Time, 14.5)) { Knife->DevelopmentRequestAttack(0); }
            if (Once(304, Time, 14.6))
            {
                Knife->DevelopmentRequestAttack(7);
                Knife->DevelopmentRequestAttack(1, Knife->DevelopmentLastSequence());
            }
            if (Once(305, Time, 15.8)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(306, Time, 17.0)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(307, Time, 18.0)) { Tap(PC, EKeys::RightMouseButton); }
            if (Once(308, Time, 18.9)) { Knife->DevelopmentRequestAttack(1); }
            if (Once(309, Time, 19.0)) { Tap(PC, EKeys::Q); }
            if (Once(310, Time, 19.8)) { Tap(PC, EKeys::RightMouseButton); }
            if (Once(311, Time, 20.9)) { Tap(PC, EKeys::RightMouseButton); }
            if (Once(312, Time, 21.8)) { Tap(PC, EKeys::Three); }
            if (Once(313, Time, 22.6)) { Tap(PC, EKeys::RightMouseButton); }
            if (Once(314, Time, 23.6)) { Tap(PC, EKeys::RightMouseButton); }
        }

        if (Once(200, Time, 14.2))
        {
            const FVSPlayerHUDData D = Driver->GetHUDData();
            Check(TEXT("knife_equipped"), Equipped(Driver, EVSWeaponSlot::Knife, false) && L.Previous == EVSWeaponSlot::Rifle
                && L.EquipCount == 5 && !Driver->GetEquippedWeapon()
                && Driver->GetMovementWeaponKind() == VSMovementRules::WeaponKind::Knife
                && D.WeaponName == TEXT("KNIFE") && !D.bHasWeapon, State());
            if (bServer) { Check(TEXT("knife_reject_equip"), Knife->DevelopmentRejections(TEXT("equip")) >= 1, TEXT("raw attack during knife equip")); }
            else
            {
                FString Views;
                Check(TEXT("knife_hides_firearms"), ModelsShow(Driver, EVSWeaponSlot::Knife, Views), Views);
            }
        }
        if (Once(201, Time, 15.2))
        {
            // Exactly one 40-point application; LMB with the knife never fires a firearm.
            Check(TEXT("knife_slash_hit_once"), TargetHealth == 60.0f && K.AttackCount == 1 && K.ImpactCount == 1
                && K.LastImpact == EVSKnifeImpact::Player && Driver->GetAcceptedShotCount(EVSWeaponSlot::Rifle) == 1
                && Driver->GetAcceptedShotCount(EVSWeaponSlot::Pistol) == 3, State());
            if (bServer)
            {
                Check(TEXT("knife_reject_cooldown"), Knife->DevelopmentRejections(TEXT("cooldown")) >= 1, TEXT("raw slash 0.1 s after a slash"));
                Check(TEXT("knife_reject_invalid"), Knife->DevelopmentRejections(TEXT("invalid")) >= 1, TEXT("raw attack kind 7"));
                Check(TEXT("knife_reject_replay"), Knife->DevelopmentRejections(TEXT("sequence")) >= 1, TEXT("raw attack replaying a sequence"));
            }
        }
        if (Once(202, Time, 16.5))
        {
            Check(TEXT("knife_out_of_range"), TargetHealth == 60.0f && K.AttackCount == 2 && K.ImpactCount == 2
                && K.LastImpact == EVSKnifeImpact::Miss, State());
        }
        if (Once(203, Time, 17.6))
        {
            Check(TEXT("knife_wall"), TargetHealth == 60.0f && K.AttackCount == 3 && K.ImpactCount == 3
                && K.LastImpact == EVSKnifeImpact::Wall, State());
        }
        if (Once(204, Time, 18.8))
        {
            // The pending stab never lands after the switch, even past its impact time.
            Check(TEXT("knife_cancel_switch"), TargetHealth == 60.0f && K.AttackCount == 4 && K.ImpactCount == 3 && K.CancelCount == 1
                && Equipped(Driver, EVSWeaponSlot::Rifle, false) && L.Previous == EVSWeaponSlot::Knife, State());
        }
        if (Once(205, Time, 19.7))
        {
            Check(TEXT("knife_equip_last"), Equipped(Driver, EVSWeaponSlot::Knife, false) && L.Previous == EVSWeaponSlot::Rifle
                && L.EquipCount == 7, State());
            if (bServer) { Check(TEXT("knife_reject_holstered"), Knife->DevelopmentRejections(TEXT("holstered")) >= 1, TEXT("raw attack with the rifle equipped")); }
        }
        if (Once(206, Time, 20.6))
        {
            Check(TEXT("knife_cancel_phase"), TargetHealth == 60.0f && K.AttackCount == 5 && K.ImpactCount == 3 && K.CancelCount == 2
                && Driver->CanMatchCombat(), State());
        }
        if (Once(207, Time, 21.6))
        {
            Check(TEXT("knife_cancel_reset"), TargetHealth == 60.0f && K.AttackCount == 6 && K.ImpactCount == 3 && K.CancelCount == 3
                && Equipped(Driver, EVSWeaponSlot::Rifle, false), State());
        }
        if (Once(208, Time, 23.3))
        {
            Check(TEXT("knife_stab_hit"), !Target->GetHealthComponent()->IsAlive() && K.AttackCount == 7 && K.ImpactCount == 4
                && K.LastImpact == EVSKnifeImpact::Player && Equipped(Driver, EVSWeaponSlot::Knife, false), State());
        }
        if (Once(209, Time, 24.4))
        {
            Check(TEXT("knife_cancel_death"), !Driver->GetHealthComponent()->IsAlive() && K.AttackCount == 8 && K.ImpactCount == 4
                && K.CancelCount == 4, State());
            if (!bServer)
            {
                // Native binding API: every cue reached this view, local only on the owner.
                const auto Count = [this](EVSKnifeCue Cue) { return KnifeCueCounts[static_cast<int32>(Cue)]; };
                const bool bOwner = Role == TEXT("driver");
                Check(TEXT("knife_presentation_cues"), Count(EVSKnifeCue::Equip) >= 3 && Count(EVSKnifeCue::Holster) >= 2
                    && Count(EVSKnifeCue::Slash) >= 1 && Count(EVSKnifeCue::Stab) >= 1 && Count(EVSKnifeCue::Impact) >= 4
                    && Count(EVSKnifeCue::Cancel) >= 4 && (bOwner ? LocalKnifeCues >= 8 : LocalKnifeCues == 0),
                    FString::Printf(TEXT("equip=%d holster=%d slash=%d stab=%d impact=%d cancel=%d local=%d"), Count(EVSKnifeCue::Equip),
                        Count(EVSKnifeCue::Holster), Count(EVSKnifeCue::Slash), Count(EVSKnifeCue::Stab), Count(EVSKnifeCue::Impact),
                        Count(EVSKnifeCue::Cancel), LocalKnifeCues));
            }
            Finish();
        }
    }

    // Rendered first-person and observer captures with seconds of slack between
    // actions. Captures are state-triggered or taken once a state has settled.
    void TickVisual(UWorld* World, double Time, AVSCharacter* Driver, AVSCharacter* Target)
    {
        const bool bServer = Role == TEXT("server");
        APlayerController* PC = LocalController(Driver, Target);
        if (!Driver || !Target || (!bServer && !PC))
        {
            if (Time > 1.0) { Finish(TEXT("players_not_ready")); }
            return;
        }
        if (Time > 40.0) { Finish(TEXT("visual_timeout")); return; }
        if (!bReady)
        {
            bReady = true;
            if (bServer && !PlacePair(Driver, Target)) { Finish(TEXT("placement_failed")); return; }
        }
        const UVSWeaponComponent* Pistol = Driver->GetWeapon(EVSWeaponSlot::Pistol);
        const UVSWeaponPresentationComponent* PistolView = Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol);
        const auto State = [&]() { return Detail(Driver, Target); };
        if (PC)
        {
            ReleaseTaps(PC);
            PC->SetControlRotation(Role == TEXT("driver") ? FRotator::ZeroRotator : ObserverView);
        }
        if (Role == TEXT("driver"))
        {
            if (Once(120, Time, 4.0)) { Tap(PC, EKeys::Two); }
            if (Once(121, Time, 10.0)) { Tap(PC, EKeys::LeftMouseButton); }
            if (Once(122, Time, 13.0)) { Tap(PC, EKeys::R); }
            if (Once(123, Time, 18.0)) { Tap(PC, EKeys::One); }
        }
        FString Views;
        if (bServer)
        {
            Eventually(0, TEXT("visual_server_sequence"), Time, 22.0, 26.0, Equipped(Driver, EVSWeaponSlot::Rifle, false)
                && Driver->GetLoadoutComponent()->GetState().EquipCount == 2 && Pistol->GetState().ShotsFired == 1
                && Pistol->GetState().Magazine == 12 && Pistol->GetState().Reserve == 35
                && Target->GetHealthComponent()->GetHealth() == 100, State());
            if (Steps.Contains(0)) { Finish(); }
            return;
        }
        if (Eventually(1, TEXT("visual_rifle_models"), Time, 1.0, 4.0,
            Equipped(Driver, EVSWeaponSlot::Rifle, false) && ModelsShow(Driver, EVSWeaponSlot::Rifle, Views), Views))
        {
            Capture(TEXT("rifle"));
        }
        // The first frame a client shows the raised pistol is the start of its equip.
        if (Eventually(2, TEXT("visual_pistol_raising"), Time, 4.0, 9.0, Equipped(Driver, EVSWeaponSlot::Pistol, true)
            && ModelsShow(Driver, EVSWeaponSlot::Pistol, Views) && PistolView->GetEquipBlend() < 1.0f, Views))
        {
            Capture(TEXT("pistol_equipping"));
        }
        if (Eventually(3, TEXT("visual_pistol_models"), Time, 7.0, 10.0, Equipped(Driver, EVSWeaponSlot::Pistol, false)
            && ModelsShow(Driver, EVSWeaponSlot::Pistol, Views) && PistolView->GetEquipBlend() == 1.0f, Views))
        {
            Capture(TEXT("pistol"));
        }
        if (Eventually(4, TEXT("visual_pistol_flash"), Time, 10.0, 13.0, Pistol->GetState().ShotsFired == 1
            && PistolView->IsFlashActive(), ViewDetail(PistolView)))
        {
            Capture(TEXT("pistol_muzzle"));
        }
        const UStaticMeshComponent* Magazine = Role == TEXT("driver") ? PistolView->GetFirstPersonMagazine() : PistolView->GetWorldMagazine();
        if (Eventually(5, TEXT("visual_pistol_reload"), Time, 13.0, 16.0, Pistol->GetState().bReloading
            && PistolView->IsReloadPoseActive() && Magazine->GetRelativeLocation().Z < -5.0, ViewDetail(PistolView)))
        {
            Capture(TEXT("pistol_reload"));
        }
        if (Eventually(6, TEXT("visual_rifle_after_switch"), Time, 19.5, 23.0, Equipped(Driver, EVSWeaponSlot::Rifle, false)
            && ModelsShow(Driver, EVSWeaponSlot::Rifle, Views), Views))
        {
            Capture(TEXT("rifle_after_switch"));
        }
        if (Steps.Contains(6) && Once(7, Time, 24.0)) { Finish(); }
    }

    void TickMatch(UWorld* World, double Time, AVSCharacter* Driver, AVSCharacter* Target)
    {
        const bool bServer = Role == TEXT("server");
        const AVSMatchGameState* GS = World->GetGameState<AVSMatchGameState>();
        if (!GS) { Finish(TEXT("not_match_game_state")); return; }
        if (Time > 90.0) { Finish(TEXT("match_timeout")); return; }
        // Phase values follow ValStrike::Rules::Phase: 1 Preparation, 2 Active.
        const uint8 Phase = GS->Match.Phase;
        const uint64 Round = GS->Match.RoundId;
        APlayerController* PC = LocalController(Driver, Target);
        if (!Driver || !Target || (!bServer && !PC)) { return; }
        const auto State = [&]() { return Detail(Driver, Target); };
        if (PC) { ReleaseTaps(PC); }

        if (FirstRound == 0 && Phase == 1 && Round != 0)
        {
            FirstRound = Round;
            PrepStart = Time;
            OldDriverId = Driver->GetUniqueID();
            OldTargetId = Target->GetUniqueID();
        }
        if (FirstRound == 0) { return; }
        FString Views;

        if (Round == FirstRound)
        {
            const double P = PrepStart;
            if (bServer && Once(0, Time, P + 0.5))
            {
                if (!PlacePair(Driver, Target)) { Finish(TEXT("placement_failed")); return; }
                Check(TEXT("preparation_gates_closed"), !Driver->CanMatchCombat() && Full(Driver) && Full(Target), State());
            }
            if (Role == TEXT("observer")) { PC->SetControlRotation(ObserverView); }
            if (Role == TEXT("driver") && Phase == 1)
            {
                if (Once(120, Time, P + 1.0)) { Tap(PC, EKeys::Two); }
                if (Once(100, Time, P + 1.1)) { Driver->GetLoadoutComponent()->DevelopmentRequestEquip(PistolRequest); }
            }
            if (Once(1, Time, P + 2.5))
            {
                Check(TEXT("preparation_blocks_equip"), Phase == 1 && Equipped(Driver, EVSWeaponSlot::Rifle, false)
                    && Driver->GetLoadoutComponent()->GetState().EquipCount == 0, State());
                if (bServer) { Check(TEXT("reject_phase"), Driver->GetLoadoutComponent()->DevelopmentRejections(TEXT("phase")) >= 1, TEXT("raw request in preparation")); }
            }
            if (!bServer && Phase == 1 && Eventually(2, TEXT("match_rifle_models"), Time, P + 3.0, P + 6.5,
                ModelsShow(Driver, EVSWeaponSlot::Rifle, Views), Views))
            {
                Capture(TEXT("match_rifle"));
            }
            if (Phase == 2 && ActiveStart < 0) { ActiveStart = Time; }
            if (ActiveStart < 0) { return; }
            const double A = ActiveStart;
            const UVSWeaponComponent* Pistol = Driver->GetWeapon(EVSWeaponSlot::Pistol);
            const UVSWeaponComponent* Rifle = Driver->GetWeapon(EVSWeaponSlot::Rifle);
            if (Role == TEXT("driver"))
            {
                const bool bBody = Time >= A + 4.2 && Time < A + 5.5;
                const bool bHead = Time >= A + 10.0 && Time < A + 12.0;
                PC->SetControlRotation(bBody || bHead ? AimAt(Driver, Target, bHead) : FRotator::ZeroRotator);
                if (Once(121, Time, A + 1.0)) { Tap(PC, EKeys::Two); }
                if (Once(122, Time, A + 4.5)) { Tap(PC, EKeys::LeftMouseButton); }
                if (Once(123, Time, A + 6.0)) { Tap(PC, EKeys::R); }
                if (Once(124, Time, A + 9.0)) { Tap(PC, EKeys::One); }
                if (Once(125, Time, A + 10.5)) { Tap(PC, EKeys::LeftMouseButton); }
            }
            Eventually(3, TEXT("active_rifle"), Time, A + 0.2, A + 3.0,
                Equipped(Driver, EVSWeaponSlot::Rifle, false) && Driver->CanMatchCombat(), State());
            if (Eventually(4, TEXT("match_equip_pistol"), Time, A + 1.2, A + 4.0, Equipped(Driver, EVSWeaponSlot::Pistol, false)
                && Driver->GetHUDData().WeaponName == TEXT("PISTOL"), State()) && !bServer)
            {
                Check(TEXT("match_pistol_models"), ModelsShow(Driver, EVSWeaponSlot::Pistol, Views), Views);
                Capture(TEXT("match_pistol"));
            }
            Eventually(5, TEXT("match_pistol_damage"), Time, A + 4.6, A + 8.0, Pistol->GetState().ShotsFired == 1
                && Pistol->GetState().Magazine == 11 && Target->GetHealthComponent()->GetHealth() == 80, State());
            Eventually(6, TEXT("match_pistol_reload"), Time, A + 6.2, A + 11.0, Pistol->GetState().ShotsFired == 1
                && !Pistol->GetState().bReloading && Pistol->GetState().Magazine == 12 && Pistol->GetState().Reserve == 35, State());
            if (Eventually(7, TEXT("match_rifle_elimination"), Time, A + 10.6, A + 15.0, Equipped(Driver, EVSWeaponSlot::Rifle, false)
                && Rifle->GetState().ShotsFired == 1 && Rifle->GetState().Magazine == 29 && !Target->GetHealthComponent()->IsAlive(), State()))
            {
                Capture(TEXT("match_elimination"));
            }
            return;
        }

        // A later round: Match/ destroyed and respawned both pawns in ResetPawns.
        if (Phase != 1) { return; }
        if (ResetSeen < 0) { ResetSeen = Time; }
        const bool bReset = Driver->GetUniqueID() != OldDriverId && Target->GetUniqueID() != OldTargetId
            && Full(Driver) && Full(Target);
        if (Eventually(8, TEXT("round_reset_restores_both"), Time, ResetSeen, ResetSeen + 4.0, Steps.Contains(7) && bReset,
            FString::Printf(TEXT("round=%llu first=%llu %s"), Round, FirstRound, *State())) && !bServer)
        {
            Check(TEXT("round_reset_models"), ModelsShow(Driver, EVSWeaponSlot::Rifle, Views)
                && Driver->GetWeaponPresentation(EVSWeaponSlot::Rifle)->GetShotPulseCount() == 0
                && Driver->GetWeaponPresentation(EVSWeaponSlot::Pistol)->GetShotPulseCount() == 0, Views);
            Capture(TEXT("match_reset"));
        }
        if (Steps.Contains(8) && Once(9, Time, ResetSeen + 5.0)) { Finish(); }
    }

    FString Role, Output, Scenario;
    FVSTestServerClock Clock;
    TArray<FKey> PendingRelease;
    int32 FrameCount = 0;
    double FrameSeconds = 0.0, MaxFrameSeconds = 0.0;
    FDelegateHandle Delegate;
    TArray<FString> Checks;
    TSet<int32> Steps;
    FVector Base = FVector::ZeroVector;
    const FRotator ObserverView = FRotator(-10, 126.87, 0);
    float StartTime = 30;
    bool bMatch = false, bCapture = false;
    bool bNameRequested = false, bReady = false, bFinished = false, bLookIsolated = false;
    bool bFireHeld = false;
    uint32 OldTargetId = 0, OldDriverId = 0;
    TWeakObjectPtr<AActor> Wall;
    FVSKnifePresentationEvents KnifeCues;
    int32 KnifeCueCounts[6] = {};
    int32 LocalKnifeCues = 0;
    bool bKnifeCuesBound = false;
    uint64 FirstRound = 0;
    double PrepStart = -1, ActiveStart = -1, ResetSeen = -1;
};
TUniquePtr<FVSWeaponSmoke> WeaponSmoke;
}

void ValStrikeTests::StartWeaponSmoke()
{
    FString Role;
    if (!WeaponSmoke && FParse::Value(FCommandLine::Get(), TEXT("VSWeaponSmoke="), Role)
        && (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        WeaponSmoke = MakeUnique<FVSWeaponSmoke>(Role);
    }
}
#else
void ValStrikeTests::StartWeaponSmoke() {}
#endif
