#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "../Effects/VSShotEffectsState.h"
#include "VSGameplayAudioState.h"
#include "VSGameplayAudioSubsystem.generated.h"

class AActor;
class AVSCharacter;
class UAudioComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class UVSHealthComponent;

struct FVSGameplayAudioStats
{
    uint32 Played[VSGameplayAudio::CueCount] = {};
    uint32 Spatialized = 0;   // Cues played with 3D attenuation.
    uint32 NonSpatial = 0;    // First-person and own-state cues.
    uint32 Baselines = 0;     // Counters/states adopted silently (first sight, reset, stale jump).
    uint32 VoicesStolen = 0;  // Oldest voice stopped to stay under MaxVoices.
};

/**
 * Client-only gameplay audio: shots, knife swings and contacts, footsteps,
 * landings, own damage/death and objective cues, spatialized with code-built
 * attenuation.
 *
 * Observes replicated state through public read-only accessors, like
 * UVSShotEffectsSubsystem, and plays each event once (no replay on respawn,
 * rejoin or replication catch-up). It sends no RPCs and changes no gameplay
 * state. It is never created on a dedicated server; -VSNoGameplayAudio or a
 * process without audio (-nosound) disables it.
 *
 * Every cue has one art slot (soft reference, settable from DefaultGame.ini
 * under [/Script/ValStrike.VSGameplayAudioSubsystem]). An empty or unloadable
 * slot plays the PLACEHOLDER procedural sound from VSGameplayAudioState.h.
 */
UCLASS(Config = Game)
class VALSTRIKE_API UVSGameplayAudioSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 MaxVoices = 24;
    static constexpr int32 PlaceholderSampleRate = 48000;

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual bool IsTickable() const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    bool IsActive() const { return bActive; }
    const FVSGameplayAudioStats& GetStats() const { return Stats; }
    uint32 GetPlayedCount(VSGameplayAudio::Cue Kind) const { return Stats.Played[static_cast<int32>(Kind)]; }
    // True when the cue's art slot resolved to an asset; false means placeholder.
    bool UsesArtAsset(VSGameplayAudio::Cue Kind) const;
    int32 GetActiveVoiceCount() const { return Voices.Num(); }

    // Art contract: one slot per cue. Null keeps the code-generated placeholder.
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> RifleShotSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> PistolShotSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> FootstepSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> LandSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> HurtSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> DeathSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> PlantStartSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> DefuseStartSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> DeviceBeepSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> DefuseCompleteSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> KnifeSwingSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> KnifeWallSound;
    UPROPERTY(Config, EditDefaultsOnly, Category = "Audio|Art") TSoftObjectPtr<USoundBase> KnifeFleshSound;

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    enum class ERange : uint8 { Shot, Footstep, Objective };

    struct FPawnWatch
    {
        VSGameplayAudio::FootstepCadence Steps;
        VSGameplayAudio::LandingDetector Landing;
        int32 NextVariant = 0;
    };

    struct FVoice
    {
        TWeakObjectPtr<UAudioComponent> Component;
        double StopAt = 0.0;
    };

    bool IsAudioWorld() const;
    void ObservePawns(double Now, float DeltaTime);
    void ObserveShots(const AVSCharacter& Pawn, bool bFirstPerson, double Now);
    void ObserveKnife(const AVSCharacter& Pawn, bool bFirstPerson);
    void ObserveOwnLife();
    void ObserveObjective();
    void Play(VSGameplayAudio::Cue Kind, int32 Variant, USceneComponent* AttachTo, const FVector& Location,
        bool bSpatial, ERange Range, float Volume);
    USoundBase* MakePlaceholder(VSGameplayAudio::Cue Kind, int32 Variant, float& OutSeconds);
    void UpdateVoices(double Now);
    TSoftObjectPtr<USoundBase>& ArtSlot(VSGameplayAudio::Cue Kind);
    void LoadArt();
    void BuildAttenuation();
    const AActor* LocalViewTarget() const;

    // Resolved art, indexed by cue; null means the placeholder.
    UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> ArtSounds;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> ShotAttenuation;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FootstepAttenuation;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> ObjectiveAttenuation;

    // PCM16 mono placeholder buffers, generated once per (cue, variant).
    TArray<TArray<uint8>> PlaceholderPcm;

    TMap<TWeakObjectPtr<const UActorComponent>, VSShotEffects::ShotCounter> ShotWatches;
    TMap<TWeakObjectPtr<const UActorComponent>, VSGameplayAudio::KnifeWatch> KnifeWatches;
    TMap<TWeakObjectPtr<const AVSCharacter>, FPawnWatch> PawnWatches;
    TWeakObjectPtr<const UVSHealthComponent> OwnHealth;
    VSGameplayAudio::LifeWatch OwnLife;
    VSGameplayAudio::ObjectiveWatch Objective;
    VSGameplayAudio::BeepScheduler Beeps;
    TArray<FVoice> Voices;
    FVSGameplayAudioStats Stats;
    bool bActive = false;
};
