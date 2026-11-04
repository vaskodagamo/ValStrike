// Standalone checks for the death-spectate target rule.
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic Tests/Match/spectate_rules_test.cpp -o Saved/Tests/Match/spectate_rules_test
#include "../../Source/ValStrike/Match/VSSpectateRules.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
int Failures = 0;
int Checks = 0;

void Expect(bool Condition, const char* What, int Line)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::printf("FAIL line %d: %s\n", Line, What);
    }
}
#define EXPECT(Condition) Expect((Condition), #Condition, __LINE__)

namespace S = ValStrike::Spectate;

S::Candidate Player(S::Id Id, std::uint8_t Team, bool Alive = true, bool Participating = true)
{
    S::Candidate Candidate;
    Candidate.PlayerId = Id;
    Candidate.TeamId = Team;
    Candidate.Alive = Alive;
    Candidate.Participating = Participating;
    return Candidate;
}

S::Id Next(const std::vector<S::Candidate>& All, const S::Viewer& Self, S::Id Current)
{
    return S::Next(All.data(), All.size(), Self, Current);
}

S::Id Keep(const std::vector<S::Candidate>& All, const S::Viewer& Self, S::Id Current)
{
    return S::Keep(All.data(), All.size(), Self, Current);
}

const S::Viewer Dead{2, 1};

void OrdersByIdAndCyclesWithWrap()
{
    // Player-array order is arbitrary; the rule orders by connection id.
    const std::vector<S::Candidate> All{Player(9, 1), Player(2, 1, false), Player(4, 1), Player(7, 1)};
    EXPECT(Next(All, Dead, 0) == 4);
    EXPECT(Next(All, Dead, 4) == 7);
    EXPECT(Next(All, Dead, 7) == 9);
    EXPECT(Next(All, Dead, 9) == 4);
    EXPECT(Keep(All, Dead, 7) == 7);
}

void NeverWatchesEnemiesSelfOrLateJoins()
{
    const std::vector<S::Candidate> All{
        Player(1, 2), Player(2, 1, true), Player(3, 2), Player(5, 1, true, false), Player(6, 0)};
    // Self (even with a stale alive flag), enemies, a non-participant and an unassigned team.
    EXPECT(Next(All, Dead, 0) == 0);
    EXPECT(Keep(All, Dead, 3) == 0);
    const S::Viewer Unassigned{8, 0};
    EXPECT(Next(All, Unassigned, 0) == 0);
    EXPECT(!S::CanWatch(Player(0, 1), Dead));
}

void SkipsDeadAndAutoAdvances()
{
    std::vector<S::Candidate> All{Player(2, 1, false), Player(3, 1), Player(5, 1), Player(4, 2)};
    EXPECT(Keep(All, Dead, 3) == 3);
    All[1].Alive = false; // The watched teammate dies.
    EXPECT(Keep(All, Dead, 3) == 5);
    All[2].Alive = false; // Last teammate dies: death-view fallback.
    EXPECT(Keep(All, Dead, 5) == 0);
    EXPECT(Next(All, Dead, 5) == 0);
}

void SingleTeammateCyclesToItself()
{
    const std::vector<S::Candidate> All{Player(2, 1, false), Player(3, 1), Player(4, 2)};
    EXPECT(Next(All, Dead, 3) == 3);
    EXPECT(S::Next(nullptr, 0, Dead, 0) == 0);
}

void WaitsForTheDeathView()
{
    EXPECT(!S::ShouldSpectate(true, true, false, 0.0));
    EXPECT(!S::ShouldSpectate(true, true, false, S::DelaySeconds - 0.01));
    EXPECT(S::ShouldSpectate(true, true, false, S::DelaySeconds));
    EXPECT(!S::ShouldSpectate(false, true, false, 10.0)); // Round reset or match over.
    EXPECT(!S::ShouldSpectate(true, false, false, 10.0)); // Late join keeps its spectator view.
    EXPECT(!S::ShouldSpectate(true, true, true, 10.0));
}
}

int main()
{
    OrdersByIdAndCyclesWithWrap();
    NeverWatchesEnemiesSelfOrLateJoins();
    SkipsDeadAndAutoAdvances();
    SingleTeammateCyclesToItself();
    WaitsForTheDeathView();
    std::printf("%d checks, %d failures\n", Checks, Failures);
    return Failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
