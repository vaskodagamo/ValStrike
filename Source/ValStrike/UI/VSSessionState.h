#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VSSessionState.generated.h"

class UNetDriver;

/** Local connection feedback survives the engine's return-to-default-map travel. */
UCLASS()
class UVSSessionState : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FString Address;
    // Optional private-server key, kept in memory only so the field survives travel.
    // Never saved, logged or shown unmasked.
    FString JoinKey;
    FText Status;
    bool bConnecting = false;
    bool bError = false;
    bool bOpenMenuOnArrival = false;

    void SetStatus(const FText& Message, bool bIsError = false);

private:
    void NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
    void TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
    void ShowFailure(const FText& Message);
    // Catches a connect attempt that ends without a network or travel failure
    // broadcast, so the menu can never stay at "Connecting…".
    bool WatchConnectAttempt(float DeltaTime);
    FTSTicker::FDelegateHandle ConnectWatchHandle;
    int32 EndedAttemptTicks = 0;
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
};
