#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Feedback/VSHitFeedbackState.h"
#include "Feedback/VSHitFeedbackView.h"
#include "GameFramework/PlayerController.h"
#include "VSMatchHUDData.h"
#include "VSPlayerController.generated.h"

class APlayerState;
class SVSSessionMenu;
class UVSSessionState;

UCLASS()
class VALSTRIKE_API AVSPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;

    bool IsSessionMenuOpen() const { return bSessionMenuOpen; }
    void SetSessionMenuOpen(bool bOpen);
    void ToggleSessionMenu();
    // JoinKey comes from its own field (empty: none) and is never parsed from Address.
    void ConnectToServer(const FString& Address, const FString& JoinKey = FString());
    void CancelConnection();
    void DisconnectFromServer();
    void QuitSession();
    bool CanDisconnect() const;
    UVSSessionState* GetSessionState() const;
    TSharedPtr<SVSSessionMenu> GetSessionMenu() const { return SessionMenu; }

    // Client-local look sensitivity from this machine's settings; never replicated.
    float GetMouseSensitivity() const;
    void SetMouseSensitivity(float Value);
    void SaveUserSettings();

    // Practice mode has no match data. The match controller overrides these hooks.
    virtual FVSMatchHUDData GetMatchHUDData() const;
    virtual void CancelObjectiveInput();
    virtual void RequestMatchRematch();

    // Server: route one confirmed damage event to this controller's owning client
    // only (UVSHitFeedbackSubsystem calls these). Kills go reliable, hits unreliable.
    void SendHitConfirmed(APawn* ShooterPawn, bool bHeadshot, bool bKilled, float Damage);
    void SendDamageTaken(APawn* VictimPawn, const APawn* InstigatorPawn, APlayerState* Killer, uint8 Weapon,
        bool bHeadshot, bool bKilled, float Damage);
    // Local HUD snapshot of server-confirmed feedback for the current pawn.
    FVSHitFeedbackView GetHitFeedbackView() const;

    // ForPawn is the receiving player's pawn on the server; events for another
    // life are dropped. Flags are VSHitFeedback::Flag bits.
    UFUNCTION(Client, Unreliable)
    void ClientHitConfirmed(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage);
    UFUNCTION(Client, Reliable)
    void ClientKillConfirmed(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage);
    UFUNCTION(Client, Unreliable)
    void ClientDamageTaken(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage, FVector_NetQuantize Source);
    UFUNCTION(Client, Reliable)
    void ClientEliminated(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage, FVector_NetQuantize Source,
        APlayerState* Killer, uint8 Weapon);

private:
    void ReceiveHitConfirmed(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage);
    void ReceiveDamageTaken(uint32 Sequence, APawn* ForPawn, uint8 Flags, float Damage, const FVector& Source,
        APlayerState* Killer, uint8 Weapon);
    // Client: a new or lost pawn clears all feedback (respawn, round reset).
    bool SyncFeedbackLife();
    double FeedbackNow() const;

    void CreateSessionMenu();
    void ApplyMenuFocus();
    TSharedPtr<SVSSessionMenu> SessionMenu;
    bool bSessionMenuOpen = false;
    mutable TOptional<float> MouseSensitivity;
    bool bUserSettingsDirty = false;

    uint32 FeedbackSequence = 0; // Server: last sequence sent to this client.
    VSHitFeedback::State Feedback; // Client: confirmed feedback for the local player.
    TWeakObjectPtr<APawn> FeedbackPawn;
    FString KillerName;
};
