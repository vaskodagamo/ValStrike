#include "VSCombatLookSmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
class FVSCombatLookSmoke
{
public:
    FVSCombatLookSmoke()
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSCombatLookOutput="), Output);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSCombatLookSmoke::Tick);
    }
    ~FVSCombatLookSmoke() { FWorldDelegates::OnWorldPostActorTick.Remove(Delegate); }
private:
    void Tick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
    {
        if (bDone || !World || !World->IsGameWorld() || World->GetNetMode() != NM_Standalone) { return; }
        APlayerController* PC = World->GetFirstPlayerController();
        AVSCharacter* Pawn = PC ? Cast<AVSCharacter>(PC->GetPawn()) : nullptr;
        if (!Pawn || !Pawn->InputComponent || !PC->PlayerCameraManager) { return; }
        if (Began < 0) { Began = World->GetTimeSeconds(); }
        const double Time = World->GetTimeSeconds() - Began - 5.0;
        if (Time < 0) { return; }
        const int32 Phase = static_cast<int32>(Time / 4.0);
        const double InPhase = Time - Phase * 4.0;
        if (Phase >= 8)
        {
            const FString Report = FString::Printf(TEXT("{\"schema_version\":1,\"completed\":true,\"checks\":[%s]}\n"), *FString::Join(Checks, TEXT(",")));
            if (FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp"))))
            {
                IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
            }
            PC->ResetIgnoreLookInput();
            PC->SetControlRotation(FRotator::ZeroRotator);
            bDone = true;
            return;
        }
        static const TCHAR* Names[] = {TEXT("right"), TEXT("left"), TEXT("up"), TEXT("down"),
            TEXT("upper_pitch_limit"), TEXT("lower_pitch_limit"), TEXT("focus_ignores_look"), TEXT("focus_return")};
        if (Phase != CurrentPhase)
        {
            CurrentPhase = Phase;
            bSent = bChecked = false;
            PC->FlushPressedKeys();
            PC->ResetIgnoreLookInput();
            PC->SetControlRotation(FRotator::ZeroRotator);
            if (Phase == 6) { PC->SetIgnoreLookInput(true); }
            if (Phase == 7) { PC->SetInputMode(FInputModeGameOnly()); }
        }
        if (InPhase >= 1.0 && !bSent)
        {
            bSent = true;
            const auto Axis = [PC](FKey Key, float Delta)
            {
                PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Axis, Delta, 1));
            };
            if (Phase < 2) { Axis(EKeys::MouseX, Phase == 0 ? 1000 : -1000); }
            else if (Phase < 6) { Axis(EKeys::MouseY, Phase == 2 ? 1000 : Phase == 3 ? -1000 : Phase == 4 ? 20000 : -20000); }
            else { Axis(EKeys::MouseX, 1000); Axis(EKeys::MouseY, 1000); }
        }
        if (InPhase >= 2.5 && !bChecked)
        {
            bChecked = true;
            const FRotator Rotation = PC->PlayerCameraManager->GetCameraRotation().GetNormalized();
            const FRotator Control = PC->GetControlRotation().GetNormalized();
            bool bPassed = false;
            if (Phase < 2) { bPassed = FMath::IsNearlyEqual(Rotation.Yaw, Phase == 0 ? 8.4 : -8.4, 0.1) && FMath::Abs(Rotation.Pitch) < 0.1; }
            else if (Phase < 4) { bPassed = FMath::IsNearlyEqual(Rotation.Pitch, Phase == 2 ? 8.4 : -8.4, 0.1) && FMath::Abs(Rotation.Yaw) < 0.1; }
            else if (Phase < 6) { bPassed = FMath::IsNearlyEqual(Rotation.Pitch, Phase == 4 ? PC->PlayerCameraManager->ViewPitchMax : PC->PlayerCameraManager->ViewPitchMin, 0.1); }
            else if (Phase == 6) { bPassed = FMath::Abs(Rotation.Pitch) < 0.1 && FMath::Abs(Rotation.Yaw) < 0.1; }
            else { bPassed = FMath::IsNearlyEqual(Rotation.Pitch, 8.4, 0.1) && FMath::IsNearlyEqual(Rotation.Yaw, 8.4, 0.1); }
            bPassed &= Rotation.Equals(Control, 0.1);
            const FString Detail = FString::Printf(TEXT("camera yaw=%.3f pitch=%.3f control yaw=%.3f pitch=%.3f"), Rotation.Yaw, Rotation.Pitch, Control.Yaw, Control.Pitch);
            Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"), Names[Phase], bPassed ? TEXT("true") : TEXT("false"), *Detail));
            UE_LOG(LogValStrike, Display, TEXT("VSLOOK %s %s %s"), Names[Phase], bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
            GEngine->AddOnScreenDebugMessage(98765, 2.0f, bPassed ? FColor::Green : FColor::Red,
                FString::Printf(TEXT("Mouse look: %s | %s"), Names[Phase], *Detail));
            FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Output, TEXT("")) + TEXT("_") + Names[Phase] + TEXT(".png"), true, false);
        }
    }
    FDelegateHandle Delegate;
    FString Output;
    TArray<FString> Checks;
    double Began = -1;
    int32 CurrentPhase = -1;
    bool bSent = false, bChecked = false, bDone = false;
};
TUniquePtr<FVSCombatLookSmoke> LookSmoke;
}

void ValStrikeTests::StartCombatLookSmoke()
{
    if (!LookSmoke && FParse::Param(FCommandLine::Get(), TEXT("VSCombatLookSmoke")))
    {
        LookSmoke = MakeUnique<FVSCombatLookSmoke>();
    }
}
#else
void ValStrikeTests::StartCombatLookSmoke() {}
#endif
