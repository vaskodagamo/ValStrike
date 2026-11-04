"""INT-ART-03: read the frozen ART-26-C1 first-person knife poses (read-only, never saved).

Blender 5.2.2, from the repository root:
  blender --background --factory-startup --threads 2 \
    Art/Source/Characters/FPArms03/KnifePilot/KnifePilot.blend \
    --python-exit-code 1 --python Art/Integration/KnifePilot01/read_source_poses.py

For each knife action at frame 1, its impact frame and its last frame, writes the
armature-space (= component, the armature object is identity) matrices of
weapon_socket_r, hand_r and hand_l, and the K01 component matrix
H(weapon_socket_r) @ weapon_local_from_socket from the frozen animation contract.
UE import compares these with the native skeleton to derive and verify the
first-person knife attachment. Output: docs/assets/knife-integration/source-poses.json.
"""

import hashlib
import json
from pathlib import Path

import bpy
from mathutils import Matrix

ROOT = Path.cwd()
CONTRACT = ROOT / "docs/assets/fparms03/knife-pilot/animation-contract.json"
OUT = ROOT / "docs/assets/knife-integration/source-poses.json"
BONES = ("weapon_socket_r", "hand_r", "hand_l")


def rows(m):
    return [[round(v, 6) for v in row] for row in m]


def main():
    contract = json.loads(CONTRACT.read_text())
    blend = Path(bpy.data.filepath)
    arm = bpy.data.objects["SKEL_FPArms03"]
    assert arm.matrix_world == Matrix.Identity(4), "Armature object is not identity"
    local = Matrix(contract["attachment"]["weapon_local_from_socket"])
    ad = arm.animation_data or arm.animation_data_create()
    report = {"blend": str(blend.relative_to(ROOT)), "blend_sha256": hashlib.sha256(blend.read_bytes()).hexdigest(),
              "contract_sha256": hashlib.sha256(CONTRACT.read_bytes()).hexdigest(), "space": "Blender armature space, cm, column vectors",
              "clips": {}}
    scene = bpy.context.scene
    for name, clip in contract["clips"].items():
        action = bpy.data.actions[clip["action"]]
        ad.action = action
        if getattr(ad, "action_slot", None) is None and getattr(action, "slots", None):
            ad.action_slot = action.slots[0]
        frames = sorted({clip["start"], clip.get("impact_frame") or clip["start"], clip["end"]})
        out = {}
        for frame in frames:
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            poses = {b: arm.pose.bones[b].matrix.copy() for b in BONES}
            out[str(frame)] = {**{b: rows(m) for b, m in poses.items()},
                               "K01_component": rows(poses["weapon_socket_r"] @ local)}
        report["clips"][name] = out
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1) + "\n")
    print(f"VALSTRIKE_KNIFE_SOURCE_POSES: {OUT}")


main()
