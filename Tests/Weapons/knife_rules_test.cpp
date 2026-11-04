// Standalone checks for Combat/VSKnifeRules.h and the knife row of the damage model (no Unreal process).
// clang++ -std=c++17 -Wall -Wextra -Werror -pedantic -Wshadow -ISource/ValStrike Tests/Weapons/knife_rules_test.cpp \
//   Source/ValStrike/Damage/VSDamageModel.cpp -o Saved/Tests/Weapons/knife_rules_test && Saved/Tests/Weapons/knife_rules_test
#include "Combat/VSKnifeRules.h"
#include "Damage/VSDamageModel.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
using namespace VSKnifeRules;
int Failures = 0;
int Checks = 0;

void Expect(bool Condition, const char* Name)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::printf("FAIL %s\n", Name);
    }
}

Request Valid()
{
    Request In;
    In.sequence = 5;
    In.lastSequence = 4;
    In.now = 10.0;
    In.nextAttackTime = 9.0;
    return In;
}

Verdict Check(const Request& In)
{
    Attack Kind = Attack::Slash;
    return Evaluate(In, Kind);
}

Candidate At(std::uint32_t Id, double Distance, double Degrees, Probe Result = Probe::Clear)
{
    return Candidate{Id, Distance, std::cos(Degrees * 3.14159265358979323846 / 180.0), Result};
}

template <std::size_t N>
Selection Pick(const Tuning& Value, const std::array<Candidate, N>& In, bool ForwardWall = false)
{
    return Select(Value, In, N, ForwardWall);
}
}

int main()
{
    {
        // Shared contract values (animation impact markers use the same times).
        Expect(Slash.damage == 40.0 && Slash.reachCm == 125.0 && Slash.halfAngleDeg == 35.0
            && Slash.impactTime == 0.15 && Slash.nextAttack == 0.55, "slash tuning");
        Expect(Stab.damage == 65.0 && Stab.reachCm == 150.0 && Stab.halfAngleDeg == 10.0
            && Stab.impactTime == 0.30 && Stab.nextAttack == 0.90, "stab tuning");
        Expect(EquipSeconds == 0.40, "knife equip time");
        Expect(Slash.nextAttack > Slash.impactTime && Stab.nextAttack > Stab.impactTime, "cooldown outlasts every impact");
    }
    {
        Attack Kind = Attack::Stab;
        Expect(DecodeAttack(0, Kind) && Kind == Attack::Slash, "byte 0 is slash");
        Expect(DecodeAttack(1, Kind) && Kind == Attack::Stab, "byte 1 is stab");
        bool bInvalid = true;
        for (int Raw = 2; Raw <= 255; ++Raw) { bInvalid = bInvalid && !DecodeAttack(static_cast<std::uint8_t>(Raw), Kind); }
        Expect(bInvalid, "bytes above 1 are invalid");
        Expect(EncodeAttack(Attack::Slash) == 0 && EncodeAttack(Attack::Stab) == 1, "attack encoding");
    }
    {
        // Validation order: each earlier failure masks every later one.
        Request In = Valid();
        Expect(Check(In) == Verdict::Accept, "valid request accepted");
        In.validOwner = false;
        In.sequence = 0;
        In.rawKind = 9;
        In.alive = false;
        In.combatAllowed = false;
        In.knifeEquipped = false;
        In.equipReady = false;
        In.nextAttackTime = 11.0;
        In.pending = MaxPending;
        Expect(Check(In) == Verdict::Owner, "owner first");
        In.validOwner = true;
        Expect(Check(In) == Verdict::Sequence, "sequence before kind");
        In.sequence = 5;
        Expect(Check(In) == Verdict::Invalid, "kind before life");
        In.rawKind = 1;
        Expect(Check(In) == Verdict::Dead, "dead before phase");
        In.alive = true;
        Expect(Check(In) == Verdict::Phase, "phase before slot");
        In.combatAllowed = true;
        Expect(Check(In) == Verdict::NotEquipped, "slot before equip");
        In.knifeEquipped = true;
        Expect(Check(In) == Verdict::Equipping, "equip before cooldown");
        In.equipReady = true;
        Expect(Check(In) == Verdict::Cooldown, "cooldown before pending bound");
        In.nextAttackTime = 9.0;
        Expect(Check(In) == Verdict::Busy, "pending bound");
        In.pending = MaxPending - 1;
        Attack Kind = Attack::Slash;
        Expect(Evaluate(In, Kind) == Verdict::Accept && Kind == Attack::Stab, "accepted stab");
        Expect(!ConsumesSequence(Verdict::Owner) && !ConsumesSequence(Verdict::Sequence) && ConsumesSequence(Verdict::Cooldown)
            && ConsumesSequence(Verdict::Accept), "rejected requests after the sequence check consume it");
    }
    {
        // Duplicate, replay and out-of-window sequences; wrap-around is accepted.
        Expect(!IsSequenceAccepted(0, 0) && !IsSequenceAccepted(4, 4) && !IsSequenceAccepted(3, 4), "zero, duplicate and replay rejected");
        Expect(IsSequenceAccepted(5, 4) && IsSequenceAccepted(4 + SequenceWindow, 4) && !IsSequenceAccepted(5 + SequenceWindow, 4), "sequence window");
        Expect(IsSequenceAccepted(2, 0xFFFFFFFFu), "sequence wrap-around");
    }
    {
        // Cooldown boundary with arrival tolerance and no sustained credit.
        Request In = Valid();
        In.nextAttackTime = 10.5;
        Expect(Check(In) == Verdict::Cooldown, "0.5 s early is cooldown");
        In.nextAttackTime = 10.015625;
        Expect(Check(In) == Verdict::Accept, "arrival inside the tolerance is accepted");
        Expect(NextAttackTime(10.0, 10.015625, Attack::Slash) == 10.015625 + 0.55, "early arrival grants no credit");
        Expect(NextAttackTime(12.0, 10.0, Attack::Stab) == 12.9, "next attack from the accepted start");
    }
    {
        // Pending impacts: scheduling, timer slack and the bound.
        Actions A;
        const int First = A.Start(1, Attack::Slash, 2.0);
        const int Second = A.Start(2, Attack::Stab, 2.0);
        Expect(First == 0 && Second == 1 && A.Start(3, Attack::Slash, 2.0) == -1 && A.Count() == MaxPending, "pending table is bounded");
        Pending Out;
        Expect(!A.Take(First, 2.1, Out) && A.Count() == 2, "impact not due before its time");
        Expect(A.Take(First, 2.15 - 0.001, Out) && Out.actionId == 1 && Out.kind == Attack::Slash, "slash resolves at 0.15 s within timer slack");
        Expect(!A.Take(First, 2.2, Out), "an impact resolves once");
        Expect(A.Take(Second, 2.3, Out) && Out.kind == Attack::Stab && A.Count() == 0, "stab resolves at 0.30 s");
        Expect(!A.Take(-1, 5.0, Out) && !A.Take(static_cast<int>(MaxPending), 5.0, Out), "out-of-range slots never resolve");
    }
    {
        // A lifecycle transition (switch, death, reset, phase) cancels pending impacts:
        // a delayed action never lands afterwards, even in a reused slot.
        Actions A;
        const int Slot = A.Start(7, Attack::Stab, 1.0);
        const std::uint32_t Epoch = A.CurrentEpoch();
        Expect(A.CancelAll() == 1 && A.CurrentEpoch() == Epoch + 1 && A.Count() == 0, "cancel drops the pending stab");
        Pending Out;
        Expect(!A.Take(Slot, 1.3, Out), "cancelled stab never resolves at its impact time");
        const int Next = A.Start(8, Attack::Slash, 1.4);
        Expect(Next == Slot && A.Take(Next, 1.55, Out) && Out.actionId == 8 && Out.epoch == Epoch + 1, "new attack after cancel resolves");
        Expect(A.CancelAll() == 0, "cancelling nothing reports zero");
    }
    {
        // Candidate filter: reach and cone boundaries, obstruction, one hit per target.
        Expect(InReachAndCone(Slash, 125.0, 1.0) && !InReachAndCone(Slash, 125.5, 1.0), "slash reach is 125 cm");
        Expect(InReachAndCone(Stab, 150.0, 1.0) && !InReachAndCone(Stab, 150.5, 1.0), "stab reach is 150 cm");
        Expect(InReachAndCone(Slash, 50.0, std::cos(34.9 * 3.14159265358979323846 / 180.0))
            && !InReachAndCone(Slash, 50.0, std::cos(35.1 * 3.14159265358979323846 / 180.0)), "slash cone is 35 degrees");
        Expect(InReachAndCone(Stab, 50.0, std::cos(9.9 * 3.14159265358979323846 / 180.0))
            && !InReachAndCone(Stab, 50.0, std::cos(10.1 * 3.14159265358979323846 / 180.0)), "stab cone is 10 degrees");
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        Expect(!InReachAndCone(Slash, NaN, 1.0) && !InReachAndCone(Slash, 10.0, NaN) && !InReachAndCone(Slash, -1.0, 1.0), "non-finite inputs rejected");

        const std::array<Candidate, 4> Crowd{At(1, 100.0, 20.0), At(1, 90.0, 5.0), At(2, 60.0, 30.0), At(3, 40.0, 0.0, Probe::World)};
        const Selection Slashed = Pick(Slash, Crowd);
        Expect(Slashed.count == 2 && Slashed.ids[0] == 2 && Slashed.ids[1] == 1 && Slashed.result == ImpactResult::Player,
            "slash hits two distinct targets nearest first, once each, never through a wall");
        const Selection Stabbed = Pick(Stab, Crowd);
        Expect(Stabbed.count == 1 && Stabbed.ids[0] == 1, "stab hits the one target inside its narrow cone");

        const std::array<Candidate, 2> Blocked{At(4, 60.0, 0.0, Probe::World), At(5, 50.0, 0.0, Probe::Other)};
        const Selection Wall = Pick(Slash, Blocked);
        Expect(Wall.count == 0 && Wall.result == ImpactResult::Wall, "obstructed target is a wall contact, not a hit");
        const std::array<Candidate, 1> Behind{At(5, 50.0, 0.0, Probe::Other)};
        Expect(Pick(Slash, Behind).result == ImpactResult::Miss, "a pawn in front blocks without a wall cue");
        const std::array<Candidate, 3> Nothing{At(6, 130.0, 0.0), At(7, 50.0, 60.0), At(0, 10.0, 0.0)};
        Expect(Pick(Slash, Nothing).count == 0 && Pick(Slash, Nothing).result == ImpactResult::Miss, "out of reach, out of cone and id 0 miss");
        Expect(Pick(Slash, Nothing, true).result == ImpactResult::Wall, "a wall straight ahead gives wall contact");
        const std::array<Candidate, 1> NoZone{At(8, 50.0, 0.0, Probe::Missed)};
        Expect(Pick(Slash, NoZone).count == 0, "a probe that touches no hit zone never damages");
        Expect(Select(Slash, Crowd, 1, false).count == 1 && Select(Slash, Crowd, 99, false).count == 2, "count is bounded by the array");
    }
    {
        // Damage model: flat knife damage per attack, no headshot, lethal head or falloff.
        namespace D = ValStrike::Damage;
        const D::DamageTable Table = D::Sanitized(D::DefaultTable());
        bool bFlat = true;
        for (int Zone = static_cast<int>(D::HitZone::Head); Zone < static_cast<int>(D::HitZone::Count); ++Zone)
        {
            for (const Tuning* Each : {&Slash, &Stab})
            {
                D::HitRequest Hit;
                Hit.Weapon = D::WeaponClass::Knife;
                Hit.Zone = static_cast<D::HitZone>(Zone);
                Hit.DistanceCm = 140.0f;
                Hit.AttackDamage = static_cast<float>(Each->damage);
                const D::HitResult Out = D::Compute(Table, Hit);
                bFlat = bFlat && Out.Status == D::HitStatus::Applied && Out.Damage == static_cast<float>(Each->damage)
                    && !Out.Lethal && Out.FalloffScale == 1.0f;
            }
        }
        Expect(bFlat, "slash 40 and stab 65 on every zone, never lethal by zone");
        Expect(!D::IsHeadshotHit(D::WeaponClass::Knife, D::HitZone::Head) && D::IsHeadshotHit(D::WeaponClass::Rifle, D::HitZone::Head),
            "the knife never reports a headshot");
        D::HitRequest Rifle;
        Rifle.Weapon = D::WeaponClass::Rifle;
        Rifle.Zone = D::HitZone::Head;
        Expect(D::Compute(Table, Rifle).Lethal, "rifle lethal headshot unchanged");
        Rifle.Zone = D::HitZone::UpperTorso;
        Expect(D::Compute(Table, Rifle).Damage == 30.0f, "rifle body damage unchanged");
        D::HitRequest Knife;
        Knife.Weapon = D::WeaponClass::Knife;
        Knife.Zone = D::HitZone::Thigh;
        Expect(D::Compute(Table, Knife).Damage == 40.0f, "knife without attack damage uses the slash base");
        Knife.AttackDamage = std::numeric_limits<float>::quiet_NaN();
        Expect(D::Compute(Table, Knife).Damage == 40.0f, "non-finite attack damage falls back to the profile");
        Knife.AttackDamage = 1.0e9f;
        Expect(D::Compute(Table, Knife).Damage == D::MaxBaseDamage, "attack damage is clamped");
        Knife.Zone = D::HitZone::None;
        Knife.AttackDamage = 65.0f;
        Expect(D::Compute(Table, Knife).Status == D::HitStatus::InvalidZone, "no zone, no damage");
        Expect(std::strcmp(D::WeaponName(D::WeaponClass::Knife), "Knife") == 0 && D::IsValidWeapon(D::WeaponClass::Knife),
            "knife damage class name");
        Expect(static_cast<int>(D::WeaponClass::Rifle) == 0 && static_cast<int>(D::WeaponClass::Pistol) == 1
            && static_cast<int>(D::WeaponClass::Knife) == 2, "replicated weapon class values");
    }
    std::printf("%d/%d knife rule checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
