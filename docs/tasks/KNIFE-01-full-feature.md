# KNIFE-01 — Complete playable knife

User scope expansion verified directly in Direktor on 2026-11-04 (Europe/Berlin): “knife holding with hands, knife in game as nr 3 item etc full implemention of knife”. This supersedes the historical ART-20 source-only exclusion. Producer owns end-to-end delivery; user merges. Preserve active Site01 work and the three-contributor cap. Reuse K01 #85 and FPArms03; **zero new Meshy jobs by default**.

## Completion, not just source readiness

- Keys 1/2/3 equip rifle/pistol/knife. Q switches to the previous of all three correctly, including prediction/reconciliation. Knife is carried by default after spawn/round reset, no purchase or ammo.
- Visible K01 held by the approved bare-forearm gloved pair in first person, with equip, idle/locomotion and distinct primary slash/secondary stab. Correct remote third-person hold/attacks and switch/death visibility. No missing hands or placeholder mesh completion claim.
- LMB performs one light slash per press; RMB one heavier stab. No charge/combo/backstab feature. Starting tunable game values: slash 40 damage, 125 cm reach, impact at 0.15 s, next attack after 0.55 s; stab 65 damage, 150 cm reach, impact at 0.30 s, next attack after 0.90 s; equip 0.40 s. Times are from accepted action start. Native animation timing must agree; changes require one shared contract update before export/wiring, not independent artist/code guesses. Knife movement uses the existing unweighted base speed (currently 450 cm/s), without altering rifle/pistol tuning.
- Server validates owner, alive state, phase/team rules, equipped knife, equip readiness and cooldown. Server computes hit candidates from authoritative pawn/view state, range/arc and world obstruction at the impact time; the client never supplies a trusted victim, damage or hit result. At most one application per target per swing, bounded pending action IDs/timers, safe duplicate/replay rejection. Cosmetic prediction/events cannot apply damage.
- Tunable attack cones: slash 35-degree half-angle, stab 10-degree half-angle about authoritative view at impact; range and world obstruction still reject candidates. Flat damage for every hit zone and no knife headshot marker. Keep the existing flat 200 kill reward; no knife bonus. Airborne/crouched attacks are allowed under the same combat gates. These producer decisions resolve the engineering brief's four proposed gaps.
- Use the current authoritative health/damage attribution path with an explicit Knife classification. No inherited rifle lethal headshot, penetration or distance falloff; retain existing match/team gates. Switching, death, destroyed pawn, round reset and closed combat phase cancel pending impacts and presentation. A delayed action must not hit after a later lifecycle transition.
- Swing/contact audio, authoritative hit feedback and knife kill-feed identity. While knife is equipped, no firearm muzzle/reload/ammo UI or stale firearm label. A miss makes a swing sound; wall/player contact has the appropriate cue. Original/provenanced audio or suitable existing cues only.
- Actual server and rendered clients verify equip 3/Q, both attacks, hit/miss/range/wall rejection, phase/death/reset/switch cancellation, remote appearance, first-person framing and feedback. Local/unit/source results alone do not complete the feature.

## Slot/protocol contract

Current slots are Rifle=0/Pistol=1; **request byte 2 already means Last**. Preserve Rifle request 0, Pistol request 1 and Last request 2; add a distinct Knife request 3. Knife may be slot 2, but slot and request are separate domains: explicit encode/decode helpers must replace assumptions that they can be `static_cast` between each other. Cover owner prediction, queued requests, server resolution, development smoke requests, invalid values and Q history. Key 3 is an input binding, not permission to send raw slot 2.

Audit firearm-only loops and null/fallback assumptions across loadout, movement, HUD, audio, economy and kill feed. `GetEquippedWeapon()` returning no firearm while holding Knife must not silently select rifle behavior. Prefer one small knife component and focused rules alongside existing systems; no general weapon framework rewrite.

## Owners and admission order

| Task | Existing owner / scope | Admission and stopping condition |
| --- | --- | --- |
| KNIFE-01 gameplay | Existing GPT-04 (loadout/prediction owner), via Claude gameplay lead; same `gpt-04-switch-prediction` worktree. | Phase A #105 is merged. Phase B #108 froze at `22df932`, engineering-reviewed with green CI; owner stopped. Frozen cue API goes to INT-ART-03; runtime pending. |
| [ART-26 first person](art/ART-26-knife-first-person.md) | Existing hands artist at High, after ART-24 frozen clean publication; derived KnifePilot source in its existing owned checkout/new branch. | Draft #109 froze held at `38b9372`; ART-26-C1 is active with the same owner/PR to correct actual grip and windup cropping. All clips keep shared timing; zero credits. |
| [ART-27 third person](art/ART-27-knife-third-person.md) | Existing character artist at High, derived UrbanEntry KnifePilot source; fresh `art27-knife-third-person`, prepared `c8bb630`. | Frozen #110 at `bca668a`, producer source-reviewed for the bounded third-person pilot; native layering/clearance pending. Owner stopped. |
| INT-ART-03 knife | Existing single native integrator after INT-ART-04 weapon replacement freezes and corrected ART-26 plus gameplay/ART-27 are accepted. | Import/wire reviewed exact sources, one combined build and affected acceptance. Full-feature verification is the completion gate; no source-only substitute. |

ART-24, ART-25 and INT-ART-02 were active at authorization. ART-24 subsequently froze and ART-26 replaced it. ART-25 then froze locally (public publication approval pending); its vacancy was explicitly released to existing GPT-04 for gameplay phase A. Lead acknowledged actual start on `claude/knife-01-gameplay` from `5407272`. After INT-ART-02 finished and stopped, producer admitted ART-27 and released gameplay phase B to the same owner. Gameplay phase B and ART-27 then froze; ART-26-C1 is the only active correction. INT-ART-04 weapon replacement is prepared but its UI dispatch awaits Mac unlock. INT-ART-03 remains queued behind that native task and corrected art; keep Site01 publication separate from knife implementation.

## Gameplay boundary for the engineering brief

Lead owns discipline review and exact engineering paths. Assign the small new `Combat/VSKnifeComponent.*`, `VSKnifeRules.h` and required shared action/event types; `VSWeaponLoadoutComponent.*`/`VSWeaponLoadoutRules.h`; knife-only damage class/attribution hooks in `Damage/VSDamageModel.*`, `VSDamageEvents.h`, `VSDamageSubsystem.*`; knife labels in HUD/kill-feed; appropriate audio/economy/firearm classification branches; and existing affected checks. List exact files before dispatch rather than claiming all Source.

`VSCharacter.*` input/component/slot hooks and shared presentation initialization are serialized behind INT-ART-02 phase A's frozen source. Gameplay may prepare disjoint new component/loadout/rule files first when admitted; do not edit the native owner's shared hunks. Existing presentation components stay native-owned until that release. New knife presentation event/API can be gameplay-owned, then frozen and handed to native for asset binding. No simultaneous shared-file writers. The lead resolves additional file needs through the producer, without restarting old owners.

Validation is V2 for melee authority/lifecycle. Reuse focused loadout, damage and existing weapon smoke infrastructure; add only meaningful missing melee authority/cancellation and slot-mapping cases. No contributor Unreal builds, no per-art suites, no duplicate harness. Integration extends the coherent existing server/client acceptance for the new attack behavior and checks firearm regression where actually affected.

Use one relevant local compiler/check command with strict warnings; do not repeat under a `g++` alias that is the same Apple clang. Leave the broad compiler matrix to CI. Economy is audit-only until a necessary exact file is named and assigned; the knife is not a purchasable firearm.

## Shared art/native contract

K01 source `d774eafb710af68bf5fa23666e5d0ddacb651d3e`; FPArms03 derives from frozen ART-24, preserving original mesh/UV/normals/PBR and 39-bone hierarchy. Third person uses the unchanged UrbanEntry rig. Artists deliver exact attachment matrices and distinct idle/equip/locomotion/slash/stab actions, 60 fps, root motion off, markers/metadata at the shared impacts. Keep supplied weapon geometry at true source scale; don't scale hands/fingers to hide grip issues. Source pose quality, native import/materials and runtime performance remain distinct gates.

Native owns new knife/hand/animation Content paths after a separate exact integration brief, preserves first-person camera and collision/hit zones, and binds reviewed assets to accepted replicated cosmetic events. Measure source-to-native basis and camera framing; avoid cropping most of the hands or blade/off-hand intersection. Animation visuals do not determine trusted damage. The final handoff includes screenshots/short capture, exact candidate/engine/platform, actual test results, source provenance and remaining limitations.
