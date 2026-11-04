#include "VSRiflePresentationComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UVSRiflePresentationComponent::UVSRiflePresentationComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Game/Art/Weapons/Rifle02/Meshes/SM_VS_Rifle02_Body.SM_VS_Rifle02_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Magazine(TEXT("/Game/Art/Weapons/Rifle02/Meshes/SM_VS_Rifle02_Magazine.SM_VS_Rifle02_Magazine"));
    Slot = EVSWeaponSlot::Rifle;
    VisualPrefix = TEXT("R02");
    BodyAsset = Body.Object;
    MagazineAsset = Magazine.Object;
    // R02 recipe sockets (docs/assets/rifle02/README.md): Grip origin, Muzzle (52,0,7.05),
    // SupportHand (29,0,2.4), Magazine (15,0,-2.5); the magazine attaches at identity.
    // SupportHand is validated and reserved for a future arms rig.
    RequiredSockets = {TEXT("Grip"), TEXT("Muzzle"), TEXT("SupportHand"), TEXT("Magazine")};
    // ART-17's first-person composition (camera (-30,12,19) from Grip, UE Y = -Blender Y):
    // R02 is 11 cm shorter than R01, so it sits 5 cm nearer and more central than
    // R01's (35,17,-20); the existing small cant toward the crosshair is kept.
    FirstPersonGrip = FTransform(FRotator(-3, -2, 0), FVector(30, 12, -19));
    WorldGrip = FTransform(FRotator::ZeroRotator, FVector(35, 20, 112));
    FlashScale = FVector(0.13f, 0.05f, 0.035f);
    // GPT-09: about 40 ms of flash and a small rifle kick that settles in about 80 ms.
    FlashDuration = 0.04f;
    KickBack = 1.4f;
    KickPitch = 1.1f;
    KickRecovery = 12.0f;
    ReloadMagazineOffset = FVector(-3.0f, 0.0f, -13.0f);
}
