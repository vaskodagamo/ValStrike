#include "VSUISmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "VSSessionState.h"
#include "../VSCharacter.h"
#include "../VSPlayerController.h"
#include "../ValStrike.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Layout/Children.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
class FSessionSmoke
{
public:
    FSessionSmoke()
    {
        bEnabled = FParse::Param(FCommandLine::Get(), TEXT("VSUISmoke"));
        FParse::Value(FCommandLine::Get(), TEXT("VSUISmokeAddress="), Address);
        FParse::Value(FCommandLine::Get(), TEXT("VSUISmokeFailureAddress="), FailureAddress);
        ScreenshotDirectory = FPaths::ProjectSavedDir() / TEXT("Tests/UI/Screenshots");
        FParse::Value(FCommandLine::Get(), TEXT("VSUISmokeScreenshotDir="), ScreenshotDirectory);
    }

    void Tick(AVSPlayerController* PC)
    {
        if (!bEnabled || bFinished || !PC || !PC->IsLocalController() || !PC->GetWorld()->IsGameWorld())
            return;
        const double Now = FPlatformTime::Seconds();
        if (Started == 0.0)
        {
            Started = Now;
            Entered = Now;
        }
        if (Now - Started > 80.0)
        {
            Check(TEXT("runtime_completed_before_deadline"), false);
            Finish(PC);
            return;
        }

        AVSCharacter* Pawn = Cast<AVSCharacter>(PC->GetPawn());
        UVSSessionState* Session = PC->GetSessionState();
        const double Age = Now - Entered;
        ++StageTicks;
        switch (Stage)
        {
        case 0:
            if (!Pawn || !Pawn->InputComponent || !Session || Age < 2.0) return;
            Check(TEXT("initial_world_is_standalone"), PC->GetNetMode() == NM_Standalone);
            Check(TEXT("initial_combat_defaults_available"), CombatDefaults(Snapshot(TEXT("initial"), Pawn)));
            Check(TEXT("initial_menu_closed"), !PC->IsSessionMenuOpen());
            Check(TEXT("slate_initialized"), FSlateApplication::IsInitialized());
            Screenshot(TEXT("baseline"));
            Key(PC, EKeys::W, true);
            Next(Now);
            break;
        case 1:
            if (Age < 0.45 || StageTicks < 12) return;
            CheckMovement(TEXT("standalone_forward_input_moves"), Pawn, true);
            Key(PC, EKeys::Escape, true);
            Next(Now);
            break;
        case 2:
            if (Age < 0.15) return;
            Key(PC, EKeys::Escape, false);
            Check(TEXT("escape_opens_menu"), PC->IsSessionMenuOpen());
            Check(TEXT("opening_flushes_held_forward"), !PC->IsInputKeyDown(EKeys::W));
            Check(TEXT("menu_ignores_movement_and_look"), PC->IsMoveInputIgnored() && PC->IsLookInputIgnored());
            Check(TEXT("menu_shows_cursor"), PC->bShowMouseCursor);
            Check(TEXT("menu_does_not_pause_world"), !PC->GetWorld()->IsPaused());
            Screenshot(TEXT("menu"));
            WorldTime = PC->GetWorld()->GetTimeSeconds();
            Rotation = PC->GetControlRotation();
            PC->AddYawInput(20.0f);
            Key(PC, EKeys::W, true);
            Next(Now);
            break;
        case 3:
            if (Age < 0.5 || StageTicks < 12) return;
            Check(TEXT("world_advances_while_menu_open"), PC->GetWorld()->GetTimeSeconds() - WorldTime > 0.2f);
            CheckMovement(TEXT("menu_blocks_injected_forward"), Pawn, false);
            Check(TEXT("menu_blocks_injected_look"), PC->GetControlRotation().Equals(Rotation, 0.1f));
            // Exercise the real pawn-restart input path without inventing combat
            // state or changing the server-owned pawn. The menu must keep focus.
            PC->ClientRestart_Implementation(Pawn);
            Check(TEXT("restart_preserves_menu_input_ownership"), PC->IsSessionMenuOpen()
                && PC->IsMoveInputIgnored() && PC->IsLookInputIgnored() && PC->bShowMouseCursor);
            Stage = 30;
            Entered = Now;
            break;
        case 30:
            // SetInputMode queues local-player Slate operations; let that reply
            // apply before delivering the next independent keyboard event.
            if (Age < 0.15) return;
            if (FSlateApplication::IsInitialized())
            {
                Focus = FSlateApplication::Get().GetUserFocusedWidget(0);
                Check(TEXT("menu_has_keyboard_focus"), Focus.IsValid());
                SlateKey(EKeys::Tab);
            }
            Stage = 4;
            Entered = Now;
            break;
        case 4:
            if (Age < 0.15) return;
            if (FSlateApplication::IsInitialized())
            {
                Check(TEXT("tab_changes_menu_focus"), FSlateApplication::Get().GetUserFocusedWidget(0) != Focus.Pin());
                SlateKey(EKeys::Tab, true);
                Check(TEXT("shift_tab_restores_menu_focus"), FSlateApplication::Get().GetUserFocusedWidget(0) == Focus.Pin());
                SlateKey(EKeys::Escape);
            }
            Next(Now);
            break;
        case 5:
            if (Age < 0.15) return;
            Check(TEXT("slate_escape_resumes"), !PC->IsSessionMenuOpen());
            Check(TEXT("resume_restores_input_and_cursor"), !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && !PC->bShowMouseCursor);
            Check(TEXT("resume_flushes_menu_keys"), !PC->IsInputKeyDown(EKeys::W));
            Key(PC, EKeys::S, true);
            Next(Now);
            break;
        case 6:
            if (Age < 0.45 || StageTicks < 12) return;
            CheckMovement(TEXT("backward_moves_after_resume"), Pawn, true);
            Key(PC, EKeys::S, false);
            PC->SetSessionMenuOpen(true);
            PC->ConnectToServer(TEXT("127.0.0.1:7777?listen"));
            Check(TEXT("invalid_address_rejected_before_travel"), Session && !Session->bConnecting && Session->bError && PC->GetNetMode() == NM_Standalone);
            Screenshot(TEXT("invalid-address"));
            Stage = 60;
            Entered = Now;
            break;
        case 60:
            if (Age < 0.4) return;
            Stage = 61;
            Entered = Now;
            if (FSlateApplication::IsInitialized())
            {
                // Resume -> Connect -> address. Disabled Cancel is skipped by
                // the menu's keyboard navigation, exactly as in a real session.
                SlateKey(EKeys::Tab, true);
                SlateKey(EKeys::Tab, true);
                for (const TCHAR Character : Address)
                    FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(Character, FModifierKeysState(), 0, false));
                Check(TEXT("address_text_entry_updates_session"), Session && Session->Address == Address);
                SlateKey(EKeys::Enter);
            }
            Check(TEXT("valid_address_starts_connection"), Session && Session->bConnecting && Session->Address == Address);
            break;
        case 61:
            // Verify the focus transfer is retained beyond the text-commit event.
            if (Age < 0.04) return;
            if (FSlateApplication::IsInitialized())
            {
                const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
                bool bCancelFocused = false;
                if (Focused && Focused->GetTypeAsString() == TEXT("SButton") && Focused->IsEnabled()
                    && Focused->GetChildren()->Num() == 1)
                {
                    const TSharedRef<SWidget> Content = Focused->GetChildren()->GetChildAt(0);
                    bCancelFocused = Content->GetTypeAsString() == TEXT("STextBlock")
                        && StaticCastSharedRef<STextBlock>(Content)->GetText().ToString() == TEXT("Cancel connection");
                }
                Check(TEXT("enter_connect_focuses_enabled_cancel"), bCancelFocused);
            }
            Stage = 7;
            Entered = Now;
            break;
        case 7:
            if (PC->GetNetMode() != NM_Client || !Pawn || !Pawn->InputComponent || !Session || Session->bConnecting) return;
            Check(TEXT("real_server_travel_completed"), true);
            Check(TEXT("connected_menu_closed"), !PC->IsSessionMenuOpen());
            Check(TEXT("connected_feedback_survives_travel"), !Session->bError && !Session->Status.IsEmpty());
            Next(Now);
            break;
        case 8:
            if (Age < 0.5) return;
            Check(TEXT("connected_combat_defaults_replicated"), Pawn && !Pawn->HasAuthority()
                && Pawn->GetLocalRole() == ROLE_AutonomousProxy && PC->GetNetMode() == NM_Client
                && CombatDefaults(Snapshot(TEXT("connected_initial"), Pawn)));
            Screenshot(TEXT("connected-hud"));
            Key(PC, EKeys::W, true);
            Key(PC, EKeys::LeftMouseButton, true);
            Next(Now);
            break;
        case 9:
            if (Age < 0.45 || StageTicks < 12) return;
            CheckMovement(TEXT("connected_forward_input_moves"), Pawn, true);
            {
                const FVSPlayerHUDData Data = Snapshot(TEXT("connected_firing"), Pawn);
                Check(TEXT("connected_fire_reduces_replicated_ammo"), Data.bHasWeapon && Data.bAlive
                    && Data.MagazineAmmo >= 1 && Data.MagazineAmmo <= 28 && Data.ReserveAmmo == 90 && !Data.bReloading);
                Check(TEXT("connected_fire_is_held_before_menu"), PC->IsInputKeyDown(EKeys::LeftMouseButton));
            }
            Key(PC, EKeys::Escape, true);
            Next(Now);
            break;
        case 10:
            if (Age < 0.15) return;
            Key(PC, EKeys::Escape, false);
            Check(TEXT("connected_escape_opens_menu"), PC->IsSessionMenuOpen());
            Check(TEXT("connected_menu_flushes_keys"), !PC->IsInputKeyDown(EKeys::W));
            Check(TEXT("connected_menu_releases_held_fire"), !PC->IsInputKeyDown(EKeys::LeftMouseButton));
            Check(TEXT("connected_menu_does_not_pause"), !PC->GetWorld()->IsPaused());
            Screenshot(TEXT("connected-menu"));
            Key(PC, EKeys::W, true);
            WorldTime = PC->GetWorld()->GetTimeSeconds();
            Stage = 100;
            Entered = Now;
            StageTicks = 0;
            break;
        case 100:
            // Let already-sent shots/replication settle before measuring that
            // taking menu focus stopped the real weapon's held-fire timer.
            if (Age < 0.35) return;
            MenuAmmo = Snapshot(TEXT("menu_settled"), Pawn);
            bMenuAmmoStable = MenuAmmo.bHasWeapon && MenuAmmo.MagazineAmmo > 0 && !MenuAmmo.bReloading;
            MenuAmmoSamples = 0;
            Stage = 11;
            Entered = Now;
            StageTicks = 0;
            break;
        case 11:
            {
                const FVSPlayerHUDData Data = Pawn ? Pawn->GetHUDData() : FVSPlayerHUDData{};
                bMenuAmmoStable &= Data.bHasWeapon && Data.bAlive && !Data.bReloading
                    && Data.MagazineAmmo == MenuAmmo.MagazineAmmo && Data.ReserveAmmo == MenuAmmo.ReserveAmmo;
                ++MenuAmmoSamples;
            }
            if (Age < 0.75 || StageTicks < 12) return;
            Snapshot(TEXT("menu_end"), Pawn);
            Check(TEXT("connected_menu_ammo_stays_stable"), bMenuAmmoStable && MenuAmmoSamples >= 12);
            CheckMovement(TEXT("connected_menu_blocks_forward"), Pawn, false);
            Check(TEXT("connected_world_advances_with_menu"), PC->GetWorld()->GetTimeSeconds() - WorldTime > 0.2f);
            if (FSlateApplication::IsInitialized()) SlateKey(EKeys::Escape);
            Check(TEXT("connected_escape_restores_input"), !PC->IsSessionMenuOpen() && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored());
            // Reverse the previous movement so nearby cover cannot turn the
            // input-focus check into an accidental collision test.
            Key(PC, EKeys::S, true);
            Next(Now);
            break;
        case 12:
            if (Age < 0.45 || StageTicks < 12) return;
            CheckMovement(TEXT("connected_backward_moves_after_resume"), Pawn, true);
            Key(PC, EKeys::S, false);
            BeforeReload = Snapshot(TEXT("reload_before"), Pawn);
            Check(TEXT("connected_reload_input_ready"), BeforeReload.bHasWeapon && BeforeReload.bAlive
                && !BeforeReload.bReloading && BeforeReload.MagazineAmmo > 0
                && BeforeReload.MagazineAmmo < BeforeReload.MagazineCapacity && BeforeReload.ReserveAmmo > 0);
            Key(PC, EKeys::LeftMouseButton, false);
            Key(PC, EKeys::R, true);
            Stage = 120;
            Entered = Now;
            break;
        case 120:
            if (Age < 0.12) return;
            Key(PC, EKeys::R, false);
            Stage = 121;
            Entered = Now;
            break;
        case 121:
            if ((!Pawn || !Pawn->GetHUDData().bReloading) && Age < 1.5) return;
            {
                const FVSPlayerHUDData Data = Snapshot(TEXT("reload_started"), Pawn);
                Check(TEXT("connected_reload_started"), Data.bHasWeapon && Data.bReloading
                    && Data.MagazineAmmo == BeforeReload.MagazineAmmo && Data.ReserveAmmo == BeforeReload.ReserveAmmo);
            }
            Screenshot(TEXT("connected-reloading"));
            Stage = 122;
            Entered = Now;
            break;
        case 122:
            {
                const FVSPlayerHUDData Data = Pawn ? Pawn->GetHUDData() : FVSPlayerHUDData{};
                const int32 Transfer = FMath::Min(BeforeReload.MagazineCapacity - BeforeReload.MagazineAmmo, BeforeReload.ReserveAmmo);
                const bool bComplete = Data.bHasWeapon && Data.bAlive && !Data.bReloading
                    && Data.MagazineAmmo == BeforeReload.MagazineAmmo + Transfer
                    && Data.ReserveAmmo == BeforeReload.ReserveAmmo - Transfer;
                if (!bComplete && Age < 3.5) return;
                Snapshot(TEXT("reload_completed"), Pawn);
                Check(TEXT("connected_reload_completed"), bComplete && Transfer > 0);
            }
            Screenshot(TEXT("connected-reloaded"));
            Stage = 123;
            Entered = Now;
            break;
        case 123:
            if (Age < 0.3) return; // Give the native screenshot a rendered frame before travel.
            PC->SetSessionMenuOpen(true);
            Stage = 13;
            Entered = Now;
            StageTicks = 0;
            PC->DisconnectFromServer();
            break;
        case 13:
            if (PC->GetNetMode() != NM_Standalone || !Pawn || !Session || Age < 0.5) return;
            Check(TEXT("disconnect_returns_to_local_world"), true);
            Check(TEXT("disconnect_reopens_menu"), PC->IsSessionMenuOpen());
            Check(TEXT("disconnect_feedback_survives_travel"), !Session->bConnecting && !Session->bError && Session->Status.ToString().Contains(TEXT("Disconnected")));
            Screenshot(TEXT("disconnected"));
            Stage = 130;
            Entered = Now;
            break;
        case 130:
            if (Age < 0.4) return;
            PC->ConnectToServer(FailureAddress);
            Check(TEXT("cancel_attempt_starts_connection"), Session && Session->bConnecting);
            Stage = 131;
            Entered = Now;
            break;
        case 131:
            if (Age < 0.4) return;
            BeforeCancelWorld = PC->GetWorld();
            PC->CancelConnection();
            Stage = 132;
            Entered = Now;
            break;
        case 132:
            if (Age < 0.5 || !Pawn || !Session || Session->bConnecting || PC->GetNetMode() != NM_Standalone || PC->GetWorld() == BeforeCancelWorld.Get()) return;
            Check(TEXT("cancel_returns_to_fresh_local_world"), true);
            Check(TEXT("cancel_reopens_menu"), PC->IsSessionMenuOpen());
            Check(TEXT("cancel_feedback_survives_travel"), !Session->bError && Session->Status.ToString().Contains(TEXT("cancelled")));
            Screenshot(TEXT("cancelled"));
            Stage = 133;
            Entered = Now;
            break;
        case 133:
            if (Age < 0.4) return;
            Stage = 14;
            Entered = Now;
            PC->ConnectToServer(FailureAddress);
            Check(TEXT("unreachable_address_starts_connection"), Session && Session->bConnecting);
            break;
        case 14:
            if (!Session || Session->bConnecting || !Session->bError || Age < 0.5) return;
            // The engine may signal failure before return-map travel. Wait for the
            // next controller as well so the error cannot disappear on arrival.
            Next(Now);
            break;
        case 15:
            if (Age < 1.0 || !Session || !Pawn) return;
            Check(TEXT("unreachable_connection_returns_to_local_world"), PC->GetNetMode() == NM_Standalone);
            Check(TEXT("connection_error_survives_travel"), Session->bError && !Session->bConnecting && !Session->Status.IsEmpty());
            Check(TEXT("connection_error_keeps_menu_open"), PC->IsSessionMenuOpen());
            Check(TEXT("connection_error_is_actionable"), Session->Status.ToString().Contains(TEXT("server")) && Session->Status.ToString().Contains(TEXT("address")));
            Screenshot(TEXT("connection-error"));
            Next(Now);
            break;
        case 16:
            if (Age < 0.5) return;
            Finish(PC);
            break;
        }
    }

private:
    static bool CombatDefaults(const FVSPlayerHUDData& Data)
    {
        return Data.bHasHealth && Data.bHasWeapon && Data.bAlive && Data.Health == 100.0f
            && Data.MaxHealth == 100.0f && Data.MagazineAmmo == 30 && Data.MagazineCapacity == 30
            && Data.ReserveAmmo == 90 && !Data.bReloading;
    }

    static FVSPlayerHUDData Snapshot(const TCHAR* Phase, const AVSCharacter* Pawn)
    {
        const FVSPlayerHUDData Data = Pawn ? Pawn->GetHUDData() : FVSPlayerHUDData{};
        UE_LOG(LogValStrike, Display, TEXT("VSUI_HUD {\"phase\":\"%s\",\"has_health\":%s,\"health\":%.3f,\"max_health\":%.3f,\"alive\":%s,\"has_weapon\":%s,\"magazine\":%d,\"capacity\":%d,\"reserve\":%d,\"reloading\":%s,\"authority\":%s,\"net_mode\":%d}"),
            Phase, Data.bHasHealth ? TEXT("true") : TEXT("false"), Data.Health, Data.MaxHealth,
            Data.bAlive ? TEXT("true") : TEXT("false"), Data.bHasWeapon ? TEXT("true") : TEXT("false"),
            Data.MagazineAmmo, Data.MagazineCapacity, Data.ReserveAmmo, Data.bReloading ? TEXT("true") : TEXT("false"),
            Pawn && Pawn->HasAuthority() ? TEXT("true") : TEXT("false"), Pawn ? static_cast<int32>(Pawn->GetNetMode()) : -1);
        return Data;
    }

    void Screenshot(const TCHAR* Name) const
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return;
        IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
        const FString Path = ScreenshotDirectory / (FString(Name) + TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        UE_LOG(LogValStrike, Display, TEXT("VSUI_SCREENSHOT %s"), *Path);
    }

    static void Key(AVSPlayerController* PC, FKey Name, bool bDown)
    {
        PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
    }

    static void SlateKey(FKey Name, bool bShift = false)
    {
        const FModifierKeysState Modifiers(bShift, false, false, false, false, false, false, false, false);
        const FKeyEvent Event(Name, Modifiers, 0, false, 0, 0);
        FSlateApplication::Get().ProcessKeyDownEvent(Event);
        FSlateApplication::Get().ProcessKeyUpEvent(Event);
    }

    void Next(double Now) { ++Stage; Entered = Now; StageTicks = 0; }

    void CheckMovement(const TCHAR* Name, const AVSCharacter* Pawn, bool bExpectMoving)
    {
        const FVector Velocity = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
        const FVector Location = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
        UE_LOG(LogValStrike, Display, TEXT("VSUI_MOVEMENT %s ticks=%d speed=%.3f velocity=(%.3f,%.3f,%.3f) location=(%.3f,%.3f,%.3f)"),
            Name, StageTicks, Velocity.Size2D(), Velocity.X, Velocity.Y, Velocity.Z, Location.X, Location.Y, Location.Z);
        Check(Name, Pawn && (bExpectMoving ? Velocity.Size2D() > 40.0f : Velocity.Size2D() < 5.0f));
    }

    void Check(const TCHAR* Name, bool bPassed)
    {
        ++Checks;
        if (!bPassed) ++Failures;
        UE_LOG(LogValStrike, Display, TEXT("VSUI_CHECK %s %s"), Name, bPassed ? TEXT("PASS") : TEXT("FAIL"));
    }

    void Finish(AVSPlayerController* PC)
    {
        Key(PC, EKeys::W, false);
        Key(PC, EKeys::S, false);
        Key(PC, EKeys::Escape, false);
        Key(PC, EKeys::LeftMouseButton, false);
        Key(PC, EKeys::R, false);
        Focus.Reset();
        bFinished = true;
        UE_LOG(LogValStrike, Display, TEXT("VSUI_DONE checks=%d failures=%d"), Checks, Failures);
        PC->QuitSession();
    }

    bool bEnabled = false;
    bool bFinished = false;
    int32 Stage = 0;
    int32 StageTicks = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    double Started = 0.0;
    double Entered = 0.0;
    float WorldTime = 0.0f;
    FVSPlayerHUDData MenuAmmo;
    FVSPlayerHUDData BeforeReload;
    bool bMenuAmmoStable = false;
    int32 MenuAmmoSamples = 0;
    FRotator Rotation;
    TWeakPtr<SWidget> Focus;
    TWeakObjectPtr<UWorld> BeforeCancelWorld;
    FString Address = TEXT("127.0.0.1:7813");
    FString FailureAddress = TEXT("127.0.0.1:7814");
    FString ScreenshotDirectory;
};
}

void VSUISmoke::Tick(AVSPlayerController* Controller, float DeltaTime)
{
    // The game instance survives absolute travel, but the controller does not.
    // Process-local sequencing deliberately follows each new local controller.
    static FSessionSmoke Smoke;
    Smoke.Tick(Controller);
}

#endif
