#include "VSPistolPresentationComponent.h"

#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

UVSPistolPresentationComponent::UVSPistolPresentationComponent()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Game/Art/Weapons/Pistol02/Meshes/SM_VS_Pistol02_Body.SM_VS_Pistol02_Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Slide(TEXT("/Game/Art/Weapons/Pistol02/Meshes/SM_VS_Pistol02_Slide.SM_VS_Pistol02_Slide"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Magazine(TEXT("/Game/Art/Weapons/Pistol02/Meshes/SM_VS_Pistol02_Magazine.SM_VS_Pistol02_Magazine"));
    Slot = EVSWeaponSlot::Pistol;
    VisualPrefix = TEXT("P02");
    BodyAsset = Body.Object;
    SlideAsset = Slide.Object;
    MagazineAsset = Magazine.Object;
    // P02 contract (docs/assets/pistol02/IMPORT.md): body, slide and magazine share the
    // Grip datum and assemble at identity, so the native Slide/Magazine attachment
    // sockets sit at the origin (markers not applied twice); Muzzle (15.5,0,5), +X forward.
    RequiredSockets = {TEXT("Grip"), TEXT("Slide"), TEXT("Magazine"), TEXT("Muzzle")};
    // P01's accepted first-person composition, kept: P02 (21 cm) shares the Grip-origin,
    // +X-forward convention and is close to P01's 24 cm length.
    FirstPersonGrip = FTransform(FRotator(-2, -3, 0), FVector(34, 12, -14));
    // Held forward of the placeholder torso so observers can read the silhouette.
    WorldGrip = FTransform(FRotator::ZeroRotator, FVector(45, 16, 120));
    FlashScale = FVector(0.08f, 0.04f, 0.028f);
    // GPT-09: about 40 ms of flash; body kick keeps the base defaults beside the slide kick.
    FlashDuration = 0.04f;
    ReloadMagazineOffset = FVector(0.0f, 0.0f, -10.0f);
    SlideKickDistance = 2.5f;
}
