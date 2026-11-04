#include "VSPlayerController.h"

#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Damage/VSDamageModel.h"
#include "Scoreboard/VSScoreboardModel.h"
#include "UI/SVSSessionMenu.h"
#include "UI/VSJoinSmoke.h"
#include "UI/VSServerAddress.h"
#include "UI/VSSessionState.h"
#include "UI/VSSettingsSmoke.h"
#include "UI/VSUISmoke.h"
#include "UI/VSUserSettings.h"
#include "VSCharacter.h"
#include "ValStrike.h"

void AVSPlayerController::BeginPlay()
{
    Super::BeginPlay();
    CreateSessionMenu();
}

void AVSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AVSPlayerController::ToggleSessionMenu);
}

void AVSPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    // Local player/viewport can become available after BeginPlay during travel.
    CreateSessionMenu();
    SyncFeedbackLife();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    VSUISmoke::Tick(this, DeltaTime);
    VSSettingsSmoke::Tick(this, DeltaTime);
    VSJoinSmoke::Tick(this, DeltaTime);
#endif
}

UVSSessionState* AVSPlayerController::GetSessionState() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UVSSessionState>() : nullptr;
}

float AVSPlayerController::GetMouseSensitivity() const
{
    if (!MouseSensitivity.IsSet())
    {
        MouseSensitivity = VSUserSettings::LoadMouseSensitivity();
    }
    return MouseSensitivity.GetValue();
}

void AVSPlayerController::SetMouseSensitivity(float Value)
{
    const float Clamped = VSUserSettings::ClampMouseSensitivity(Value);
    if (Clamped != GetMouseSensitivity())
    {
        MouseSensitivity = Clamped;
        // In memory now, so travel keeps it; written to disk by SaveUserSettings.
        VSUserSettings::StoreMouseSensitivity(Clamped);
        bUserSettingsDirty = true;
    }
}

void AVSPlayerController::SaveUserSettings()
{
    if (bUserSettingsDirty)
    {
        bUserSettingsDirty = !VSUserSettings::SaveUserSettings();
    }
}

FVSMatchHUDData AVSPlayerController::GetMatchHUDData() const
{
    return {};
}

void AVSPlayerController::CancelObjectiveInput() {}
void AVSPlayerController::RequestMatchRematch() {}

void AVSPlayerController::SendHitConfirmed(APawn* ShooterPawn, bool bHeadshot, bool bKilled, float Damage)
{
    if (!HasAuthority() || !ShooterPawn)
    {
        return;
    }
    using namespace VSHitFeedback;
    const uint8 Flags = static_cast<uint8>((bHeadshot ? FlagHeadshot : 0) | (bKilled ? FlagKilled : 0));
    if (bKilled) { ClientKillConfirmed(++FeedbackSequence, ShooterPawn, Flags, Damage); }
    else { ClientHitConfirmed(++FeedbackSequence, ShooterPawn, Flags, Damage); }
}

void AVSPlayerController::SendDamageTaken(APawn* VictimPawn, const APawn* InstigatorPawn, APlayerState* Killer,
    uint8 Weapon, bool bHeadshot, bool bKilled, float Damage)
{
    if (!HasAuthority() || !VictimPawn)
    {
        return;
    }
    using namespace VSHitFeedback;
    // Only the instigator's location travels; the client turns it into a direction.
    const bool bSource = InstigatorPawn && InstigatorPawn != VictimPawn;
    const FVector Source = bSource ? InstigatorPawn->GetActorLocation() : FVector::ZeroVector;
    const uint8 Flags = static_cast<uint8>(
        (bHeadshot ? FlagHeadshot : 0) | (bKilled ? FlagKilled : 0) | (bSource ? FlagHasSource : 0));
    if (bKilled) { ClientEliminated(++FeedbackSequence, VictimPawn, Flags, Damage, Source, Killer, Weapon); }
    else { ClientDamageTaken(++FeedbackSequence, VictimPawn, Flags, Damage, Source); }
}

void AVSPlayerController::ClientHitConfirmed_Implementation(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage)
{
    ReceiveHitConfirmed(Sequence, ForPawn, Flags, Damage);
}

void AVSPlayerController::ClientKillConfirmed_Implementation(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage)
{
    ReceiveHitConfirmed(Sequence, ForPawn, Flags, Damage);
}

void AVSPlayerController::ClientDamageTaken_Implementation(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage,
    FVector_NetQuantize Source)
{
    ReceiveDamageTaken(Sequence, ForPawn, Flags, Damage, Source, nullptr, 0);
}

void AVSPlayerController::ClientEliminated_Implementation(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage,
    FVector_NetQuantize Source, APlayerState* Killer, uint8 Weapon)
{
    ReceiveDamageTaken(Sequence, ForPawn, Flags, Damage, Source, Killer, Weapon);
}

double AVSPlayerController::FeedbackNow() const
{
    // Real time: feedback lifetimes are presentation and ignore pause/dilation.
    return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
}

bool AVSPlayerController::SyncFeedbackLife()
{
    if (!IsLocalController())
    {
        return false;
    }
    if (GetPawn() != FeedbackPawn.Get())
    {
        FeedbackPawn = GetPawn();
        Feedback.BeginLife();
        KillerName.Reset();
    }
    return FeedbackPawn.IsValid();
}

void AVSPlayerController::ReceiveHitConfirmed(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage)
{
    // Unresolved or previous-life pawns drop the event rather than mark a new life.
    if (!SyncFeedbackLife() || !ForPawn || ForPawn != FeedbackPawn.Get())
    {
        return;
    }
    VSHitFeedback::Event Event;
    Event.sequence = Sequence;
    Event.flags = static_cast<uint8>(Flags & (VSHitFeedback::FlagHeadshot | VSHitFeedback::FlagKilled));
    Event.damage = FMath::IsFinite(Damage) ? FMath::Max(0.0f, Damage) : 0.0f;
    Feedback.OnHitConfirmed(Event, FeedbackNow());
}

void AVSPlayerController::ReceiveDamageTaken(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage,
    const FVector& Source, APlayerState* Killer, uint8 Weapon)
{
    if (!SyncFeedbackLife() || !ForPawn || ForPawn != FeedbackPawn.Get())
    {
        return;
    }
    VSHitFeedback::Event Event;
    Event.sequence = Sequence;
    Event.flags = static_cast<uint8>(Flags & (VSHitFeedback::FlagHeadshot | VSHitFeedback::FlagKilled | VSHitFeedback::FlagHasSource));
    Event.damage = FMath::IsFinite(Damage) ? FMath::Max(0.0f, Damage) : 0.0f;
    const FVector Offset = Source - ForPawn->GetActorLocation();
    if (Offset.SizeSquared2D() < 1.0f) { Event.flags = static_cast<uint8>(Event.flags & ~VSHitFeedback::FlagHasSource); }
    Event.sourceYawDeg = static_cast<float>(Offset.Rotation().Yaw);
    Event.weapon = Weapon;
    const bool bWasEliminated = Feedback.CurrentDeath().eliminated;
    if (Feedback.OnDamageTaken(Event, FeedbackNow()) == VSHitFeedback::Outcome::Shown &&
        !bWasEliminated && Feedback.CurrentDeath().eliminated)
    {
        KillerName = Killer ? VSScoreboardModel::DisplayName(Killer->GetPlayerName()) : FString();
    }
}

FVSHitFeedbackView AVSPlayerController::GetHitFeedbackView() const
{
    FVSHitFeedbackView View;
    if (!IsLocalController() || !GetPawn() || GetPawn() != FeedbackPawn.Get())
    {
        return View;
    }
    const double Now = FeedbackNow();
    const VSHitFeedback::Marker Marker = Feedback.CurrentMarker(Now);
    View.Marker = Marker.kind;
    View.MarkerAlpha = Marker.alpha;
    View.MarkerDamage = Marker.damage;

    VSHitFeedback::Indicator Indicators[VSHitFeedback::MaxIndicators];
    const int32 Count = static_cast<int32>(Feedback.CurrentIndicators(Now, Indicators, VSHitFeedback::MaxIndicators));
    const float ViewYaw = static_cast<float>(GetControlRotation().Yaw);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        View.Indicators.Add({VSHitFeedback::DeltaDegrees(ViewYaw, Indicators[Index].sourceYawDeg), Indicators[Index].alpha});
    }

    const VSHitFeedback::Death& Death = Feedback.CurrentDeath();
    View.bEliminated = Death.eliminated;
    if (Death.eliminated)
    {
        namespace VSD = ValStrike::Damage;
        const VSD::WeaponClass Weapon = static_cast<VSD::WeaponClass>(Death.weapon);
        View.EliminatedText = KillerName.IsEmpty() ? FString(TEXT("Eliminated"))
            : FString::Printf(TEXT("Eliminated by %s"), *KillerName);
        if (VSD::IsValidWeapon(Weapon))
        {
            View.EliminatedText += FString(TEXT(" \u00B7 ")) + FString(UTF8_TO_TCHAR(VSD::WeaponName(Weapon)));
        }
        if (Death.headshot) { View.EliminatedText += TEXT(" \u00B7 headshot"); }
    }
    return View;
}

void AVSPlayerController::CreateSessionMenu()
{
    if (SessionMenu || !IsLocalController() || !GetLocalPlayer() || !GetWorld()->GetGameViewport())
    {
        return;
    }
    SAssignNew(SessionMenu, SVSSessionMenu).Controller(this);
    SessionMenu->SetVisibility(EVisibility::Collapsed);
    GetWorld()->GetGameViewport()->AddViewportWidgetForPlayer(GetLocalPlayer(), SessionMenu.ToSharedRef(), 100);
    if (UVSSessionState* State = GetSessionState())
    {
        if (State->bConnecting && GetNetMode() == NM_Client)
        {
            State->bConnecting = false;
            State->SetStatus(NSLOCTEXT("ValStrike", "Connected", "Connected to the server."));
        }
        if (State->bOpenMenuOnArrival)
        {
            State->bOpenMenuOnArrival = false;
            SetSessionMenuOpen(true);
        }
    }
}

void AVSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    SaveUserSettings();
    if (SessionMenu && GetWorld()->GetGameViewport() && GetLocalPlayer())
    {
        GetWorld()->GetGameViewport()->RemoveViewportWidgetForPlayer(GetLocalPlayer(), SessionMenu.ToSharedRef());
    }
    SessionMenu.Reset();
    Super::EndPlay(EndPlayReason);
}

void AVSPlayerController::ApplyMenuFocus()
{
    if (!SessionMenu)
    {
        return;
    }
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(SessionMenu->InitialFocus());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    bShowMouseCursor = true;
}

void AVSPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
    Super::ClientRestart_Implementation(NewPawn);
    // The existing pawn installs GameOnly when binding its input. Preserve menu
    // ownership if a respawn arrives while the local menu is already open.
    if (bSessionMenuOpen)
    {
        CancelObjectiveInput();
        if (AVSCharacter* Character = Cast<AVSCharacter>(NewPawn))
        {
            Character->StopCombatInput();
        }
        FlushPressedKeys();
        // Super resets the ignore counters as part of restarting the pawn.
        SetIgnoreMoveInput(true);
        SetIgnoreLookInput(true);
        ApplyMenuFocus();
    }
}

void AVSPlayerController::SetSessionMenuOpen(bool bOpen)
{
    if (!IsLocalController() || bSessionMenuOpen == bOpen)
    {
        return;
    }
    CreateSessionMenu();
    if (!SessionMenu)
    {
        return;
    }
    if (bOpen)
    {
        CancelObjectiveInput();
        if (AVSCharacter* Character = Cast<AVSCharacter>(GetPawn()))
        {
            Character->StopCombatInput();
        }
    }
    else
    {
        SessionMenu->CloseSettings(false);
        SaveUserSettings();
    }
    FlushPressedKeys();
    bSessionMenuOpen = bOpen;
    // Balanced calls preserve ignore-input requests from other systems.
    SetIgnoreMoveInput(bOpen);
    SetIgnoreLookInput(bOpen);
    SessionMenu->SetVisibility(bOpen ? EVisibility::Visible : EVisibility::Collapsed);
    if (bOpen)
    {
        ApplyMenuFocus();
    }
    else
    {
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
    }
}

void AVSPlayerController::ToggleSessionMenu()
{
    const UVSSessionState* State = GetSessionState();
    if (!bSessionMenuOpen || !State || !State->bConnecting)
    {
        SetSessionMenuOpen(!bSessionMenuOpen);
    }
}

void AVSPlayerController::ConnectToServer(const FString& Address, const FString& JoinKey)
{
    UVSSessionState* State = GetSessionState();
    if (!IsLocalController() || !State || State->bConnecting)
    {
        return;
    }
    FString ValidAddress;
    FText Error;
    if (!VSServerAddress::Validate(Address, ValidAddress, Error))
    {
        // Never take a key from the address text; point to the key field instead.
        if (Address.Contains(TEXT("JoinKey"), ESearchCase::IgnoreCase))
        {
            Error = NSLOCTEXT("ValStrike", "KeyInAddress", "Enter the join key in the Join key field, not in the server address.");
        }
        State->SetStatus(Error, true);
        return;
    }
    FString Key;
    FString Destination;
    if (!VSServerAddress::ValidateJoinKey(JoinKey, Key, Error))
    {
        State->SetStatus(Error, true);
        return;
    }
    if (!VSServerAddress::JoinDestination(ValidAddress, Key, Destination))
    {
        State->SetStatus(NSLOCTEXT("ValStrike", "DestinationInvalid", "This address cannot be joined. Check it and try again."), true);
        return;
    }
    State->Address = ValidAddress;
    State->JoinKey = Key;
    State->bOpenMenuOnArrival = false;
    State->bConnecting = true;
    // Status text, like every log line here, names the address only, never the key.
    State->SetStatus(FText::Format(Key.IsEmpty()
        ? NSLOCTEXT("ValStrike", "Connecting", "Connecting to {0}…")
        : NSLOCTEXT("ValStrike", "ConnectingWithKey", "Connecting to {0} with a join key…"), FText::FromString(ValidAddress)));
    SetSessionMenuOpen(true);
    // Validated host:port plus, at most, the validated JoinKey option. Never
    // execute player-supplied console text or other URL options.
    ClientTravel(FString(TEXT("unreal://")) + Destination, TRAVEL_Absolute);
}

void AVSPlayerController::CancelConnection()
{
    UVSSessionState* State = GetSessionState();
    if (!IsLocalController() || !State || !State->bConnecting || !GEngine)
    {
        return;
    }
    if (FWorldContext* Context = GEngine->GetWorldContextFromWorld(GetWorld()))
    {
        Context->TravelURL.Empty();
        GEngine->CancelPending(GetWorld());
    }
    State->bConnecting = false;
    State->bOpenMenuOnArrival = true;
    State->SetStatus(NSLOCTEXT("ValStrike", "Cancelled", "Connection cancelled. You can edit the address and try again."));
    // Starting a replacement connection can already destroy the previous
    // world's net driver. Return to a fresh local map, never resume stale state.
    GEngine->HandleDisconnect(GetWorld(), GetWorld()->GetNetDriver());
}

bool AVSPlayerController::CanDisconnect() const
{
    return GetNetMode() == NM_Client || GetNetMode() == NM_ListenServer;
}

void AVSPlayerController::DisconnectFromServer()
{
    if (!IsLocalController() || !CanDisconnect() || !GEngine)
    {
        return;
    }
    if (UVSSessionState* State = GetSessionState())
    {
        State->bConnecting = false;
        State->bOpenMenuOnArrival = true;
        State->SetStatus(NSLOCTEXT("ValStrike", "Disconnected", "Disconnected. Enter a server address to join another session."));
    }
    GEngine->HandleDisconnect(GetWorld(), GetWorld()->GetNetDriver());
}

void AVSPlayerController::QuitSession()
{
    if (IsLocalController())
    {
        SaveUserSettings();
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
    }
}
