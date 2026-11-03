#include "VSRifleComponent.h"

UVSRifleComponent::UVSRifleComponent()
{
    // Unchanged MVP rifle tuning: automatic, 30/90, 0.12 s, 2 s reload. Damage: the
    // Rifle row of the zone table (30 upper torso, lethal head).
    DisplayName = TEXT("RIFLE");
    bAutomatic = true;
    MagazineCapacity = 30;
    InitialReserve = 90;
    FireInterval = 0.12f;
    ReloadDuration = 2.0f;
    Range = 10000.0f;
    DamageClass = ValStrike::Damage::WeaponClass::Rifle;
}
