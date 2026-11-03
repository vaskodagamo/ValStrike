#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Engine-independent dedicated-server contract (decision 0002, SRV-03).
// No Unreal types: the standalone tests compile this with the standard library only.
namespace ValStrike::Server
{
using TimeMs = std::int64_t;

constexpr int MaxPlayersCap = 4; // The rules model and both teams' caps fix this at four.
constexpr TimeMs DefaultEmptyTimeoutMs = 60'000;
constexpr TimeMs DefaultResultGraceMs = 15'000;
constexpr TimeMs MaxEmptyTimeoutMs = 86'400'000;
constexpr TimeMs MaxResultGraceMs = 3'600'000;
constexpr TimeMs DrainMs = 2'000; // Time for the closing message to reach clients before exit.
constexpr std::size_t MaxSessionIdLength = 64;

// Process exit codes. Zero is reserved for a completed lifecycle.
constexpr int ExitCompleted = 0;
constexpr int ExitRuntimeFailure = 1;
constexpr int ExitInvalidLaunch = 2;

// ---- Startup parameters ------------------------------------------------------

struct LaunchOptions
{
    std::string SessionId; // Empty until the adapter assigns a generated local identifier.
    bool ExitWhenDone = false;
    TimeMs EmptyTimeoutMs = DefaultEmptyTimeoutMs;
    TimeMs ResultGraceMs = DefaultResultGraceMs;
    int MaxPlayers = MaxPlayersCap;
    std::string Provider = "local";
};

// One command-line switch without its leading dash: "Key" or "Key=Value".
struct Switch
{
    std::string Key;
    std::string Value;
    bool HasValue = false;
};

struct ParseResult
{
    bool Ok = false;
    LaunchOptions Options;
    std::string Error;
};

// Reads only switches whose key starts with "VSServer" (ASCII case-insensitive, as Unreal
// matches switches). Every such switch must be one of the allowlisted keys below, appear at
// most once and carry a valid value; anything else fails the launch. Other switches belong
// to the engine and are ignored here.
//   VSServerSession=<[A-Za-z0-9][A-Za-z0-9_-]{0,63}>
//   VSServerExitWhenDone
//   VSServerEmptyTimeout=<seconds 1..86400>      (requires VSServerExitWhenDone)
//   VSServerResultGrace=<seconds 0..3600>        (requires VSServerExitWhenDone)
//   VSServerMaxPlayers=<1..4>
//   VSServerProvider=local
ParseResult ParseLaunchOptions(const std::vector<Switch>& Switches);
bool IsValidSessionId(const std::string& Value);

// ---- Map and mode allowlist ---------------------------------------------------

struct MapMode
{
    bool Ok = false;
    std::string MapKey;  // "graybox" or "site01"
    std::string ModeKey; // "practice", "match" or "development-test"
    std::string Error;
};

// The server itself refuses a map/mode outside the launcher's allowlist, so an orchestrator
// or hand-written command line cannot start an arbitrary Unreal map or game mode.
MapMode ValidateMapMode(const std::string& MapPackage, const std::string& ModeClassPath,
                        bool AllowDevelopmentModes);

// ---- Lifecycle ------------------------------------------------------------------

enum class State : std::uint8_t { Starting, Ready, Allocated, InMatch, Ending, Exited };
enum class Reason : std::uint8_t
{
    None, Listening, PlayerJoined, ServerEmpty, MatchStarted, MatchCleared,
    MatchComplete, EmptyTimeout, Failure, Drained
};
const char* ToString(State Value);
const char* ToString(Reason Value);

struct Transition
{
    State From = State::Starting;
    State To = State::Starting;
    Reason Why = Reason::None;
    TimeMs At = 0;
};

struct Decision
{
    std::vector<Transition> Transitions;
    bool ExitNow = false; // The adapter must end the process with ExitCode.
    int ExitCode = ExitCompleted;
};

// Server-only state machine: Starting -> Ready -> Allocated -> InMatch -> Ending -> Exited.
// Time is a monotonic millisecond clock supplied by the caller. Without ExitWhenDone the
// process never ends on its own (today's wait-and-clear behavior): an emptied server returns
// to Ready. With ExitWhenDone it is disposable and ends after a completed match (once the
// result grace elapses or every player has left) or after being empty for EmptyTimeoutMs.
// Fail() always ends the process with a non-zero code, with or without ExitWhenDone.
class Lifecycle
{
public:
    Lifecycle(bool ExitWhenDone, TimeMs EmptyTimeoutMs, TimeMs ResultGraceMs, TimeMs DrainTimeMs = DrainMs);

    State Current() const { return Now; }
    int Players() const { return PlayerCount; }
    bool IsAdmitting() const;
    int ExitCode() const { return Code; }

    Decision MarkReady(TimeMs At);
    Decision Fail(TimeMs At, int ExitCode);
    Decision ObservePlayers(TimeMs At, int Count);
    Decision MatchStarted(TimeMs At);
    Decision MatchCompleted(TimeMs At);
    Decision MatchCleared(TimeMs At);
    Decision Tick(TimeMs At);

private:
    void Move(Decision& Out, State To, Reason Why, TimeMs At);
    void Evaluate(Decision& Out, TimeMs At);

    bool ExitWhenDone;
    TimeMs EmptyTimeoutMs;
    TimeMs ResultGraceMs;
    TimeMs DrainTimeMs;
    State Now = State::Starting;
    int PlayerCount = 0;
    bool HasEmptySince = false;
    TimeMs EmptySince = 0;
    bool MatchDone = false; // Latest match completed and no new match started since.
    TimeMs CompletedAt = 0;
    TimeMs EndingAt = 0;
    int Code = ExitCompleted;
};

// ---- Structured match result ---------------------------------------------------

struct RoundRecord
{
    int Round = 0;
    int AttackingTeam = 0;
    int WinningTeam = 0; // 0 none, 1 team one, 2 team two
    std::string Reason;  // elimination, timeout, detonation, defuse
};

struct MatchRecord
{
    std::string SessionId;
    int MatchIndex = 0; // 1-based within this process
    std::string Status; // completed or abandoned
    std::string Map;
    std::string Mode;
    std::string SourceRevision;
    std::string EngineVersion;
    std::string BuildConfiguration;
    std::string StartedUtc;
    std::string EndedUtc;
    int ScoreOne = 0;
    int ScoreTwo = 0;
    int CompletedRounds = 0;
    std::string Result; // team_one, team_two, draw or none
    int ParticipantsOne = 0;
    int ParticipantsTwo = 0;
    std::vector<RoundRecord> Rounds;
};

// One line of JSON with schema_version 1; no player names or addresses.
std::string ToJson(const MatchRecord& Record);
std::string EscapeJson(const std::string& Value);
}
