#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VSMatchObjective.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class VALSTRIKE_API AVSMatchObjective : public AActor
{
    GENERATED_BODY()

public:
    AVSMatchObjective();
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void SetObjectiveState(uint8 NewState);

    UPROPERTY(ReplicatedUsing=OnRep_ObjectiveState)
    uint8 ObjectiveState = 0;

    // Reviewed device art uses a bottom-center pivot and centimeter units at scale 1.
    // The fallback remains the 30 x 18 x 10 cm engine primitive.
    UPROPERTY(EditDefaultsOnly, Category="Match|Presentation")
    TObjectPtr<UStaticMesh> DeviceMesh;

private:
    UFUNCTION()
    void OnRep_ObjectiveState();

    void ConfigureMesh();
    void RefreshStatus();

    UPROPERTY(VisibleAnywhere, Category="Match|Presentation")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category="Match|Presentation")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, Category="Match|Presentation")
    TObjectPtr<UTextRenderComponent> StatusLabel;

    UPROPERTY()
    TObjectPtr<UStaticMesh> PrimitiveMesh;
};
