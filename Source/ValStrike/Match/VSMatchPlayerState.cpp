#include "VSMatchPlayerState.h"

#include "Net/UnrealNetwork.h"

void AVSMatchPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSMatchPlayerState, ConnectionId);
    DOREPLIFETIME(AVSMatchPlayerState, TeamId);
    DOREPLIFETIME(AVSMatchPlayerState, bParticipating);
    DOREPLIFETIME(AVSMatchPlayerState, bAlive);
    DOREPLIFETIME_CONDITION(AVSMatchPlayerState, InteractionPrompt, COND_OwnerOnly);
}
