#include "VSMatchGameState.h"

#include "Net/UnrealNetwork.h"

void AVSMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSMatchGameState, Match);
}
