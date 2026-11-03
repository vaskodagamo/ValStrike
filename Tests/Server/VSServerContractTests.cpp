#include "Server/VSServerContract.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace ValStrike::Server;

namespace
{
int Checks = 0;
int Failures = 0;
const char* CurrentTest = "";

bool Check(bool Condition, const char* Expression, int Line)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::fprintf(stderr, "FAIL %s:%d: %s\n", CurrentTest, Line, Expression);
    }
    return Condition;
}

#define CHECK(Expression) Check((Expression), #Expression, __LINE__)
#define REQUIRE(Expression) do { if (!CHECK(Expression)) { return; } } while (false)

Switch Flag(const char* Key) { return Switch{Key, "", false}; }
Switch Value(const char* Key, const char* Text) { return Switch{Key, Text, true}; }

ParseResult Parse(std::vector<Switch> Switches) { return ParseLaunchOptions(Switches); }
bool Rejects(std::vector<Switch> Switches) { return !ParseLaunchOptions(Switches).Ok; }

bool Moved(const Decision& Out, State From, State To, Reason Why)
{
    for (const Transition& Item : Out.Transitions)
    {
        if (Item.From == From && Item.To == To && Item.Why == Why) { return true; }
    }
    return false;
}

void DefaultsIgnoreEngineSwitches()
{
    const ParseResult Result = Parse({Value("port", "7777"), Flag("nullrhi"), Value("abslog", "/tmp/x.log"),
                                      Value("VSMatchSmoke", "server")});
    REQUIRE(Result.Ok);
    CHECK(Result.Options.SessionId.empty());
    CHECK(!Result.Options.ExitWhenDone);
    CHECK(Result.Options.EmptyTimeoutMs == DefaultEmptyTimeoutMs);
    CHECK(Result.Options.ResultGraceMs == DefaultResultGraceMs);
    CHECK(Result.Options.MaxPlayers == 4);
    CHECK(Result.Options.Provider == "local");
}

void AcceptsEveryAllowlistedSwitch()
{
    const ParseResult Result = Parse({Value("VSServerSession", "match-01_A"), Flag("vsserverexitwhendone"),
                                      Value("VSSERVEREMPTYTIMEOUT", "5"), Value("VSServerResultGrace", "0"),
                                      Value("VSServerMaxPlayers", "2"), Value("VSServerProvider", "Local")});
    REQUIRE(Result.Ok);
    CHECK(Result.Options.SessionId == "match-01_A");
    CHECK(Result.Options.ExitWhenDone);
    CHECK(Result.Options.EmptyTimeoutMs == 5000);
    CHECK(Result.Options.ResultGraceMs == 0);
    CHECK(Result.Options.MaxPlayers == 2);
    CHECK(Result.Options.Provider == "local");
    CHECK(Parse({Flag("VSServerExitWhenDone"), Value("VSServerEmptyTimeout", "86400"),
                 Value("VSServerResultGrace", "3600")}).Ok);
}

void RejectsUnknownDuplicateAndMalformedSwitches()
{
    CHECK(Rejects({Flag("VSServerExitWhenDon")}));
    CHECK(Rejects({Value("VSServerSessionId", "a")}));
    CHECK(Rejects({Value("vsservermap", "/Game/Other")}));
    CHECK(Rejects({Value("VSServerSession", "a"), Value("vsserversession", "b")}));
    CHECK(Rejects({Flag("VSServerExitWhenDone"), Flag("VSServerExitWhenDone")}));
    CHECK(Rejects({Value("VSServerExitWhenDone", "1")}));
    CHECK(Rejects({Flag("VSServerSession")}));
    CHECK(Rejects({Value("VSServerSession", "")}));
    CHECK(Rejects({Flag("VSServerMaxPlayers")}));
    const ParseResult Unknown = Parse({Flag("VSServerTypo")});
    CHECK(Unknown.Error.find("-VSServerTypo") != std::string::npos);
}

void RejectsInvalidValues()
{
    for (const char* Session : {"-lead", "_lead", "has space", "a/b", "a?game=x", "a.b", "\xc3\xa9",
                                "0123456789012345678901234567890123456789012345678901234567890123x"})
    {
        CHECK(Rejects({Value("VSServerSession", Session)}));
    }
    CHECK(IsValidSessionId("0123456789012345678901234567890123456789012345678901234567890123"));
    CHECK(IsValidSessionId("A"));
    for (const char* Seconds : {"0", "86401", "-5", "+5", " 5", "5 ", "5.0", "1e3", "0x10", "9999999999"})
    {
        CHECK(Rejects({Flag("VSServerExitWhenDone"), Value("VSServerEmptyTimeout", Seconds)}));
    }
    for (const char* Seconds : {"3601", "-1", "abc"})
    {
        CHECK(Rejects({Flag("VSServerExitWhenDone"), Value("VSServerResultGrace", Seconds)}));
    }
    for (const char* Count : {"0", "5", "-1", "four"})
    {
        CHECK(Rejects({Value("VSServerMaxPlayers", Count)}));
    }
    CHECK(Rejects({Value("VSServerProvider", "agones")}));
    CHECK(Rejects({Value("VSServerProvider", "local ")}));
}

void JoinKeyParsingAndComparison()
{
    const ParseResult Open = Parse({Flag("VSServerExitWhenDone")});
    REQUIRE(Open.Ok);
    CHECK(Open.Options.JoinKey.empty());
    const ParseResult Keyed = Parse({Value("vsserverjoinkey", "lan-Key_01")});
    REQUIRE(Keyed.Ok);
    CHECK(Keyed.Options.JoinKey == "lan-Key_01");
    for (const char* Key : {"short7_", "has space", "a?game=x", "key/with/slash", "k\xc3\xa9yyyyyyy", "",
                            "01234567890123456789012345678901234567890123456789012345678901234"})
    {
        CHECK(Rejects({Value("VSServerJoinKey", Key)}));
    }
    const ParseResult Bad = Parse({Value("VSServerJoinKey", "secret?value")});
    CHECK(!Bad.Ok && Bad.Error.find("secret") == std::string::npos);
    CHECK(Rejects({Flag("VSServerJoinKey")}));
    CHECK(IsValidJoinKey("12345678") && IsValidJoinKey(std::string(64, 'a')) && !IsValidJoinKey(std::string(65, 'a')));
    CHECK(JoinKeyMatches("lan-Key_01", "lan-Key_01"));
    CHECK(!JoinKeyMatches("lan-Key_01", "lan-Key_02"));
    CHECK(!JoinKeyMatches("lan-Key_01", "lan-Key_0"));
    CHECK(!JoinKeyMatches("lan-Key_01", "lan-Key_01x"));
    CHECK(!JoinKeyMatches("lan-Key_01", ""));
    CHECK(!JoinKeyMatches("lan-Key_01", "LAN-KEY_01"));
    CHECK(!JoinKeyMatches("", "") && !JoinKeyMatches("", "anything"));
}

void TimersRequireExitWhenDone()
{
    const ParseResult Timeout = Parse({Value("VSServerEmptyTimeout", "30")});
    CHECK(!Timeout.Ok);
    CHECK(Timeout.Error.find("require -VSServerExitWhenDone") != std::string::npos);
    CHECK(Rejects({Value("VSServerResultGrace", "5")}));
    CHECK(Parse({Value("VSServerEmptyTimeout", "30"), Flag("VSServerExitWhenDone")}).Ok);
}

void MapModeAllowlist()
{
    const MapMode Practice = ValidateMapMode("/Game/Maps/VS_Graybox", "/Script/ValStrike.VSGameMode", false);
    CHECK(Practice.Ok && Practice.MapKey == "graybox" && Practice.ModeKey == "practice");
    CHECK(ValidateMapMode("/Game/Maps/Site01/VS_Site01", "/Script/ValStrike.VSGameMode", false).Ok);
    const MapMode Match = ValidateMapMode("/Game/Maps/Site01/VS_Site01", "/Script/ValStrike.VSMatchGameMode", false);
    CHECK(Match.Ok && Match.MapKey == "site01" && Match.ModeKey == "match");
    CHECK(!ValidateMapMode("/Game/Maps/VS_Graybox", "/Script/ValStrike.VSMatchGameMode", true).Ok);
    const char* Test = "/Script/ValStrike.VSRiflePresentationTestGameMode";
    CHECK(!ValidateMapMode("/Game/Maps/VS_Graybox", Test, false).Ok);
    CHECK(ValidateMapMode("/Game/Maps/VS_Graybox", Test, true).ModeKey == "development-test");
    CHECK(!ValidateMapMode("/Game/Maps/Site01/VS_Site01", Test, true).Ok);
    for (const char* Map : {"/Game/Maps/VS_Graybox2", "/game/maps/vs_graybox", "/Game/Maps/VS_Graybox?game=x",
                            "/Engine/Maps/Entry", ""})
    {
        CHECK(!ValidateMapMode(Map, "/Script/ValStrike.VSGameMode", true).Ok);
    }
    for (const char* Mode : {"/Script/Engine.GameModeBase", "/Script/ValStrike.VSGameModeX",
                             "/Script/ValStrike.vsgamemode", ""})
    {
        CHECK(!ValidateMapMode("/Game/Maps/VS_Graybox", Mode, true).Ok);
    }
}

void PersistentServerNeverExitsOnItsOwn()
{
    Lifecycle Server(false, 5000, 1000);
    CHECK(Server.Current() == State::Starting);
    CHECK(!Server.IsAdmitting());
    CHECK(Server.Tick(1'000'000).Transitions.empty());
    const Decision Ready = Server.MarkReady(10);
    CHECK(Moved(Ready, State::Starting, State::Ready, Reason::Listening));
    CHECK(Server.IsAdmitting());
    CHECK(Moved(Server.ObservePlayers(20, 1), State::Ready, State::Allocated, Reason::PlayerJoined));
    CHECK(Server.ObservePlayers(25, 2).Transitions.empty());
    CHECK(Moved(Server.MatchStarted(30), State::Allocated, State::InMatch, Reason::MatchStarted));
    CHECK(Server.MatchCompleted(40).Transitions.empty());
    CHECK(Server.Tick(1'000'000).Transitions.empty());
    CHECK(Server.ObservePlayers(1'000'010, 0).Transitions.empty());
    CHECK(Moved(Server.MatchCleared(1'000'020), State::InMatch, State::Ready, Reason::MatchCleared));
    const Decision Later = Server.Tick(9'000'000);
    CHECK(Later.Transitions.empty() && !Later.ExitNow);
    CHECK(Server.Current() == State::Ready);
    CHECK(Moved(Server.ObservePlayers(9'000'010, 1), State::Ready, State::Allocated, Reason::PlayerJoined));
    CHECK(Moved(Server.ObservePlayers(9'000'020, 0), State::Allocated, State::Ready, Reason::ServerEmpty));
}

void EmptyTimeoutWithoutAnyPlayer()
{
    Lifecycle Server(true, 5000, 1000);
    Server.MarkReady(100);
    CHECK(Server.Tick(5099).Transitions.empty());
    const Decision Out = Server.Tick(5100);
    CHECK(Moved(Out, State::Ready, State::Ending, Reason::EmptyTimeout));
    CHECK(Moved(Out, State::Ending, State::Exited, Reason::Drained));
    CHECK(Out.ExitNow && Out.ExitCode == ExitCompleted);
    CHECK(Server.Current() == State::Exited);
    CHECK(!Server.IsAdmitting());
    CHECK(Server.Tick(9000).Transitions.empty());
}

void EmptyTimeoutCountsFromLastLeave()
{
    Lifecycle Server(true, 5000, 1000);
    Server.MarkReady(0);
    Server.ObservePlayers(4000, 1);
    CHECK(Server.Tick(20'000).Transitions.empty());
    const Decision Left = Server.ObservePlayers(20'000, 0);
    CHECK(Left.Transitions.empty()); // A disposable server is not re-advertised as Ready.
    CHECK(Server.Current() == State::Allocated);
    CHECK(Server.Tick(24'999).Transitions.empty());
    Server.ObservePlayers(24'000, 1);
    CHECK(Server.Tick(40'000).Transitions.empty());
    Server.ObservePlayers(40'000, 0);
    CHECK(Server.Tick(44'999).Transitions.empty());
    const Decision Out = Server.Tick(45'000);
    CHECK(Moved(Out, State::Allocated, State::Ending, Reason::EmptyTimeout));
    CHECK(Out.ExitNow && Out.ExitCode == ExitCompleted);
}

void CompletedMatchWaitsForGraceThenDrains()
{
    Lifecycle Server(true, 60'000, 15'000, 2000);
    Server.MarkReady(0);
    Server.ObservePlayers(10, 2);
    Server.MatchStarted(20);
    CHECK(Server.MatchCompleted(1000).Transitions.empty());
    CHECK(Server.Tick(15'999).Transitions.empty());
    const Decision Ending = Server.Tick(16'000);
    CHECK(Moved(Ending, State::InMatch, State::Ending, Reason::MatchComplete));
    CHECK(!Ending.ExitNow);
    CHECK(!Server.IsAdmitting());
    CHECK(!Server.Tick(17'999).ExitNow);
    const Decision Exit = Server.Tick(18'000);
    CHECK(Moved(Exit, State::Ending, State::Exited, Reason::Drained));
    CHECK(Exit.ExitNow && Exit.ExitCode == ExitCompleted);
}

void DrainEndsEarlyWhenClientsLeave()
{
    Lifecycle Server(true, 60'000, 0, 2000);
    Server.MarkReady(0);
    Server.ObservePlayers(1, 2);
    Server.MatchStarted(2);
    const Decision Ending = Server.MatchCompleted(100);
    CHECK(Moved(Ending, State::InMatch, State::Ending, Reason::MatchComplete));
    CHECK(!Ending.ExitNow);
    const Decision Exit = Server.ObservePlayers(150, 0);
    CHECK(Exit.ExitNow && Moved(Exit, State::Ending, State::Exited, Reason::Drained));
}

void RematchDuringGraceKeepsServer()
{
    Lifecycle Server(true, 60'000, 15'000);
    Server.MarkReady(0);
    Server.ObservePlayers(1, 2);
    Server.MatchStarted(2);
    Server.MatchCompleted(1000);
    // The adapter reports a rematch as a new match start; a clear-then-start pair must also hold.
    const Decision Cleared = Server.MatchCleared(5000);
    CHECK(Moved(Cleared, State::InMatch, State::Allocated, Reason::MatchCleared));
    CHECK(!Server.MatchStarted(5000).ExitNow);
    CHECK(Server.Current() == State::InMatch);
    CHECK(Server.Tick(100'000).Transitions.empty());
    Server.MatchCompleted(200'000);
    CHECK(!Server.MatchStarted(201'000).ExitNow); // In-match rematch without a clear.
    CHECK(Server.Tick(300'000).Transitions.empty());
}

void CompletedMatchExitsWhenEveryoneLeaves()
{
    Lifecycle Server(true, 60'000, 15'000);
    Server.MarkReady(0);
    Server.ObservePlayers(1, 2);
    Server.MatchStarted(2);
    Server.MatchCompleted(1000);
    CHECK(Server.ObservePlayers(2000, 1).Transitions.empty());
    const Decision Out = Server.ObservePlayers(3000, 0);
    CHECK(Moved(Out, State::InMatch, State::Ending, Reason::MatchComplete));
    CHECK(Out.ExitNow && Out.ExitCode == ExitCompleted);
}

void AbandonedMatchUsesEmptyTimeout()
{
    Lifecycle Server(true, 5000, 1000);
    Server.MarkReady(0);
    Server.ObservePlayers(1, 2);
    Server.MatchStarted(2);
    Server.ObservePlayers(100, 0);
    CHECK(Moved(Server.MatchCleared(100), State::InMatch, State::Allocated, Reason::MatchCleared));
    CHECK(Server.Tick(5099).Transitions.empty());
    const Decision Out = Server.Tick(5100);
    CHECK(Moved(Out, State::Allocated, State::Ending, Reason::EmptyTimeout) && Out.ExitNow);
}

void FailureAlwaysExitsNonZero()
{
    Lifecycle Starting(false, 5000, 1000);
    const Decision Out = Starting.Fail(5, ExitInvalidLaunch);
    CHECK(Moved(Out, State::Starting, State::Ending, Reason::Failure));
    CHECK(Out.ExitNow && Out.ExitCode == ExitInvalidLaunch);
    CHECK(Starting.Fail(6, ExitRuntimeFailure).Transitions.empty());
    CHECK(Starting.ExitCode() == ExitInvalidLaunch);

    Lifecycle Running(false, 5000, 1000);
    Running.MarkReady(0);
    Running.ObservePlayers(1, 1);
    const Decision Failed = Running.Fail(10, ExitCompleted);
    CHECK(Moved(Failed, State::Allocated, State::Ending, Reason::Failure));
    CHECK(!Failed.ExitNow);
    CHECK(!Running.IsAdmitting());
    CHECK(Running.MatchStarted(11).Transitions.empty());
    const Decision Exit = Running.Tick(10 + DrainMs);
    CHECK(Exit.ExitNow && Exit.ExitCode == ExitRuntimeFailure);
}

void JsonRecordIsStableAndEscaped()
{
    MatchRecord Record;
    Record.SessionId = "s1";
    Record.MatchIndex = 2;
    Record.Status = "completed";
    Record.Map = "site01";
    Record.Mode = "match";
    Record.SourceRevision = "abc";
    Record.EngineVersion = "5.8.4";
    Record.BuildConfiguration = "Development";
    Record.StartedUtc = "2026-11-03T20:00:00.000Z";
    Record.EndedUtc = "2026-11-03T20:10:00.000Z";
    Record.ScoreOne = 3;
    Record.ScoreTwo = 3;
    Record.CompletedRounds = 6;
    Record.Result = "draw";
    Record.ParticipantsOne = 1;
    Record.ParticipantsTwo = 2;
    Record.Rounds = {{1, 1, 2, "timeout"}, {2, 1, 1, "detonation"}};
    const std::string Expected =
        "{\"schema_version\":1,\"session_id\":\"s1\",\"match_index\":2,\"status\":\"completed\","
        "\"map\":\"site01\",\"mode\":\"match\",\"source_revision\":\"abc\",\"engine_version\":\"5.8.4\","
        "\"build_configuration\":\"Development\",\"started_utc\":\"2026-11-03T20:00:00.000Z\","
        "\"ended_utc\":\"2026-11-03T20:10:00.000Z\",\"result\":\"draw\",\"completed_rounds\":6,"
        "\"scores\":{\"team_one\":3,\"team_two\":3},\"participants\":{\"team_one\":1,\"team_two\":2},"
        "\"rounds\":[{\"round\":1,\"attacking_team\":1,\"winning_team\":2,\"reason\":\"timeout\"},"
        "{\"round\":2,\"attacking_team\":1,\"winning_team\":1,\"reason\":\"detonation\"}]}";
    CHECK(ToJson(Record) == Expected);
    CHECK(ToJson(MatchRecord{}).find("\"rounds\":[]}") != std::string::npos);
    CHECK(EscapeJson("a\"b\\c\n\t\x01\x7f") == "a\\\"b\\\\c\\n\\t\\u0001\\u007f");
    CHECK(EscapeJson("\xc3\xa9") == "\xc3\xa9");
}

struct TestCase
{
    const char* Name;
    void (*Run)();
};
}

int main()
{
    const TestCase Tests[] = {
        {"defaults ignore engine switches", DefaultsIgnoreEngineSwitches},
        {"accepts every allowlisted switch", AcceptsEveryAllowlistedSwitch},
        {"rejects unknown duplicate malformed switches", RejectsUnknownDuplicateAndMalformedSwitches},
        {"rejects invalid values", RejectsInvalidValues},
        {"join key parsing and comparison", JoinKeyParsingAndComparison},
        {"timers require exit-when-done", TimersRequireExitWhenDone},
        {"map and mode allowlist", MapModeAllowlist},
        {"persistent server never exits on its own", PersistentServerNeverExitsOnItsOwn},
        {"empty timeout without any player", EmptyTimeoutWithoutAnyPlayer},
        {"empty timeout counts from last leave", EmptyTimeoutCountsFromLastLeave},
        {"completed match waits for grace then drains", CompletedMatchWaitsForGraceThenDrains},
        {"drain ends early when clients leave", DrainEndsEarlyWhenClientsLeave},
        {"rematch during grace keeps server", RematchDuringGraceKeepsServer},
        {"completed match exits when everyone leaves", CompletedMatchExitsWhenEveryoneLeaves},
        {"abandoned match uses empty timeout", AbandonedMatchUsesEmptyTimeout},
        {"failure always exits non-zero", FailureAlwaysExitsNonZero},
        {"json record is stable and escaped", JsonRecordIsStableAndEscaped},
    };
    int Passed = 0;
    for (const TestCase& Test : Tests)
    {
        CurrentTest = Test.Name;
        const int Before = Failures;
        Test.Run();
        if (Failures == Before)
        {
            ++Passed;
            std::printf("PASS %s\n", Test.Name);
        }
    }
    const int Total = static_cast<int>(sizeof(Tests) / sizeof(Tests[0]));
    std::printf("Server contract: %d/%d cases passed, %d checks, %d failures\n", Passed, Total, Checks, Failures);
    return Failures == 0 ? 0 : 1;
}
