#pragma once

#include "CoreMinimal.h"
#include "../VSPlayerController.h"
#include "VSMatchPlayerController.generated.h"

UENUM()
enum class EVSMatchCommand : uint8
{
    Press,
    Release,
    Drop,
    Rematch
};

UCLASS()
class VALSTRIKE_API AVSMatchPlayerController : public AVSPlayerController
{
    GENERATED_BODY()

public:
    using AVSPlayerController::InputKey;
    virtual void BeginPlay() override;
    virtual bool NotifyServerReceivedClientData(APawn* InPawn, float TimeStamp) override;
    virtual void SetupInputComponent() override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    virtual FVSMatchHUDData GetMatchHUDData() const override;
    virtual void CancelObjectiveInput() override;
    virtual void RequestMatchRematch() override;

    // Requests contain context only; GameMode validates ownership, sequence,
    // round/device identity, phase and authoritative world eligibility.
    UFUNCTION(Server, Reliable)
    void ServerCommand(EVSMatchCommand Command, uint32 RequestId, uint64 RoundId, uint64 DeviceRevision);

    UFUNCTION(Client, Reliable)
    void ClientCancelObjective();

private:
    void ObjectivePressed();
    void ObjectiveReleased();
    void DropObjective();
    bool SendLocalCommand(EVSMatchCommand Command);

    uint32 LocalRequestId = 0;
    bool bObjectiveHeld = false;
    // Only a raw pressed event grants this permit. FlushPressedKeys can synthesize
    // releases, and a subsequent repeat can synthesize a binding-level press.
    bool bFreshObjectivePress = false;
};
