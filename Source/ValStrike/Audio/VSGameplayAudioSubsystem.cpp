#include "VSGameplayAudioSubsystem.h"

#include "../Combat/VSHealthComponent.h"
#include "../Combat/VSKnifeComponent.h"
#include "../Combat/VSWeaponComponent.h"
#include "../Combat/VSWeaponLoadoutComponent.h"
#include "../Match/VSMatchGameState.h"
#include "../Match/VSMatchObjective.h"
#include "../Match/VSMatchPlayerState.h"
#include "../VSCharacter.h"
#include "../ValStrike.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"

namespace VSGameplayAudioDetail
{
using VSGameplayAudio::Cue;
constexpr int32 MaxVariants = VSGameplayAudio::Placeholder::FootstepVariants;
constexpr float VoiceMargin = 0.05f;    // s past the end of a placeholder before it is stopped.
constexpr float MaxArtSeconds = 3.0f;   // Cap for art cues that report no finite duration.
// Footsteps sound at the feet: capsule half height below the pawn origin.
constexpr float FootOffset = -88.0f;
constexpr float OwnFootstepVolume = 0.35f;
constexpr float FirstPersonShotVolume = 0.8f;
constexpr float FirstPersonKnifeVolume = 0.7f;

int32 PcmIndex(Cue Kind, int32 Variant)
{
    return static_cast<int32>(Kind) * MaxVariants + FMath::Clamp(Variant, 0, MaxVariants - 1);
}
}

bool UVSGameplayAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Purely cosmetic: a dedicated server never creates this module.
    return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool UVSGameplayAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UVSGameplayAudioSubsystem::IsTickable() const
{
    return bActive && HasCalledBeginPlay();
}

TStatId UVSGameplayAudioSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVSGameplayAudioSubsystem, STATGROUP_Tickables);
}

bool UVSGameplayAudioSubsystem::IsAudioWorld() const
{
    const UWorld* World = GetWorld();
    return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

void UVSGameplayAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    // Also covers in-process dedicated server worlds, such as multi-client PIE.
    if (!IsAudioWorld()) { return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("VSNoGameplayAudio")))
    {
        UE_LOG(LogValStrike, Log, TEXT("Gameplay audio disabled (-VSNoGameplayAudio)."));
        return;
    }
    if (!FApp::CanEverRenderAudio() || !InWorld.GetAudioDeviceRaw())
    {
        UE_LOG(LogValStrike, Log, TEXT("Gameplay audio inactive: no audio device."));
        return;
    }
    BuildAttenuation();
    LoadArt();
    bActive = true;
    int32 ArtCount = 0;
    for (const USoundBase* Sound : ArtSounds) { ArtCount += Sound ? 1 : 0; }
    UE_LOG(LogValStrike, Log, TEXT("Gameplay audio active (client-only; %d/%d cues from art, rest placeholder)."),
        ArtCount, VSGameplayAudio::CueCount);
}

void UVSGameplayAudioSubsystem::Deinitialize()
{
    for (const FVoice& Voice : Voices)
    {
        if (UAudioComponent* Component = Voice.Component.Get()) { Component->Stop(); }
    }
    Voices.Reset();
    ShotWatches.Reset();
    KnifeWatches.Reset();
    PawnWatches.Reset();
    ArtSounds.Reset();
    PlaceholderPcm.Reset();
    bActive = false;
    Super::Deinitialize();
}

void UVSGameplayAudioSubsystem::BuildAttenuation()
{
    auto Make = [this](const TCHAR* Name, float Inner, float Falloff)
    {
        USoundAttenuation* Attenuation = NewObject<USoundAttenuation>(this, Name, RF_Transient);
        FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
        Settings.bAttenuate = true;
        Settings.bSpatialize = true;
        Settings.AttenuationShape = EAttenuationShape::Sphere;
        Settings.AttenuationShapeExtents = FVector(Inner, 0.0, 0.0);
        Settings.FalloffDistance = Falloff;
        Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
        Settings.dBAttenuationAtMax = -60.0f;
        // Occlusion and reverb sends are out of scope for the MVP.
        Settings.bEnableOcclusion = false;
        return Attenuation;
    };
    // cm. Shots carry across the map; running footsteps across a room or two.
    ShotAttenuation = Make(TEXT("VSShotAttenuation"), 400.0f, 7000.0f);
    FootstepAttenuation = Make(TEXT("VSFootstepAttenuation"), 150.0f, 2200.0f);
    ObjectiveAttenuation = Make(TEXT("VSObjectiveAttenuation"), 250.0f, 3500.0f);
}

TSoftObjectPtr<USoundBase>& UVSGameplayAudioSubsystem::ArtSlot(VSGameplayAudio::Cue Kind)
{
    using VSGameplayAudio::Cue;
    switch (Kind)
    {
    case Cue::RifleShot: return RifleShotSound;
    case Cue::PistolShot: return PistolShotSound;
    case Cue::Footstep: return FootstepSound;
    case Cue::Land: return LandSound;
    case Cue::Hurt: return HurtSound;
    case Cue::Death: return DeathSound;
    case Cue::PlantStart: return PlantStartSound;
    case Cue::DefuseStart: return DefuseStartSound;
    case Cue::DeviceBeep: return DeviceBeepSound;
    case Cue::KnifeSwing: return KnifeSwingSound;
    case Cue::KnifeWall: return KnifeWallSound;
    case Cue::KnifeFlesh: return KnifeFleshSound;
    case Cue::DefuseComplete:
    case Cue::Count: break;
    }
    return DefuseCompleteSound;
}

void UVSGameplayAudioSubsystem::LoadArt()
{
    ArtSounds.SetNum(VSGameplayAudio::CueCount);
    for (int32 Index = 0; Index < VSGameplayAudio::CueCount; ++Index)
    {
        TSoftObjectPtr<USoundBase>& Slot = ArtSlot(static_cast<VSGameplayAudio::Cue>(Index));
        ArtSounds[Index] = nullptr;
        if (Slot.IsNull()) { continue; }
        // A handful of short cues, loaded once at world start, never in the event path.
        ArtSounds[Index] = Slot.LoadSynchronous();
        if (!ArtSounds[Index])
        {
            UE_LOG(LogValStrike, Warning, TEXT("Gameplay audio: art slot %s did not load; using placeholder."),
                *Slot.ToString());
        }
    }
}

bool UVSGameplayAudioSubsystem::UsesArtAsset(VSGameplayAudio::Cue Kind) const
{
    const int32 Index = static_cast<int32>(Kind);
    return ArtSounds.IsValidIndex(Index) && ArtSounds[Index] != nullptr;
}

const AActor* UVSGameplayAudioSubsystem::LocalViewTarget() const
{
    const APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
    return PC ? PC->GetViewTarget() : nullptr;
}

void UVSGameplayAudioSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!IsAudioWorld()) { bActive = false; return; }
    const double Now = GetWorld()->GetTimeSeconds();
    UpdateVoices(Now);
    ObservePawns(Now, DeltaTime);
    ObserveOwnLife();
    ObserveObjective();
}

void UVSGameplayAudioSubsystem::ObservePawns(double Now, float DeltaTime)
{
    using VSGameplayAudio::Cue;
    const AActor* ViewTarget = LocalViewTarget();
    for (TActorIterator<AVSCharacter> It(GetWorld()); It; ++It)
    {
        AVSCharacter* Pawn = *It;
        if (!IsValid(Pawn) || !Pawn->HasActorBegunPlay()) { continue; }
        const bool bFirstPerson = Pawn == ViewTarget && Pawn->IsLocallyControlled();
        ObserveShots(*Pawn, bFirstPerson, Now);
        ObserveKnife(*Pawn, bFirstPerson);

        const UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
        const UVSHealthComponent* Health = Pawn->GetHealthComponent();
        if (!Movement || (Health && !Health->IsAlive())) { continue; }
        // A new pawn (respawn, rejoin, relevancy return) gets fresh watches.
        FPawnWatch& Watch = PawnWatches.FindOrAdd(TWeakObjectPtr<const AVSCharacter>(Pawn));
        const FVector Velocity = Movement->Velocity;
        if (Watch.Landing.Observe(Movement->IsFalling(), Velocity.Z))
        {
            Play(Cue::Land, 0, Pawn->GetRootComponent(), FVector(0.0, 0.0, VSGameplayAudioDetail::FootOffset),
                !bFirstPerson, ERange::Footstep, bFirstPerson ? 0.5f : 1.0f);
        }
        if (Watch.Steps.Step(DeltaTime, Velocity.Size2D(), Movement->MaxWalkSpeed, Movement->IsMovingOnGround(),
            Pawn->bIsCrouched))
        {
            const int32 Variant = Watch.NextVariant;
            Watch.NextVariant = (Watch.NextVariant + 1) % VSGameplayAudio::Placeholder::FootstepVariants;
            Play(Cue::Footstep, Variant, Pawn->GetRootComponent(), FVector(0.0, 0.0, VSGameplayAudioDetail::FootOffset),
                !bFirstPerson, ERange::Footstep, bFirstPerson ? VSGameplayAudioDetail::OwnFootstepVolume : 1.0f);
        }
    }
    for (auto It = ShotWatches.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
    for (auto It = KnifeWatches.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
    for (auto It = PawnWatches.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); }
    }
}

void UVSGameplayAudioSubsystem::ObserveShots(const AVSCharacter& Pawn, bool bFirstPerson, double Now)
{
    using VSGameplayAudio::Cue;
    // Same replicated accepted-shot counters and replay rule as the tracers.
    for (const EVSWeaponSlot Slot : {EVSWeaponSlot::Rifle, EVSWeaponSlot::Pistol})
    {
        const UVSWeaponComponent* Weapon = Pawn.GetWeapon(Slot);
        if (!Weapon) { continue; }
        VSShotEffects::ShotCounter& Counter = ShotWatches.FindOrAdd(TWeakObjectPtr<const UActorComponent>(Weapon));
        const VSShotEffects::Decision Decision = Counter.Observe(Weapon->GetState().ShotsFired, Now, Weapon->GetFireInterval());
        if (Decision.baseline) { ++Stats.Baselines; }
        if (!Decision.spawn) { continue; }
        // A coalesced replication batch plays one report, like its single tracer.
        const Cue Shot = Slot == EVSWeaponSlot::Pistol ? Cue::PistolShot : Cue::RifleShot;
        Play(Shot, 0, nullptr, Pawn.GetPawnViewLocation(), !bFirstPerson, ERange::Shot,
            bFirstPerson ? VSGameplayAudioDetail::FirstPersonShotVolume : 1.0f);
    }
}

void UVSGameplayAudioSubsystem::ObserveKnife(const AVSCharacter& Pawn, bool bFirstPerson)
{
    // Server-resolved impacts only: a miss swings, a wall or player has contact.
    // Firearm cues never play for the knife (it has no shot counter).
    const UVSKnifeComponent* Knife = Pawn.FindComponentByClass<UVSKnifeComponent>();
    if (!Knife) { return; }
    VSGameplayAudio::KnifeWatch& Watch = KnifeWatches.FindOrAdd(TWeakObjectPtr<const UActorComponent>(Knife));
    const VSGameplayAudio::KnifeDecision Decision =
        Watch.Observe(Knife->GetState().ImpactCount, static_cast<uint8>(Knife->GetState().LastImpact));
    if (Decision.baseline) { ++Stats.Baselines; }
    if (!Decision.play) { return; }
    Play(Decision.cue, 0, nullptr, Pawn.GetPawnViewLocation(), !bFirstPerson, ERange::Footstep,
        bFirstPerson ? VSGameplayAudioDetail::FirstPersonKnifeVolume : 1.0f);
}

void UVSGameplayAudioSubsystem::ObserveOwnLife()
{
    using VSGameplayAudio::Cue;
    const APlayerController* PC = GEngine ? GEngine->GetFirstLocalPlayerController(GetWorld()) : nullptr;
    const AVSCharacter* Own = PC ? Cast<AVSCharacter>(PC->GetPawn()) : nullptr;
    const UVSHealthComponent* Health = Own ? Own->GetHealthComponent() : OwnHealth.Get();
    if (!Health) { return; }
    if (Health != OwnHealth.Get())
    {
        // New pawn: its first value is a baseline, never a replayed hit.
        OwnHealth = Health;
        OwnLife = VSGameplayAudio::LifeWatch();
    }
    const VSGameplayAudio::LifeDecision Decision = OwnLife.Observe(Health->GetHealth());
    if (Decision.baseline) { ++Stats.Baselines; }
    if (Decision.hurt) { Play(Cue::Hurt, 0, nullptr, FVector::ZeroVector, false, ERange::Shot, 1.0f); }
    if (Decision.death) { Play(Cue::Death, 0, nullptr, FVector::ZeroVector, false, ERange::Shot, 1.0f); }
}

void UVSGameplayAudioSubsystem::ObserveObjective()
{
    using VSGameplayAudio::Cue;
    UWorld* World = GetWorld();
    const AVSMatchGameState* State = World->GetGameState<AVSMatchGameState>();
    if (!State || !State->Match.bReady) { return; }
    const FVSMatchRepState& Match = State->Match;

    const AVSMatchObjective* Device = nullptr;
    for (TActorIterator<AVSMatchObjective> It(World); It; ++It) { Device = *It; break; }
    const bool bDeviceVisible = Device && !Device->IsHidden();

    const VSGameplayAudio::ObjectiveDecision Decision =
        Objective.Observe(Match.RoundId, Match.ObjectiveState, Match.InteractionToken);
    if (Decision.baseline) { ++Stats.Baselines; }
    if (Decision.plantStart || Decision.defuseStart)
    {
        // The interacting player's pawn, else the device, else non-spatial.
        const APawn* Interactor = nullptr;
        for (const APlayerState* Player : State->PlayerArray)
        {
            const AVSMatchPlayerState* MatchPlayer = Cast<AVSMatchPlayerState>(Player);
            if (MatchPlayer && MatchPlayer->ConnectionId != 0 && MatchPlayer->ConnectionId == Match.InteractionPlayerId)
            {
                Interactor = MatchPlayer->GetPawn();
                break;
            }
        }
        const bool bHasSource = Interactor || bDeviceVisible;
        const FVector Location = Interactor ? Interactor->GetActorLocation()
            : bDeviceVisible ? Device->GetActorLocation() : FVector::ZeroVector;
        const bool bOwn = Interactor && Interactor == LocalViewTarget();
        Play(Decision.defuseStart ? Cue::DefuseStart : Cue::PlantStart, 0, nullptr, Location, bHasSource && !bOwn,
            ERange::Objective, 1.0f);
    }
    const bool bPlanted = Match.ObjectiveState == VSGameplayAudio::ObjectiveWatch::Planted;
    if (Beeps.Observe(bPlanted, State->GetServerWorldTimeSeconds(), Match.PhaseDeadlineSeconds))
    {
        Play(Cue::DeviceBeep, 0, nullptr, bDeviceVisible ? Device->GetActorLocation() : FVector::ZeroVector,
            bDeviceVisible, ERange::Objective, 1.0f);
    }
    if (Decision.defuseComplete)
    {
        Play(Cue::DefuseComplete, 0, nullptr, bDeviceVisible ? Device->GetActorLocation() : FVector::ZeroVector,
            bDeviceVisible, ERange::Objective, 1.0f);
    }
}

USoundBase* UVSGameplayAudioSubsystem::MakePlaceholder(VSGameplayAudio::Cue Kind, int32 Variant, float& OutSeconds)
{
    // PLACEHOLDER: code-generated sound until the cue's art slot is filled.
    // A procedural wave streams its queued PCM to a single playback, so each
    // play gets its own small wave; the PCM itself is generated only once.
    const int32 Index = VSGameplayAudioDetail::PcmIndex(Kind, Variant);
    if (PlaceholderPcm.Num() <= Index) { PlaceholderPcm.SetNum(VSGameplayAudio::CueCount * VSGameplayAudioDetail::MaxVariants); }
    TArray<uint8>& Pcm = PlaceholderPcm[Index];
    if (Pcm.IsEmpty())
    {
        std::vector<float> Samples;
        VSGameplayAudio::Placeholder::Generate(Kind, Variant, PlaceholderSampleRate, Samples);
        Pcm.SetNumUninitialized(static_cast<int32>(Samples.size()) * sizeof(int16));
        int16* Out = reinterpret_cast<int16*>(Pcm.GetData());
        for (size_t Sample = 0; Sample < Samples.size(); ++Sample)
        {
            Out[Sample] = static_cast<int16>(FMath::RoundToInt(FMath::Clamp(Samples[Sample], -1.0f, 1.0f) * 32767.0f));
        }
    }
    if (Pcm.IsEmpty()) { return nullptr; }
    USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this, NAME_None, RF_Transient);
    Wave->SetSampleRate(PlaceholderSampleRate);
    Wave->NumChannels = 1;
    Wave->SoundGroup = SOUNDGROUP_Effects;
    Wave->bLooping = false;
    Wave->QueueAudio(Pcm.GetData(), Pcm.Num());
    OutSeconds = static_cast<float>(Pcm.Num() / sizeof(int16)) / PlaceholderSampleRate;
    return Wave;
}

void UVSGameplayAudioSubsystem::Play(VSGameplayAudio::Cue Kind, int32 Variant, USceneComponent* AttachTo,
    const FVector& Location, bool bSpatial, ERange Range, float Volume)
{
    float Seconds = 0.0f;
    USoundBase* Sound = UsesArtAsset(Kind) ? ArtSounds[static_cast<int32>(Kind)].Get() : nullptr;
    if (Sound)
    {
        const float Duration = Sound->GetDuration();
        Seconds = Duration > 0.0f && Duration < VSGameplayAudioDetail::MaxArtSeconds ? Duration : VSGameplayAudioDetail::MaxArtSeconds;
    }
    else
    {
        Sound = MakePlaceholder(Kind, Variant, Seconds);
    }
    if (!Sound) { return; }

    USoundAttenuation* Attenuation = Range == ERange::Footstep ? FootstepAttenuation.Get()
        : Range == ERange::Objective ? ObjectiveAttenuation.Get() : ShotAttenuation.Get();
    // Art that ships its own attenuation keeps it.
    if (Sound->AttenuationSettings) { Attenuation = nullptr; }

    if (Voices.Num() >= MaxVoices)
    {
        if (UAudioComponent* Oldest = Voices[0].Component.Get()) { Oldest->Stop(); }
        Voices.RemoveAt(0);
        ++Stats.VoicesStolen;
    }
    UAudioComponent* Component = nullptr;
    if (!bSpatial)
    {
        Component = UGameplayStatics::SpawnSound2D(this, Sound, Volume);
    }
    else if (AttachTo)
    {
        Component = UGameplayStatics::SpawnSoundAttached(Sound, AttachTo, NAME_None, Location, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset, true, Volume, 1.0f, 0.0f, Attenuation);
    }
    else
    {
        Component = UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, Volume, 1.0f,
            0.0f, Attenuation);
    }
    // Counted even if the engine culled the voice: the event itself was emitted once.
    ++Stats.Played[static_cast<int32>(Kind)];
    if (bSpatial) { ++Stats.Spatialized; } else { ++Stats.NonSpatial; }
    if (Component)
    {
        Voices.Add({Component, GetWorld()->GetTimeSeconds() + Seconds + VSGameplayAudioDetail::VoiceMargin});
    }
}

void UVSGameplayAudioSubsystem::UpdateVoices(double Now)
{
    // Procedural placeholders report an open-ended duration; stop each voice
    // after its known length so it never lingers as a silent source.
    for (int32 Index = Voices.Num() - 1; Index >= 0; --Index)
    {
        UAudioComponent* Component = Voices[Index].Component.Get();
        if (!Component || !Component->IsPlaying())
        {
            Voices.RemoveAt(Index);
        }
        else if (Now >= Voices[Index].StopAt)
        {
            Component->Stop();
            Voices.RemoveAt(Index);
        }
    }
}
