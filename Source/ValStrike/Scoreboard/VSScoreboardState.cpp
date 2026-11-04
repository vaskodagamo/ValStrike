#include "VSScoreboardState.h"

#include "VSScoreboardSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

bool FVSScoreboardRow::SameAs(const FVSScoreboardRow& Other) const
{
    return PlayerState == Other.PlayerState && PlayerKey == Other.PlayerKey && PlayerName == Other.PlayerName &&
        TeamId == Other.TeamId && Life == Other.Life && Kills == Other.Kills && Deaths == Other.Deaths &&
        Assists == Other.Assists && HeadshotKills == Other.HeadshotKills && DamageDealt == Other.DamageDealt;
}

AVSScoreboardState::AVSScoreboardState()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    bNetLoadOnClient = false;
    SetReplicatingMovement(false);
    // Changes push a ForceNetUpdate; the low base rate only covers missed updates.
    SetNetUpdateFrequency(2.0f);
}

void AVSScoreboardState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSScoreboardState, Rows);
    DOREPLIFETIME(AVSScoreboardState, Revision);
    DOREPLIFETIME(AVSScoreboardState, KillFeed);
}

void AVSScoreboardState::BeginPlay()
{
    Super::BeginPlay();
    if (UVSScoreboardSubsystem* Presenter = GetWorld() ? GetWorld()->GetSubsystem<UVSScoreboardSubsystem>() : nullptr)
    {
        Presenter->RegisterState(this);
    }
}

void AVSScoreboardState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UVSScoreboardSubsystem* Presenter = GetWorld() ? GetWorld()->GetSubsystem<UVSScoreboardSubsystem>() : nullptr)
    {
        Presenter->UnregisterState(this);
    }
    Super::EndPlay(EndPlayReason);
}

const FVSScoreboardRow* AVSScoreboardState::FindRow(const APlayerState* Player) const
{
    return Player ? Rows.FindByPredicate([Player](const FVSScoreboardRow& Row) { return Row.PlayerState == Player; }) : nullptr;
}

bool AVSScoreboardState::ServerPublish(const TArray<FVSScoreboardRow>& NewRows)
{
    if (!HasAuthority())
    {
        return false;
    }
    bool bSame = NewRows.Num() == Rows.Num();
    for (int32 Index = 0; bSame && Index < Rows.Num(); ++Index)
    {
        bSame = Rows[Index].SameAs(NewRows[Index]);
    }
    if (bSame)
    {
        return false;
    }
    Rows = NewRows;
    ++Revision;
    ForceNetUpdate();
    OnRep_Rows();
    return true;
}

bool AVSScoreboardState::ServerPublishKillFeed(const TArray<FVSKillFeedEntry>& NewFeed)
{
    if (!HasAuthority())
    {
        return false;
    }
    // Entries are immutable once appended, so equal sequences mean equal content.
    bool bSame = NewFeed.Num() == KillFeed.Num();
    for (int32 Index = 0; bSame && Index < KillFeed.Num(); ++Index)
    {
        bSame = KillFeed[Index].Sequence == NewFeed[Index].Sequence;
    }
    if (bSame)
    {
        return false;
    }
    KillFeed = NewFeed;
    ForceNetUpdate();
    return true;
}

void AVSScoreboardState::OnRep_Rows()
{
    if (UVSScoreboardSubsystem* Presenter = GetWorld() ? GetWorld()->GetSubsystem<UVSScoreboardSubsystem>() : nullptr)
    {
        Presenter->MarkDirty();
    }
}
