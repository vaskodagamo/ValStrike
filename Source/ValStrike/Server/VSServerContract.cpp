#include "VSServerContract.h"

#include <cstdio>

namespace ValStrike::Server
{
namespace
{
char Lower(char Value)
{
    return Value >= 'A' && Value <= 'Z' ? static_cast<char>(Value - 'A' + 'a') : Value;
}

bool EqualsNoCase(const std::string& Left, const char* Right)
{
    std::size_t Index = 0;
    for (; Right[Index] != '\0'; ++Index)
    {
        if (Index >= Left.size() || Lower(Left[Index]) != Lower(Right[Index])) { return false; }
    }
    return Index == Left.size();
}

bool StartsWithNoCase(const std::string& Value, const char* Prefix)
{
    for (std::size_t Index = 0; Prefix[Index] != '\0'; ++Index)
    {
        if (Index >= Value.size() || Lower(Value[Index]) != Lower(Prefix[Index])) { return false; }
    }
    return true;
}

// Plain decimal digits only: no sign, whitespace, exponent or leading '+'.
bool ParseBounded(const std::string& Text, long long Min, long long Max, long long& Out)
{
    if (Text.empty() || Text.size() > 9) { return false; }
    long long Value = 0;
    for (const char Digit : Text)
    {
        if (Digit < '0' || Digit > '9') { return false; }
        Value = Value * 10 + (Digit - '0');
    }
    if (Value < Min || Value > Max) { return false; }
    Out = Value;
    return true;
}

std::string Shown(const std::string& Value)
{
    std::string Result;
    for (const char Character : Value.substr(0, 64))
    {
        Result += (Character >= 0x20 && Character < 0x7f) ? Character : '?';
    }
    return Value.size() > 64 ? Result + "..." : Result;
}

enum Key { Session, ExitWhenDone, EmptyTimeout, ResultGrace, MaxPlayers, Provider, KeyCount };
const char* const KeyNames[KeyCount] = {
    "VSServerSession", "VSServerExitWhenDone", "VSServerEmptyTimeout",
    "VSServerResultGrace", "VSServerMaxPlayers", "VSServerProvider",
};

ParseResult Failed(const std::string& Error)
{
    ParseResult Result;
    Result.Error = Error;
    return Result;
}
}

bool IsValidSessionId(const std::string& Value)
{
    if (Value.empty() || Value.size() > MaxSessionIdLength) { return false; }
    for (std::size_t Index = 0; Index < Value.size(); ++Index)
    {
        const char C = Value[Index];
        const bool Alnum = (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9');
        if (!Alnum && (Index == 0 || (C != '-' && C != '_'))) { return false; }
    }
    return true;
}

ParseResult ParseLaunchOptions(const std::vector<Switch>& Switches)
{
    ParseResult Result;
    bool Seen[KeyCount] = {};
    for (const Switch& Item : Switches)
    {
        if (!StartsWithNoCase(Item.Key, "VSServer")) { continue; }
        int Found = -1;
        for (int Index = 0; Index < KeyCount; ++Index)
        {
            if (EqualsNoCase(Item.Key, KeyNames[Index])) { Found = Index; break; }
        }
        if (Found < 0)
        {
            return Failed("Unknown server switch -" + Shown(Item.Key) + "; allowed: -VSServerSession, "
                          "-VSServerExitWhenDone, -VSServerEmptyTimeout, -VSServerResultGrace, "
                          "-VSServerMaxPlayers, -VSServerProvider");
        }
        const std::string Name = KeyNames[Found];
        if (Seen[Found]) { return Failed("Duplicate server switch -" + Name); }
        Seen[Found] = true;
        if (Found == ExitWhenDone)
        {
            if (Item.HasValue) { return Failed("-VSServerExitWhenDone takes no value"); }
            Result.Options.ExitWhenDone = true;
            continue;
        }
        if (!Item.HasValue || Item.Value.empty()) { return Failed("-" + Name + " requires a value"); }
        long long Number = 0;
        switch (Found)
        {
        case Session:
            if (!IsValidSessionId(Item.Value))
            { return Failed("-VSServerSession must be 1-64 letters, digits, '-' or '_', starting with a letter or digit"); }
            Result.Options.SessionId = Item.Value;
            break;
        case EmptyTimeout:
            if (!ParseBounded(Item.Value, 1, MaxEmptyTimeoutMs / 1000, Number))
            { return Failed("-VSServerEmptyTimeout must be whole seconds from 1 to 86400"); }
            Result.Options.EmptyTimeoutMs = Number * 1000;
            break;
        case ResultGrace:
            if (!ParseBounded(Item.Value, 0, MaxResultGraceMs / 1000, Number))
            { return Failed("-VSServerResultGrace must be whole seconds from 0 to 3600"); }
            Result.Options.ResultGraceMs = Number * 1000;
            break;
        case MaxPlayers:
            if (!ParseBounded(Item.Value, 1, MaxPlayersCap, Number))
            { return Failed("-VSServerMaxPlayers must be 1 to 4"); }
            Result.Options.MaxPlayers = static_cast<int>(Number);
            break;
        case Provider:
            if (!EqualsNoCase(Item.Value, "local"))
            { return Failed("-VSServerProvider must be local; fleet providers are not implemented"); }
            Result.Options.Provider = "local";
            break;
        default:
            return Failed("Unhandled server switch -" + Name);
        }
    }
    if ((Seen[EmptyTimeout] || Seen[ResultGrace]) && !Result.Options.ExitWhenDone)
    {
        return Failed("-VSServerEmptyTimeout and -VSServerResultGrace require -VSServerExitWhenDone; "
                      "without it the server never exits on its own");
    }
    Result.Ok = true;
    return Result;
}

MapMode ValidateMapMode(const std::string& MapPackage, const std::string& ModeClassPath, bool AllowDevelopmentModes)
{
    MapMode Result;
    if (MapPackage == "/Game/Maps/VS_Graybox") { Result.MapKey = "graybox"; }
    else if (MapPackage == "/Game/Maps/Site01/VS_Site01") { Result.MapKey = "site01"; }
    else
    {
        Result.Error = "Map " + Shown(MapPackage) + " is not allowlisted; use /Game/Maps/VS_Graybox or /Game/Maps/Site01/VS_Site01";
        return Result;
    }
    if (ModeClassPath == "/Script/ValStrike.VSGameMode") { Result.ModeKey = "practice"; }
    else if (ModeClassPath == "/Script/ValStrike.VSMatchGameMode") { Result.ModeKey = "match"; }
    else if (AllowDevelopmentModes && ModeClassPath == "/Script/ValStrike.VSRiflePresentationTestGameMode")
    { Result.ModeKey = "development-test"; }
    else
    {
        Result.Error = "Game mode " + Shown(ModeClassPath) + " is not allowlisted; use practice or match";
        return Result;
    }
    if (Result.ModeKey == "match" && Result.MapKey != "site01")
    {
        Result.Error = "Match mode requires Site01 and its authored spawn/site markers";
        return Result;
    }
    if (Result.ModeKey == "development-test" && Result.MapKey != "graybox")
    {
        Result.Error = "The rifle presentation test mode runs only on VS_Graybox";
        return Result;
    }
    Result.Ok = true;
    return Result;
}

const char* ToString(State Value)
{
    switch (Value)
    {
    case State::Starting: return "starting";
    case State::Ready: return "ready";
    case State::Allocated: return "allocated";
    case State::InMatch: return "in_match";
    case State::Ending: return "ending";
    case State::Exited: return "exited";
    }
    return "unknown";
}

const char* ToString(Reason Value)
{
    switch (Value)
    {
    case Reason::None: return "none";
    case Reason::Listening: return "listening";
    case Reason::PlayerJoined: return "player_joined";
    case Reason::ServerEmpty: return "server_empty";
    case Reason::MatchStarted: return "match_started";
    case Reason::MatchCleared: return "match_cleared";
    case Reason::MatchComplete: return "match_complete";
    case Reason::EmptyTimeout: return "empty_timeout";
    case Reason::Failure: return "failure";
    case Reason::Drained: return "drained";
    }
    return "unknown";
}

Lifecycle::Lifecycle(bool InExitWhenDone, TimeMs InEmptyTimeoutMs, TimeMs InResultGraceMs, TimeMs InDrainTimeMs)
    : ExitWhenDone(InExitWhenDone), EmptyTimeoutMs(InEmptyTimeoutMs), ResultGraceMs(InResultGraceMs),
      DrainTimeMs(InDrainTimeMs)
{
}

bool Lifecycle::IsAdmitting() const
{
    return Now == State::Ready || Now == State::Allocated || Now == State::InMatch;
}

void Lifecycle::Move(Decision& Out, State To, Reason Why, TimeMs At)
{
    if (Now == To) { return; }
    Out.Transitions.push_back({Now, To, Why, At});
    Now = To;
    if (To == State::Ending) { EndingAt = At; }
}

void Lifecycle::Evaluate(Decision& Out, TimeMs At)
{
    if (Now == State::Ready && PlayerCount > 0) { Move(Out, State::Allocated, Reason::PlayerJoined, At); }
    if (Now == State::Allocated && PlayerCount == 0 && !ExitWhenDone) { Move(Out, State::Ready, Reason::ServerEmpty, At); }
    if (ExitWhenDone && IsAdmitting())
    {
        if (MatchDone && (PlayerCount == 0 || At - CompletedAt >= ResultGraceMs))
        {
            Code = ExitCompleted;
            Move(Out, State::Ending, Reason::MatchComplete, At);
        }
        else if (PlayerCount == 0 && HasEmptySince && At - EmptySince >= EmptyTimeoutMs)
        {
            Code = ExitCompleted;
            Move(Out, State::Ending, Reason::EmptyTimeout, At);
        }
    }
    if (Now == State::Ending && (PlayerCount == 0 || At - EndingAt >= DrainTimeMs))
    {
        Move(Out, State::Exited, Reason::Drained, At);
        Out.ExitNow = true;
        Out.ExitCode = Code;
    }
}

Decision Lifecycle::MarkReady(TimeMs At)
{
    Decision Out;
    if (Now == State::Starting)
    {
        Move(Out, State::Ready, Reason::Listening, At);
        if (PlayerCount == 0) { HasEmptySince = true; EmptySince = At; }
    }
    Evaluate(Out, At);
    return Out;
}

Decision Lifecycle::Fail(TimeMs At, int ExitCode)
{
    Decision Out;
    if (Now != State::Ending && Now != State::Exited)
    {
        Code = ExitCode == ExitCompleted ? ExitRuntimeFailure : ExitCode;
        Move(Out, State::Ending, Reason::Failure, At);
    }
    Evaluate(Out, At);
    return Out;
}

Decision Lifecycle::ObservePlayers(TimeMs At, int Count)
{
    Decision Out;
    Count = Count < 0 ? 0 : Count;
    if (Count == 0 && (PlayerCount > 0 || !HasEmptySince) && Now != State::Starting)
    {
        HasEmptySince = true;
        EmptySince = At;
    }
    if (Count > 0) { HasEmptySince = false; }
    PlayerCount = Count;
    Evaluate(Out, At);
    return Out;
}

Decision Lifecycle::MatchStarted(TimeMs At)
{
    Decision Out;
    if (Now == State::Ready || Now == State::Allocated) { Move(Out, State::InMatch, Reason::MatchStarted, At); }
    if (Now == State::InMatch) { MatchDone = false; } // A rematch replaces the completed match.
    Evaluate(Out, At);
    return Out;
}

Decision Lifecycle::MatchCompleted(TimeMs At)
{
    Decision Out;
    if (Now == State::InMatch)
    {
        MatchDone = true;
        CompletedAt = At;
    }
    Evaluate(Out, At);
    return Out;
}

Decision Lifecycle::MatchCleared(TimeMs At)
{
    Decision Out;
    if (Now == State::InMatch)
    {
        Move(Out, PlayerCount == 0 && !ExitWhenDone ? State::Ready : State::Allocated, Reason::MatchCleared, At);
    }
    Evaluate(Out, At);
    return Out;
}

Decision Lifecycle::Tick(TimeMs At)
{
    Decision Out;
    Evaluate(Out, At);
    return Out;
}

std::string EscapeJson(const std::string& Value)
{
    std::string Result;
    Result.reserve(Value.size() + 2);
    for (const char Character : Value)
    {
        const unsigned char Code = static_cast<unsigned char>(Character);
        switch (Character)
        {
        case '"': Result += "\\\""; break;
        case '\\': Result += "\\\\"; break;
        case '\b': Result += "\\b"; break;
        case '\f': Result += "\\f"; break;
        case '\n': Result += "\\n"; break;
        case '\r': Result += "\\r"; break;
        case '\t': Result += "\\t"; break;
        default:
            if (Code < 0x20 || Code == 0x7f)
            {
                char Buffer[8];
                std::snprintf(Buffer, sizeof(Buffer), "\\u%04x", static_cast<unsigned>(Code));
                Result += Buffer;
            }
            else { Result += Character; }
        }
    }
    return Result;
}

std::string ToJson(const MatchRecord& Record)
{
    std::string Json = "{\"schema_version\":1";
    const auto Text = [&Json](const char* Key, const std::string& Value)
    {
        Json += ",\"";
        Json += Key;
        Json += "\":\"";
        Json += EscapeJson(Value);
        Json += '"';
    };
    const auto Number = [&Json](const char* Key, long long Value)
    {
        Json += ",\"";
        Json += Key;
        Json += "\":";
        Json += std::to_string(Value);
    };
    Text("session_id", Record.SessionId);
    Number("match_index", Record.MatchIndex);
    Text("status", Record.Status);
    Text("map", Record.Map);
    Text("mode", Record.Mode);
    Text("source_revision", Record.SourceRevision);
    Text("engine_version", Record.EngineVersion);
    Text("build_configuration", Record.BuildConfiguration);
    Text("started_utc", Record.StartedUtc);
    Text("ended_utc", Record.EndedUtc);
    Text("result", Record.Result);
    Number("completed_rounds", Record.CompletedRounds);
    Json += ",\"scores\":{\"team_one\":" + std::to_string(Record.ScoreOne) +
            ",\"team_two\":" + std::to_string(Record.ScoreTwo) + "}";
    Json += ",\"participants\":{\"team_one\":" + std::to_string(Record.ParticipantsOne) +
            ",\"team_two\":" + std::to_string(Record.ParticipantsTwo) + "}";
    Json += ",\"rounds\":[";
    for (std::size_t Index = 0; Index < Record.Rounds.size(); ++Index)
    {
        const RoundRecord& Round = Record.Rounds[Index];
        if (Index > 0) { Json += ','; }
        Json += "{\"round\":" + std::to_string(Round.Round) +
                ",\"attacking_team\":" + std::to_string(Round.AttackingTeam) +
                ",\"winning_team\":" + std::to_string(Round.WinningTeam) +
                ",\"reason\":\"" + EscapeJson(Round.Reason) + "\"}";
    }
    Json += "]}";
    return Json;
}
}
