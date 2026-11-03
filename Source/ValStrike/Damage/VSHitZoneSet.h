#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VSDamageModel.h"
#include "VSHitZoneLayout.h"
#include "VSHitZoneSet.generated.h"

class ACharacter;
class UCapsuleComponent;
class UPrimitiveComponent;
class UShapeComponent;
class UVSHealthComponent;

UENUM()
enum class EVSHitZoneTraceMode : uint8
{
    // Overlap on the trace channel. Single-hit traces (the current rifle) ignore
    // the zones; multi-hit traces report them. Used while the legacy body/head
    // regions still own hit registration, so gameplay is unchanged.
    Shadow,
    // Block on the trace channel: the zones are the hit registration.
    Authoritative,
};

/**
 * Hidden, query-only hit zones for one character: head, upper/lower torso, upper
 * arms, forearms/hands, thighs, lower legs and feet (13 shapes). Shapes follow the
 * capsule (crouch included) and are disabled while the owner's health is zero.
 * Nothing here is replicated: every machine builds the same geometry locally and
 * only the server's traces decide damage.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class VALSTRIKE_API UVSHitZoneSet : public USceneComponent
{
    GENERATED_BODY()

public:
    UVSHitZoneSet();

    static UVSHitZoneSet* FindOn(const AActor* Actor);
    // Runtime creation on any character: attaches to its capsule and registers.
    // Returns the existing set when the character already has one.
    static UVSHitZoneSet* AddTo(ACharacter* Character, EVSHitZoneTraceMode Mode = EVSHitZoneTraceMode::Shadow);

    // HitZone::None for components that are not one of this set's shapes.
    ValStrike::Damage::HitZone ResolveZone(const UPrimitiveComponent* Component) const;
    int32 FindShapeIndex(const UPrimitiveComponent* Component) const;
    int32 GetShapeCount() const { return Shapes.Num(); }
    UShapeComponent* GetShape(int32 Index) const;
    // Geometry currently applied to the shapes (capsule-relative).
    const ValStrike::Damage::Layout& GetLayout() const { return CurrentLayout; }

    void SetTraceMode(EVSHitZoneTraceMode Mode);
    EVSHitZoneTraceMode GetTraceMode() const { return TraceMode; }
    ECollisionChannel GetTraceChannel() const { return TraceChannel.GetValue(); }
    bool AreZonesEnabled() const { return bZonesEnabled; }

    // Re-reads the capsule half-height. Owners call this right after a size
    // change (crouch); TickComponent also catches any change as a safety net.
    void RefreshLayout();

    // Development builds: vs.Damage.DrawHitZones 1 draws other players' zones
    // as depth-tested lines just outside each shape. True while drawn this frame.
    static bool IsDebugDrawRequested();
    bool IsDebugDrawn() const { return bDebugDrawn; }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
    void CreateShapes();
    void ApplyCollision();
    void DrawDebugZones();
    void HandleLifeStateChanged();
    float ReadHalfHeight() const;
    UCapsuleComponent* FindCapsule() const;

    UPROPERTY(EditAnywhere, Category = "Hit Zones")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, Category = "Hit Zones")
    EVSHitZoneTraceMode TraceMode = EVSHitZoneTraceMode::Shadow;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UShapeComponent>> Shapes;

    ValStrike::Damage::Layout CurrentLayout;
    TWeakObjectPtr<UVSHealthComponent> Health;
    FDelegateHandle LifeHandle;
    float AppliedHalfHeight = -1.0f;
    bool bZonesEnabled = true;
    bool bDebugDrawn = false;
};
