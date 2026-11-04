#include "VSSessionState.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "../VSPlayerController.h"
#include "../ValStrike.h"

void UVSSessionState::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UVSSessionState::NetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UVSSessionState::TravelFailure);
    }
    ConnectWatchHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVSSessionState::WatchConnectAttempt));
}

void UVSSessionState::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    FTSTicker::RemoveTicker(ConnectWatchHandle);
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
    EndedAttemptTicks = 0;
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
    // The server's refusal text for a private server (Server/VSServerProvider). It
    // never contains either key; show actionable guidance instead of "unreachable".
    if (Error.Contains(TEXT("Join key missing or wrong")))
    {
        ShowFailure(NSLOCTEXT("ValStrike", "JoinKeyRefused", "Join key missing or wrong. Enter the key the host gave you, then connect again."));
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

bool UVSSessionState::WatchConnectAttempt(float DeltaTime)
{
    // Cancel and failure handlers clear bConnecting first, so this sees only an
    // attempt the engine ended silently (for example a pending net game torn
    // down through HandleDisconnect or CancelPending without a broadcast).
    const UGameInstance* GameInstance = bConnecting ? GetGameInstance() : nullptr;
    const FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
    if (!Context)
    {
        EndedAttemptTicks = 0;
        return true;
    }
    const UWorld* World = Context->World();
    const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
    // In flight: travel queued (ClientTravel sets TravelURL before Browse creates
    // the pending net game), pending handshake, a map still loading, or arrival in
    // the server's world before the new controller clears bConnecting.
    const bool bInFlight = !World || Context->PendingNetGame || !Context->TravelURL.IsEmpty() || (Driver && Driver->ServerConnection);
    if (bInFlight)
    {
        EndedAttemptTicks = 0;
        return true;
    }
    // Require two consecutive ticks so no single-frame engine transition can
    // be mistaken for an ended attempt.
    if (++EndedAttemptTicks < 2)
    {
        return true;
    }
    UE_LOG(LogValStrike, Warning, TEXT("Connection attempt to %s ended without a network or travel failure; showing a connection error."), *Address);
    ShowFailure(NSLOCTEXT("ValStrike", "ConnectEnded", "The connection attempt ended before it completed. Check the server address and port, then connect again."));
    return true;
}
