#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "../VSEconomyTypes.h"
#include "VSEconomyClientSubsystem.generated.h"

class AVSEconomyWallet;
class AVSPlayerController;
class SVSEconomyHUD;
class UGameViewportClient;
class UInputComponent;
class ULocalPlayer;

/**
 * Local presentation for the round economy: the credits badge and the B buy menu. It adds
 * its own viewport widget and pushes its own input component onto the local controller,
 * so the HUD, controller and session menu stay untouched. It only sends requests; the
 * server decides credits and ownership. The Escape session menu always wins focus.
 */
UCLASS()
class VALSTRIKE_API UVSEconomyClientSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    AVSEconomyWallet* GetLocalWallet() const;
    AVSPlayerController* GetLocalController() const;
    bool HasPresentation() const;
    bool IsSessionMenuOpen() const;
    bool IsBuyMenuOpen() const { return bMenuOpen; }
    bool CanOpenBuyMenu() const;
    void SetBuyMenuOpen(bool bOpen);
    void ToggleBuyMenu();
    /** Buys the item, or refunds it when it was bought during this buy phase. */
    bool ActivateItem(EVSEconomyItem Item);
    /** Synchronized server seconds left in the buy phase; negative when unknown. */
    double GetBuySecondsRemaining() const;

private:
    void Attach(AVSPlayerController* NewController, AVSEconomyWallet* NewWallet);
    void Detach();
    void ApplyMenuInput();

    TWeakObjectPtr<AVSPlayerController> Controller;
    TWeakObjectPtr<AVSEconomyWallet> Wallet;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    TWeakObjectPtr<ULocalPlayer> Player;
    TSharedPtr<SVSEconomyHUD> Widget;

    UPROPERTY(Transient)
    TObjectPtr<UInputComponent> Input;

    bool bMenuOpen = false;
};
