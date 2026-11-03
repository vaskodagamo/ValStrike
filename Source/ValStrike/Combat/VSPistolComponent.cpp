#include "VSPistolComponent.h"

UVSPistolComponent::UVSPistolComponent()
{
    // Proposed starting values (NEXT-03 scope expansion), tunable as class defaults.
    // One press = one request; the server still enforces the 0.2 s cadence.
    DisplayName = TEXT("PISTOL");
    bAutomatic = false;
    MagazineCapacity = 12;
    InitialReserve = 36;
    FireInterval = 0.2f;
    ReloadDuration = 1.6f;
    Range = 8000.0f;
    BodyDamage = 20.0f;
    HeadDamage = 60.0f;
}
