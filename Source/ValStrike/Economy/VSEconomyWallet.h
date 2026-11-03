#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "VSEconomyTypes.h"
#include "VSEconomyWallet.generated.h"

/**
 * One per player, owned by that player's PlayerController and relevant only to it.
 * It carries the player's replicated credits/ownership and the purchase RPC. The server
 * economy subsystem spawns it, writes it and decides every request; clients only ask.
 */
UCLASS(NotPlaceable, Transient)
class VALSTRIKE_API AVSEconomyWallet : public AInfo
{
    GENERATED_BODY()

public:
    AVSEconomyWallet();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    const FVSEconomyWalletView& GetView() const { return View; }
    int32 GetPrice(EVSEconomyItem Item) const;
    bool IsStarter(EVSEconomyItem Item) const;

    // Owning client: send a request naming the replicated round. False when nothing was sent.
    bool RequestPurchase(EVSEconomyItem Item) { return SendRequest(EVSEconomyRequest::Purchase, Item); }
    bool RequestRefund(EVSEconomyItem Item) { return SendRequest(EVSEconomyRequest::Refund, Item); }
    uint32 GetLocalRequestId() const { return LocalRequestId; }

    // The request contains context only. The subsystem validates caller, identity, sequence,
    // team, life state, buy phase, round, ownership and price before anything changes.
    UFUNCTION(Server, Reliable)
    void ServerRequest(EVSEconomyRequest Kind, EVSEconomyItem Item, uint64 RoundId, uint32 RequestId);

    // Server only: written exclusively by UVSEconomySubsystem.
    void ServerSetCatalog(const TArray<int32>& InPrices, uint32 InStarterMask);
    bool ServerPublish(const FVSEconomyWalletView& NewView);

private:
    bool SendRequest(EVSEconomyRequest Kind, EVSEconomyItem Item);

    UPROPERTY(Replicated)
    FVSEconomyWalletView View;

    UPROPERTY(Replicated)
    TArray<int32> Prices; // Indexed by EVSEconomyItem, from the server's tuning table.

    UPROPERTY(Replicated)
    uint32 StarterMask = 0;

    uint32 LocalRequestId = 0;
};
