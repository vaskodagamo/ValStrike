#include "VSJoinSmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "SVSSessionMenu.h"
#include "VSSessionState.h"
#include "../VSPlayerController.h"
#include "../ValStrike.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Layout/Children.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Input/SEditableTextBox.h"

namespace
{
class FJoinSmoke
{
public:
    FJoinSmoke()
    {
        bEnabled = FParse::Param(FCommandLine::Get(), TEXT("VSJoinSmoke"));
        FParse::Value(FCommandLine::Get(), TEXT("VSJoinSmokeAddress="), Address);
        ScreenshotDirectory = FPaths::ProjectSavedDir() / TEXT("Tests/JoinKey/Screenshots");
        FParse::Value(FCommandLine::Get(), TEXT("VSJoinSmokeScreenshotDir="), ScreenshotDirectory);
        if (bEnabled)
        {
            Key = FPlatformMisc::GetEnvironmentVariable(TEXT("VSJOINSMOKE_KEY"));
        }
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
        if (Now - Started > 100.0)
        {
            Check(TEXT("runtime_completed_before_deadline"), false, FString::Printf(TEXT("stage=%d"), Stage));
            Finish(PC);
            return;
        }
        Menu = PC->GetSessionMenu();
        UVSSessionState* Session = PC->GetSessionState();
        const double Age = Now - Entered;
        Step(PC, Session, Age, Now);
        // Never keep Slate widgets alive in this static object (engine exit).
        Menu.Reset();
    }

private:
    void Step(AVSPlayerController* PC, UVSSessionState* Session, double Age, double Now)
    {
        const bool bReady = PC->GetPawn() && PC->GetPawn()->InputComponent && Menu && Session;
        switch (Stage)
        {
        case 0:
            if (!bReady || Age < 2.0) return;
            Check(TEXT("initial_world_is_standalone"), PC->GetNetMode() == NM_Standalone);
            // Only lengths and booleans are ever reported about the key.
            Check(TEXT("key_supplied_out_of_band"), Key.Len() >= 8 && !FString(FCommandLine::Get()).Contains(Key),
                FString::Printf(TEXT("length=%d"), Key.Len()));
            Check(TEXT("address_supplied"), !Address.IsEmpty(), Address);
            GameKey(PC, EKeys::Escape, true);
            Next(Now);
            break;
        case 1:
            if (Age < 0.15) return;
            GameKey(PC, EKeys::Escape, false);
            Check(TEXT("escape_opens_menu"), PC->IsSessionMenuOpen());
            Next(Now);
            break;
        case 2:
            if (Age < 0.4) return;
            Screenshot(TEXT("menu"));
            Next(Now);
            break;
        case 3:
            if (Age < 0.4) return;
            // Resume -> (disabled Cancel) -> Connect -> join key -> address.
            SlateKey(EKeys::Tab, true);
            SlateKey(EKeys::Tab, true);
            SlateKey(EKeys::Tab, true);
            Check(TEXT("shift_tab_reaches_address"), IsFocused(TEXT("VS.Address")));
            Type(*Address);
            Check(TEXT("address_entry_updates_session"), Session->Address == Address);
            SlateKey(EKeys::Tab);
            Check(TEXT("tab_reaches_join_key"), IsFocused(TEXT("VS.JoinKey")));
            Type(TEXT("short12"));
            SlateKey(EKeys::Enter);
            Next(Now);
            break;
        case 4:
            if (Age < 0.3) return;
            Check(TEXT("invalid_key_refused_before_travel"), PC->GetNetMode() == NM_Standalone && !Session->bConnecting && Session->bError
                && Session->Status.ToString().Contains(TEXT("join key")), Session->Status.ToString());
            Check(TEXT("rejected_enter_keeps_key_focus"), IsFocused(TEXT("VS.JoinKey")));
            Screenshot(TEXT("invalid-key"));
            Next(Now);
            break;
        case 5:
            if (Age < 0.4) return;
            // The address text is passed verbatim, exactly as the menu does.
            PC->ConnectToServer(Address + TEXT("?JoinKey=") + Key, FString());
            Check(TEXT("key_in_address_refused"), PC->GetNetMode() == NM_Standalone && !Session->bConnecting && Session->bError
                && Session->Status.ToString().Contains(TEXT("Join key field")), Session->Status.ToString());
            // Clear the (selected) invalid key, then join without a key.
            for (int32 Count = 0; Count < 16; ++Count) SlateKey(EKeys::BackSpace);
            Check(TEXT("key_field_cleared"), KeyText().IsEmpty() && Session->JoinKey.IsEmpty());
            BeforeWorld = PC->GetWorld();
            SlateKey(EKeys::Enter);
            Check(TEXT("keyless_attempt_starts_connection"), Session->bConnecting);
            Next(Now);
            break;
        case 6:
            if (!Session || Session->bConnecting || !Session->bError)
            {
                if (Age > 30.0) { Check(TEXT("keyless_attempt_returns"), false); Finish(PC); }
                return;
            }
            Stage = 60;
            Entered = Now;
            break;
        case 60:
            // The engine may load a fresh local map after the failure; let it settle.
            if (!bReady || PC->GetNetMode() != NM_Standalone || Age < 2.0)
            {
                if (Age > 30.0) { Check(TEXT("keyless_attempt_returns"), false); Finish(PC); }
                return;
            }
            UE_LOG(LogValStrike, Display, TEXT("VSJOIN_STATE refusal_world_changed=%s"), PC->GetWorld() != BeforeWorld.Get() ? TEXT("true") : TEXT("false"));
            Check(TEXT("keyless_join_refused_by_server"), Session->bError && Session->Status.ToString().Contains(TEXT("Join key missing or wrong")),
                Session->Status.ToString());
            Check(TEXT("refusal_keeps_menu_open"), PC->IsSessionMenuOpen());
            Check(TEXT("address_survives_refusal"), Session->Address == Address);
            Stage = 7;
            Entered = Now;
            break;
        case 7:
            if (Age < 0.5) return;
            Screenshot(TEXT("refused"));
            Next(Now);
            break;
        case 8:
            if (Age < 0.4) return;
            // Resume -> (disabled Cancel) -> Connect -> join key.
            SlateKey(EKeys::Tab, true);
            SlateKey(EKeys::Tab, true);
            Check(TEXT("shift_tab_reaches_join_key"), IsFocused(TEXT("VS.JoinKey")));
            Type(*Key);
            Check(TEXT("typed_key_is_kept_in_memory"), Session->JoinKey == Key && KeyText() == Key);
            Next(Now);
            break;
        case 9:
            if (Age < 0.4) return;
            Screenshot(TEXT("key-entered"));
            Next(Now);
            break;
        case 10:
            if (Age < 0.4) return;
            BeforeWorld = PC->GetWorld();
            SlateKey(EKeys::Enter);
            Check(TEXT("keyed_attempt_starts_connection"), Session->bConnecting
                && Session->Status.ToString().Contains(TEXT("with a join key")) && !Session->Status.ToString().Contains(Key),
                Session->Status.ToString());
            Next(Now);
            break;
        case 11:
            if (!bReady || Session->bConnecting || PC->GetNetMode() != NM_Client || Age < 0.5)
            {
                if (Age > 30.0) { Check(TEXT("keyed_join_completes"), false, Session ? Session->Status.ToString() : FString()); Finish(PC); }
                return;
            }
            Check(TEXT("keyed_join_connects"), !Session->bError && !PC->IsSessionMenuOpen() && PC->GetPawn()->GetLocalRole() == ROLE_AutonomousProxy,
                Session->Status.ToString());
            Next(Now);
            break;
        case 12:
            if (Age < 1.0) return;
            GameKey(PC, EKeys::Escape, true);
            Next(Now);
            break;
        case 13:
            if (Age < 0.15) return;
            GameKey(PC, EKeys::Escape, false);
            Check(TEXT("connected_menu_opens"), PC->IsSessionMenuOpen() && PC->GetNetMode() == NM_Client);
            Check(TEXT("connected_menu_keeps_key_masked_in_field"), KeyText() == Key);
            Next(Now);
            break;
        case 14:
            if (Age < 0.5) return;
            Screenshot(TEXT("connected-menu"));
            Next(Now);
            break;
        case 15:
            if (Age < 0.5) return;
            Finish(PC);
            break;
        }
    }

    static TSharedPtr<SWidget> FindTagged(const TSharedRef<SWidget>& Root, FName Tag)
    {
        if (Root->GetTag() == Tag) return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
            if (TSharedPtr<SWidget> Found = FindTagged(Children->GetChildAt(Index), Tag)) return Found;
        return nullptr;
    }

    TSharedPtr<SWidget> Tagged(const TCHAR* Tag) const
    {
        return Menu ? FindTagged(Menu.ToSharedRef(), FName(Tag)) : nullptr;
    }

    bool IsFocused(const TCHAR* Tag) const
    {
        const TSharedPtr<SWidget> Widget = Tagged(Tag);
        return Widget && (Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants());
    }

    // Compared in memory only; never logged.
    FString KeyText() const
    {
        const TSharedPtr<SWidget> Widget = Tagged(TEXT("VS.JoinKey"));
        return Widget ? StaticCastSharedPtr<SEditableTextBox>(Widget)->GetText().ToString() : FString(TEXT("<missing>"));
    }

    static void Type(const TCHAR* Text)
    {
        for (const TCHAR* Character = Text; *Character; ++Character)
            FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(*Character, FModifierKeysState(), 0, false));
    }

    static void GameKey(AVSPlayerController* PC, FKey Name, bool bDown)
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

    void Screenshot(const TCHAR* Name) const
    {
        IFileManager::Get().MakeDirectory(*ScreenshotDirectory, true);
        const FString Path = ScreenshotDirectory / (FString(Name) + TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        UE_LOG(LogValStrike, Display, TEXT("VSJOIN_SCREENSHOT %s"), *Path);
    }

    void Next(double Now) { ++Stage; Entered = Now; }

    void Check(const TCHAR* Name, bool bPassed, const FString& Detail = FString())
    {
        ++Checks;
        if (!bPassed) ++Failures;
        UE_LOG(LogValStrike, Display, TEXT("VSJOIN_CHECK %s %s %s"), Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail.Replace(TEXT("\n"), TEXT(" ")));
    }

    void Finish(AVSPlayerController* PC)
    {
        bFinished = true;
        GameKey(PC, EKeys::Escape, false);
        UE_LOG(LogValStrike, Display, TEXT("VSJOIN_DONE checks=%d failures=%d"), Checks, Failures);
        PC->QuitSession();
    }

    bool bEnabled = false;
    bool bFinished = false;
    FString Address;
    FString Key;
    FString ScreenshotDirectory;
    TSharedPtr<SVSSessionMenu> Menu;
    TWeakObjectPtr<UWorld> BeforeWorld;
    int32 Stage = 0;
    int32 Checks = 0;
    int32 Failures = 0;
    double Started = 0.0;
    double Entered = 0.0;
};
}

void VSJoinSmoke::Tick(AVSPlayerController* Controller, float DeltaTime)
{
    // Follows each new local controller across travel, like the session harness.
    static FJoinSmoke JoinSmoke;
    JoinSmoke.Tick(Controller);
}

#endif
