#pragma once

namespace ValStrikeTests
{
// Idempotent registration from character BeginPlay. Inactive unless the process
// was launched with -VSWeaponSmoke=server|driver|observer (development builds).
void StartWeaponSmoke();
}
