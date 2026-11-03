#include "VSHitZoneLayout.h"

#include <cmath>

namespace ValStrike::Damage
{
namespace
{
bool VSHitZoneLayoutPositive(double Value) { return std::isfinite(Value) && Value > 0.0; }

ZoneVolume VSHitZoneLayoutBox(HitZone Zone, BodySide Side, const char* Name, const char* Bone,
    double CenterY, double HalfY, double Bottom, double Top, double HalfDepth)
{
    ZoneVolume Volume;
    Volume.Zone = Zone;
    Volume.Side = Side;
    Volume.Shape = ZoneShape::Box;
    Volume.Center = {0.0, CenterY, (Bottom + Top) * 0.5};
    Volume.HalfExtent = {HalfDepth, HalfY, (Top - Bottom) * 0.5};
    Volume.Name = Name;
    Volume.Bone = Bone;
    return Volume;
}
}

bool IsValidConfig(const LayoutConfig& Config)
{
    return VSHitZoneLayoutPositive(Config.HeadRadius) && VSHitZoneLayoutPositive(Config.BodyHalfDepth) &&
        VSHitZoneLayoutPositive(Config.BodyHalfWidth) && VSHitZoneLayoutPositive(Config.ArmWidth) &&
        Config.ArmWidth < Config.BodyHalfWidth &&
        VSHitZoneLayoutPositive(Config.FootTop) && Config.FootTop < Config.LowerLegTop &&
        Config.LowerLegTop < Config.ThighTop && Config.ThighTop < Config.LowerTorsoTop &&
        Config.LowerTorsoTop < 1.0;
}

Layout BuildLayout(double HalfHeight, const LayoutConfig& Config)
{
    Layout Result;
    if (!IsValidConfig(Config) || !std::isfinite(HalfHeight) || HalfHeight <= Config.HeadRadius)
    {
        return Result;
    }
    Result.Valid = true;
    Result.CapsuleHalfHeight = HalfHeight;

    const double Floor = -HalfHeight;
    const double BodyHeight = 2.0 * HalfHeight - 2.0 * Config.HeadRadius;
    const auto Level = [&](double Fraction) { return Floor + Fraction * BodyHeight; };
    const double FootTop = Level(Config.FootTop);
    const double ShinTop = Level(Config.LowerLegTop);
    const double ThighTop = Level(Config.ThighTop);
    const double StomachTop = Level(Config.LowerTorsoTop);
    const double Shoulders = Level(1.0);
    const double D = Config.BodyHalfDepth;
    const double W = Config.BodyHalfWidth;
    const double CoreHalf = W - Config.ArmWidth;
    const double ArmHalf = Config.ArmWidth * 0.5;
    const double ArmCenter = W - ArmHalf;
    const double LegHalf = W * 0.5;

    std::size_t Next = 0;
    ZoneVolume& Head = Result.Volumes[Next++];
    Head.Zone = HitZone::Head;
    Head.Shape = ZoneShape::Sphere;
    Head.Center = {0.0, 0.0, HalfHeight - Config.HeadRadius};
    Head.Radius = Config.HeadRadius;
    Head.Name = "Head";
    Head.Bone = "head";

    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::UpperTorso, BodySide::Center, "UpperTorso", "spine_03", 0.0, CoreHalf, StomachTop, Shoulders, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::UpperArm, BodySide::Left, "UpperArm_L", "upperarm_l", -ArmCenter, ArmHalf, StomachTop, Shoulders, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::UpperArm, BodySide::Right, "UpperArm_R", "upperarm_r", ArmCenter, ArmHalf, StomachTop, Shoulders, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::LowerTorso, BodySide::Center, "LowerTorso", "spine_01", 0.0, CoreHalf, ThighTop, StomachTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::Forearm, BodySide::Left, "Forearm_L", "lowerarm_l", -ArmCenter, ArmHalf, ThighTop, StomachTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::Forearm, BodySide::Right, "Forearm_R", "lowerarm_r", ArmCenter, ArmHalf, ThighTop, StomachTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::Thigh, BodySide::Left, "Thigh_L", "thigh_l", -LegHalf, LegHalf, ShinTop, ThighTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::Thigh, BodySide::Right, "Thigh_R", "thigh_r", LegHalf, LegHalf, ShinTop, ThighTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::LowerLeg, BodySide::Left, "LowerLeg_L", "calf_l", -LegHalf, LegHalf, FootTop, ShinTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::LowerLeg, BodySide::Right, "LowerLeg_R", "calf_r", LegHalf, LegHalf, FootTop, ShinTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::Foot, BodySide::Left, "Foot_L", "foot_l", -LegHalf, LegHalf, Floor, FootTop, D);
    Result.Volumes[Next++] = VSHitZoneLayoutBox(HitZone::Foot, BodySide::Right, "Foot_R", "foot_r", LegHalf, LegHalf, Floor, FootTop, D);
    return Result;
}
}
