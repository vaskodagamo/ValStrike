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
            Check(TEXT("baseline_combat_data_is_unavailable"), !Pawn->GetHUDData().bHasHealth && !Pawn->GetHUDData().bHasWeapon);
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
            Key(PC, EKeys::W, true);
            Next(Now);
            break;
        case 9:
            if (Age < 0.45 || StageTicks < 12) return;
            CheckMovement(TEXT("connected_forward_input_moves"), Pawn, true);
            Key(PC, EKeys::Escape, true);
            Next(Now);
            break;
        case 10:
            if (Age < 0.15) return;
            Key(PC, EKeys::Escape, false);
            Check(TEXT("connected_escape_opens_menu"), PC->IsSessionMenuOpen());
            Check(TEXT("connected_menu_flushes_keys"), !PC->IsInputKeyDown(EKeys::W));
            Check(TEXT("connected_menu_does_not_pause"), !PC->GetWorld()->IsPaused());
            Screenshot(TEXT("connected-menu"));
            Key(PC, EKeys::W, true);
            WorldTime = PC->GetWorld()->GetTimeSeconds();
            Next(Now);
            break;
        case 11:
            if (Age < 0.55 || StageTicks < 12) return;
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
            PC->SetSessionMenuOpen(true);
            Next(Now);
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
