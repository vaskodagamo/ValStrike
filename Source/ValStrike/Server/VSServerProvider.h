#pragma once

#include "CoreMinimal.h"
#include "VSServerContract.h"

#include <string>

struct FUniqueNetIdRepl;

/** Periodic server health sample; frame time excludes the engine's idle wait for the tick-rate cap. */
struct FVSServerHealth
{
    int32 Players = 0;
    int32 Frames = 0;
    double AverageFrameMs = 0.0;
    double MaxFrameMs = 0.0;
    double AverageTickHz = 0.0;
    double UptimeSeconds = 0.0;
    ValStrike::Server::State State = ValStrike::Server::State::Starting;
};

/**
 * Hosting-provider adapter (decision 0002). A fleet provider maps these calls to its SDK
 * (for example Agones Ready/Health/Shutdown or GameLift ProcessReady/ProcessEnding).
 * Implementations run on the game thread of a dedicated server and must not block.
 */
class IVSServerProvider
{
public:
    virtual ~IVSServerProvider() = default;
    virtual const TCHAR* GetName() const = 0;
    virtual void OnTransition(const ValStrike::Server::Transition& Transition, int32 Players) = 0;
    virtual void OnHealth(const FVSServerHealth& Health) = 0;
    virtual void OnMatchResult(const FString& ResultJson) = 0;
};

/** Default provider for development and single-host use: logs every call, owns no external state. */
class FVSLocalServerProvider final : public IVSServerProvider
{
public:
    virtual const TCHAR* GetName() const override { return TEXT("local"); }
    virtual void OnTransition(const ValStrike::Server::Transition& Transition, int32 Players) override;
    virtual void OnHealth(const FVSServerHealth& Health) override;
    virtual void OnMatchResult(const FString& ResultJson) override;
};

/** Join request seen by PreLogin, after the engine's own capacity and option checks. */
struct FVSAdmissionRequest
{
    const FString& Options;
    const FString& Address;
    const FUniqueNetIdRepl& UniqueId;
};

/**
 * Pluggable join admission. A later matchmaker validates a server-issued join ticket here;
 * the ticket must come from the client's join flow, never from text the player types.
 */
class IVSServerAdmission
{
public:
    virtual ~IVSServerAdmission() = default;
    virtual const TCHAR* GetName() const = 0;
    /** Returns false and sets OutError to reject the join. */
    virtual bool Admit(const FVSAdmissionRequest& Request, FString& OutError) const = 0;
};

/**
 * Private-server admission: the join URL must carry ?JoinKey=<key> equal to the server's
 * -VSServerJoinKey. The launcher and runners add the option in code; it is never typed as a URL.
 */
class FVSJoinKeyAdmission final : public IVSServerAdmission
{
public:
    explicit FVSJoinKeyAdmission(std::string InKey) : Key(MoveTemp(InKey)) {}
    virtual const TCHAR* GetName() const override { return TEXT("join_key"); }
    virtual bool Admit(const FVSAdmissionRequest& Request, FString& OutError) const override;

private:
    std::string Key;
};

/** Development admission: accepts everyone, as before SRV-03. */
class FVSOpenAdmission final : public IVSServerAdmission
{
public:
    virtual const TCHAR* GetName() const override { return TEXT("open"); }
    virtual bool Admit(const FVSAdmissionRequest& Request, FString& OutError) const override { return true; }
};
