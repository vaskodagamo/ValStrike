# ART-27 — UrbanEntry K01 third-person motion

Editable derived source: `UrbanEntry01_KnifePilot.blend`. Six animation-only FBXs in `Exports/` use the existing `SKEL_UE01`/66-bone pelvis-root hierarchy. The saved scene contains the preserved character and actual K01 mesh for fit review; the knife is preview-only and excluded from animation exports. Use the existing K01 mesh export for native assembly.

The base character geometry, UVs, custom normals, weights, rest matrices, materials and eight pilot/source actions are unchanged. The existing PistolPilot source stays separate and unchanged. Knife geometry/materials remain exactly equal to frozen K01 `d774eafb710af68bf5fa23666e5d0ddacb651d3e`; no mesh scaling, smoothing, remeshing, rig modification or paid generation.

| Action / FBX | Frames at 60 FPS | Duration | Impact | Usage |
| --- | --- | --- | --- | --- |
| `A_UE01_Knife_Hold` | 1–61 | 1.00 s | — | Constant ready loop |
| `A_UE01_Knife_Equip` | 1–25 | 0.40 s | — | Hip-side lift to ready |
| `A_UE01_Knife_Walk` | 1–63 | 1.033333 s | — | Original Walking lower body |
| `A_UE01_Knife_Run` | 1–39 | 0.633333 s | — | Original Running lower body |
| `A_UE01_Knife_Slash` | 1–34 | 0.55 s | Frame 10 / 0.15 s | Right-to-left cut, recover to ready |
| `A_UE01_Knife_Stab` | 1–55 | 0.90 s | Frame 19 / 0.30 s | Forward thrust, withdraw, recover |

Frame 1 is time zero. Endpoints count intervals, not samples: duration = `(last_frame - 1) / 60`. Both attacks begin/end in the hold; equip ends in the hold. `Impact` and `Ready` source pose markers and the JSON metadata are the authoritative artist handoff; FBX marker transport is not assumed. Native integration creates presentation notifies at the stated seconds. Damage timing stays server-owned.

Root-motion extraction stays **off**. Lower-body Walking/Running curves are retained exactly, including their in-place pelvis motion. The four stationary actions have a neutral fixed lower body. Native must layer equip/attack upper body from `spine_01` over its existing locomotion/crouch/airborne lower body rather than freezing the legs during moving attacks. No crouch or directional gait redesign is included. The walking/running clip cadence still needs matching to native movement speed; source readiness does not prove runtime foot-lock or blending.

## Attach K01 to `hand_r`

[Exact matrix and timing contract](../../../../../docs/assets/urban-entry01/knife-pilot/animation-contract.json) stores the constant knife-to-hand transform `S`. These are **Blender bone-head-local**, centimeter, column-vector matrices. Source is +X forward, Z up, character left +Y. Knife +X runs along the blade; knife origin is Grip. Character, bones and knife use scale 1.

In Blender, `W_K = W_character × H_hand_r × S`. The preview empty uses Copy Transforms against the bone head, then `S` on the knife child. Do not use Blender's default bone-tail parent offset. Both FBXs use the existing -Y-forward/Z-up serialization; those exporter arguments are not Unreal socket Euler angles.

Native recipe:

1. Import these animation-only files onto the existing measured UrbanEntry skeleton, without renaming/reorienting bones or adding a root. Use the actual existing K01 mesh/material recipe, not the held retopology candidate. Disable root motion. Import 60 FPS and the listed ranges with no extra frame-zero key.
2. Create/use a socket on **`hand_r`**, not a weapon bone, palm tail or Manny hand assumption. Keep relative/component scales 1. Preserve the imported character and K01 geometry scale.
3. Measure the native component-space hand transform `H_U` in Hold frame 1 and the imported knife's local basis. Let `C` map Blender character component points into native character component space, and `K` map native knife-local points into source knife-local points. Solve `S_U = inverse(H_U) × C × H_B × S × K`, where `H_B` is the source component hand matrix measured in that same pose. If bones retain their source local basis and only a common basis conversion is applied, this reduces to the corresponding basis-converted local matrix. A raw copied XYZ/Euler offset is not a verified conversion. Measure any Y reflection, handedness and imported bone-axis correction; do not guess them from axis labels.
4. Validate the assembly on Hold, slash frame 10/16 and stab frame 19 using the source contact and side views. Keep the blade out of the hand/body and the guard hand on its own side. Verify actual native material channel routing, mirrored shading, socket alignment, camera distance, animation transitions and remote-client visibility before acceptance.
5. Native owns equipped/switch/death visibility and blend cancellation. This file does not supply a replicated gameplay event or authoritative hit volume. Cosmetic motion does not determine damage reach.

## Reproduce the assigned source work

From this worktree root with Blender **5.2.2 LTS**, CPU threads 2:

```sh
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/KnifePilot/prepare_knife_motion.py
blender --background --factory-startup --threads 2 --python-exit-code 1 --python Art/Source/Characters/UrbanEntry01/KnifePilot/review_knife_motion.py
```

`prepare_knife_motion.py` supports `-- --preview-only` for fitting. Both scripts also support `-- --attacks-only` to update/review only slash and stab after an attack-specific change, retaining the earlier unchanged-export evidence. The review reopens the saved source, compares preserved data, reimports the actual new exports onto the existing original character export and renders the changed poses. It reuses the project's source/export helpers and prior fingerprint approach. No unrelated clips are rerendered. The PBR material is rebound to the imported character for matched shading; this is not automatic FBX/native material verification.

[Review and limits](../../../../../docs/assets/urban-entry01/knife-pilot/README.md). Native/import/server-client/performance verification remains pending.
