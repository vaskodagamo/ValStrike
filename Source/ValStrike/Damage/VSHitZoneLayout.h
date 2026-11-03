#pragma once

#include "VSDamageModel.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Engine-independent hit-zone geometry, relative to the character capsule center
// (X forward, Y right, Z up, centimeters). The union of the volumes is exactly the
// current prototype silhouette: a 50 x 40 body box from the floor to 32 cm below
// the capsule top plus a 16 cm head sphere touching the top. Crouching scales the
// body bands with the current capsule half-height, like the prototype visuals.
// Each volume names the bone it should follow once a skeletal character exists.
namespace ValStrike::Damage
{
enum class ZoneShape : std::uint8_t { Box, Sphere };
enum class BodySide : std::uint8_t { Center, Left, Right };

struct ZoneVolume
{
    HitZone Zone = HitZone::None;
    BodySide Side = BodySide::Center;
    ZoneShape Shape = ZoneShape::Box;
    std::array<double, 3> Center{};     // Capsule-relative.
    std::array<double, 3> HalfExtent{}; // Box only.
    double Radius = 0.0;                // Sphere only.
    const char* Name = "";              // Stable component suffix, e.g. "UpperArm_L".
    const char* Bone = "";              // Proposed skeleton bone for later binding.
};

struct LayoutConfig
{
    double HeadRadius = 16.0;
    double BodyHalfDepth = 25.0;  // X
    double BodyHalfWidth = 20.0;  // Y
    double ArmWidth = 7.0;        // Outer Y strip per side for upper arms and forearms.
    // Band tops as fractions of the body height (floor to shoulders). Standing
    // body height is 144 cm: feet 10, shins 46, thighs 84, stomach 112, chest 144.
    double FootTop = 10.0 / 144.0;
    double LowerLegTop = 46.0 / 144.0;
    double ThighTop = 84.0 / 144.0;
    double LowerTorsoTop = 112.0 / 144.0;
};

constexpr std::size_t VolumeCount = 13;

struct Layout
{
    bool Valid = false;
    double CapsuleHalfHeight = 0.0;
    std::array<ZoneVolume, VolumeCount> Volumes{};
};

bool IsValidConfig(const LayoutConfig& Config);
// Invalid half-heights (non-finite or not above the head radius) or configs yield Valid=false.
Layout BuildLayout(double CapsuleHalfHeight, const LayoutConfig& Config = LayoutConfig());
}
