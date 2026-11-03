#include "VSSessionState.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "../VSPlayerController.h"

void UVSSessionState::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UVSSessionState::NetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UVSSessionState::TravelFailure);
    }
}

void UVSSessionState::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    Super::Deinitialize();
}

void UVSSessionState::SetStatus(const FText& Message, bool bIsError)
{
    Status = Message;
    bError = bIsError;
}

void UVSSessionState::ShowFailure(const FText& Message)
{
    bConnecting = false;
    bOpenMenuOnArrival = true;
    SetStatus(Message, true);
    if (AVSPlayerController* PC = Cast<AVSPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))
    {
        PC->SetSessionMenuOpen(true);
    }
}

void UVSSessionState::NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
    // Engine delegates are global in PIE. Never surface another world's failure,
    // or a remote player's disconnect as a failure of a local listen host.
    bool bOwnFailure = World && World->GetGameInstance() == GetGameInstance();
    if (!World && Driver && GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.OwningGameInstance == GetGameInstance() && Context.PendingNetGame && Context.PendingNetGame->NetDriver == Driver)
            {
                bOwnFailure = true;
                break;
            }
        }
    }
    if (!bOwnFailure || !Driver || Driver->GetNetMode() != NM_Client)
    {
        return;
    }
    switch (Type)
    {
    case ENetworkFailure::ConnectionTimeout:
    case ENetworkFailure::PendingConnectionFailure:
        ShowFailure(NSLOCTEXT("ValStrike", "Unreachable", "Could not reach the server. Check its address and port, and ask the host to confirm it is running."));
        break;
    case ENetworkFailure::ConnectionLost:
        ShowFailure(NSLOCTEXT("ValStrike", "ConnectionLost", "Connection to the server was lost. Check your connection, then connect again."));
        break;
    case ENetworkFailure::OutdatedClient:
    case ENetworkFailure::OutdatedServer:
    case ENetworkFailure::NetGuidMismatch:
    case ENetworkFailure::NetChecksumMismatch:
        ShowFailure(NSLOCTEXT("ValStrike", "VersionMismatch", "This client and server are incompatible. Use the same ValStrike build as the host."));
        break;
    default:
        ShowFailure(NSLOCTEXT("ValStrike", "ConnectionRejected", "The connection failed or was rejected. Check the server address and confirm the host is accepting players with this build."));
        break;
    }
}

void UVSSessionState::TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
    if (World && World->GetGameInstance() == GetGameInstance() && World->GetNetMode() != NM_DedicatedServer)
    {
        ShowFailure(NSLOCTEXT("ValStrike", "TravelFailed", "The session could not be loaded. Check that your game files and build match the server, then try again."));
    }
}
