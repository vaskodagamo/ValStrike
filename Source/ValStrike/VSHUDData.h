#pragma once

#include "CoreMinimal.h"

/** Read-only local HUD snapshot. Missing systems must not look like zero health/ammo. */
struct FVSPlayerHUDData
{
    bool bHasHealth = false;
    bool bHasWeapon = false;
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    int32 MagazineAmmo = 0;
    int32 MagazineCapacity = 0;
    int32 ReserveAmmo = 0;
    bool bReloading = false;
    bool bAlive = false;
    // Additive: equipped weapon identity. Ammo/reload fields above describe it.
    FString WeaponName;
    bool bEquipping = false;
};
