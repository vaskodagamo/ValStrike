#include "VSServerSubsystem.h"

#include "../Match/VSMatchGameMode.h"
#include "../ValStrike.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/NetworkVersion.h"
#include "Misc/Paths.h"
#include "UObject/UObjectGlobals.h"

// Written by ValStrike.Build.cs into Intermediate/VSBuildInfo; never committed.
#if __has_include("VSBuildInfo.h")
#include "VSBuildInfo.h"
#endif
#ifndef VS_SOURCE_REVISION
#define VS_SOURCE_REVISION TEXT("unknown")
#endif

// Unique names: this file shares a unity translation unit with the rest of the module.
namespace VSServerApi = ValStrike::Server;
namespace VSServerRules = ValStrike::Rules;

namespace
{
constexpr VSServerApi::TimeMs VSServerHealthIntervalMs = 10'000;

std::string VSServerToStd(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }
FString VSServerToUnreal(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }

const char* VSServerWinReason(VSServerRules::WinReason Reason)
{
    switch (Reason)
    {
    case VSServerRules::WinReason::Elimination: return "elimination";
    case VSServerRules::WinReason::Timeout: return "timeout";
    case VSServerRules::WinReason::Detonation: return "detonation";
    case VSServerRules::WinReason::Defuse: return "defuse";
    default: return "none";
    }
}

const char* VSServerMatchResult(VSServerRules::MatchResult Result)
{
    switch (Result)
    {
    case VSServerRules::MatchResult::TeamOne: return "team_one";
    case VSServerRules::MatchResult::TeamTwo: return "team_two";
    case VSServerRules::MatchResult::Draw: return "draw";
    default: return "none";
    }
}

FString VSServerProjectVersion()
{
    FString Version;
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
    return Version.IsEmpty() ? FString(TEXT("unknown")) : Version;
}

bool VSServerAllowsDevelopmentModes()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return false;
#else
    return true;
#endif
}
}

bool UVSServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

VSServerApi::TimeMs UVSServerSubsystem::NowMs()
{
    return static_cast<VSServerApi::TimeMs>(FPlatformTime::Seconds() * 1000.0);
}

void UVSServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StartedAt = NowMs();
    HealthWindowStart = StartedAt;
    Provider = MakeUnique<FVSLocalServerProvider>();
    Admission = MakeUnique<FVSOpenAdmission>();

    // Only -VSServer* switches are interpreted here; the parser rejects any that are not allowlisted.
    TArray<FString> Tokens;
    TArray<FString> Switches;
    FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);
    std::vector<VSServerApi::Switch> Input;
    for (const FString& Item : Switches)
    {
        FString Key = Item;
        FString Value;
        const bool bHasValue = Item.Split(TEXT("="), &Key, &Value);
        Input.push_back({VSServerToStd(Key), VSServerToStd(Value), bHasValue});
    }
    const VSServerApi::ParseResult Parsed = VSServerApi::ParseLaunchOptions(Input);
    if (Parsed.Ok) { Options = Parsed.Options; }
    SessionId = Options.SessionId.empty()
        ? FString::Printf(TEXT("local-%s-%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")),
            FPlatformProcess::GetCurrentProcessId())
        : VSServerToUnreal(Options.SessionId);
    Lifecycle.Emplace(Options.ExitWhenDone, Options.EmptyTimeoutMs, Options.ResultGraceMs);

    const FEngineVersion& Engine = FEngineVersion::Current();
    UE_LOG(LogValStrike, Display,
        TEXT("VSSERVER_VERSION project=%s revision=%s engine=%s changelist=%u compatible_changelist=%u configuration=%s target=%s net_version=%u session=%s"),
        *VSServerProjectVersion(), VS_SOURCE_REVISION, *Engine.ToString(), Engine.GetChangelist(),
        FEngineVersion::CompatibleWith().GetChangelist(), LexToString(FApp::GetBuildConfiguration()),
        LexToString(FApp::GetBuildTargetType()), FNetworkVersion::GetLocalNetworkVersion(), *SessionId);
    UE_LOG(LogValStrike, Display,
        TEXT("VSSERVER_CONFIG session=%s exit_when_done=%s empty_timeout_s=%lld result_grace_s=%lld max_players=%d provider=%s admission=%s"),
        *SessionId, Options.ExitWhenDone ? TEXT("true") : TEXT("false"),
        static_cast<int64>(Options.EmptyTimeoutMs / 1000), static_cast<int64>(Options.ResultGraceMs / 1000),
        Options.MaxPlayers, Provider->GetName(), Admission->GetName());

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVSServerSubsystem::HandlePostLoadMap);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UVSServerSubsystem::TickServer), 0.0f);
    if (!Parsed.Ok) { Fail(VSServerApi::ExitInvalidLaunch, VSServerToUnreal(Parsed.Error)); }
}

void UVSServerSubsystem::Deinitialize()
{
    FTSTicker::RemoveTicker(TickHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    Super::Deinitialize();
}

UVSServerSubsystem* UVSServerSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    return GameInstance ? GameInstance->GetSubsystem<UVSServerSubsystem>() : nullptr;
}

VSServerApi::State UVSServerSubsystem::GetState() const
{
    return Lifecycle ? Lifecycle->Current() : VSServerApi::State::Starting;
}

void UVSServerSubsystem::HandlePostLoadMap(UWorld* World)
{
    // Only the first server map matters; a failed launch never becomes Ready.
    if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_DedicatedServer ||
        !Lifecycle || Lifecycle->Current() != VSServerApi::State::Starting)
    {
        return;
    }
    ServerWorld = World;
    const FString MapPackage = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());
    AGameModeBase* GameMode = World->GetAuthGameMode();
    const FString ModePath = GameMode ? GameMode->GetClass()->GetPathName() : FString();
    const VSServerApi::MapMode Checked = VSServerApi::ValidateMapMode(
        VSServerToStd(MapPackage), VSServerToStd(ModePath), VSServerAllowsDevelopmentModes());
    if (!Checked.Ok)
    {
        Fail(VSServerApi::ExitInvalidLaunch, VSServerToUnreal(Checked.Error));
        return;
    }
    MapKey = VSServerToUnreal(Checked.MapKey);
    ModeKey = VSServerToUnreal(Checked.ModeKey);
    if (const AVSMatchGameMode* Match = Cast<AVSMatchGameMode>(GameMode); Match && !Match->IsMapReady())
    {
        Fail(VSServerApi::ExitRuntimeFailure, TEXT("Site01 match contract is invalid; see the VSMATCH map error above"));
        return;
    }
    if (GameMode->GameSession) { GameMode->GameSession->MaxPlayers = Options.MaxPlayers; }
    UE_LOG(LogValStrike, Display, TEXT("VSSERVER_MAP map=%s mode=%s port=%d max_players=%d"),
        *MapKey, *ModeKey, World->URL.Port, Options.MaxPlayers);
    Apply(Lifecycle->MarkReady(NowMs()));
}

int32 UVSServerSubsystem::CountPlayers() const
{
    const UWorld* World = ServerWorld.Get();
    if (!World) { return 0; }
    int32 Count = 0;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        if (PC && PC->GetNetConnection() && PC->PlayerState) { ++Count; }
    }
    return Count;
}

bool UVSServerSubsystem::TickServer(float DeltaSeconds)
{
    if (!Lifecycle || bExitRequested) { return true; }
    const VSServerApi::TimeMs Now = NowMs();
    const double Delta = FApp::GetDeltaTime();
    const double BusyMs = FMath::Max(0.0, Delta - FApp::GetIdleTime()) * 1000.0;
    ++HealthFrames;
    HealthBusyMs += BusyMs;
    HealthMaxBusyMs = FMath::Max(HealthMaxBusyMs, BusyMs);
    HealthDeltaSeconds += Delta;

    const int32 Players = CountPlayers();
    if (Players != LastPlayers)
    {
        UE_LOG(LogValStrike, Display, TEXT("VSSERVER_PLAYERS count=%d at_ms=%lld"), Players, static_cast<int64>(Now));
        LastPlayers = Players;
        Apply(Lifecycle->ObservePlayers(Now, Players));
    }
    if (!bExitRequested) { Apply(Lifecycle->Tick(Now)); }
    if (!bExitRequested && Now - HealthWindowStart >= VSServerHealthIntervalMs) { ReportHealth(Now); }
    return true;
}

void UVSServerSubsystem::ReportHealth(VSServerApi::TimeMs Now)
{
    FVSServerHealth Health;
    Health.Players = LastPlayers;
    Health.Frames = HealthFrames;
    Health.AverageFrameMs = HealthFrames > 0 ? HealthBusyMs / HealthFrames : 0.0;
    Health.MaxFrameMs = HealthMaxBusyMs;
    Health.AverageTickHz = HealthDeltaSeconds > 0.0 ? HealthFrames / HealthDeltaSeconds : 0.0;
    Health.UptimeSeconds = (Now - StartedAt) / 1000.0;
    Health.State = GetState();
    Provider->OnHealth(Health);
    HealthWindowStart = Now;
    HealthFrames = 0;
    HealthBusyMs = 0.0;
    HealthMaxBusyMs = 0.0;
    HealthDeltaSeconds = 0.0;
}

void UVSServerSubsystem::Apply(const VSServerApi::Decision& Decision)
{
    for (const VSServerApi::Transition& Item : Decision.Transitions)
    {
        Provider->OnTransition(Item, Lifecycle->Players());
        if (Item.To == VSServerApi::State::Ending) { NotifyClientsClosing(Item.Why); }
    }
    if (!Decision.ExitNow || bExitRequested) { return; }
    bExitRequested = true;
    UE_LOG(LogValStrike, Display, TEXT("VSSERVER_EXIT code=%d session=%s uptime_s=%.1f"),
        Decision.ExitCode, *SessionId, (NowMs() - StartedAt) / 1000.0);
    if (GLog) { GLog->Flush(); }
    if (Decision.ExitCode == VSServerApi::ExitCompleted)
    {
        RequestEngineExit(TEXT("VSServer lifecycle complete"));
    }
    else
    {
        // Linux and Windows return this code; the Mac editor binary always exits 0 on request.
        FPlatformMisc::RequestExitWithStatus(false, static_cast<uint8>(Decision.ExitCode), TEXT("VSServer lifecycle failure"));
    }
}

void UVSServerSubsystem::Fail(int32 ExitCode, const FString& Reason)
{
    UE_LOG(LogValStrike, Error, TEXT("VSSERVER_FAILURE code=%d reason=%s"), ExitCode, *Reason);
    if (Lifecycle) { Apply(Lifecycle->Fail(NowMs(), ExitCode)); }
}

void UVSServerSubsystem::NotifyClientsClosing(VSServerApi::Reason Why) const
{
    const UWorld* World = ServerWorld.Get();
    if (!World) { return; }
    const FText Message = FText::FromString(
        Why == VSServerApi::Reason::MatchComplete ? TEXT("The match is over and the server has closed.")
        : Why == VSServerApi::Reason::EmptyTimeout ? TEXT("The server closed after being empty.")
        : TEXT("The server stopped because of a server error."));
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->GetNetConnection()) { PC->ClientReturnToMainMenuWithTextReason(Message); }
    }
}

void UVSServerSubsystem::CheckAdmission(const FString& JoinOptions, const FString& Address,
    const FUniqueNetIdRepl& UniqueId, FString& OutError) const
{
    if (!OutError.IsEmpty()) { return; }
    if (!Lifecycle || !Lifecycle->IsAdmitting())
    {
        OutError = TEXT("Server is not accepting players");
    }
    else if (!Admission->Admit(FVSAdmissionRequest{JoinOptions, Address, UniqueId}, OutError) && OutError.IsEmpty())
    {
        OutError = TEXT("Join rejected by server admission");
    }
    if (!OutError.IsEmpty())
    {
        UE_LOG(LogValStrike, Display, TEXT("VSSERVER_ADMISSION rejected state=%s admission=%s reason=%s"),
            UTF8_TO_TCHAR(VSServerApi::ToString(GetState())), Admission->GetName(), *OutError);
    }
}

void UVSServerSubsystem::HandleMatchEffect(const VSServerRules::Effect& Effect, const VSServerRules::Snapshot& Snapshot)
{
    if (!Lifecycle) { return; }
    const VSServerApi::TimeMs Now = NowMs();
    switch (Effect.Kind)
    {
    case VSServerRules::EffectKind::PrepareRound:
        if (!bRecordOpen)
        {
            Record = VSServerApi::MatchRecord();
            Record.MatchIndex = ++MatchIndex;
            Record.StartedUtc = VSServerToStd(FDateTime::UtcNow().ToIso8601());
            bRecordOpen = true;
            UE_LOG(LogValStrike, Display, TEXT("VSSERVER_MATCH event=started match=%d at_ms=%lld session=%s"),
                MatchIndex, static_cast<int64>(Now), *SessionId);
            Apply(Lifecycle->MatchStarted(Now));
        }
        Record.ParticipantsOne = static_cast<int>(Snapshot.ParticipantCount(VSServerRules::Team::One));
        Record.ParticipantsTwo = static_cast<int>(Snapshot.ParticipantCount(VSServerRules::Team::Two));
        break;
    case VSServerRules::EffectKind::RoundAwarded:
        if (bRecordOpen)
        {
            Record.Rounds.push_back({static_cast<int>(Record.Rounds.size()) + 1, static_cast<int>(Snapshot.AttackingTeam),
                static_cast<int>(Effect.TeamId), VSServerWinReason(Effect.Reason)});
        }
        break;
    case VSServerRules::EffectKind::MatchCompleted:
        UE_LOG(LogValStrike, Display, TEXT("VSSERVER_MATCH event=completed match=%d at_ms=%lld session=%s"),
            MatchIndex, static_cast<int64>(Now), *SessionId);
        if (bRecordOpen) { FinishMatch(TEXT("completed"), Snapshot); }
        Apply(Lifecycle->MatchCompleted(Now));
        break;
    case VSServerRules::EffectKind::MatchCleared:
        if (bRecordOpen) { FinishMatch(TEXT("abandoned"), Snapshot); }
        // A rematch clears and prepares in one rules frame; only an abandoned match returns to waiting.
        if (Snapshot.CurrentPhase == VSServerRules::Phase::Waiting) { Apply(Lifecycle->MatchCleared(Now)); }
        break;
    default:
        break;
    }
}

void UVSServerSubsystem::FinishMatch(const TCHAR* Status, const VSServerRules::Snapshot& Snapshot)
{
    bRecordOpen = false;
    const FEngineVersion& Engine = FEngineVersion::Current();
    Record.SessionId = VSServerToStd(SessionId);
    Record.Status = VSServerToStd(Status);
    Record.Map = VSServerToStd(MapKey);
    Record.Mode = VSServerToStd(ModeKey);
    Record.SourceRevision = VSServerToStd(VS_SOURCE_REVISION);
    Record.EngineVersion = VSServerToStd(Engine.ToString());
    Record.BuildConfiguration = VSServerToStd(LexToString(FApp::GetBuildConfiguration()));
    Record.EndedUtc = VSServerToStd(FDateTime::UtcNow().ToIso8601());
    // Scores come from the awarded rounds: an abandoned match's snapshot is already cleared.
    Record.ScoreOne = 0;
    Record.ScoreTwo = 0;
    for (const VSServerApi::RoundRecord& Round : Record.Rounds)
    {
        Record.ScoreOne += Round.WinningTeam == 1 ? 1 : 0;
        Record.ScoreTwo += Round.WinningTeam == 2 ? 1 : 0;
    }
    Record.CompletedRounds = static_cast<int>(Record.Rounds.size());
    const bool bCompleted = Record.Status == "completed";
    Record.Result = bCompleted ? VSServerMatchResult(Snapshot.Result) : "none";
    if (bCompleted && (Snapshot.Scores[0] != Record.ScoreOne || Snapshot.Scores[1] != Record.ScoreTwo ||
        Snapshot.CompletedRounds != Record.CompletedRounds))
    {
        UE_LOG(LogValStrike, Error, TEXT("VSSERVER_RESULT_MISMATCH rules %d-%d over %d rounds, recorded %d-%d over %d"),
            Snapshot.Scores[0], Snapshot.Scores[1], Snapshot.CompletedRounds, Record.ScoreOne, Record.ScoreTwo,
            Record.CompletedRounds);
    }

    const FString Json = VSServerToUnreal(VSServerApi::ToJson(Record));
    Provider->OnMatchResult(Json);
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ServerResults"));
    const FString Path = Directory / FString::Printf(TEXT("%s_match%d.json"), *SessionId, Record.MatchIndex);
    const FString Temporary = Path + TEXT(".tmp");
    const bool bSaved = IFileManager::Get().MakeDirectory(*Directory, true) &&
        FFileHelper::SaveStringToFile(Json + TEXT("\n"), *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        IFileManager::Get().Move(*Path, *Temporary, true, true);
    UE_LOG(LogValStrike, Display, TEXT("VSSERVER_RESULT_FILE saved=%s path=%s"), bSaved ? TEXT("true") : TEXT("false"), *Path);
}
