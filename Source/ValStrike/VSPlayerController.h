#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VSPlayerController.generated.h"

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
    void ConnectToServer(const FString& Address);
    void CancelConnection();
    void DisconnectFromServer();
    void QuitSession();
    bool CanDisconnect() const;
    UVSSessionState* GetSessionState() const;

private:
    void CreateSessionMenu();
    void ApplyMenuFocus();
    TSharedPtr<SVSSessionMenu> SessionMenu;
    bool bSessionMenuOpen = false;
};
