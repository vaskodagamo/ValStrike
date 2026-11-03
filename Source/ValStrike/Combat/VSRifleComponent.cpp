#include "VSRifleComponent.h"

UVSRifleComponent::UVSRifleComponent()
{
    // Unchanged MVP rifle tuning: automatic, 30/90, 0.12 s, 2 s reload, 30/100 damage.
    DisplayName = TEXT("RIFLE");
    bAutomatic = true;
    MagazineCapacity = 30;
    InitialReserve = 90;
    FireInterval = 0.12f;
    ReloadDuration = 2.0f;
    Range = 10000.0f;
    BodyDamage = 30.0f;
    HeadDamage = 100.0f;
}
