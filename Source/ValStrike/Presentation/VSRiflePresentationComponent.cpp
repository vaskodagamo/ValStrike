#include "VSRiflePresentationComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UVSRiflePresentationComponent::UVSRiflePresentationComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Game/Art/Weapons/Rifle01/Meshes/SM_VS_Rifle01_Body.SM_VS_Rifle01_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Magazine(TEXT("/Game/Art/Weapons/Rifle01/Meshes/SM_VS_Rifle01_Magazine.SM_VS_Rifle01_Magazine"));
    Slot = EVSWeaponSlot::Rifle;
    VisualPrefix = TEXT("R01");
    BodyAsset = Body.Object;
    MagazineAsset = Magazine.Object;
    // SupportHand is validated and reserved for a future arms rig.
    RequiredSockets = {TEXT("Grip"), TEXT("Muzzle"), TEXT("SupportHand"), TEXT("Magazine")};
    FirstPersonGrip = FTransform(FRotator(-3, -2, 0), FVector(35, 17, -20));
    WorldGrip = FTransform(FRotator::ZeroRotator, FVector(35, 20, 112));
    FlashScale = FVector(0.13f, 0.045f, 0.045f);
    ReloadMagazineOffset = FVector(-3.0f, 0.0f, -13.0f);
}
