# INT-ART-02 — Pistol/QA batch, then first Site01 facade section

- Owner: existing Claude **INT-ART-01 Meshy playtest integration** session. Reuse its clean `.parallel-worktrees/int-art-01-meshy-playtest` on new `claude/int-art-02-site01-followups` from current main. Preserve #93 branch/head. One native writer; no additional QA worker or contributor build.
- Priority/backlog: PLAY-02 / PLAY-04. User's new map-art priority is a coherent courtyard/service-facade section and its approach in existing Site01, not a new map/layout. First-person hands are not a dependency.
- Native phase A may begin after existing GPF-04 writer is frozen and its exact #97 diff reviewed. Current source head `100245d7145601b1c4876011c1f8dfc59f52ca0d`; ART-23 source `887de0a` / handoff `9232dad`, merged #95. Record exact combined candidate; user retains merges.
- Every UE command uses `Scripts/with_ue_slot.py`; exit 75 returns to queue. Pinned UE 5.8.4/Xcode 26.1.1. No rendered UI manipulation during tests. Build once for the coherent C++ candidate; rerun only relevant changes/failures.

## Exclusive writes

- Existing UrbanEntry native animation assets under `Content/Art/Characters/UrbanEntry01/`, only the approved ART-23 pistol animation; original skeleton/mesh/other clips remain unchanged.
- `Source/ValStrike/Presentation/VSCharacterPresentationComponent.*` and `VSCharacterAnimInstance.*` for stationary pistol pose selection; `VSWeaponPresentationComponent.*` only for matching remote P01 attachment. `VSCharacter.*` presentation-only wiring if essential after movement writer freezes. Preserve movement/input/replication/health/camera/first-person/hit-zone rules.
- New `Content/Art/Environment/MeshySite01/CourtyardPilot/`, `Content/Maps/Site01/VS_Site01.umap` and map-local `Materials/` for phase B. No layout/config/default-map change.
- Reproducible scripts under `Art/Integration/MeshyPilot02/`; evidence `docs/assets/meshy-site01-followups/`; handoff `docs/workstreams/int-art-02-site01-followups.md`; update `docs/playtest/meshy-pilot.md` for the actual new candidate.

All source art, gameplay tests/runners, shared boards/central logs and other checkouts remain read-only. A failure outside these paths returns to its existing owner.

## Phase A — Source-ready fixes and one combined acceptance

Import ART-23's single animation-only FBX onto the existing skeleton, root motion off. Activate the pose and matching **P01** offset together, measured in native bone coordinates from the supplied matrices. Producer accepted stationary third-person P01 at representative distance; close-up glove/contact limitations remain. P02 replacement, first-person hands and additional animation systems are excluded.

Combine GPF-04 #97 with already merged GPF-02 #84 and GPF-03 #86. #97 changes boundary-run test timing, not movement gameplay. Its explanation (teleport correction still in flight) is a hypothesis to verify: read the exact diff/evidence; no weakened bounds or claim that D3 is resolved before runtime. Producer permits the bounded `VSMovementSmoke.cpp` / `Scripts/test_movement.py` timing correction and existing runner-test changes; those old owners are idle. Do not edit these in integration.

Run affected existing checks once on the combined build: movement LAN and `typical` with correction classification, session rendered ×3 for the previously intermittent teardown, reachable-spectate match scenario (including its fourth mate client), and focused rendered remote pistol switching/visibility on Site01 with server/two clients. Reuse #93 map/props/rifle evidence where unchanged. If in-motion corrections remain, return D3 to the gameplay owner; do not keep retrying to manufacture a pass. Record exact runtime/source identity and screenshots. No broad historical suite.

## Phase B — Build a visible map section from ready structures

Reserve this same native owner for [ART-25](../art/ART-25-courtyard-structure.md)'s first frozen, reviewed W01/W08/W09 subset. Preflight placement independently while phase A is underway, but do not import mutable/unaccepted source. If ART-25 is not ready, publish phase A and stop with that exact trigger; the producer resumes this same owner. Do not wait for all remaining world objects or first-person hands.

Use the approved IndustrialV2 anchor and existing props to dress a coherent courtyard/service facade plus approach route. Stage ready pieces rather than waiting for all six remaining object types. Preserve original routes, spawns, objective/device/recovery markers, cover heights and collision. Native decorative replacements must not introduce doorway-blocking collision. Keep existing geometry/contracts where needed and avoid regenerating the whole map.

Inspect the reported competing directional-light warning within this map's lighting actors. Establish one intentional dominant sun/skylight arrangement only if actual actor inspection supports it; preserve material colors and visibility. This is a bounded map-lighting correction, not a renderer/config overhaul.

Save/reopen, capture genuine eye-level approach/facade/courtyard and overhead Unreal views, compare before/after, and run the existing map route/cover/collision inspection plus affected local server/two-client walk/shoot/round smoke. Map-only changes do not require another C++ build if the existing binary remains valid. Report uncovered gaps honestly. Publish one useful main-targeted PR per completed coherent phase, with exact evidence and next trigger; no empty logs. User merges; cross-Mac/packaging remain separate.
