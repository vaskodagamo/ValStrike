#include "VSEconomyClientSubsystem.h"

#include "SVSEconomyHUD.h"
#include "../VSEconomyWallet.h"
#include "../../Match/VSMatchGameState.h"
#include "../../Rules/VSMatchRules.h"
#include "../../VSCharacter.h"
#include "../../VSPlayerController.h"
#include "../../ValStrike.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

bool UVSEconomyClientSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UVSEconomyClientSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UVSEconomyClientSubsystem::Deinitialize()
{
    Detach();
    Super::Deinitialize();
}

TStatId UVSEconomyClientSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVSEconomyClientSubsystem, STATGROUP_Tickables);
}

AVSEconomyWallet* UVSEconomyClientSubsystem::GetLocalWallet() const { return Wallet.Get(); }
AVSPlayerController* UVSEconomyClientSubsystem::GetLocalController() const { return Controller.Get(); }
bool UVSEconomyClientSubsystem::HasPresentation() const { return Widget.IsValid(); }

bool UVSEconomyClientSubsystem::IsSessionMenuOpen() const
{
    return Controller.IsValid() && Controller->IsSessionMenuOpen();
}

void UVSEconomyClientSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer) { return; }
    AVSPlayerController* Local = Cast<AVSPlayerController>(World->GetFirstPlayerController());
    if (Local && !Local->IsLocalController()) { Local = nullptr; }
    AVSEconomyWallet* Found = Wallet.Get();
    if (!Local || !Found || Found->GetOwner() != Local)
    {
        Found = nullptr;
        for (TActorIterator<AVSEconomyWallet> It(World); It && Local; ++It)
        {
            if (It->GetOwner() == Local) { Found = *It; break; }
        }
    }
    // Practice mode never spawns wallets, so nothing is shown there.
    if (!Local || !Found) { Detach(); return; }
    if (Controller.Get() != Local || Wallet.Get() != Found || !Widget) { Attach(Local, Found); }
    if (!bMenuOpen) { return; }
    if (Local->IsSessionMenuOpen())
    {
        // The session menu took focus (Escape): yield without touching its input mode.
        SetBuyMenuOpen(false);
    }
    else if (!CanOpenBuyMenu())
    {
        SetBuyMenuOpen(false); // The buy phase ended, or this player can no longer buy.
    }
    else if (!Local->IsLookInputIgnored())
    {
        // A pawn restart reset the controller's input state while the menu was open.
        ApplyMenuInput();
    }
}

void UVSEconomyClientSubsystem::Attach(AVSPlayerController* NewController, AVSEconomyWallet* NewWallet)
{
    Detach();
    Controller = NewController;
    Wallet = NewWallet;
    UWorld* World = GetWorld();
    UGameViewportClient* GameViewport = World ? World->GetGameViewport() : nullptr;
    ULocalPlayer* LocalPlayer = NewController->GetLocalPlayer();
    if (GameViewport && LocalPlayer)
    {
        SAssignNew(Widget, SVSEconomyHUD).Economy(this);
        // Below the session menu (100), above the canvas HUD.
        GameViewport->AddViewportWidgetForPlayer(LocalPlayer, Widget.ToSharedRef(), 50);
        Viewport = GameViewport;
        Player = LocalPlayer;
    }
    Input = NewObject<UInputComponent>(NewController, TEXT("VSEconomyInput"));
    Input->bBlockInput = false;
    Input->BindKey(EKeys::B, IE_Pressed, this, &UVSEconomyClientSubsystem::ToggleBuyMenu);
    NewController->PushInputComponent(Input);
    UE_LOG(LogValStrike, Display, TEXT("VSECON client attached player=%llu widget=%d"),
        NewWallet->GetView().PlayerId, Widget ? 1 : 0);
}

void UVSEconomyClientSubsystem::Detach()
{
    if (bMenuOpen) { SetBuyMenuOpen(false); }
    if (Widget && Viewport.IsValid() && Player.IsValid())
    {
        Viewport->RemoveViewportWidgetForPlayer(Player.Get(), Widget.ToSharedRef());
    }
    Widget.Reset();
    if (Input && Controller.IsValid()) { Controller->PopInputComponent(Input); }
    Input = nullptr;
    Controller.Reset();
    Wallet.Reset();
    Viewport.Reset();
    Player.Reset();
}

bool UVSEconomyClientSubsystem::CanOpenBuyMenu() const
{
    const AVSPlayerController* Local = Controller.Get();
    const AVSEconomyWallet* LocalWallet = Wallet.Get();
    const UWorld* World = GetWorld();
    const AVSMatchGameState* GS = World ? World->GetGameState<AVSMatchGameState>() : nullptr;
    // Presentation gate only: the server still judges every request.
    return Local && LocalWallet && Widget && !Local->IsSessionMenuOpen() && GS && GS->Match.bReady &&
        GS->Match.Phase == static_cast<uint8>(ValStrike::Rules::Phase::Preparation) &&
        LocalWallet->GetView().bBuyOpen && LocalWallet->GetView().RoundId == GS->Match.RoundId;
}

void UVSEconomyClientSubsystem::ApplyMenuInput()
{
    AVSPlayerController* Local = Controller.Get();
    if (!Local || !Widget) { return; }
    // Balanced with SetBuyMenuOpen(false); preserves other systems' ignore requests.
    Local->SetIgnoreMoveInput(true);
    Local->SetIgnoreLookInput(true);
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(Widget);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    Local->SetInputMode(Mode);
    Local->bShowMouseCursor = true;
}

void UVSEconomyClientSubsystem::SetBuyMenuOpen(bool bOpen)
{
    AVSPlayerController* Local = Controller.Get();
    if (bOpen == bMenuOpen || (bOpen && !CanOpenBuyMenu()))
    {
        return;
    }
    bMenuOpen = bOpen;
    if (!Local) { return; }
    if (bOpen)
    {
        // Shared UI contract: release held gameplay input before taking focus.
        Local->CancelObjectiveInput();
        if (AVSCharacter* Character = Cast<AVSCharacter>(Local->GetPawn())) { Character->StopCombatInput(); }
        Local->FlushPressedKeys();
        ApplyMenuInput();
    }
    else
    {
        Local->SetIgnoreMoveInput(false);
        Local->SetIgnoreLookInput(false);
        if (!Local->IsSessionMenuOpen())
        {
            Local->SetInputMode(FInputModeGameOnly());
            Local->bShowMouseCursor = false;
        }
    }
    UE_LOG(LogValStrike, Display, TEXT("VSECON client menu open=%d"), bOpen ? 1 : 0);
}

void UVSEconomyClientSubsystem::ToggleBuyMenu()
{
    SetBuyMenuOpen(!bMenuOpen);
}

bool UVSEconomyClientSubsystem::ActivateItem(EVSEconomyItem Item)
{
    AVSEconomyWallet* LocalWallet = Wallet.Get();
    if (!LocalWallet || !bMenuOpen) { return false; }
    return LocalWallet->GetView().CanRefund(Item) ? LocalWallet->RequestRefund(Item) : LocalWallet->RequestPurchase(Item);
}

double UVSEconomyClientSubsystem::GetBuySecondsRemaining() const
{
    const UWorld* World = GetWorld();
    const AVSMatchGameState* GS = World ? World->GetGameState<AVSMatchGameState>() : nullptr;
    const AVSEconomyWallet* LocalWallet = Wallet.Get();
    if (!GS || !LocalWallet || !LocalWallet->GetView().bBuyOpen || !FMath::IsFinite(GS->Match.PhaseDeadlineSeconds))
    {
        return -1.0;
    }
    return FMath::Max(0.0, GS->Match.PhaseDeadlineSeconds - GS->GetServerWorldTimeSeconds());
}
