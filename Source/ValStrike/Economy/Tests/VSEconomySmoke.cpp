#include "VSEconomySmoke.h"

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "../VSEconomySubsystem.h"
#include "../VSEconomyWallet.h"
#include "../UI/VSEconomyClientSubsystem.h"
#include "../../Combat/VSHealthComponent.h"
#include "../../Match/VSMatchGameState.h"
#include "../../Match/VSMatchObjective.h"
#include "../../Match/VSMatchPlayerState.h"
#include "../../Rules/VSMatchRules.h"
#include "../../VSCharacter.h"
#include "../../VSPlayerController.h"
#include "../../ValStrike.h"
#include "Engine/TriggerBox.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
namespace VSEconomySmokeImpl
{
using EcoStatus = ValStrike::Economy::Status;
using EcoPhase = ValStrike::Economy::Phase;
using SmokeMatchPhase = ValStrike::Rules::Phase;
using SmokeDevice = ValStrike::Rules::DeviceState;

// The first match of the 1v1 scenario. Team One is alpha (joins first), Team Two is bravo.
// Tests/Economy/VSEconomyRulesTests.cpp replays the same script against the plain model.
struct FEconomyRound
{
    uint8 Winner;      // Expected round winner.
    bool bPlant;       // Attackers plant this round.
    uint8 KillerTeam;  // The team credited with the round's one kill.
    int32 SpendOne;    // Net credits Team One spends in the buy phase.
    int32 SpendTwo;
};
const FEconomyRound EconomyScript[6] = {
    {2, true, 2, 0, 0},           // 1: One plants; Two kills the planter and defuses.
    {1, false, 1, 2700, 2700},    // 2: both buy rifles (Two by spamming); One kills Two.
    {2, false, 2, 0, 2700},       // 3: Two buys, refunds and buys again; Two kills One.
    {2, false, 2, 0, 0},          // 4-6, after the side swap: Two attacks and kills One.
    {2, false, 2, 0, 0},
    {2, false, 2, 0, 0},
};
const TCHAR* EconomyScreens[] = {
    TEXT("01-credits-hud"), TEXT("02-buy-menu-round1"), TEXT("03-escape-over-buy"),
    TEXT("04-buy-menu-purchased"), TEXT("05-round-live"),
};

FString EconomyJson(const FString& In)
{
    return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));
}

class FEconomySmoke
{
public:
    explicit FEconomySmoke(FString InRole) : Role(MoveTemp(InRole))
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSEconomyOutput="), Output);
        FParse::Value(FCommandLine::Get(), TEXT("VSEconomyScreenshots="), ScreenDirectory);
        bRendered = !ScreenDirectory.IsEmpty();
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FEconomySmoke::Tick);
    }
    ~FEconomySmoke() { FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle); }

private:
    struct FResult { bool bPassed = true; FString Detail; };

    // A check passes only if every evaluation passed; the first failure's detail is kept.
    void Check(const FString& Name, bool bPassed, const FString& Detail)
    {
        FResult* Existing = Results.Find(Name);
        const bool bLog = !Existing || Existing->Detail != Detail;
        if (!Existing)
        {
            Order.Add(Name);
            Results.Add(Name, {bPassed, Detail});
        }
        else if (Existing->bPassed && !bPassed)
        {
            *Existing = {false, Detail};
        }
        else if (Existing->bPassed)
        {
            Existing->Detail = Detail;
        }
        if (bLog)
        {
            UE_LOG(LogValStrike, Display, TEXT("VSECONCHECK %s %s %s %s"), *Role, *Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
        }
    }

    void Finish(const TCHAR* Error = nullptr)
    {
        if (bFinished) { return; }
        bFinished = true;
        TArray<FString> Items;
        for (const FString& Name : Order)
        {
            const FResult& Value = Results[Name];
            Items.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"), *Name,
                Value.bPassed ? TEXT("true") : TEXT("false"), *EconomyJson(Value.Detail)));
        }
        FString Extra;
        if (Role == TEXT("server"))
        {
            TArray<FString> Players;
            for (const TPair<uint64, TMap<FString, int64>>& Player : Ledger)
            {
                Players.Add(FString::Printf(TEXT("\"%llu\":%s"), Player.Key, *Numbers(Player.Value)));
            }
            Extra = FString::Printf(TEXT(",\"ledger\":{%s}"), *FString::Join(Players, TEXT(",")));
        }
        else
        {
            Extra = FString::Printf(TEXT(",\"player_id\":%llu,\"checkpoints\":%s"), PlayerId, *Numbers(Checkpoints));
        }
        const FString Report = FString::Printf(TEXT("{\"schema_version\":1,\"role\":\"%s\",\"completed\":true,\"checks\":[%s],\"error\":%s%s}\n"),
            *Role, *FString::Join(Items, TEXT(",")), Error ? *FString::Printf(TEXT("\"%s\""), *EconomyJson(Error)) : TEXT("null"), *Extra);
        if (!Output.IsEmpty() &&
            FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
        }
        UE_LOG(LogValStrike, Display, TEXT("VSECON smoke finished role=%s error=%s"), *Role, Error ? Error : TEXT("none"));
    }

    static FString Numbers(const TMap<FString, int64>& Values)
    {
        TArray<FString> Parts;
        for (const TPair<FString, int64>& Pair : Values) { Parts.Add(FString::Printf(TEXT("\"%s\":%lld"), *Pair.Key, Pair.Value)); }
        return TEXT("{") + FString::Join(Parts, TEXT(",")) + TEXT("}");
    }

    void Tick(UWorld* World, ELevelTick, float)
    {
        if (bFinished || !World || !World->IsGameWorld()) { return; }
        const bool bServer = Role == TEXT("server");
        if (bServer ? World->GetNetMode() != NM_DedicatedServer : World->GetNetMode() != NM_Client) { return; }
        // Seven 30 s buy phases plus rounds and startup.
        if (World->GetTimeSeconds() > 420.0) { Finish(TEXT("native_scenario_timeout")); return; }
        if (bServer) { ServerTick(World); } else { ClientTick(World); }
    }

    // ------------------------------------------------------------------------------- server

    uint32 Count(const UVSEconomySubsystem* Economy, uint64 Id, EcoStatus Status) const
    {
        return Economy->DevelopmentStatusCount(Id, Status);
    }

    static bool Place(APlayerController* Controller, FVector Where)
    {
        AVSCharacter* Pawn = Controller ? Cast<AVSCharacter>(Controller->GetPawn()) : nullptr;
        if (!Pawn) { return false; }
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Where.Z = 90;
        if (!Pawn->TeleportTo(Where, FRotator::ZeroRotator)) { return false; }
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Controller->ClientSetLocation(Pawn->GetActorLocation(), FRotator::ZeroRotator);
        Pawn->ForceNetUpdate();
        return true;
    }

    void KillThroughHealth(UVSEconomySubsystem* Economy, APlayerController* Killer, APlayerController* Victim)
    {
        AVSCharacter* Pawn = Victim ? Cast<AVSCharacter>(Victim->GetPawn()) : nullptr;
        const bool bDied = Pawn && Pawn->GetHealthComponent()->ApplyPointDamage(1000.0f) > 0.0f && !Pawn->GetHealthComponent()->IsAlive();
        // Combat does not report killers yet (handoff); the harness reports through the same
        // server-only entry point combat will call. A repeat report must not pay again.
        const bool bFirst = Economy->ReportKill(Killer, Victim);
        const bool bRepeat = Economy->ReportKill(Killer, Victim);
        Check(TEXT("kill_reward"), bDied && bFirst && !bRepeat,
            FString::Printf(TEXT("round=%d died=%d first=%d repeat=%d"), Sequence, bDied, bFirst, bRepeat));
    }

    void OnGrant(APlayerController*, const FVSEconomyLoadout& Loadout, EVSEconomyGrantReason Reason)
    {
        ++Grants.FindOrAdd(static_cast<uint8>(Reason));
        if (Reason == EVSEconomyGrantReason::Purchase && Loadout.Owns(EVSEconomyItem::Rifle)) { ++RifleGrants; }
        if (Reason == EVSEconomyGrantReason::Reset && Loadout.Owns(EVSEconomyItem::Rifle)) { ++ResetRifleGrants; }
    }

    int32 LossFor(const ValStrike::Economy::Tuning& Tuning, int32 Streak) const
    {
        const int32 Steps = FMath::Min(Streak - 1, static_cast<int32>(Tuning.LossStreakMaxSteps));
        return FMath::Min(Tuning.RoundLossReward + Steps * Tuning.LossStreakStep, Tuning.CreditCap);
    }

    void ServerTick(UWorld* World)
    {
        UVSEconomySubsystem* Economy = World->GetSubsystem<UVSEconomySubsystem>();
        const AVSMatchGameState* GS = World->GetGameState<AVSMatchGameState>();
        if (!Economy || !GS || !GS->Match.bReady) { return; }
        if (!bSubscribed)
        {
            Economy->OnLoadoutChanged().AddRaw(this, &FEconomySmoke::OnGrant);
            bSubscribed = true;
        }
        Check(TEXT("economy_active"), Economy->IsEconomyActive(), TEXT("match GameState present; wallets spawned at login"));
        APlayerController* One = nullptr;
        APlayerController* Two = nullptr;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* Controller = It->Get();
            const AVSMatchPlayerState* PS = Controller ? Controller->GetPlayerState<AVSMatchPlayerState>() : nullptr;
            const AVSEconomyWallet* Wallet = Economy->FindWallet(Controller);
            if (!PS || !Wallet || Wallet->GetView().PlayerId == 0) { continue; }
            (PS->TeamId == 1 ? One : Two) = Controller;
        }
        if (!One || !Two) { return; }
        const uint64 OneId = Economy->FindWallet(One)->GetView().PlayerId;
        const uint64 TwoId = Economy->FindWallet(Two)->GetView().PlayerId;
        const ValStrike::Economy::Snapshot& S = Economy->GetModel().Read();
        const ValStrike::Economy::Tuning& Tuning = Economy->GetModel().GetTuning();
        const ValStrike::Economy::Wallet* W1 = S.Find(OneId);
        const ValStrike::Economy::Wallet* W2 = S.Find(TwoId);
        if (!W1 || !W2) { return; }
        const double Now = World->GetTimeSeconds();
        const uint32 RifleBit = ValStrike::Economy::ItemBit(ValStrike::Economy::Item::Rifle);
        const auto Record = [this, OneId, TwoId, W1, W2](const TCHAR* Kind, int32 Index, bool bOwned)
        {
            Ledger.FindOrAdd(OneId).Add(FString::Printf(TEXT("%s_%d"), Kind, Index), W1->Balance);
            Ledger.FindOrAdd(TwoId).Add(FString::Printf(TEXT("%s_%d"), Kind, Index), W2->Balance);
            if (bOwned)
            {
                Ledger.FindOrAdd(OneId).Add(FString::Printf(TEXT("owned_%d"), Index), W1->Owned);
                Ledger.FindOrAdd(TwoId).Add(FString::Printf(TEXT("owned_%d"), Index), W2->Owned);
            }
        };

        // Buy phase opened.
        if (S.CurrentPhase == EcoPhase::Buy && S.RoundId != SeenRound)
        {
            SeenRound = S.RoundId;
            ++Sequence;
            Actions = 0;
            const bool bFresh = Sequence == 1 || Sequence == 4 || Sequence == 7;
            if (bFresh) { ExpectOne = ExpectTwo = Tuning.StartingCredits; StreakOne = StreakTwo = 0; }
            Check(TEXT("rewards_match_ledger"), W1->Balance == ExpectOne && W2->Balance == ExpectTwo,
                FString::Printf(TEXT("open %d: one=%d expected %d, two=%d expected %d"), Sequence, W1->Balance, ExpectOne, W2->Balance, ExpectTwo));
            Record(TEXT("open"), Sequence, true);
            for (int32 Index = 0; Index < static_cast<int32>(ValStrike::Economy::StatusCount); ++Index)
            {
                OpenCounts[0][Index] = Count(Economy, OneId, static_cast<EcoStatus>(Index));
                OpenCounts[1][Index] = Count(Economy, TwoId, static_cast<EcoStatus>(Index));
            }
            if (Sequence == 1)
            {
                Check(TEXT("starting_credits"), W1->Balance == 800 && W2->Balance == 800 &&
                    W1->Owned == Tuning.StarterItems && W2->Owned == Tuning.StarterItems,
                    FString::Printf(TEXT("one=%d two=%d owned=%u/%u"), W1->Balance, W2->Balance, W1->Owned, W2->Owned));
            }
            if (Sequence == 3)
            {
                Check(TEXT("survivor_keeps_purchase"), (W1->Owned & RifleBit) != 0, FString::Printf(TEXT("team one survived round 2; owned=%u"), W1->Owned));
                Check(TEXT("death_loses_purchase"), (W2->Owned & RifleBit) == 0 && W2->Owned == Tuning.StarterItems,
                    FString::Printf(TEXT("team two fell in round 2 holding a rifle; owned=%u"), W2->Owned));
            }
            if (Sequence == 4)
            {
                Check(TEXT("side_swap_reset"), bTwoHadRifle && W1->Balance == 800 && W2->Balance == 800 &&
                    W1->Owned == Tuning.StarterItems && W2->Owned == Tuning.StarterItems && S.LossStreak[0] == 0 &&
                    S.LossStreak[1] == 0 && S.AttackingTeam == ValStrike::Economy::Team::Two,
                    FString::Printf(TEXT("two held a rifle before=%d; balances %d/%d; owned %u/%u; attackers=%d"), bTwoHadRifle,
                        W1->Balance, W2->Balance, W1->Owned, W2->Owned, static_cast<int32>(S.AttackingTeam)));
            }
            if (Sequence == 7)
            {
                Check(TEXT("rematch_reset"), W1->Balance == 800 && W2->Balance == 800 && S.CompletedRounds == 0 &&
                    S.AttackingTeam == ValStrike::Economy::Team::One && (W2->Owned & RifleBit) == 0,
                    FString::Printf(TEXT("balances %d/%d completed=%d resets=%u"), W1->Balance, W2->Balance, S.CompletedRounds, S.Resets));
                FinishAt = Now + 2.0;
            }
        }
        // Buy phase closed: purchases must equal the script.
        if (S.CurrentPhase != EcoPhase::Buy && S.RoundId == SeenRound && ClosedSequence != Sequence && Sequence <= 6)
        {
            ClosedSequence = Sequence;
            ActiveAt = Now;
            const FEconomyRound& Round = EconomyScript[Sequence - 1];
            ExpectOne -= Round.SpendOne;
            ExpectTwo -= Round.SpendTwo;
            Check(TEXT("purchase_deltas_match_ledger"), W1->Balance == ExpectOne && W2->Balance == ExpectTwo,
                FString::Printf(TEXT("close %d: one=%d expected %d, two=%d expected %d"), Sequence, W1->Balance, ExpectOne, W2->Balance, ExpectTwo));
            Record(TEXT("close"), Sequence, false);
            const auto Delta = [this, Economy](int32 Team, uint64 Id, EcoStatus Status)
            { return static_cast<int32>(Count(Economy, Id, Status)) - static_cast<int32>(OpenCounts[Team][static_cast<int32>(Status)]); };
            if (Sequence == 1)
            {
                Check(TEXT("unaffordable_rejected"), Delta(0, OneId, EcoStatus::Unaffordable) >= 1 && Delta(1, TwoId, EcoStatus::Unaffordable) >= 1 &&
                    W1->Balance == 800 && W2->Balance == 800, FString::Printf(TEXT("rifle at 800: one=%d two=%d"),
                    Delta(0, OneId, EcoStatus::Unaffordable), Delta(1, TwoId, EcoStatus::Unaffordable)));
                Check(TEXT("starter_owned_rejected"), Delta(0, OneId, EcoStatus::AlreadyOwned) >= 1, TEXT("pistol purchase while holding the starter pistol"));
                // Item None always arrives; an out-of-catalog wire value may be narrowed by enum serialization.
                Check(TEXT("unknown_item_rejected"), Delta(0, OneId, EcoStatus::UnknownItem) >= 1 && W1->Owned == Tuning.StarterItems,
                    FString::Printf(TEXT("unknown_item=%d (None plus wire value 3); owned=%u"), Delta(0, OneId, EcoStatus::UnknownItem), W1->Owned));
                Check(TEXT("replay_rejected"), Delta(0, OneId, EcoStatus::Replayed) >= 1, TEXT("RPC resent with an already used request id"));
            }
            if (Sequence == 2)
            {
                const int32 Applied = Delta(1, TwoId, EcoStatus::Applied);
                const int32 Owned = Delta(1, TwoId, EcoStatus::AlreadyOwned);
                const int32 Limited = Delta(1, TwoId, EcoStatus::RateLimited);
                Check(TEXT("spam_bounded"), Applied == 1 && Owned == Tuning.MaxRequestsPerRound - 1 &&
                    Limited == 40 - Tuning.MaxRequestsPerRound && W2->Balance == ExpectTwo && (W2->Owned & RifleBit) != 0,
                    FString::Printf(TEXT("40 rifle RPCs in one client frame: applied=%d already_owned=%d rate_limited=%d balance=%d"),
                        Applied, Owned, Limited, W2->Balance));
            }
            if (Sequence == 3)
            {
                Check(TEXT("refund_same_phase"), Delta(1, TwoId, EcoStatus::Applied) == 3 && W2->Balance == ExpectTwo && (W2->Owned & RifleBit) != 0,
                    FString::Printf(TEXT("buy, refund, buy: applied=%d balance=%d"), Delta(1, TwoId, EcoStatus::Applied), W2->Balance));
                bTwoHadRifle = (W2->Owned & RifleBit) != 0;
            }
        }
        // Scripted live-round actions.
        const SmokeMatchPhase Phase = static_cast<SmokeMatchPhase>(GS->Match.Phase);
        const bool bLive = Phase == SmokeMatchPhase::Active || Phase == SmokeMatchPhase::Planted;
        if (bLive && ClosedSequence == Sequence && Sequence <= 6 && GS->Match.RoundId == SeenRound)
        {
            if (Sequence == 1)
            {
                if ((Actions & 1) == 0 && Now - ActiveAt > 0.3)
                {
                    for (TActorIterator<ATriggerBox> It(World); It; ++It)
                    {
                        if (It->ActorHasTag(TEXT("VS_Site_A"))) { Place(One, It->GetActorLocation() + FVector(-100, 0, 0)); }
                    }
                    Actions |= 1;
                }
                if ((Actions & 2) == 0 && GS->Match.ObjectiveState == static_cast<uint8>(SmokeDevice::Planted))
                {
                    KillThroughHealth(Economy, Two, One);
                    for (TActorIterator<AVSMatchObjective> It(World); It; ++It) { Place(Two, It->GetActorLocation() + FVector(100, 0, 0)); }
                    Actions |= 2;
                }
            }
            else if ((Actions & 2) == 0 && Now - ActiveAt > 1.2)
            {
                const bool bOneKills = EconomyScript[Sequence - 1].KillerTeam == 1;
                KillThroughHealth(Economy, bOneKills ? One : Two, bOneKills ? Two : One);
                Actions |= 2;
            }
        }
        // Round result: income must equal the script and the tuning table.
        if (S.ResultAwarded && S.RoundId == SeenRound && ResultSequence != Sequence && Sequence <= 6)
        {
            ResultSequence = Sequence;
            const FEconomyRound& Round = EconomyScript[Sequence - 1];
            const uint8 Attackers = Sequence <= 3 ? 1 : 2;
            int32& WinStreak = Round.Winner == 1 ? StreakOne : StreakTwo;
            int32& LoseStreak = Round.Winner == 1 ? StreakTwo : StreakOne;
            WinStreak = 0;
            ++LoseStreak;
            const auto Income = [&](uint8 Team)
            {
                int32 Amount = Round.Winner == Team ? Tuning.RoundWinReward : LossFor(Tuning, Team == 1 ? StreakOne : StreakTwo);
                Amount += Round.bPlant && Team == Attackers ? Tuning.PlantReward : 0;
                Amount += Round.KillerTeam == Team ? Tuning.KillReward : 0;
                return Amount;
            };
            const int32 UncappedTwo = ExpectTwo + Income(2);
            ExpectOne = FMath::Min(ExpectOne + Income(1), Tuning.CreditCap);
            ExpectTwo = FMath::Min(UncappedTwo, Tuning.CreditCap);
            Check(TEXT("rewards_match_ledger"), GS->Match.Winner == Round.Winner && W1->Balance == ExpectOne && W2->Balance == ExpectTwo,
                FString::Printf(TEXT("result %d: winner=%d (script %d) one=%d expected %d, two=%d expected %d"), Sequence,
                    GS->Match.Winner, Round.Winner, W1->Balance, ExpectOne, W2->Balance, ExpectTwo));
            Record(TEXT("result"), Sequence, false);
            if (Sequence == 1)
            {
                Check(TEXT("plant_reward"), S.PlantAwarded && W1->RoundIncome == Tuning.PlantReward + LossFor(Tuning, 1) &&
                    W2->RoundIncome == Tuning.RoundWinReward + Tuning.KillReward,
                    FString::Printf(TEXT("attackers planted then lost: one +%d, two +%d"), W1->RoundIncome, W2->RoundIncome));
                Check(TEXT("dead_rejected"), Count(Economy, OneId, EcoStatus::NotAlive) >= 1, TEXT("request from the fallen planter during the planted phase"));
                Check(TEXT("out_of_phase_rejected"), Count(Economy, TwoId, EcoStatus::NotBuyPhase) >= 1, TEXT("living defender's request after preparation"));
            }
            if (Sequence >= 4) { LossIncomes.Add(W1->RoundIncome); }
            if (Sequence == 6)
            {
                Check(TEXT("loss_streak"), LossIncomes == TArray<int32>({LossFor(Tuning, 1), LossFor(Tuning, 2), LossFor(Tuning, 3)}),
                    FString::Printf(TEXT("team one losses after swap: %s"), *FString::JoinBy(LossIncomes, TEXT(","), [](int32 V) { return FString::FromInt(V); })));
                Check(TEXT("credit_cap"), UncappedTwo > Tuning.CreditCap && W2->Balance == Tuning.CreditCap,
                    FString::Printf(TEXT("uncapped=%d balance=%d cap=%d"), UncappedTwo, W2->Balance, Tuning.CreditCap));
            }
            const int32 Ignored = Economy->DevelopmentReplayRoundEvents();
            Check(TEXT("duplicate_round_events_ignored"), Ignored == 5,
                FString::Printf(TEXT("round %d: buy open/close, plant and both result claims replayed; ignored=%d"), Sequence, Ignored));
        }
        if (FinishAt > 0.0 && Now >= FinishAt)
        {
            const int32 RoundStarts = Grants.FindRef(static_cast<uint8>(EVSEconomyGrantReason::RoundStart));
            const int32 Resets = Grants.FindRef(static_cast<uint8>(EVSEconomyGrantReason::Reset));
            const int32 Refunds = Grants.FindRef(static_cast<uint8>(EVSEconomyGrantReason::Refund));
            Check(TEXT("grant_notifications"), RifleGrants >= 4 && Refunds >= 1 && Resets >= 6 && ResetRifleGrants == 0 && RoundStarts >= 6,
                FString::Printf(TEXT("purchase grants with rifle=%d refund=%d reset=%d (rifle in reset=%d) round_start=%d"),
                    RifleGrants, Refunds, Resets, ResetRifleGrants, RoundStarts));
            Finish();
        }
    }

    // ------------------------------------------------------------------------------- clients

    static void PressKey(APlayerController* Controller, FKey Key)
    {
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.0f));
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Released, 0.0f));
    }

    // Rendered runs route keys through Slate, as a real keyboard would reach the focused menu.
    void TypeKey(APlayerController* Controller, FKey Key)
    {
        if (bRendered && FSlateApplication::IsInitialized())
        {
            FSlateApplication& Slate = FSlateApplication::Get();
            const FKeyEvent Down(Key, Slate.GetModifierKeys(), 0, false, 0, 0);
            Slate.ProcessKeyDownEvent(Down);
            Slate.ProcessKeyUpEvent(FKeyEvent(Key, Slate.GetModifierKeys(), 0, false, 0, 0));
        }
        else
        {
            PressKey(Controller, Key);
        }
    }

    void Screenshot(int32 Index)
    {
        if (!bRendered) { return; }
        IFileManager::Get().MakeDirectory(*ScreenDirectory, true);
        const FString Path = FPaths::Combine(ScreenDirectory, FString(EconomyScreens[Index]) + TEXT(".png"));
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        UE_LOG(LogValStrike, Display, TEXT("VSECON screenshot %s"), *Path);
    }

    void HoldE(APlayerController* Controller, bool bWanted)
    {
        if (bWanted != bEHeld)
        {
            Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::E, bWanted ? IE_Pressed : IE_Released, bWanted ? 1.0f : 0.0f));
            bEHeld = bWanted;
        }
    }

    void ClientTick(UWorld* World)
    {
        AVSPlayerController* Controller = Cast<AVSPlayerController>(World->GetFirstPlayerController());
        UVSEconomyClientSubsystem* UI = World->GetSubsystem<UVSEconomyClientSubsystem>();
        const AVSMatchGameState* GS = World->GetGameState<AVSMatchGameState>();
        const AVSMatchPlayerState* PS = Controller ? Controller->GetPlayerState<AVSMatchPlayerState>() : nullptr;
        AVSEconomyWallet* Wallet = UI ? UI->GetLocalWallet() : nullptr;
        if (!Controller || !Controller->IsLocalController() || !GS || !PS || !Wallet || PS->ConnectionId == 0 ||
            Wallet->GetView().PlayerId == 0 || !UI->HasPresentation())
        {
            return;
        }
        const FVSEconomyWalletView& View = Wallet->GetView();
        PlayerId = View.PlayerId;
        const double Now = World->GetTimeSeconds();
        const bool bOne = PS->TeamId == 1;

        int32 Visible = 0;
        bool bOwn = true;
        for (TActorIterator<AVSEconomyWallet> It(World); It; ++It) { ++Visible; bOwn &= It->GetOwner() == Controller; }
        Check(TEXT("single_private_wallet"), Visible == 1 && bOwn && View.PlayerId == PS->ConnectionId,
            FString::Printf(TEXT("visible wallets=%d own=%d id=%llu"), Visible, bOwn, View.PlayerId));

        // Replicated checkpoints, compared with the server ledger by the runner.
        if (View.RoundId != 0 && View.RoundId != SeenRound)
        {
            SeenRound = View.RoundId;
            ++Sequence;
            OpenAt = Now;
            Step = 0;
            bMenuUsed = false;
            Checkpoints.Add(FString::Printf(TEXT("open_%d"), Sequence), View.Credits);
            Checkpoints.Add(FString::Printf(TEXT("owned_%d"), Sequence), View.OwnedMask);
            if (Sequence == 7)
            {
                Check(TEXT("rematch_seen"), View.Credits == 800, FString::Printf(TEXT("credits after rematch=%d"), View.Credits));
                FinishAt = Now + 2.0;
            }
        }
        if (!View.bBuyOpen && SeenRound == View.RoundId && ClosedSequence != Sequence && Sequence <= 6 && Now - OpenAt > 1.0)
        {
            ClosedSequence = Sequence;
            Checkpoints.Add(FString::Printf(TEXT("close_%d"), Sequence), View.Credits);
        }
        if (View.ResultRoundId == SeenRound && ResultSequence != Sequence && Sequence <= 6)
        {
            ResultSequence = Sequence;
            Checkpoints.Add(FString::Printf(TEXT("result_%d"), Sequence), View.Credits);
        }

        const SmokeMatchPhase Phase = static_cast<SmokeMatchPhase>(GS->Match.Phase);
        const bool bBuying = Phase == SmokeMatchPhase::Preparation && GS->Match.RoundId == View.RoundId && View.bBuyOpen;
        const double T = Now - OpenAt;
        const bool bPrevious = View.LastRequestId == Wallet->GetLocalRequestId(); // Previous request judged.
        if (bBuying && Sequence == 1)
        {
            if (Step == 0 && T > 0.6) { if (bOne) { Screenshot(0); } ++Step; }
            else if (Step == 1 && T > 1.0) { PressKey(Controller, EKeys::B); ++Step; }
            else if (Step == 2 && T > 1.5)
            {
                Check(TEXT("menu_opens_in_buy_phase"), UI->IsBuyMenuOpen() && Controller->bShowMouseCursor,
                    FString::Printf(TEXT("B in buy phase: open=%d cursor=%d"), UI->IsBuyMenuOpen(), Controller->bShowMouseCursor));
                bMenuUsed = true;
                Wallet->RequestPurchase(EVSEconomyItem::Rifle); // Unaffordable at the starting balance.
                if (bOne)
                {
                    Wallet->RequestPurchase(EVSEconomyItem::Pistol); // The starter pistol is already owned.
                    Wallet->RequestPurchase(EVSEconomyItem::None);
                    Wallet->RequestPurchase(static_cast<EVSEconomyItem>(3)); // Outside the catalog.
                    // Resend the last request id: the server must not evaluate it again.
                    Wallet->ServerRequest(EVSEconomyRequest::Purchase, EVSEconomyItem::Rifle, View.RoundId, Wallet->GetLocalRequestId());
                }
                ++Step;
            }
            else if (Step == 3 && T > 2.2) { if (bOne) { Screenshot(1); } ++Step; }
            else if (Step == 4 && T > 2.8) { TypeKey(Controller, EKeys::Escape); ++Step; }
            else if (Step == 5 && T > 3.3)
            {
                const bool bYielded = Controller->IsSessionMenuOpen() && !UI->IsBuyMenuOpen();
                if (bOne) { Screenshot(2); }
                PressKey(Controller, EKeys::B); // Must not reopen over the session menu.
                EscapeEvidence = FString::Printf(TEXT("escape: session=%d buy=%d"), Controller->IsSessionMenuOpen(), UI->IsBuyMenuOpen());
                bEscapeOk = bYielded;
                ++Step;
            }
            else if (Step == 6 && T > 3.8)
            {
                bEscapeOk &= Controller->IsSessionMenuOpen() && !UI->IsBuyMenuOpen();
                EscapeEvidence += FString::Printf(TEXT("; B over session: session=%d buy=%d"), Controller->IsSessionMenuOpen(), UI->IsBuyMenuOpen());
                Controller->SetSessionMenuOpen(false);
                ++Step;
            }
            else if (Step == 7 && T > 4.3)
            {
                bEscapeOk &= !Controller->IsSessionMenuOpen() && !Controller->bShowMouseCursor;
                PressKey(Controller, EKeys::B); // Reopen and leave it open into the live round.
                ++Step;
            }
            else if (Step == 8 && T > 4.8)
            {
                bEscapeOk &= UI->IsBuyMenuOpen();
                Check(TEXT("escape_menu_keeps_focus"), bEscapeOk, EscapeEvidence + FString::Printf(TEXT("; resumed then B: buy=%d"), UI->IsBuyMenuOpen()));
                ++Step;
            }
        }
        else if (bBuying && Sequence == 2)
        {
            if (Step == 0 && T > 0.8) { PressKey(Controller, EKeys::B); bMenuUsed = true; ++Step; }
            else if (Step == 1 && T > 1.3)
            {
                if (bOne)
                {
                    // Keyboard path: rendered runs send "2" through Slate to the focused menu.
                    if (bRendered) { TypeKey(Controller, EKeys::Two); } else { UI->ActivateItem(EVSEconomyItem::Rifle); }
                    KeyboardRequest = Wallet->GetLocalRequestId();
                }
                else
                {
                    for (int32 Index = 0; Index < 40; ++Index) { Wallet->RequestPurchase(EVSEconomyItem::Rifle); }
                }
                ++Step;
            }
            else if (Step == 2 && T > 2.4)
            {
                if (bOne && bRendered)
                {
                    Screenshot(3);
                    Check(TEXT("keyboard_purchase"), KeyboardRequest > 0 && View.LastRequestId == KeyboardRequest &&
                        View.LastStatus == static_cast<uint8>(EcoStatus::Applied) && View.Owns(EVSEconomyItem::Rifle) && View.CanRefund(EVSEconomyItem::Rifle),
                        FString::Printf(TEXT("Slate key 2: request=%u judged=%u status=%u credits=%d"), KeyboardRequest, View.LastRequestId, View.LastStatus, View.Credits));
                }
                ++Step;
            }
        }
        else if (bBuying && Sequence == 3)
        {
            if (Step == 0 && T > 0.8) { PressKey(Controller, EKeys::B); bMenuUsed = true; ++Step; }
            else if (bOne && Step == 1 && T > 1.2) { Wallet->RequestPurchase(EVSEconomyItem::Rifle); ++Step; } // Survivor: already owned.
            else if (!bOne && Step >= 1 && Step <= 3 && T > 1.2 + 0.5 * Step && bPrevious)
            {
                UI->ActivateItem(EVSEconomyItem::Rifle); // Buy, then refund (still refundable), then buy.
                ++Step;
            }
        }

        // Leaving preparation must close the menu and return input to the game.
        if ((Phase == SmokeMatchPhase::Active || Phase == SmokeMatchPhase::Planted) && ActiveSequence != Sequence && GS->Match.RoundId == View.RoundId)
        {
            ActiveSequence = Sequence;
            ActiveAt = Now;
            if (Sequence == 1 && !bOne) { Wallet->RequestPurchase(EVSEconomyItem::Rifle); } // Alive, but out of phase.
        }
        if (ActiveSequence == Sequence && bMenuUsed && MenuChecked != Sequence && Now - ActiveAt > 0.6)
        {
            MenuChecked = Sequence;
            Check(TEXT("menu_closes_after_buy_phase"), !UI->IsBuyMenuOpen() && !Controller->bShowMouseCursor && !Controller->IsLookInputIgnored(),
                FString::Printf(TEXT("round %d live: open=%d cursor=%d look_ignored=%d"), Sequence, UI->IsBuyMenuOpen(),
                    Controller->bShowMouseCursor, Controller->IsLookInputIgnored()));
            if (Sequence == 1 && bOne) { Screenshot(4); }
        }

        // Round 1 objective: the planter and then the defender hold E when the server offers it.
        const bool bLive = Phase == SmokeMatchPhase::Active || Phase == SmokeMatchPhase::Planted;
        HoldE(Controller, Sequence == 1 && bLive && PS->bAlive && PS->InteractionPrompt.StartsWith(TEXT("Hold E")));
        if (Sequence == 1 && bOne && bLive && PS->bParticipating && !PS->bAlive && !bDeadProbe)
        {
            bDeadProbe = true;
            Wallet->RequestPurchase(EVSEconomyItem::Rifle);
        }
        if (Phase == SmokeMatchPhase::MatchComplete && bOne && !bRematchRequested)
        {
            if (CompleteAt == 0.0) { CompleteAt = Now; }
            if (Now - CompleteAt > 1.0) { Controller->RequestMatchRematch(); bRematchRequested = true; }
        }
        if (FinishAt > 0.0 && Now >= FinishAt)
        {
            if (bRendered && bOne)
            {
                bool bAll = true;
                for (const TCHAR* Name : EconomyScreens)
                { bAll &= IFileManager::Get().FileExists(*FPaths::Combine(ScreenDirectory, FString(Name) + TEXT(".png"))); }
                Check(TEXT("screenshots_written"), bAll, ScreenDirectory);
            }
            Finish();
        }
    }

    FString Role, Output, ScreenDirectory;
    TArray<FString> Order;
    TMap<FString, FResult> Results;
    TMap<uint64, TMap<FString, int64>> Ledger;
    TMap<FString, int64> Checkpoints;
    TMap<uint8, int32> Grants;
    TArray<int32> LossIncomes;
    FDelegateHandle TickHandle;
    FString EscapeEvidence;
    uint32 OpenCounts[2][ValStrike::Economy::StatusCount] = {};
    uint64 SeenRound = 0, PlayerId = 0;
    uint32 KeyboardRequest = 0;
    int32 Sequence = 0, ClosedSequence = 0, ResultSequence = 0, ActiveSequence = 0, MenuChecked = 0, Step = 0, Actions = 0;
    int32 ExpectOne = 0, ExpectTwo = 0, StreakOne = 0, StreakTwo = 0, RifleGrants = 0, ResetRifleGrants = 0;
    double ActiveAt = 0.0, OpenAt = 0.0, FinishAt = 0.0, CompleteAt = 0.0;
    bool bFinished = false, bRendered = false, bSubscribed = false, bEHeld = false, bDeadProbe = false;
    bool bRematchRequested = false, bMenuUsed = false, bEscapeOk = false, bTwoHadRifle = false;
};

TUniquePtr<FEconomySmoke> EconomySmokeInstance;
}
}

namespace ValStrikeEconomyTests
{
void StartEconomySmoke()
{
    FString Role;
    if (!VSEconomySmokeImpl::EconomySmokeInstance && FParse::Value(FCommandLine::Get(), TEXT("VSEconomySmoke="), Role) &&
        (Role == TEXT("server") || Role == TEXT("alpha") || Role == TEXT("bravo")))
    {
        VSEconomySmokeImpl::EconomySmokeInstance = MakeUnique<VSEconomySmokeImpl::FEconomySmoke>(Role);
    }
}
}
#else
namespace ValStrikeEconomyTests { void StartEconomySmoke() {} }
#endif
