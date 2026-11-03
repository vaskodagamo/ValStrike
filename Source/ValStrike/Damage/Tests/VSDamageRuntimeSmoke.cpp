#include "VSDamageRuntimeSmoke.h"

#include "Net/UnrealNetwork.h"

AVSDamageSmokeDirector::AVSDamageSmokeDirector()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(30.0f);
}

void AVSDamageSmokeDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSDamageSmokeDirector, Stage);
    DOREPLIFETIME(AVSDamageSmokeDirector, StageAt);
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "../VSDamageSubsystem.h"
#include "../VSHitZoneSet.h"
#include "../../Combat/VSHealthComponent.h"
#include "../../Combat/VSWeaponComponent.h"
#include "../../Combat/VSWeaponLoadoutComponent.h"
#include "../../Economy/VSEconomyRules.h"
#include "../../Economy/VSEconomySubsystem.h"
#include "../../Economy/VSEconomyWallet.h"
#include "../../Match/VSMatchGameMode.h"
#include "../../Match/VSMatchGameState.h"
#include "../../Match/VSMatchPlayerController.h"
#include "../../Match/VSMatchPlayerState.h"
#include "../../Scoreboard/SVSScoreboard.h"
#include "../../Scoreboard/VSMatchStatsSubsystem.h"
#include "../../Scoreboard/VSScoreboardState.h"
#include "../../Scoreboard/VSScoreboardSubsystem.h"
#include "../../VSCharacter.h"
#include "../../ValStrike.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include <cstring>

namespace
{
namespace VSD = ValStrike::Damage;
namespace VSDR = ValStrike::Rules;

constexpr const TCHAR* VSDamageDriverName = TEXT("VSDamageDriver");
constexpr const TCHAR* VSDamageObserverName = TEXT("VSDamageObserver");

struct FVSDamageProbe
{
    int32 Shape = INDEX_NONE;
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
    FString Label;
};

struct FVSCapturedDamage
{
    FString Instigator;
    FString Victim;
    VSD::HitZone Zone = VSD::HitZone::None;
    VSD::WeaponClass Weapon = VSD::WeaponClass::Rifle;
    float Applied = 0.0f;
    bool bKilled = false;
    bool bLethal = false;
};

int32 VSDamageSmokeShapeByName(const UVSHitZoneSet* Zones, const char* Name)
{
    if (!Zones) { return INDEX_NONE; }
    for (int32 Index = 0; Index < static_cast<int32>(VSD::VolumeCount); ++Index)
    {
        if (std::strcmp(Zones->GetLayout().Volumes[Index].Name, Name) == 0) { return Index; }
    }
    return INDEX_NONE;
}

// Simulated proxies shrink their capsule by CharacterMovement's NetProxyShrinkHalfHeight
// (0.01 cm by default); the zones follow the actual capsule, so allow that much.
constexpr double VSDamageSmokeTolerance = 0.05;

bool VSDamageSmokeLayoutMatches(const UVSHitZoneSet* Zones, double HalfHeight, FString& Detail)
{
    if (!Zones) { Detail = TEXT("missing zone set"); return false; }
    const VSD::Layout Expected = VSD::BuildLayout(HalfHeight);
    if (!Expected.Valid || Zones->GetShapeCount() != static_cast<int32>(VSD::VolumeCount) ||
        !FMath::IsNearlyEqual(Zones->GetLayout().CapsuleHalfHeight, HalfHeight, VSDamageSmokeTolerance))
    {
        Detail = FString::Printf(TEXT("shapes=%d layoutHalfHeight=%.2f expected=%.2f"), Zones->GetShapeCount(),
            Zones->GetLayout().CapsuleHalfHeight, HalfHeight);
        return false;
    }
    const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Zones->GetAttachParent());
    if (!Capsule || !FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleHalfHeight(), static_cast<float>(HalfHeight), static_cast<float>(VSDamageSmokeTolerance)))
    {
        Detail = TEXT("zone set not attached to a capsule of the expected half-height");
        return false;
    }
    for (int32 Index = 0; Index < Zones->GetShapeCount(); ++Index)
    {
        const VSD::ZoneVolume& Volume = Expected.Volumes[Index];
        const UShapeComponent* Shape = Zones->GetShape(Index);
        const FVector Center(Volume.Center[0], Volume.Center[1], Volume.Center[2]);
        bool bOk = Shape && Shape->GetAttachParent() == Zones && Shape->GetRelativeLocation().Equals(Center, VSDamageSmokeTolerance) &&
            Zones->ResolveZone(Shape) == Volume.Zone;
        if (const UBoxComponent* Box = Cast<UBoxComponent>(Shape))
        {
            bOk &= Volume.Shape == VSD::ZoneShape::Box &&
                Box->GetUnscaledBoxExtent().Equals(FVector(Volume.HalfExtent[0], Volume.HalfExtent[1], Volume.HalfExtent[2]), VSDamageSmokeTolerance);
        }
        else if (const USphereComponent* Sphere = Cast<USphereComponent>(Shape))
        {
            bOk &= Volume.Shape == VSD::ZoneShape::Sphere &&
                FMath::IsNearlyEqual(Sphere->GetUnscaledSphereRadius(), static_cast<float>(Volume.Radius), static_cast<float>(VSDamageSmokeTolerance));
        }
        else
        {
            bOk = false;
        }
        if (!bOk)
        {
            Detail = FString::Printf(TEXT("shape %d (%hs) differs from layout at half-height %.1f"), Index, Volume.Name, HalfHeight);
            return false;
        }
    }
    const VSD::ZoneVolume& Head = Expected.Volumes[0];
    const VSD::ZoneVolume& Chest = Expected.Volumes[1];
    const VSD::ZoneVolume& Foot = Expected.Volumes[VSD::VolumeCount - 1];
    Detail = FString::Printf(TEXT("capsuleHalfHeight=%.2f shapes=13 head z=%.1f r=%.0f chestTop=%.1f footBottom=%.1f bodyHalf=25x20"),
        Zones->GetLayout().CapsuleHalfHeight, Head.Center[2], Head.Radius, Chest.Center[2] + Chest.HalfExtent[2], Foot.Center[2] - Foot.HalfExtent[2]);
    return true;
}

bool VSDamageSmokeCollisionMatches(const UVSHitZoneSet* Zones, ECollisionResponse TraceResponse, ECollisionEnabled::Type Enabled, FString& Detail)
{
    if (!Zones || Zones->GetShapeCount() != static_cast<int32>(VSD::VolumeCount)) { Detail = TEXT("missing shapes"); return false; }
    for (int32 Index = 0; Index < Zones->GetShapeCount(); ++Index)
    {
        const UShapeComponent* Shape = Zones->GetShape(Index);
        const bool bOk = Shape && Shape->GetCollisionEnabled() == Enabled &&
            Shape->GetCollisionResponseToChannel(ECC_Visibility) == TraceResponse &&
            Shape->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore &&
            Shape->GetCollisionResponseToChannel(ECC_Camera) == ECR_Ignore &&
            Shape->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Ignore &&
            Shape->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Ignore &&
            Shape->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Ignore &&
            !Shape->IsSimulatingPhysics() && !Shape->GetGenerateOverlapEvents() && !Shape->CanEverAffectNavigation() &&
            Shape->bHiddenInGame && !Shape->GetIsReplicated();
        if (!bOk)
        {
            Detail = FString::Printf(TEXT("shape %d collision/visibility profile differs"), Index);
            return false;
        }
    }
    Detail = FString::Printf(TEXT("13 shapes: %s, visibility=%s, other channels ignore, no physics/overlap events/navigation, hidden, not replicated"),
        Enabled == ECollisionEnabled::QueryOnly ? TEXT("QueryOnly") : TEXT("NoCollision"),
        TraceResponse == ECR_Overlap ? TEXT("Overlap") : TEXT("Block"));
    return true;
}

class FVSDamageRuntimeSmoke
{
public:
    explicit FVSDamageRuntimeSmoke(FString InRole) : Role(MoveTemp(InRole))
    {
        FParse::Value(FCommandLine::Get(), TEXT("VSDamageOutput="), Output);
        ShotDirectory = FPaths::GetPath(Output);
        FParse::Value(FCommandLine::Get(), TEXT("VSDamageShots="), ShotDirectory);
        Delegate = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FVSDamageRuntimeSmoke::Tick);
    }

    ~FVSDamageRuntimeSmoke()
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(Delegate);
        if (UVSDamageSubsystem* Damage = BoundDamage.Get())
        {
            Damage->OnDamage().RemoveAll(this);
            Damage->OnKill().RemoveAll(this);
        }
    }

private:
    // ------------------------------------------------------------------ report
    void Check(const TCHAR* Name, bool bPassed, const FString& Detail)
    {
        if (Names.Contains(Name)) { return; }
        Names.Add(Name);
        const FString Safe = Detail.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));
        Checks.Add(FString::Printf(TEXT("{\"name\":\"%s\",\"passed\":%s,\"detail\":\"%s\"}"), Name, bPassed ? TEXT("true") : TEXT("false"), *Safe));
        UE_LOG(LogValStrike, Display, TEXT("VSDAMAGECHECK %s %s %s %s"), *Role, Name, bPassed ? TEXT("PASS") : TEXT("FAIL"), *Detail);
    }

    void Finish(const TCHAR* Error = nullptr)
    {
        if (bFinished) { return; }
        bFinished = true;
        const FString Report = FString::Printf(TEXT("{\"schema_version\":1,\"role\":\"%s\",\"completed\":true,\"checks\":[%s],\"error\":%s}\n"),
            *Role, *FString::Join(Checks, TEXT(",")), Error ? *FString::Printf(TEXT("\"%s\""), Error) : TEXT("null"));
        if (!Output.IsEmpty() && FFileHelper::SaveStringToFile(Report, *(Output + TEXT(".tmp")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            IFileManager::Get().Move(*Output, *(Output + TEXT(".tmp")), true, true);
        }
        UE_LOG(LogValStrike, Display, TEXT("VSDAMAGE_FINISHED %s error=%s"), *Role, Error ? Error : TEXT("none"));
    }

    void Screenshot(const TCHAR* Name) const
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) { return; }
        IFileManager::Get().MakeDirectory(*ShotDirectory, true);
        const FString Path = ShotDirectory / FString::Printf(TEXT("%s_%s.png"), *Role, Name);
        FScreenshotRequest::RequestScreenshot(Path, true, false);
        UE_LOG(LogValStrike, Display, TEXT("VSDAMAGE_SCREENSHOT %s"), *Path);
    }

    static void Key(APlayerController* PC, const FKey& Name, bool bDown, bool& bHeld)
    {
        if (PC && bDown != bHeld)
        {
            PC->InputKey(FInputKeyEventArgs::CreateSimulated(Name, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
            bHeld = bDown;
        }
    }

    static void SetDrawZones(bool bOn)
    {
        if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("vs.Damage.DrawHitZones")))
        {
            Variable->Set(bOn ? 1 : 0, ECVF_SetByCode);
        }
    }

    // ------------------------------------------------------------------ shared
    static AVSMatchPlayerController* Named(UWorld* World, const TCHAR* Name)
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            AVSMatchPlayerController* PC = Cast<AVSMatchPlayerController>(It->Get());
            if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == Name) { return PC; }
        }
        return nullptr;
    }

    static AVSCharacter* PawnNamed(UWorld* World, const TCHAR* Name)
    {
        for (TActorIterator<AVSCharacter> It(World); It; ++It)
        {
            if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerName() == Name) { return *It; }
        }
        return nullptr;
    }

    static const FVSScoreboardRow* RowFor(const AVSScoreboardState* State, const TCHAR* Name)
    {
        if (!State) { return nullptr; }
        for (const FVSScoreboardRow& Row : State->GetRows())
        {
            const FString RowName = Row.PlayerState ? Row.PlayerState->GetPlayerName() : Row.PlayerName;
            if (RowName == Name) { return &Row; }
        }
        return nullptr;
    }

    static FString Describe(const FVSScoreboardRow* Row)
    {
        return Row ? FString::Printf(TEXT("%s team=%d K=%d D=%d A=%d HS=%d DMG=%d life=%d"), *Row->PlayerName, Row->TeamId,
            Row->Kills, Row->Deaths, Row->Assists, Row->HeadshotKills, Row->DamageDealt, static_cast<int32>(Row->Life)) : FString(TEXT("missing"));
    }

    static bool RowIs(const FVSScoreboardRow* Row, int32 K, int32 D, int32 HS, int32 Damage)
    {
        return Row && Row->Kills == K && Row->Deaths == D && Row->HeadshotKills == HS && Row->DamageDealt == Damage && Row->Assists == 0;
    }

    // ------------------------------------------------------------------ server
    void Stage(AVSDamageSmokeDirector* Director, int32 Value, double Now)
    {
        Director->Stage = Value;
        Director->StageAt = Now;
        Director->ForceNetUpdate();
        UE_LOG(LogValStrike, Display, TEXT("VSDAMAGE stage=%d"), Value);
    }

    static bool Place(AVSMatchPlayerController* PC, FVector Where, float Yaw)
    {
        AVSCharacter* Pawn = PC ? Cast<AVSCharacter>(PC->GetPawn()) : nullptr;
        if (!Pawn) { return false; }
        const FRotator Rotation(0.0f, Yaw, 0.0f);
        Pawn->GetCharacterMovement()->StopMovementImmediately();
        Where.Z = 90.0;
        if (!Pawn->TeleportTo(Where, Rotation)) { return false; }
        Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        PC->SetControlRotation(Rotation);
        PC->ClientSetRotation(Rotation);
        PC->ClientSetLocation(Pawn->GetActorLocation(), Rotation);
        Pawn->ForceNetUpdate();
        return true;
    }

    void OnDamageEvent(const FVSDamageEvent& Event)
    {
        FVSCapturedDamage& Captured = DamageEvents.AddDefaulted_GetRef();
        Captured.Instigator = Event.Instigator && Event.Instigator->PlayerState ? Event.Instigator->PlayerState->GetPlayerName() : FString();
        Captured.Victim = Event.Victim && Event.Victim->GetPlayerState() ? Event.Victim->GetPlayerState()->GetPlayerName() : FString();
        Captured.Zone = Event.Zone;
        Captured.Weapon = Event.Weapon;
        Captured.Applied = Event.AppliedDamage;
        Captured.bKilled = Event.bKilled;
        Captured.bLethal = Event.bLethalRule;
    }

    void OnKillEvent(const FVSDamageEvent& Event)
    {
        ++KillEvents;
        LastKiller = Event.Instigator && Event.Instigator->PlayerState ? Event.Instigator->PlayerState->GetPlayerName() : FString();
        // OnKill fires right after the damage path's ReportKill and before the match
        // resolves the round, so this income contains the kill reward but no round result.
        UWorld* World = Event.Victim ? Event.Victim->GetWorld() : nullptr;
        IncomeAtKill = WalletIncome(World ? World->GetSubsystem<UVSEconomySubsystem>() : nullptr, Event.Instigator);
    }

    static TArray<FVSDamageProbe> BuildProbes(const UVSHitZoneSet* Zones)
    {
        TArray<FVSDamageProbe> Probes;
        const UCapsuleComponent* Capsule = Zones ? Cast<UCapsuleComponent>(Zones->GetAttachParent()) : nullptr;
        if (!Capsule) { return Probes; }
        const FTransform Frame = Capsule->GetComponentTransform();
        const VSD::Layout& Layout = Zones->GetLayout();
        const auto Add = [&](int32 Shape, const FVector& Start, const FVector& End, const FString& Label)
        {
            FVSDamageProbe& Probe = Probes.AddDefaulted_GetRef();
            Probe.Shape = Shape;
            Probe.Start = Frame.TransformPosition(Start);
            Probe.End = Frame.TransformPosition(End);
            Probe.Label = Label;
        };
        for (int32 Index = 0; Index < static_cast<int32>(VSD::VolumeCount); ++Index)
        {
            const VSD::ZoneVolume& Volume = Layout.Volumes[Index];
            const double Y = Volume.Center[1];
            const double Z = Volume.Center[2];
            Add(Index, FVector(175.0, Y, Z), FVector(-60.0, Y, Z), FString::Printf(TEXT("front:%hs"), Volume.Name));
        }
        for (const char* Arm : {"UpperArm_L", "UpperArm_R", "Forearm_L", "Forearm_R"})
        {
            const int32 Index = VSDamageSmokeShapeByName(Zones, Arm);
            const VSD::ZoneVolume& Volume = Layout.Volumes[Index];
            const double Side = Volume.Center[1] < 0.0 ? -1.0 : 1.0;
            Add(Index, FVector(0.0, Side * 175.0, Volume.Center[2]), FVector(0.0, 0.0, Volume.Center[2]), FString::Printf(TEXT("side:%hs"), Arm));
        }
        const double Top = Layout.CapsuleHalfHeight;
        Add(0, FVector(0.0, 0.0, Top + 150.0), FVector(0.0, 0.0, Top - 40.0), TEXT("above:Head"));
        const VSD::ZoneVolume& Chest = Layout.Volumes[1];
        Add(1, FVector(-175.0, 0.0, Chest.Center[2]), FVector(60.0, 0.0, Chest.Center[2]), TEXT("behind:UpperTorso"));
        return Probes;
    }

    // Same query as the weapon trace: complex off, only the shooter ignored.
    static FCollisionQueryParams ProbeParams(AActor* Shooter)
    {
        return FCollisionQueryParams(SCENE_QUERY_STAT(VSDamageSmokeProbe), false, Shooter);
    }

    // Returns the first zone shape along the probe, INDEX_NONE for no zone.
    static int32 TraceZone(UWorld* World, const FVSDamageProbe& Probe, const UVSHitZoneSet* Zones, const FCollisionQueryParams& Params, bool bSingle)
    {
        if (bSingle)
        {
            FHitResult Hit;
            return World->LineTraceSingleByChannel(Hit, Probe.Start, Probe.End, Zones->GetTraceChannel(), Params)
                ? Zones->FindShapeIndex(Hit.GetComponent()) : INDEX_NONE;
        }
        TArray<FHitResult> Hits;
        World->LineTraceMultiByChannel(Hits, Probe.Start, Probe.End, Zones->GetTraceChannel(), Params);
        int32 Best = INDEX_NONE;
        double BestDistance = TNumericLimits<double>::Max();
        for (const FHitResult& Hit : Hits)
        {
            const int32 Index = Zones->FindShapeIndex(Hit.GetComponent());
            if (Index != INDEX_NONE && Hit.Distance < BestDistance)
            {
                Best = Index;
                BestDistance = Hit.Distance;
            }
        }
        return Best;
    }

    bool RunProbes(UWorld* World, AVSCharacter* Target, AActor* Other, bool bSingle, FString& Detail)
    {
        const UVSHitZoneSet* Zones = UVSHitZoneSet::FindOn(Target);
        const TArray<FVSDamageProbe> Probes = BuildProbes(Zones);
        if (Probes.Num() != static_cast<int32>(VSD::VolumeCount) + 6) { Detail = TEXT("probe construction failed"); return false; }
        const FCollisionQueryParams Params = ProbeParams(Other);
        TArray<FString> Failures;
        TSet<int32> Zoned;
        for (const FVSDamageProbe& Probe : Probes)
        {
            const int32 Found = TraceZone(World, Probe, Zones, Params, bSingle);
            const VSD::HitZone Expected = Zones->GetLayout().Volumes[Probe.Shape].Zone;
            const VSD::HitZone Resolved = Found == INDEX_NONE ? VSD::HitZone::None : Zones->ResolveZone(Zones->GetShape(Found));
            if (Found != Probe.Shape || Resolved != Expected)
            {
                Failures.Add(FString::Printf(TEXT("%s->%hs"), *Probe.Label, VSD::ZoneName(Resolved)));
            }
            Zoned.Add(static_cast<int32>(Resolved));
        }
        Detail = Failures.IsEmpty()
            ? FString::Printf(TEXT("%d probes (13 front, 4 side arms, above head, behind chest) on %s resolved to their zones; %d zone types"),
                Probes.Num(), *Target->GetName(), Zoned.Num())
            : FString::Join(Failures, TEXT(", "));
        return Failures.IsEmpty() && Zoned.Num() == static_cast<int32>(VSD::ZoneCount) - 1;
    }

    // Real client fire at the observer, one zone per step. Boxes are aimed at the
    // centre of the face toward the shooter, so the first surface the ray meets is
    // that zone even when the ray descends (the foot).
    struct FVSFireTarget
    {
        const char* Shape;
        VSD::HitZone Zone;
        float Applied;
        float HealthAfter;
        const TCHAR* CheckName;
    };
    static const FVSFireTarget& FireTarget(int32 Step)
    {
        static const FVSFireTarget Targets[] = {
            {"Thigh_L", VSD::HitZone::Thigh, 24.0f, 76.0f, TEXT("real_fire_thigh")},
            {"UpperArm_R", VSD::HitZone::UpperArm, 24.0f, 52.0f, TEXT("real_fire_upper_arm")},
            {"Foot_L", VSD::HitZone::Foot, 17.0f, 35.0f, TEXT("real_fire_foot")},
            {"Head", VSD::HitZone::Head, 35.0f, 0.0f, TEXT("real_fire_lethal_headshot")},
        };
        return Targets[FMath::Clamp(Step, 0, 3)];
    }

    static FVector ZoneAimPoint(const AVSCharacter* Target, const char* ShapeName, const FVector& From)
    {
        const UVSHitZoneSet* Zones = UVSHitZoneSet::FindOn(Target);
        const int32 Index = VSDamageSmokeShapeByName(Zones, ShapeName);
        const UCapsuleComponent* Capsule = Zones ? Cast<UCapsuleComponent>(Zones->GetAttachParent()) : nullptr;
        if (Index == INDEX_NONE || !Capsule) { return Target ? Target->GetActorLocation() : FVector::ZeroVector; }
        const FTransform Frame = Capsule->GetComponentTransform();
        const VSD::ZoneVolume& Volume = Zones->GetLayout().Volumes[Index];
        FVector Local(Volume.Center[0], Volume.Center[1], Volume.Center[2]);
        if (Volume.Shape == VSD::ZoneShape::Box)
        {
            const FVector LocalFrom = Frame.InverseTransformPosition(From);
            Local.X += LocalFrom.X >= 0.0 ? Volume.HalfExtent[0] : -Volume.HalfExtent[0];
        }
        return Frame.TransformPosition(Local);
    }

    static int32 WalletIncome(UVSEconomySubsystem* Economy, const AController* Controller)
    {
        const AVSEconomyWallet* Wallet = Economy ? Economy->FindWallet(Controller) : nullptr;
        return Wallet ? Wallet->GetView().RoundIncome : -1;
    }

    void Server(UWorld* World, AVSMatchGameMode* Mode, AVSDamageSmokeDirector* Director)
    {
        const double Now = World->GetTimeSeconds();
        const double Time = Now - Director->StageAt;
        const VSDR::Snapshot& S = Mode->ReadRules();
        AVSMatchPlayerController* Driver = Named(World, VSDamageDriverName);
        AVSMatchPlayerController* Observer = Named(World, VSDamageObserverName);
        UVSDamageSubsystem* Damage = UVSDamageSubsystem::Get(World);
        UVSMatchStatsSubsystem* Stats = UVSMatchStatsSubsystem::Get(World);
        UVSEconomySubsystem* Economy = World->GetSubsystem<UVSEconomySubsystem>();
        if (!Driver || !Observer || !Mode->IsMapReady() || !Damage || !Stats) { return; }
        if (!BoundDamage.IsValid())
        {
            BoundDamage = Damage;
            Damage->OnDamage().AddRaw(this, &FVSDamageRuntimeSmoke::OnDamageEvent);
            Damage->OnKill().AddRaw(this, &FVSDamageRuntimeSmoke::OnKillEvent);
        }
        AVSCharacter* DriverPawn = Cast<AVSCharacter>(Driver->GetPawn());
        AVSCharacter* ObserverPawn = Cast<AVSCharacter>(Observer->GetPawn());
        const AVSScoreboardState* Board = Stats->GetState();

        switch (Director->Stage)
        {
        case 0:
        {
            if (S.CurrentPhase != VSDR::Phase::Preparation || !DriverPawn || !ObserverPawn || !Board || Board->GetRows().Num() != 2) { return; }
            int32 Pawns = 0, WithZones = 0, Blocking = 0;
            TArray<FString> Blockers;
            for (TActorIterator<AVSCharacter> It(World); It; ++It)
            {
                ++Pawns;
                const UVSHitZoneSet* Zones = UVSHitZoneSet::FindOn(*It);
                TInlineComponentArray<UVSHitZoneSet*> Sets(*It);
                WithZones += Zones && Sets.Num() == 1 && Zones == It->GetHitZones() && Zones->GetFName() == TEXT("HitZones") &&
                    Zones->GetShapeCount() == static_cast<int32>(VSD::VolumeCount) &&
                    Zones->GetTraceMode() == EVSHitZoneTraceMode::Authoritative && Zones->GetTraceChannel() == ECC_Visibility ? 1 : 0;
                TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
                for (const UPrimitiveComponent* Primitive : Primitives)
                {
                    const bool bZone = Zones && Zones->FindShapeIndex(Primitive) != INDEX_NONE;
                    if (!bZone && Primitive->IsCollisionEnabled() && Primitive->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Ignore)
                    {
                        ++Blocking;
                        Blockers.Add(Primitive->GetName());
                    }
                }
            }
            Check(TEXT("zones_on_every_pawn"), Pawns == 2 && WithZones == 2,
                FString::Printf(TEXT("pawns=%d withCharacterHitZonesAuthoritativeOnVisibility=%d"), Pawns, WithZones));
            Check(TEXT("legacy_regions_removed"), Blocking == 0,
                FString::Printf(TEXT("non-zone primitives answering Visibility traces: %d %s"), Blocking, *FString::Join(Blockers, TEXT(","))));
            FString Detail;
            const bool bProfile = VSDamageSmokeCollisionMatches(UVSHitZoneSet::FindOn(DriverPawn), ECR_Block, ECollisionEnabled::QueryOnly, Detail) &&
                VSDamageSmokeCollisionMatches(UVSHitZoneSet::FindOn(ObserverPawn), ECR_Block, ECollisionEnabled::QueryOnly, Detail);
            Check(TEXT("zones_collision_profile"), bProfile, Detail);
            const bool bLayout = VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(DriverPawn), 88.0, Detail) &&
                VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(ObserverPawn), 88.0, Detail);
            Check(TEXT("zones_standing_layout"), bLayout, Detail);
            int32 StateActors = 0;
            for (TActorIterator<AVSScoreboardState> It(World); It; ++It) { ++StateActors; }
            const FVSScoreboardRow* DriverRow = RowFor(Board, VSDamageDriverName);
            const FVSScoreboardRow* ObserverRow = RowFor(Board, VSDamageObserverName);
            Check(TEXT("stats_actor_spawned"), StateActors == 1 && Board->HasAuthority() && Board->GetIsReplicated() && DriverRow && ObserverRow &&
                DriverRow->TeamId != 0 && ObserverRow->TeamId != 0 && DriverRow->TeamId != ObserverRow->TeamId,
                FString::Printf(TEXT("actors=%d rows=%d; %s; %s"), StateActors, Board->GetRows().Num(), *Describe(DriverRow), *Describe(ObserverRow)));
            const UVSHitZoneSet* DriverZones = UVSHitZoneSet::FindOn(DriverPawn);
            const int32 Before = DamageEvents.Num();
            FVSWeaponHit Blocked;
            Blocked.Shooter = ObserverPawn;
            Blocked.Victim = DriverPawn;
            Blocked.HitComponent = DriverZones->GetShape(1);
            Blocked.Weapon = VSD::WeaponClass::Rifle;
            Blocked.DistanceCm = 400.0f;
            const float Applied = Damage->ApplyWeaponHit(Blocked);
            Check(TEXT("hook_respects_preparation_gate"), Applied == 0.0f && DriverPawn->GetHealthComponent()->GetHealth() == 100.0f && DamageEvents.Num() == Before,
                FString::Printf(TEXT("preparation: applied=%.1f health=%.1f events=%d"), Applied, DriverPawn->GetHealthComponent()->GetHealth(), DamageEvents.Num() - Before));
            RoundOne = S.RoundId;
            RoundOneDriver = DriverPawn;
            RoundOneObserver = ObserverPawn;
            Stage(Director, 1, Now);
            return;
        }
        case 1:
            if (S.CurrentPhase == VSDR::Phase::Active && DriverPawn && ObserverPawn)
            {
                if (!PlaceBoth(Mode, Driver, Observer)) { Finish(TEXT("placement_failed")); return; }
                Stage(Director, 2, Now);
            }
            return;
        case 2:
            if (Time > 2.5) { Stage(Director, 3, Now); }
            return;
        case 3:
        {
            const float HalfHeight = DriverPawn ? DriverPawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.0f;
            if (DriverPawn && DriverPawn->bIsCrouched && FMath::IsNearlyEqual(HalfHeight, 56.0f) && Time > 0.3)
            {
                const UVSHitZoneSet* Zones = UVSHitZoneSet::FindOn(DriverPawn);
                FString Detail;
                Check(TEXT("zones_crouched_layout"), VSDamageSmokeLayoutMatches(Zones, 56.0, Detail), Detail + TEXT("; driven by real client crouch input"));
                const bool bTraces = RunProbes(World, DriverPawn, ObserverPawn, true, Detail);
                Check(TEXT("crouched_trace_zones"), bTraces, Detail + TEXT("; weapon-style single Visibility trace"));
                Stage(Director, 4, Now);
            }
            else if (Time > 8.0)
            {
                Check(TEXT("zones_crouched_layout"), false, FString::Printf(TEXT("driver never crouched on the server: halfHeight=%.1f"), HalfHeight));
                Stage(Director, 4, Now);
            }
            return;
        }
        case 4:
            if (Time > 2.5) { Stage(Director, 5, Now); }
            return;
        case 5:
        {
            const float HalfHeight = DriverPawn ? DriverPawn->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 0.0f;
            if (DriverPawn && !DriverPawn->bIsCrouched && FMath::IsNearlyEqual(HalfHeight, 88.0f) && Time > 0.3)
            {
                FString Detail;
                Check(TEXT("zones_restand_layout"), VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(DriverPawn), 88.0, Detail), Detail);
                Stage(Director, 6, Now);
            }
            else if (Time > 8.0)
            {
                Check(TEXT("zones_restand_layout"), false, FString::Printf(TEXT("driver never stood up: halfHeight=%.1f"), HalfHeight));
                Stage(Director, 6, Now);
            }
            return;
        }
        case 6:
            if (S.CurrentPhase == VSDR::Phase::Active && DriverPawn && ObserverPawn)
            {
                FString Detail;
                Check(TEXT("weapon_trace_zones"), RunProbes(World, ObserverPawn, DriverPawn, true, Detail),
                    Detail + TEXT("; LineTraceSingleByChannel(Visibility) ignoring only the shooter, like the weapon"));
                StepEvents = DamageEvents.Num();
                Stage(Director, 70, Now);
            }
            return;
        case 70:
        case 71:
        case 72:
        case 73:
            RealFireStep(Director, Now, Time, Stats, Economy, Driver, DriverPawn, ObserverPawn);
            return;
        case 7:
            if (S.CurrentPhase == VSDR::Phase::Resolved && S.CompletedRounds == 1 && !Names.Contains(TEXT("stats_life_state")))
            {
                const FVSScoreboardRow* DriverRow = RowFor(Board, VSDamageDriverName);
                const FVSScoreboardRow* ObserverRow = RowFor(Board, VSDamageObserverName);
                const AVSMatchPlayerState* DriverState = Driver->GetPlayerState<AVSMatchPlayerState>();
                Check(TEXT("stats_life_state"), DriverRow && ObserverRow && DriverRow->Life == EVSScoreboardLife::Alive &&
                    ObserverRow->Life == EVSScoreboardLife::Dead && DriverState && S.RoundWinner == static_cast<VSDR::Team>(DriverState->TeamId) &&
                    S.Reason == VSDR::WinReason::Elimination,
                    FString::Printf(TEXT("round 1 elimination by a real rifle headshot; %s; %s"), *Describe(DriverRow), *Describe(ObserverRow)));
            }
            if (S.CurrentPhase == VSDR::Phase::Preparation && S.RoundId != RoundOne && DriverPawn && ObserverPawn)
            {
                if (FreshSeenAt < 0.0) { FreshSeenAt = Now; }
                const FVSScoreboardRow* DriverRow = RowFor(Board, VSDamageDriverName);
                const FVSScoreboardRow* ObserverRow = RowFor(Board, VSDamageObserverName);
                const bool bRowsAlive = DriverRow && ObserverRow && DriverRow->Life == EVSScoreboardLife::Alive &&
                    ObserverRow->Life == EVSScoreboardLife::Alive;
                if (!bRowsAlive && Now - FreshSeenAt < 1.0) { return; }
                const bool bFresh = DriverPawn != RoundOneDriver.Get() && ObserverPawn != RoundOneObserver.Get();
                FString Detail;
                const bool bLayout = VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(DriverPawn), 88.0, Detail) &&
                    VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(ObserverPawn), 88.0, Detail);
                FString Profile;
                const bool bEnabled = VSDamageSmokeCollisionMatches(UVSHitZoneSet::FindOn(ObserverPawn), ECR_Block, ECollisionEnabled::QueryOnly, Profile);
                Check(TEXT("fresh_pawn_zones"), bFresh && bLayout && bEnabled,
                    FString::Printf(TEXT("fresh=%d %s; previously dead observer's new pawn: %s"), bFresh ? 1 : 0, *Detail, *Profile));
                Check(TEXT("stats_persist_across_rounds"), RowIs(DriverRow, 1, 0, 1, 100) && RowIs(ObserverRow, 0, 1, 0, 0) && bRowsAlive,
                    FString::Printf(TEXT("rows alive %.3f s after fresh pawns were seen; %s; %s"), Now - FreshSeenAt,
                        *Describe(DriverRow), *Describe(ObserverRow)));
                Stage(Director, 8, Now);
            }
            return;
        case 8:
            if (S.CurrentPhase == VSDR::Phase::Active && DriverPawn && ObserverPawn)
            {
                if (!PlaceBoth(Mode, Driver, Observer)) { Finish(TEXT("placement_failed")); return; }
                StepEvents = DamageEvents.Num();
                ObserverIncome = WalletIncome(Economy, Observer);
                Stage(Director, 9, Now);
            }
            return;
        case 9:
        {
            // Observer equips the pistol (key 2) and fires one real shot at the driver's head.
            const FVSCapturedDamage* Shot = DamageEvents.Num() > StepEvents ? &DamageEvents[StepEvents] : nullptr;
            if (Shot && !Names.Contains(TEXT("real_fire_pistol_headshot")))
            {
                const int32 Income = IncomeAtKill;
                const int32 Reward = Economy ? Economy->GetModel().GetTuning().KillReward : -1;
                Check(TEXT("real_fire_pistol_headshot"), DamageEvents.Num() == StepEvents + 1 && Shot->Instigator == VSDamageObserverName &&
                    Shot->Victim == VSDamageDriverName && Shot->Weapon == VSD::WeaponClass::Pistol && Shot->Zone == VSD::HitZone::Head &&
                    Shot->bLethal && Shot->bKilled && Shot->Applied == 100.0f && KillEvents == 2 && LastKiller == VSDamageObserverName &&
                    DriverPawn && !DriverPawn->GetHealthComponent()->IsAlive() && Income == ObserverIncome + Reward,
                    FString::Printf(TEXT("pistol (key 2) head: weapon=%hs zone=%hs applied=%.1f lethalRule=%d killed=%d killEvents=%d killer income %d before, %d inside OnKill (kill reward +%d)"),
                        VSD::WeaponName(Shot->Weapon), VSD::ZoneName(Shot->Zone), Shot->Applied, Shot->bLethal ? 1 : 0, Shot->bKilled ? 1 : 0,
                        KillEvents, ObserverIncome, Income, Reward));
            }
            if (!Shot && Time > 10.0)
            {
                Check(TEXT("real_fire_pistol_headshot"), false, TEXT("no damage event from the observer's pistol shot"));
                Stage(Director, 10, Now);
                return;
            }
            if ((S.CurrentPhase == VSDR::Phase::Resolved || S.CurrentPhase == VSDR::Phase::Preparation) && S.CompletedRounds == 2)
            {
                const FVSScoreboardRow* DriverRow = RowFor(Board, VSDamageDriverName);
                const FVSScoreboardRow* ObserverRow = RowFor(Board, VSDamageObserverName);
                Check(TEXT("stats_final"), RowIs(DriverRow, 1, 1, 1, 100) && RowIs(ObserverRow, 1, 1, 1, 100) && S.Scores[0] == 1 && S.Scores[1] == 1,
                    FString::Printf(TEXT("score=%d:%d; %s; %s"), S.Scores[0], S.Scores[1], *Describe(DriverRow), *Describe(ObserverRow)));
                Stage(Director, 10, Now);
            }
            return;
        }
        case 10:
            if (Time > 4.0) { Finish(); }
            return;
        default:
            return;
        }
    }

    bool PlaceBoth(AVSMatchGameMode* Mode, AVSMatchPlayerController* Driver, AVSMatchPlayerController* Observer)
    {
        const FVector Center = Mode->GetSiteCenter();
        return Place(Driver, Center + FVector(-100.0, 0.0, 0.0), 0.0f) && Place(Observer, Center + FVector(200.0, 0.0, 0.0), 180.0f);
    }

    void RealFireStep(AVSDamageSmokeDirector* Director, double Now, double Time, UVSMatchStatsSubsystem* Stats,
        UVSEconomySubsystem* Economy, AVSMatchPlayerController* Driver, AVSCharacter* DriverPawn, AVSCharacter* ObserverPawn)
    {
        const int32 Step = Director->Stage - 70;
        const FVSFireTarget& Target = FireTarget(Step);
        const FVSCapturedDamage* Shot = DamageEvents.Num() > StepEvents ? &DamageEvents[StepEvents] : nullptr;
        if (!Shot && Time <= 6.0) { return; }
        UVSWeaponComponent* Rifle = DriverPawn ? DriverPawn->GetWeapon(EVSWeaponSlot::Rifle) : nullptr;
        const float Health = ObserverPawn ? ObserverPawn->GetHealthComponent()->GetHealth() : -1.0f;
        if (!Shot)
        {
            Check(Target.CheckName, false, FString::Printf(TEXT("no damage event within 6 s; rifle shots=%u rejected direction=%u origin=%u"),
                Rifle ? Rifle->GetState().ShotsFired : 0, Rifle ? Rifle->DevelopmentRejections(TEXT("direction")) : 0,
                Rifle ? Rifle->DevelopmentRejections(TEXT("origin")) : 0));
        }
        else
        {
            const bool bKill = Step == 3;
            Check(Target.CheckName, DamageEvents.Num() == StepEvents + 1 && Shot->Instigator == VSDamageDriverName &&
                Shot->Victim == VSDamageObserverName && Shot->Weapon == VSD::WeaponClass::Rifle && Shot->Zone == Target.Zone &&
                Shot->Applied == Target.Applied && Health == Target.HealthAfter && Shot->bKilled == bKill && Shot->bLethal == bKill,
                FString::Printf(TEXT("aimed %hs: event %s->%s %hs %hs applied=%.1f health=%.1f killed=%d lethalRule=%d rifleShots=%u"),
                    Target.Shape, *Shot->Instigator, *Shot->Victim, VSD::WeaponName(Shot->Weapon), VSD::ZoneName(Shot->Zone), Shot->Applied,
                    Health, Shot->bKilled ? 1 : 0, Shot->bLethal ? 1 : 0, Rifle ? Rifle->GetState().ShotsFired : 0));
        }
        StepEvents = DamageEvents.Num();
        if (Step < 3)
        {
            // The killer's income right before the lethal shot, for the economy check.
            if (Step == 2) { DriverIncome = WalletIncome(Economy, Driver); }
            Stage(Director, Director->Stage + 1, Now);
            return;
        }
        AfterRoundOneKill(Stats, Economy, Driver, DriverPawn, ObserverPawn);
        Stage(Director, 7, Now);
    }

    void AfterRoundOneKill(UVSMatchStatsSubsystem* Stats, UVSEconomySubsystem* Economy, AVSMatchPlayerController* Driver,
        AVSCharacter* DriverPawn, AVSCharacter* ObserverPawn)
    {
        Check(TEXT("kill_event_once"), KillEvents == 1 && LastKiller == VSDamageDriverName,
            FString::Printf(TEXT("OnKill broadcasts=%d killer=%s"), KillEvents, *LastKiller));
        const int32 Reward = Economy ? Economy->GetModel().GetTuning().KillReward : -1;
        Check(TEXT("economy_kill_reward"), Economy && Economy->IsEconomyActive() && Reward > 0 && IncomeAtKill == DriverIncome + Reward,
            FString::Printf(TEXT("killer round income %d before the shot, %d inside OnKill (kill reward %d via ReportKill in the damage path), %d after the round result"),
                DriverIncome, IncomeAtKill, Reward, WalletIncome(Economy, Driver)));

        UVSHitZoneSet* ObserverZones = UVSHitZoneSet::FindOn(ObserverPawn);
        const UVSHitZoneSet* DriverZones = UVSHitZoneSet::FindOn(DriverPawn);
        UVSDamageSubsystem* Damage = BoundDamage.Get();
        FVSWeaponHit Dead;
        Dead.Shooter = DriverPawn;
        Dead.Victim = ObserverPawn;
        Dead.HitComponent = ObserverZones ? ObserverZones->GetShape(0) : nullptr;
        FVSWeaponHit Foreign;
        Foreign.Shooter = ObserverPawn;
        Foreign.Victim = DriverPawn;
        Foreign.HitComponent = ObserverZones ? ObserverZones->GetShape(1) : nullptr; // another pawn's shape
        FVSWeaponHit BadWeapon = Foreign;
        BadWeapon.HitComponent = DriverZones ? DriverZones->GetShape(1) : nullptr;
        BadWeapon.Weapon = VSD::WeaponClass::Count;
        FVSWeaponHit NonZone = Foreign;
        NonZone.HitComponent = DriverPawn ? DriverPawn->GetCapsuleComponent() : nullptr; // owned by the victim, not a zone
        FVSWeaponHit Missing = Foreign;
        Missing.HitComponent = nullptr;
        const float Health = DriverPawn ? DriverPawn->GetHealthComponent()->GetHealth() : -1.0f;
        const int32 EventsAfter = DamageEvents.Num();
        const float Rejected = Damage ? Damage->ApplyWeaponHit(Dead) + Damage->ApplyWeaponHit(Foreign) + Damage->ApplyWeaponHit(BadWeapon) +
            Damage->ApplyWeaponHit(NonZone) + Damage->ApplyWeaponHit(Missing) : -1.0f;
        Check(TEXT("hook_rejects_invalid"), Rejected == 0.0f && DamageEvents.Num() == EventsAfter && DriverPawn &&
            DriverPawn->GetHealthComponent()->GetHealth() == Health,
            TEXT("dead victim, another pawn's shape, invalid weapon class, the victim's capsule and a null component all applied 0 without events"));

        FString Profile;
        Check(TEXT("dead_zones_disabled"), VSDamageSmokeCollisionMatches(ObserverZones, ECR_Block, ECollisionEnabled::NoCollision, Profile), Profile);

        const AVSScoreboardState* Board = Stats->GetState();
        const FVSScoreboardRow* DriverRow = RowFor(Board, VSDamageDriverName);
        const FVSScoreboardRow* ObserverRow = RowFor(Board, VSDamageObserverName);
        Check(TEXT("stats_kill_attributed"), RowIs(DriverRow, 1, 0, 1, 100) && RowIs(ObserverRow, 0, 1, 0, 0),
            FString::Printf(TEXT("%s; %s"), *Describe(DriverRow), *Describe(ObserverRow)));
    }

    // ------------------------------------------------------------------ client
    void Client(UWorld* World, AVSMatchGameState* GS, AVSDamageSmokeDirector* Director)
    {
        APlayerController* PC = World->GetFirstPlayerController();
        if (!PC || !PC->IsLocalController() || !PC->PlayerState) { return; }
        const bool bDriver = Role == TEXT("driver");
        const TCHAR* Mine = bDriver ? VSDamageDriverName : VSDamageObserverName;
        const TCHAR* Theirs = bDriver ? VSDamageObserverName : VSDamageDriverName;
        if (!bNameRequested)
        {
            PC->SetName(Mine);
            bNameRequested = true;
        }
        if (!Director || PC->PlayerState->GetPlayerName() != Mine) { return; }
        // Local clock from when this client first saw the stage: immune to server-time estimate skew.
        if (Director->Stage != SeenStage)
        {
            SeenStage = Director->Stage;
            SeenStageAt = World->GetTimeSeconds();
        }
        const double Time = World->GetTimeSeconds() - SeenStageAt;
        UVSScoreboardSubsystem* Presenter = World->GetSubsystem<UVSScoreboardSubsystem>();
        const AVSScoreboardState* Board = Presenter ? Presenter->GetState() : nullptr;
        AVSCharacter* Own = PawnNamed(World, Mine);
        AVSCharacter* Other = PawnNamed(World, Theirs);
        const int32 StageValue = Director->Stage;

        if (StageValue >= 1 && StageValue <= 2 && Own && Other)
        {
            FString OwnDetail, OtherDetail;
            const bool bOwn = VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(Own), 88.0, OwnDetail);
            const bool bOther = VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(Other), 88.0, OtherDetail);
            int32 Pawns = 0;
            for (TActorIterator<AVSCharacter> It(World); It; ++It) { ++Pawns; }
            Check(TEXT("client_zones_present"), bOwn && bOther && Pawns == 2 && !Other->HasAuthority() &&
                Other->GetLocalRole() == ROLE_SimulatedProxy && Own->GetLocalRole() == ROLE_AutonomousProxy &&
                Own->GetHitZones() == UVSHitZoneSet::FindOn(Own) && Other->GetHitZones() == UVSHitZoneSet::FindOn(Other),
                FString::Printf(TEXT("autonomous: %s | simulated: %s"), *OwnDetail, *OtherDetail));
            RoundOneOwn = Own->GetName();
            RoundOneOther = Other->GetName();
        }
        if (StageValue >= 1 && Board)
        {
            Check(TEXT("client_stats_actor"), !Board->HasAuthority() && Board->GetRows().Num() == 2 && RowFor(Board, Mine) && RowFor(Board, Theirs),
                FString::Printf(TEXT("replicated rows=%d revision=%d; %s; %s"), Board->GetRows().Num(), Board->GetRevision(),
                    *Describe(RowFor(Board, Mine)), *Describe(RowFor(Board, Theirs))));
        }

        // Crouch input and zone views.
        Key(PC, EKeys::LeftControl, bDriver && (StageValue == 3 || StageValue == 4), bCrouchHeld);
        if (!bDriver && StageValue >= 2 && StageValue <= 4 && Other && Own)
        {
            const FVector Eye = Own->GetPawnViewLocation();
            PC->SetControlRotation((Other->GetActorLocation() - Eye).Rotation());
            if (Time > 0.3) { SetDrawZones(true); }
        }
        if (!bDriver && StageValue == 2 && Time > 1.0 && Other && !bShotStanding)
        {
            const UVSHitZoneSet* OtherZones = UVSHitZoneSet::FindOn(Other);
            const UVSHitZoneSet* OwnZones = UVSHitZoneSet::FindOn(Own);
            Check(TEXT("client_debug_draw"), OtherZones && OtherZones->IsDebugDrawn() && OwnZones && !OwnZones->IsDebugDrawn() &&
                UVSHitZoneSet::IsDebugDrawRequested(),
                TEXT("vs.Damage.DrawHitZones 1 draws the other player's zones; own pawn's zones stay undrawn"));
            Screenshot(TEXT("zones_standing"));
            bShotStanding = true;
        }
        if (StageValue == 4 && Time > 0.4)
        {
            AVSCharacter* Crouched = bDriver ? Own : Other;
            FString Detail;
            const bool bCrouched = Crouched && Crouched->bIsCrouched && VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(Crouched), 56.0, Detail);
            if (bCrouched || Time > 1.4)
            {
                Check(TEXT("client_crouched_zones"), bCrouched, FString::Printf(TEXT("%s %s: %s"),
                    bDriver ? TEXT("own autonomous pawn") : TEXT("simulated driver"), Crouched && Crouched->bIsCrouched ? TEXT("crouched") : TEXT("standing"), *Detail));
                if (!bDriver && bCrouched && !bShotCrouched)
                {
                    Screenshot(TEXT("zones_crouched"));
                    bShotCrouched = true;
                }
            }
        }
        if (!bDriver && StageValue >= 5) { SetDrawZones(false); }

        // Real fire: the driver aims at one observer zone per step and taps the left mouse button.
        if (bDriver && StageValue >= 70 && StageValue <= 73 && Own && Other)
        {
            const FVector Eye = Own->GetPawnViewLocation();
            const FVector AimPoint = ZoneAimPoint(Other, FireTarget(StageValue - 70).Shape, Eye);
            PC->SetControlRotation((AimPoint - Eye).Rotation());
            if (Time > 0.6 && FiredStage != StageValue)
            {
                Key(PC, EKeys::LeftMouseButton, true, bFireHeld);
                FiredStage = StageValue;
                FireReleaseAt = World->GetTimeSeconds() + 0.03;
            }
        }
        // Round two: the observer switches to the pistol and shoots the driver's head once.
        if (!bDriver && StageValue == 9 && Own && Other && Own->GetLoadoutComponent())
        {
            if (Time > 0.3 && !bEquipSent) { Key(PC, EKeys::Two, true, bEquipHeld); bEquipSent = true; }
            else if (bEquipHeld) { Key(PC, EKeys::Two, false, bEquipHeld); }
            const FVector Eye = Own->GetPawnViewLocation();
            PC->SetControlRotation((ZoneAimPoint(Other, "Head", Eye) - Eye).Rotation());
            const FVSLoadoutState& Loadout = Own->GetLoadoutComponent()->GetState();
            if (Time > 1.0 && Loadout.Equipped == EVSWeaponSlot::Pistol && !Loadout.bEquipping && FiredStage != StageValue)
            {
                Key(PC, EKeys::LeftMouseButton, true, bFireHeld);
                FiredStage = StageValue;
                FireReleaseAt = World->GetTimeSeconds() + 0.03;
            }
        }
        // One rifle shot per tap: release before the 0.12 s automatic repeat.
        if (bFireHeld && World->GetTimeSeconds() >= FireReleaseAt) { Key(PC, EKeys::LeftMouseButton, false, bFireHeld); }

        // After the attributed round-one kill: replicated stats and the Tab board.
        const FVSScoreboardRow* DriverRow = RowFor(Board, VSDamageDriverName);
        const FVSScoreboardRow* ObserverRow = RowFor(Board, VSDamageObserverName);
        if (StageValue >= 7 && StageValue < 70 && RowIs(DriverRow, 1, 0, 1, 100) && RowIs(ObserverRow, 0, 1, 0, 0))
        {
            Check(TEXT("stats_replicated_kill"), ObserverRow->Life == EVSScoreboardLife::Dead || StageValue >= 8,
                FString::Printf(TEXT("%s; %s"), *Describe(DriverRow), *Describe(ObserverRow)));
            if (BoardStep == 0) { BoardStep = 1; BoardAt = World->GetTimeSeconds(); }
        }
        if (BoardStep > 0 && Presenter) { BoardSequence(World, PC, Presenter, GS); }

        if (StageValue >= 8 && StageValue < 70 && Own && Other && !RoundOneOwn.IsEmpty() && !Names.Contains(TEXT("client_fresh_pawn_zones")))
        {
            FString OwnDetail, OtherDetail;
            const bool bFresh = Own->GetName() != RoundOneOwn && Other->GetName() != RoundOneOther;
            const bool bLayout = VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(Own), 88.0, OwnDetail) &&
                VSDamageSmokeLayoutMatches(UVSHitZoneSet::FindOn(Other), 88.0, OtherDetail);
            const UVSHitZoneSet* OtherZones = UVSHitZoneSet::FindOn(Other);
            if (bFresh && bLayout && OtherZones->AreZonesEnabled() && UVSHitZoneSet::FindOn(Own)->AreZonesEnabled())
            {
                Check(TEXT("client_fresh_pawn_zones"), true, FString::Printf(TEXT("round 2 pawns %s / %s (round 1: %s / %s) rebuilt zones: %s"),
                    *Own->GetName(), *Other->GetName(), *RoundOneOwn, *RoundOneOther, *OwnDetail));
            }
            else if (World->GetTimeSeconds() - SeenStageAt > 5.0)
            {
                Check(TEXT("client_fresh_pawn_zones"), false, FString::Printf(TEXT("fresh=%d own: %s | other: %s"), bFresh ? 1 : 0, *OwnDetail, *OtherDetail));
            }
        }

        if (StageValue >= 10 && StageValue < 70 && RowIs(DriverRow, 1, 1, 1, 100) && RowIs(ObserverRow, 1, 1, 1, 100) &&
            GS->Match.ScoreOne == 1 && GS->Match.ScoreTwo == 1 && BoardStep >= 99)
        {
            Check(TEXT("stats_replicated_final"), true, FString::Printf(TEXT("%s; %s"), *Describe(DriverRow), *Describe(ObserverRow)));
            const FVSScoreboardModel Model = Presenter->BuildModel();
            const FVSScoreboardTeam* One = Model.FindTeam(1);
            const FVSScoreboardTeam* Two = Model.FindTeam(2);
            const FVSScoreboardLine* MineLine = Model.FindLine(Mine);
            Check(TEXT("scoreboard_round_score"), Model.bHasMatch && One && Two && One->Score == GS->Match.ScoreOne &&
                Two->Score == GS->Match.ScoreTwo && One->Lines.Num() == 1 && Two->Lines.Num() == 1 && MineLine && MineLine->bLocal,
                FString::Printf(TEXT("model %s %d : %d %s from replicated match state %d:%d; status '%s'"),
                    One ? *One->Role : TEXT("?"), One ? One->Score : -1, Two ? Two->Score : -1, Two ? *Two->Role : TEXT("?"),
                    GS->Match.ScoreOne, GS->Match.ScoreTwo, *Model.Status));
            if (!bFinalShot)
            {
                bFinalShot = true;
                FinalAt = World->GetTimeSeconds();
                Key(PC, EKeys::Tab, true, bTabHeld);
            }
        }
        if (bFinalShot && World->GetTimeSeconds() - FinalAt > 0.4 && !bFinalShotTaken)
        {
            Screenshot(TEXT("scoreboard_final"));
            bFinalShotTaken = true;
        }
        if (bFinalShotTaken && World->GetTimeSeconds() - FinalAt > 1.0)
        {
            Key(PC, EKeys::Tab, false, bTabHeld);
            Finish();
        }
    }

    void BoardSequence(UWorld* World, APlayerController* PC, UVSScoreboardSubsystem* Presenter, AVSMatchGameState* GS)
    {
        const double Elapsed = World->GetTimeSeconds() - BoardAt;
        AVSPlayerController* Session = Cast<AVSPlayerController>(PC);
        const bool bSlate = FSlateApplication::IsInitialized();
        switch (BoardStep)
        {
        case 1:
            FocusBefore = bSlate ? FSlateApplication::Get().GetKeyboardFocusedWidget() : TSharedPtr<SWidget>();
            Check(TEXT("scoreboard_hidden_by_default"), !Presenter->IsScoreboardVisible(), TEXT("not shown without Tab"));
            Key(PC, EKeys::Tab, true, bTabHeld);
            BoardStep = 2;
            return;
        case 2:
            if (Elapsed > 0.35)
            {
                const TSharedPtr<SVSScoreboard> Widget = Presenter->GetWidget();
                const FVSScoreboardModel& Model = Presenter->GetShownModel();
                const FVSScoreboardLine* DriverLine = Model.FindLine(VSDamageDriverName);
                const FVSScoreboardLine* ObserverLine = Model.FindLine(VSDamageObserverName);
                const TSharedPtr<SWidget> FocusNow = bSlate ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
                const bool bShown = Presenter->IsScoreboardVisible() && Widget.IsValid() &&
                    Widget->GetVisibility() == EVisibility::HitTestInvisible && !Widget->SupportsKeyboardFocus() &&
                    FocusNow == FocusBefore.Pin() && (!FocusNow.IsValid() || FocusNow != Widget);
                const bool bContent = Model.bHasMatch && Model.Teams.Num() == 2 && DriverLine && ObserverLine &&
                    DriverLine->Kills == 1 && DriverLine->HeadshotKills == 1 && DriverLine->Deaths == 0 && DriverLine->DamageDealt == 100 &&
                    ObserverLine->Deaths == 1 && ObserverLine->DamageDealt == 0 && DriverLine->PingMs >= 0 && ObserverLine->PingMs >= 0 &&
                    DriverLine->Life == EVSScoreboardLife::Alive && (ObserverLine->Life == EVSScoreboardLife::Dead || GS->Match.Phase == static_cast<uint8>(VSDR::Phase::Preparation));
                Check(TEXT("scoreboard_tab_shows"), bShown && bContent,
                    FString::Printf(TEXT("Tab held: visible=%d hitTestInvisible=%d focusable=%d focusUnchanged=%d (%s); driver K%d D%d HS%d DMG%d ping%d %s; observer K%d D%d DMG%d ping%d %s; status '%s'"),
                        Presenter->IsScoreboardVisible() ? 1 : 0, Widget.IsValid() && Widget->GetVisibility() == EVisibility::HitTestInvisible ? 1 : 0,
                        Widget.IsValid() && Widget->SupportsKeyboardFocus() ? 1 : 0, FocusNow == FocusBefore.Pin() ? 1 : 0,
                        FocusNow.IsValid() ? *FocusNow->GetTypeAsString() : TEXT("none"),
                        DriverLine ? DriverLine->Kills : -1, DriverLine ? DriverLine->Deaths : -1, DriverLine ? DriverLine->HeadshotKills : -1,
                        DriverLine ? DriverLine->DamageDealt : -1, DriverLine ? DriverLine->PingMs : -1,
                        DriverLine && DriverLine->Life == EVSScoreboardLife::Alive ? TEXT("ALIVE") : TEXT("not alive"),
                        ObserverLine ? ObserverLine->Kills : -1, ObserverLine ? ObserverLine->Deaths : -1, ObserverLine ? ObserverLine->DamageDealt : -1,
                        ObserverLine ? ObserverLine->PingMs : -1, ObserverLine && ObserverLine->Life == EVSScoreboardLife::Dead ? TEXT("DEAD") : TEXT("not dead"),
                        *Model.Status));
                Screenshot(TEXT("scoreboard"));
                BoardStep = 3;
            }
            return;
        case 3:
            if (Elapsed > 0.9) { Key(PC, EKeys::Tab, false, bTabHeld); BoardStep = 4; }
            return;
        case 4:
            if (Elapsed > 1.2)
            {
                Check(TEXT("scoreboard_tab_hides"), !Presenter->IsScoreboardVisible() &&
                    (!Presenter->GetWidget().IsValid() || Presenter->GetWidget()->GetVisibility() == EVisibility::Collapsed), TEXT("released Tab collapses the board"));
                if (Session) { Session->SetSessionMenuOpen(true); }
                BoardStep = 5;
            }
            return;
        case 5:
            if (Elapsed > 1.5)
            {
                FocusMenu = bSlate ? FSlateApplication::Get().GetKeyboardFocusedWidget() : TSharedPtr<SWidget>();
                Key(PC, EKeys::Tab, true, bTabHeld);
                Presenter->DevelopmentForceVisible(true);
                BoardStep = 6;
            }
            return;
        case 6:
            if (Elapsed > 1.85)
            {
                const TSharedPtr<SWidget> FocusNow = bSlate ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
                const TSharedPtr<SWidget> MenuFocus = FocusMenu.Pin();
                const TSharedPtr<SWidget> BoardWidget = Presenter->GetWidget();
                Check(TEXT("scoreboard_respects_menu"), Session && Session->IsSessionMenuOpen() && !Presenter->IsScoreboardVisible() &&
                    FocusNow == MenuFocus && (!FocusNow.IsValid() || FocusNow != BoardWidget),
                    FString::Printf(TEXT("menu open + Tab state + forced show: board visible=%d, menu focus kept=%d (focused %s)"),
                        Presenter->IsScoreboardVisible() ? 1 : 0, FocusNow == MenuFocus ? 1 : 0,
                        FocusNow.IsValid() ? *FocusNow->GetTypeAsString() : TEXT("none")));
                Presenter->DevelopmentForceVisible(false);
                Key(PC, EKeys::Tab, false, bTabHeld);
                if (Session) { Session->SetSessionMenuOpen(false); }
                BoardStep = 99;
            }
            return;
        default:
            return;
        }
    }

    // ------------------------------------------------------------------ tick
    void Tick(UWorld* World, ELevelTick, float)
    {
        if (bFinished || !World || !World->IsGameWorld()) { return; }
        const bool bServer = Role == TEXT("server");
        if ((bServer && World->GetNetMode() != NM_DedicatedServer) || (!bServer && World->GetNetMode() != NM_Client)) { return; }
        AVSMatchGameState* GS = World->GetGameState<AVSMatchGameState>();
        if (!GS) { return; }
        AVSDamageSmokeDirector* Director = nullptr;
        TActorIterator<AVSDamageSmokeDirector> DirectorIt(World);
        if (DirectorIt) { Director = *DirectorIt; }
        if (bServer)
        {
            if (!Director) { Director = World->SpawnActor<AVSDamageSmokeDirector>(); }
            AVSMatchGameMode* Mode = World->GetAuthGameMode<AVSMatchGameMode>();
            if (Mode && Director) { Server(World, Mode, Director); }
        }
        else
        {
            Client(World, GS, Director);
        }
        if (!bFinished && World->GetTimeSeconds() > 200.0) { Finish(TEXT("native_scenario_timeout")); }
    }

    FString Role, Output, ShotDirectory, LastKiller;
    TArray<FString> Checks;
    TSet<FString> Names;
    TArray<FVSCapturedDamage> DamageEvents;
    FDelegateHandle Delegate;
    TWeakObjectPtr<UVSDamageSubsystem> BoundDamage;
    TWeakObjectPtr<AVSCharacter> RoundOneDriver, RoundOneObserver;
    FString RoundOneOwn, RoundOneOther;
    int32 SeenStage = -1, FiredStage = -1, StepEvents = 0, DriverIncome = -1, ObserverIncome = -1, IncomeAtKill = -1;
    double SeenStageAt = 0.0, FreshSeenAt = -1.0, FireReleaseAt = 0.0;
    bool bFireHeld = false, bEquipHeld = false, bEquipSent = false;
    TWeakPtr<SWidget> FocusBefore, FocusMenu;
    uint64 RoundOne = 0;
    int32 KillEvents = 0, BoardStep = 0;
    double BoardAt = 0.0, FinalAt = 0.0;
    bool bFinished = false, bNameRequested = false, bCrouchHeld = false, bTabHeld = false;
    bool bShotStanding = false, bShotCrouched = false, bFinalShot = false, bFinalShotTaken = false;
};

TUniquePtr<FVSDamageRuntimeSmoke> GVSDamageRuntimeSmoke;
}

void ValStrikeDamageTests::StartRuntimeSmoke()
{
    FString Role;
    if (!GVSDamageRuntimeSmoke && FParse::Value(FCommandLine::Get(), TEXT("VSDamageSmoke="), Role) &&
        (Role == TEXT("server") || Role == TEXT("driver") || Role == TEXT("observer")))
    {
        GVSDamageRuntimeSmoke = MakeUnique<FVSDamageRuntimeSmoke>(Role);
        UE_LOG(LogValStrike, Display, TEXT("VSDAMAGE runtime smoke started role=%s"), *Role);
    }
}
#else
void ValStrikeDamageTests::StartRuntimeSmoke() {}
#endif
