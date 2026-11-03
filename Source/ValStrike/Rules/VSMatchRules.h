#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Engine-independent, server-owned domain model. No client RPCs belong here.
namespace ValStrike::Rules
{
using TimeMs = std::int64_t;
using Id = std::uint64_t;
constexpr std::size_t MaxPlayers = 4;
constexpr std::size_t MaxEvents = 16;
constexpr TimeMs MaxDurationMs = 86'400'000;
constexpr TimeMs MaxServerTimeMs = INT64_MAX - 4 * MaxDurationMs;
constexpr Id MaxSequence = UINT64_MAX / 64 - 1;
constexpr char ObjectiveSite = 'A';

enum class Team : std::uint8_t { None, One, Two };
enum class Phase : std::uint8_t { Waiting, Preparation, Active, Planted, Resolved, MatchComplete };
enum class DeviceState : std::uint8_t { Unavailable, Carried, PlacementPending, Dropped, Planted, Defused, Detonated };
enum class InteractionKind : std::uint8_t { None, Plant, Defuse };
enum class WinReason : std::uint8_t { None, Elimination, Timeout, Detonation, Defuse };
enum class MatchResult : std::uint8_t { None, TeamOne, TeamTwo, Draw };

struct Config
{
    std::uint8_t MinimumPerTeam = 1; // Development: 1; MVP playtest: 2. Team cap is always 2.
    // Preparation is also the economy's buy phase: players stay in spawn without combat.
    // 30 s (was 8 s) leaves time to read the buy menu and buy; a playtest proposal.
    TimeMs PreparationMs = 30'000;
    TimeMs RoundMs = 90'000;
    TimeMs PlantMs = 3'000;
    TimeMs DefuseMs = 5'000;
    TimeMs DetonationMs = 30'000;
};

struct Player
{
    Id PlayerId = 0; // Nonzero server-issued connection identity, never reused in this model's lifetime.
    Team TeamId = Team::None;
    bool Participating = false;
    bool Alive = false;
    Id LastRequestId = 0; // Retained across rounds/rematches for this connection.
};

struct Device
{
    DeviceState State = DeviceState::Unavailable;
    Id Revision = 0;
    Id Carrier = 0;
    char Site = 0;
};

struct Interaction
{
    InteractionKind Kind = InteractionKind::None;
    Id Token = 0;
    Id PlayerId = 0;
    TimeMs StartedAt = 0;
    TimeMs CompletesAt = 0;
};

struct Snapshot
{
    bool ConfigurationValid = false;
    TimeMs Now = 0;
    Phase CurrentPhase = Phase::Waiting;
    Id RoundId = 0; // Changes on every preparation, including an aborted preparation's retry.
    Team AttackingTeam = Team::One;
    std::array<std::uint8_t, 2> Scores{};
    std::uint8_t CompletedRounds = 0;
    Team RoundWinner = Team::None;
    WinReason Reason = WinReason::None;
    MatchResult Result = MatchResult::None;
    TimeMs PhaseDeadline = 0; // Zero in Waiting/Resolved/MatchComplete.
    std::array<Player, MaxPlayers> Players{};
    Device Objective{};
    Interaction CurrentInteraction{};

    Team DefendingTeam() const;
    std::size_t RosterCount(Team Value) const;
    std::size_t ParticipantCount(Team Value) const;
    std::size_t LivingCount(Team Value) const;
    const Player* FindPlayer(Id PlayerId) const;
    // These gates additionally exclude dead players and late-join spectators.
    bool CanMove(Id PlayerId) const;
    bool CanCombat(Id PlayerId) const;
};

enum class EventKind : std::uint8_t
{
    Join, Disconnect, Death, Drop, Retrieve, StartPlant, StartDefuse, StopInteraction,
    ConfirmPlacement, RecoverDevice, NextRound, Rematch
};

struct Event
{
    EventKind Kind = EventKind::Join;
    Id PlayerId = 0;
    Team TeamId = Team::None; // Join only; membership cannot change during a connection.
    Id RoundId = 0; // Required for every event except Join/Disconnect.
    Id RequestId = 0; // Required for Drop/Retrieve/StartPlant/StartDefuse/StopInteraction.
    Id DeviceRevision = 0; // Required for Retrieve/StartPlant/StartDefuse/ConfirmPlacement/RecoverDevice.
    Id InteractionToken = 0; // Required for StopInteraction.
    bool WorldConfirmed = false; // Server validation, NEVER a forwarded client boolean.
    char Site = 0; // StartPlant must name site A.
};

struct Frame
{
    TimeMs Now = 0; // Integer milliseconds, >= 0. Strictly increases after the first accepted frame.
    Id Sequence = 0; // Nonzero, increasing, <= MaxSequence. Never reset on rematch/empty server.
    // Certifies the existing interaction was continuously held/world-eligible over
    // [previous Now, Now], except explicit invalidations in this frame at Now.
    // Zero/mismatch cancels at previous Now BEFORE any elapsed time is processed.
    // Endpoint sampling alone is insufficient evidence. See the stream handoff.
    Id ContinuousInteractionToken = 0;
    std::array<Event, MaxEvents> Events{};
    std::size_t EventCount = 0;
};

enum class EventStatus : std::uint8_t
{
    NotProcessed, Applied, Unchanged, UnknownPlayer, InvalidIdentity, TeamFull,
    InvalidTeam, StaleRound, ReplayedRequest, WrongPhase, NotAlive, Ineligible,
    WrongDevice, WrongInteraction, Busy
};
enum class FrameStatus : std::uint8_t { Applied, InvalidConfiguration, InvalidTime, ReplayedSequence, InvalidBatch };
enum class EffectKind : std::uint8_t
{
    MatchCleared, WaitingForRoster, PrepareRound, RoundActivated, RoundAwarded, MatchCompleted,
    DropPlacementRequested, RecoveryPlacementRequested, DevicePlaced, DeviceRetrieved,
    InteractionStarted, InteractionCanceled, DevicePlanted, DeviceDefused, DeviceDetonated
};

struct Effect
{
    Id EffectId = 0; // Sequence * 64 + ordinal (1..63), stable and unique for this model lifetime.
    TimeMs At = 0; // Logical event time, which can precede Frame.Now during catch-up.
    EffectKind Kind = EffectKind::MatchCleared;
    Id RoundId = 0;
    Id PlayerId = 0;
    Team TeamId = Team::None;
    WinReason Reason = WinReason::None;
    Id DeviceRevision = 0;
};

struct StepResult
{
    FrameStatus Status = FrameStatus::Applied;
    std::array<EventStatus, MaxEvents> Events{}; // Same order as the input frame.
    std::vector<Effect> Effects; // This call's one-time effects; no retained/unbounded queue.
};

class MatchRules
{
public:
    explicit MatchRules(const Config& Tuning = Config{});
    static bool IsValidConfig(const Config& Tuning);
    const Snapshot& Read() const { return State; }
    StepResult Apply(const Frame& Input);

private:
    Config Tuning;
    Snapshot State;
    bool HasFrame = false;
    Id LastSequence = 0;
    Id LastJoinedId = 0;

    Player* FindPlayer(Id PlayerId);
    bool HasMinimumRoster() const;
    bool HasMinimumParticipants() const;
    Id Emit(StepResult& Out, EffectKind Kind, TimeMs At, Id PlayerId = 0,
            Team TeamId = Team::None, WinReason Reason = WinReason::None);
    void CancelInteraction(StepResult& Out, TimeMs At);
    void ClearMatch(StepResult& Out, TimeMs At);
    void WaitForRoster(StepResult& Out, TimeMs At);
    void Prepare(StepResult& Out, TimeMs At);
    void Resolve(StepResult& Out, TimeMs At, Team Winner, WinReason Reason);
    void CheckElimination(StepResult& Out, TimeMs At);
    void DropDevice(StepResult& Out, TimeMs At, bool Recovery);
    void CompleteInteraction(StepResult& Out, TimeMs At);
    void HardDeadline(StepResult& Out, TimeMs At);
    void AdvanceBefore(StepResult& Out, TimeMs Until);
    EventStatus ApplyAction(const Event& Input, StepResult& Out);
};
}
