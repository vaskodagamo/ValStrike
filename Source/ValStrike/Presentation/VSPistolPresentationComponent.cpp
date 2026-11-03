#include "VSPistolPresentationComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UVSPistolPresentationComponent::UVSPistolPresentationComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Game/Art/Weapons/Pistol01/Meshes/SM_VS_Pistol01_Body.SM_VS_Pistol01_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Slide(TEXT("/Game/Art/Weapons/Pistol01/Meshes/SM_VS_Pistol01_Slide.SM_VS_Pistol01_Slide"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Magazine(TEXT("/Game/Art/Weapons/Pistol01/Meshes/SM_VS_Pistol01_Magazine.SM_VS_Pistol01_Magazine"));
    Slot = EVSWeaponSlot::Pistol;
    VisualPrefix = TEXT("P01");
    BodyAsset = Body.Object;
    SlideAsset = Slide.Object;
    MagazineAsset = Magazine.Object;
    // P01 attachment contract (docs/assets/pistol01/MANIFEST.md): child meshes
    // attach at zero offset to Slide/Magazine; Muzzle is body-space, +X forward.
    RequiredSockets = {TEXT("Grip"), TEXT("Slide"), TEXT("Magazine"), TEXT("Muzzle")};
    // Starts from the art direction's first-person composition (34,12,-14).
    FirstPersonGrip = FTransform(FRotator(-2, -3, 0), FVector(34, 12, -14));
    // Held forward of the placeholder torso so observers can read the silhouette.
    WorldGrip = FTransform(FRotator::ZeroRotator, FVector(45, 16, 120));
    FlashScale = FVector(0.08f, 0.035f, 0.035f);
    ReloadMagazineOffset = FVector(0.0f, 0.0f, -10.0f);
    SlideKickDistance = 2.5f;
}
