#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "VSScoreboardState.generated.h"

class APlayerState;

UENUM()
enum class EVSScoreboardLife : uint8
{
    Spectating, // No pawn this round (late join, between rounds).
    Alive,
    Dead,
};

/** One player's replicated scoreboard line. Written only by the server. */
USTRUCT()
struct VALSTRIKE_API FVSScoreboardRow
{
    GENERATED_BODY()

    // Name and ping are read live from here on clients.
    UPROPERTY() TObjectPtr<APlayerState> PlayerState;
    UPROPERTY() int32 PlayerKey = 0;
    // Server snapshot used until the PlayerState reference resolves on a client.
    UPROPERTY() FString PlayerName;
    UPROPERTY() uint8 TeamId = 0;
    UPROPERTY() EVSScoreboardLife Life = EVSScoreboardLife::Spectating;
    UPROPERTY() int32 Kills = 0;
    UPROPERTY() int32 Deaths = 0;
    UPROPERTY() int32 Assists = 0;
    UPROPERTY() int32 HeadshotKills = 0;
    UPROPERTY() int32 DamageDealt = 0;

    bool SameAs(const FVSScoreboardRow& Other) const;
};

/**
 * Server-authoritative per-player match statistics, replicated to every client.
 * Spawned by UVSMatchStatsSubsystem on the server; clients only read it.
 */
UCLASS(NotPlaceable, Transient)
class VALSTRIKE_API AVSScoreboardState : public AInfo
{
    GENERATED_BODY()

public:
    AVSScoreboardState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    const TArray<FVSScoreboardRow>& GetRows() const { return Rows; }
    const FVSScoreboardRow* FindRow(const APlayerState* Player) const;
    int32 GetRevision() const { return Revision; }

    // Authority only. Replaces the rows when they differ; returns true on change.
    bool ServerPublish(const TArray<FVSScoreboardRow>& NewRows);

private:
    UFUNCTION()
    void OnRep_Rows();

    UPROPERTY(ReplicatedUsing = OnRep_Rows)
    TArray<FVSScoreboardRow> Rows;

    UPROPERTY(Replicated)
    int32 Revision = 0;
};
