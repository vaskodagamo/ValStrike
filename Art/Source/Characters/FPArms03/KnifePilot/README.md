# FPArms03 KnifePilot — ART-26

**Held source checkpoint:** top-view palm/handle contact and attack windup cropping still fail appearance acceptance. See `docs/assets/fparms03/knife-pilot/review.md`.

Derived first-person K01 hands and five source actions for KNIFE-01. This package does not implement equip inputs, melee authority, damage, audio or native presentation. Those belong to the separate gameplay/native owners. See the task-owned readiness report before admission.

## Files and source identity

- `KnifePilot.blend`: paired skinned hands and compatible rig only, with five knife actions. Packed original PBR. No staged knife geometry saved in the deliverable.
- `Exports/SK_FPArms03_KnifePilot.fbx`: paired hands and 39-bone rig; no weapon/character reference mesh.
- `Exports/A_FPArms03_Knife*.fbx`: equip, idle, locomotion, slash and stab. 60 fps, root motion off.
- Frozen ART-24 input is `5e53cb4de062f8ae0f91b171b09406c85ce6444f`; K01 is `d774eafb710af68bf5fa23666e5d0ddacb651d3e`. Exact paths/hashes are in `docs/assets/fparms03/knife-pilot/inputs.json`. Ignored `Reference/Frozen/` is staging recoverable with `git show <commit>:<path> | git lfs smudge`.
- Existing base source/originals/textures remain unchanged. The material maps are the original packed inputs and the established `../Textures/Embedded/` JPEG files: image 0 sRGB base color, image 1 linear G roughness/B metalness, image 2 linear tangent normal. Retain original factors/connections; actual FBX comparison explicitly rebinds this original graph, not a claim of automatic native material import.

## Animation contract

All times start at frame 1, so seconds = `(frame - 1) / 60`. The last frame is the endpoint, not one extra frame of duration. `animation-contract.json` is the machine-readable authority for names, ranges, impact markers, camera and attachment matrix.

| Action suffix | Frames | Duration | Impact |
| --- | --- | --- | --- |
| Equip | 1–25 | 0.40 s | — |
| Idle | 1–61 | 1.00 s loop | — |
| Locomotion | 1–49 | 0.80 s loop | — |
| Slash | 1–34 | 0.55 s | Frame 10 / 0.15 s |
| Stab | 1–55 | 0.90 s | Frame 19 / 0.30 s |

Slash sweeps laterally with a separate windup/follow-through; stab extends forward and recovers. The off hand withdraws during attacks. Equip enters from below the view. Attack/equip endpoints return to the idle attachment. Impact markers are stored in the blend/JSON; recreate native notifies from JSON rather than assuming FBX imports them. They are visual synchronization metadata only: server gameplay determines valid hits and cancellations. Integration must carry these exact timings into native sequences/events; do not independently retime the clips.

## Attachment and camera

Authored in centimeters, +X forward and +Z up. Root stays identity. Bone names, rest transforms and hierarchy match frozen ART-24; original mesh topology/positions, UVs, custom normals, weights and PBR are retained. No finger/weapon rescaling or optimization. The pair is still 780,956 triangles; runtime budget/LOD approval remains pending.

For column vectors, `K01_component = Arm_component @ posed_weapon_socket_r @ weapon_local_from_socket`. Use the exact matrix in `animation-contract.json`; K01 remains at scale 1 with its authored grip-local origin. Do not attach a second knife mesh embedded in these exports—none is present. Convert the measured basis consistently at native import and verify with the actual skeleton.

The source camera is at (−20, 0, 5) cm looking +X with 90° horizontal FOV. To place the same component convention relative to a camera at the origin, subtract that source camera position, giving component translation (+20, 0, −5) cm before the native camera transform. This is a source framing recipe, not a change to gameplay camera/collision. Native integration owns the final measured offset and must preserve blade/off-hand clearance.

## Reproduction and evidence

Run one Blender 5.2.2 LTS process at a time, with `--background --factory-startup --threads 2 --python-exit-code 1 --python <script>`. Stage the exact frozen inputs first. `author_knife.py -- build` creates only the derived source and contract; use Blender's `--` separator before `build`. `inspect_contacts.py` provides sparse grip/off-hand observations, not exhaustive collision proof. `export_and_inspect.py` reuses focused source/FBX checks, verifies original surface/rest/weights, clip timing/root motion and actual round trips. `render_evidence.py -- source` and `-- export` produce matched actual idle/contact, equip, locomotion, impact and recovery views using the same K01, camera, lights and original PBR.

Rebuilding overwrites derived outputs, not base FPArms03. No service/account call, credit spend, Unreal operation, build or gameplay test suite is part of this task. Source appearance, native import/material/camera correctness and full playable knife acceptance are separate gates.
