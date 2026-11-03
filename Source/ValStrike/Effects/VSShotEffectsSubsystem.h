#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VSShotEffectsState.h"
#include "VSShotEffectsSubsystem.generated.h"

class AActor;
class AVSCharacter;
class UActorComponent;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EVSShotImpactKind : uint8
{
    None,       // The cosmetic trace reached full range without a hit.
    World,      // Lingering mark on level geometry or cover.
    Character,  // Short hit flash attached to the struck character.
    Self,       // The struck character is this client's own view; no mark in the camera.
};

/** Read-only description of the most recent cosmetic shot effect on this client. */
struct FVSShotEffectRecord
{
    uint32 Sequence = 0;
    TWeakObjectPtr<const AVSCharacter> Shooter;
    uint32 RepresentedShots = 0;
    FVector Eye = FVector::ZeroVector;
    FVector Direction = FVector::ForwardVector;
    FVector TracerStart = FVector::ZeroVector;
    FVector TracerEnd = FVector::ZeroVector;
    FVector ImpactPoint = FVector::ZeroVector;
    FVector ImpactNormal = FVector::UpVector;
    bool bTracer = false;
    bool bMuzzleOrigin = false;
    TWeakObjectPtr<const USceneComponent> MuzzleComponent;
    EVSShotImpactKind Impact = EVSShotImpactKind::None;
    TWeakObjectPtr<const AActor> HitActor;
    int32 TracerSlot = INDEX_NONE;
    int32 MarkSlot = INDEX_NONE;
    double Time = 0.0;
};

USTRUCT()
struct FVSShotTracerSlot
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Component;
    double SpawnTime = 0.0;
    bool bActive = false;
};

USTRUCT()
struct FVSShotMarkSlot
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Component;
    double SpawnTime = 0.0;
    float Hold = 0.0f;
    float Fade = 0.0f;
    float LastOpacity = -1.0f;
    float LastHeat = -1.0f;
    bool bActive = false;
    bool bAttached = false;
    bool bCharacter = false;
};

struct FVSShotEffectsStats
{
    uint32 Effects = 0;          // Observations that produced effects.
    uint32 RepresentedShots = 0; // Accepted shots those effects stand for.
    uint32 CoalescedShots = 0;   // Shots folded into a batch's single effect.
    uint32 Tracers = 0;
    uint32 WorldMarks = 0;
    uint32 CharacterMarks = 0;
    uint32 SelfHitsSuppressed = 0;
    uint32 Baselines = 0;        // Counters adopted without effects (first sight, reset, stale jump).
    uint32 TracersRecycled = 0;
    uint32 MarksRecycled = 0;
    int32 MaxActiveMarks = 0;
};

/**
 * Client-only shot feedback: tracer plus impact mark for every accepted shot.
 *
 * Observes each AVSCharacter's replicated accepted-shot counter through public
 * read-only APIs and draws effects locally. It sends no RPCs and changes no
 * gameplay state. It is never created on a dedicated server.
 *
 * The impact point comes from a cosmetic trace that mirrors the server's rifle
 * trace (pawn view location, base aim, rifle range, Visibility channel,
 * shooter ignored). For moving targets, or a shooter turning between the shot
 * and its replication, the cosmetic point can differ from the server's
 * authoritative hit.
 */
UCLASS()
class VALSTRIKE_API UVSShotEffectsSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    UVSShotEffectsSubsystem();

    static constexpr int32 MaxTracers = 16;
    static constexpr int32 MaxMarks = 32;
    static const FName MuzzleSocket;

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual bool IsTickable() const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    const FVSShotEffectsStats& GetStats() const { return Stats; }
    const FVSShotEffectRecord& GetLastEffect() const { return LastEffect; }
    const AActor* GetEffectsActor() const { return EffectsActor; }
    int32 GetTracerPoolSize() const { return Tracers.Num(); }
    int32 GetMarkPoolSize() const { return Marks.Num(); }
    int32 GetActiveTracerCount() const;
    int32 GetActiveMarkCount() const;
    const UStaticMeshComponent* GetTracerComponent(int32 Slot) const;
    const UStaticMeshComponent* GetMarkComponent(int32 Slot) const;
    // Current mark opacity in [0, 1]; negative for an invalid or inactive slot.
    float GetMarkOpacity(int32 Slot) const;
    const UMaterialInterface* GetTracerMaterial() const { return TracerMaterial; }
    const UMaterialInterface* GetMarkMaterial() const { return MarkMaterial; }

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Test fixture only (-VSShotEffectsTest=...): lengthen world-mark timing so
    // the pool cap can be exercised. Applies to marks spawned afterwards.
    bool DevelopmentSetWorldMarkTiming(float HoldSeconds, float FadeSeconds);
#endif

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    struct FShotWatch
    {
        VSShotEffects::ShotCounter Counter;
    };

    bool IsEffectsWorld() const;
    void ObserveCharacters(double Now);
    void SpawnShotEffect(const AVSCharacter& Shooter, const UActorComponent& Weapon, uint32 Represented, double Now);
    FVector TracerOrigin(const AVSCharacter& Shooter, bool bFirstPersonView, const FVector& Eye, const FRotator& Aim,
        bool& bOutMuzzle, const USceneComponent*& OutMuzzle) const;
    int32 ShowTracer(const FVector& Start, const FVector& End, double Now);
    int32 ShowMark(const FHitResult& Hit, bool bCharacter, const FVector& ViewLocation, double Now);
    void UpdateEffects(double Now);
    void HideMark(FVSShotMarkSlot& Mark);
    bool EnsureEffectsActor();
    UStaticMeshComponent* MakeEffectComponent(const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material);

    UPROPERTY(Transient) TObjectPtr<UStaticMesh> TracerMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> MarkMesh;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TracerMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> MarkMaterial;
    UPROPERTY(Transient) TObjectPtr<AActor> EffectsActor;

    TMap<TWeakObjectPtr<const UActorComponent>, FShotWatch> Watches;
    UPROPERTY(Transient) TArray<FVSShotTracerSlot> Tracers;
    UPROPERTY(Transient) TArray<FVSShotMarkSlot> Marks;
    FVSShotEffectsStats Stats;
    FVSShotEffectRecord LastEffect;
    float WorldMarkHold = 2.5f;
    float WorldMarkFade = 1.5f;
    bool bDisabled = false;
    bool bWarnedMissingAssets = false;
};
