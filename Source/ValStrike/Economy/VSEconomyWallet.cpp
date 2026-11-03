#include "VSEconomyWallet.h"

#include "VSEconomySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

bool FVSEconomyWalletView::operator==(const FVSEconomyWalletView& Other) const
{
    return PlayerId == Other.PlayerId && TeamId == Other.TeamId && Credits == Other.Credits &&
        OwnedMask == Other.OwnedMask && RefundableMask == Other.RefundableMask &&
        RoundIncome == Other.RoundIncome && LastRoundIncome == Other.LastRoundIncome &&
        RoundId == Other.RoundId && ResultRoundId == Other.ResultRoundId && bBuyOpen == Other.bBuyOpen &&
        BuyDeadlineSeconds == Other.BuyDeadlineSeconds && LastRequestId == Other.LastRequestId &&
        LastKind == Other.LastKind && LastItem == Other.LastItem && LastStatus == Other.LastStatus;
}

const TCHAR* VSEconomyGrantReasonName(EVSEconomyGrantReason Reason)
{
    switch (Reason)
    {
    case EVSEconomyGrantReason::Join: return TEXT("join");
    case EVSEconomyGrantReason::RoundStart: return TEXT("round_start");
    case EVSEconomyGrantReason::Reset: return TEXT("reset");
    case EVSEconomyGrantReason::Purchase: return TEXT("purchase");
    case EVSEconomyGrantReason::Refund: return TEXT("refund");
    }
    return TEXT("unknown");
}

AVSEconomyWallet::AVSEconomyWallet()
{
    bReplicates = true;
    bOnlyRelevantToOwner = true; // Credits and purchases are private to their player.
    bAlwaysRelevant = false;
    SetReplicatingMovement(false);
    SetNetUpdateFrequency(20.0f);
    SetMinNetUpdateFrequency(2.0f);
    PrimaryActorTick.bCanEverTick = false;
}

void AVSEconomyWallet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSEconomyWallet, View);
    DOREPLIFETIME(AVSEconomyWallet, Prices);
    DOREPLIFETIME(AVSEconomyWallet, StarterMask);
}

int32 AVSEconomyWallet::GetPrice(EVSEconomyItem Item) const
{
    const int32 Index = static_cast<int32>(Item);
    return Prices.IsValidIndex(Index) ? Prices[Index] : 0;
}

bool AVSEconomyWallet::IsStarter(EVSEconomyItem Item) const
{
    return (StarterMask & ValStrike::Economy::ItemBit(static_cast<ValStrike::Economy::Item>(Item))) != 0;
}

bool AVSEconomyWallet::SendRequest(EVSEconomyRequest Kind, EVSEconomyItem Item)
{
    const APlayerController* Owner = Cast<APlayerController>(GetOwner());
    if (!Owner || !Owner->IsLocalController() || LocalRequestId == MAX_uint32 || View.PlayerId == 0)
    {
        return false; // Never wrap the per-wallet replay counter.
    }
    ServerRequest(Kind, Item, View.RoundId, ++LocalRequestId);
    return true;
}

void AVSEconomyWallet::ServerRequest_Implementation(EVSEconomyRequest Kind, EVSEconomyItem Item, uint64 RoundId, uint32 RequestId)
{
    if (UVSEconomySubsystem* Economy = GetWorld() ? GetWorld()->GetSubsystem<UVSEconomySubsystem>() : nullptr)
    {
        Economy->HandleRequest(this, Kind, Item, RoundId, RequestId);
    }
}

void AVSEconomyWallet::ServerSetCatalog(const TArray<int32>& InPrices, uint32 InStarterMask)
{
    if (HasAuthority())
    {
        Prices = InPrices;
        StarterMask = InStarterMask;
        ForceNetUpdate();
    }
}

bool AVSEconomyWallet::ServerPublish(const FVSEconomyWalletView& NewView)
{
    if (!HasAuthority() || View == NewView)
    {
        return false;
    }
    View = NewView;
    ForceNetUpdate();
    return true;
}
