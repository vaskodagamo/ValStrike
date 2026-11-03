#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../Rules/VSMatchRules.h"
#include "VSServerContract.h"
#include "VSServerProvider.h"
#include "VSServerSubsystem.generated.h"

class UWorld;

/**
 * Provider-neutral dedicated-server contract (decision 0002, SRV-03): allowlisted startup
 * parameters, lifecycle states, opt-in exit when done, pluggable join admission, a startup
 * version line and a structured match result. Exists only in dedicated-server processes;
 * clients, PIE and standalone games never create it.
 */
UCLASS()
class VALSTRIKE_API UVSServerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Null outside a dedicated server. */
    static UVSServerSubsystem* Get(const UObject* WorldContext);

    /** Called from PreLogin after the engine's checks; leaves OutError empty to admit the join. */
    void CheckAdmission(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
        FString& OutError) const;

    /** Called by the match game mode once per new rules effect, with the post-frame snapshot. */
    void HandleMatchEffect(const ValStrike::Rules::Effect& Effect, const ValStrike::Rules::Snapshot& Snapshot);

    ValStrike::Server::State GetState() const;
    const FString& GetSessionId() const { return SessionId; }

private:
    void HandlePostLoadMap(UWorld* World);
    bool TickServer(float DeltaSeconds);
    void Apply(const ValStrike::Server::Decision& Decision);
    void Fail(int32 ExitCode, const FString& Reason);
    void NotifyClientsClosing(ValStrike::Server::Reason Why) const;
    int32 CountPlayers() const;
    void ReportHealth(ValStrike::Server::TimeMs Now);
    void FinishMatch(const TCHAR* Status, const ValStrike::Rules::Snapshot& Snapshot);
    static ValStrike::Server::TimeMs NowMs();

    ValStrike::Server::LaunchOptions Options;
    TOptional<ValStrike::Server::Lifecycle> Lifecycle;
    TUniquePtr<IVSServerProvider> Provider;
    TUniquePtr<IVSServerAdmission> Admission;
    TWeakObjectPtr<UWorld> ServerWorld;
    FTSTicker::FDelegateHandle TickHandle;
    FDelegateHandle PostLoadMapHandle;
    FString SessionId;
    FString MapKey;
    FString ModeKey;
    int32 LastPlayers = 0;
    bool bExitRequested = false;

    ValStrike::Server::MatchRecord Record;
    bool bRecordOpen = false;
    int32 MatchIndex = 0;

    ValStrike::Server::TimeMs StartedAt = 0;
    ValStrike::Server::TimeMs HealthWindowStart = 0;
    int32 HealthFrames = 0;
    double HealthBusyMs = 0.0;
    double HealthMaxBusyMs = 0.0;
    double HealthDeltaSeconds = 0.0;
};
