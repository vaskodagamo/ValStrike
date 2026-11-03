#include "VSMatchObjective.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "../Rules/VSMatchRules.h"
#include "UObject/ConstructorHelpers.h"

AVSMatchObjective::AVSMatchObjective()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ObjectiveRoot"));
    SetRootComponent(SceneRoot);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceBody"));
    Body->SetupAttachment(SceneRoot);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetGenerateOverlapEvents(false);
    Body->SetCanEverAffectNavigation(false);
    Body->SetMobility(EComponentMobility::Movable);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    PrimitiveMesh = Cube.Object;
    Body->SetStaticMesh(PrimitiveMesh);
    Body->SetRelativeScale3D(FVector(0.30, 0.18, 0.10));
    Body->SetRelativeLocation(FVector(0.0, 0.0, 5.0));

    StatusLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("DeviceStatus"));
    StatusLabel->SetupAttachment(SceneRoot);
    StatusLabel->SetRelativeLocation(FVector(0.0, 0.0, 18.0));
    StatusLabel->SetWorldSize(7.0f);
    StatusLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    StatusLabel->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    StatusLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StatusLabel->SetGenerateOverlapEvents(false);
    StatusLabel->SetCanEverAffectNavigation(false);
    StatusLabel->SetText(FText::GetEmpty());
}

void AVSMatchObjective::BeginPlay()
{
    Super::BeginPlay();
    ConfigureMesh();
    RefreshStatus();
}

void AVSMatchObjective::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ConfigureMesh();
    RefreshStatus();
}

void AVSMatchObjective::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSMatchObjective, ObjectiveState);
}

void AVSMatchObjective::ConfigureMesh()
{
    Body->SetStaticMesh(DeviceMesh ? DeviceMesh : PrimitiveMesh);
    Body->SetRelativeScale3D(DeviceMesh ? FVector::OneVector : FVector(0.30, 0.18, 0.10));
    Body->SetRelativeLocation(DeviceMesh ? FVector::ZeroVector : FVector(0.0, 0.0, 5.0));
}

void AVSMatchObjective::SetObjectiveState(uint8 NewState)
{
    if (!HasAuthority())
    {
        return;
    }
    const uint8 ValidState = NewState <= static_cast<uint8>(ValStrike::Rules::DeviceState::Detonated) ? NewState : 0;
    if (ObjectiveState == ValidState)
    {
        return;
    }
    ObjectiveState = ValidState;
    RefreshStatus();
    ForceNetUpdate();
}

void AVSMatchObjective::OnRep_ObjectiveState()
{
    RefreshStatus();
}

void AVSMatchObjective::RefreshStatus()
{
    using DeviceState = ValStrike::Rules::DeviceState;
    FText Label;
    FColor Color(118, 204, 224);
    switch (static_cast<DeviceState>(ObjectiveState))
    {
    case DeviceState::Carried:
    case DeviceState::Dropped:
        Label = NSLOCTEXT("ValStrike", "ObjectiveDevice", "DEVICE");
        break;
    case DeviceState::Planted:
        Label = NSLOCTEXT("ValStrike", "ObjectiveArmed", "ARMED");
        Color = FColor(255, 178, 54);
        break;
    case DeviceState::Defused:
        Label = NSLOCTEXT("ValStrike", "ObjectiveDefused", "DEFUSED");
        Color = FColor(94, 224, 158);
        break;
    case DeviceState::Detonated:
        Label = NSLOCTEXT("ValStrike", "ObjectiveDetonated", "DETONATED");
        Color = FColor(245, 94, 99);
        break;
    default:
        Label = FText::GetEmpty();
        break;
    }
    StatusLabel->SetText(Label);
    StatusLabel->SetTextRenderColor(Color);
}
