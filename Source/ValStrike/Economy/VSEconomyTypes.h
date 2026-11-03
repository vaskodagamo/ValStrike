#pragma once

#include "CoreMinimal.h"
#include "VSEconomyRules.h"
#include "VSEconomyTypes.generated.h"

class APlayerController;

// Wire values equal ValStrike::Economy::Item. Append new items; never renumber.
UENUM()
enum class EVSEconomyItem : uint8
{
    None = 0,
    Pistol = 1,
    Rifle = 2,
};
static_assert(static_cast<uint8>(EVSEconomyItem::Pistol) == static_cast<uint8>(ValStrike::Economy::Item::Pistol));
static_assert(static_cast<uint8>(EVSEconomyItem::Rifle) == static_cast<uint8>(ValStrike::Economy::Item::Rifle));

UENUM()
enum class EVSEconomyRequest : uint8
{
    Purchase,
    Refund,
};

/** Replicated to the owning client only. The server subsystem is the sole writer. */
USTRUCT()
struct VALSTRIKE_API FVSEconomyWalletView
{
    GENERATED_BODY()

    UPROPERTY() uint64 PlayerId = 0;     // The match connection id; zero until registered.
    UPROPERTY() uint8 TeamId = 0;
    UPROPERTY() int32 Credits = 0;
    UPROPERTY() uint32 OwnedMask = 0;      // Bit per EVSEconomyItem value.
    UPROPERTY() uint32 RefundableMask = 0; // Bought during the open buy phase.
    UPROPERTY() int32 RoundIncome = 0;     // Credited since this round's buy phase opened.
    UPROPERTY() int32 LastRoundIncome = 0; // The previous completed round's income.
    UPROPERTY() uint64 RoundId = 0;        // Economy round; requests must name it.
    UPROPERTY() uint64 ResultRoundId = 0;  // Latest round whose result is included in Credits.
    UPROPERTY() bool bBuyOpen = false;     // Buy phase open and this player alive on a team.
    UPROPERTY() double BuyDeadlineSeconds = 0.0; // Server world seconds; presentation only.
    UPROPERTY() uint32 LastRequestId = 0;  // Latest request the server judged, with its outcome.
    UPROPERTY() uint8 LastKind = 0;        // EVSEconomyRequest
    UPROPERTY() uint8 LastItem = 0;        // EVSEconomyItem
    UPROPERTY() uint8 LastStatus = 0;      // ValStrike::Economy::Status

    bool operator==(const FVSEconomyWalletView& Other) const;
    bool Owns(EVSEconomyItem Item) const { return (OwnedMask & ValStrike::Economy::ItemBit(static_cast<ValStrike::Economy::Item>(Item))) != 0; }
    bool CanRefund(EVSEconomyItem Item) const { return (RefundableMask & ValStrike::Economy::ItemBit(static_cast<ValStrike::Economy::Item>(Item))) != 0; }
};

// ---------------------------------------------------------------------------------------------
// Weapon grant interface (server side).
//
// A purchase changes ownership in the economy; it does not touch the pawn itself. The economy
// subsystem broadcasts OnLoadoutChanged() whenever a player's owned items change, and once for
// every registered player when a buy phase opens (the match has spawned fresh pawns by then).
// The weapons owner subscribes and grants/removes rifle/pistol on that controller's pawn, or
// pulls UVSEconomySubsystem::GetLoadout() when a pawn is possessed. Until that subscriber exists
// the subsystem only logs each grant ("VSECON grant ...").
// ---------------------------------------------------------------------------------------------
struct FVSEconomyLoadout
{
    uint32 OwnedMask = 0;
    bool Owns(EVSEconomyItem Item) const { return (OwnedMask & ValStrike::Economy::ItemBit(static_cast<ValStrike::Economy::Item>(Item))) != 0; }
};

enum class EVSEconomyGrantReason : uint8
{
    Join,       // Registered with the starter items.
    RoundStart, // A buy phase opened: apply to the fresh pawn.
    Reset,      // Side swap or new match: starter items only.
    Purchase,
    Refund,
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FVSEconomyLoadoutChanged,
    APlayerController* /*Owner*/, const FVSEconomyLoadout& /*Loadout*/, EVSEconomyGrantReason /*Reason*/);

VALSTRIKE_API const TCHAR* VSEconomyGrantReasonName(EVSEconomyGrantReason Reason);
