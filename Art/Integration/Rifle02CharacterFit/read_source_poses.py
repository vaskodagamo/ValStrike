"""INT-ART-05: read the frozen ART-28 R02 rifle poses (read-only, never saved).

Blender 5.2.2, from the repository root:
  blender --background --factory-startup --threads 2 \
    Art/Source/Characters/UrbanEntry01/Rifle02Pilot/UrbanEntry01_Rifle02Pilot.blend \
    --python-exit-code 1 --python Art/Integration/Rifle02CharacterFit/read_source_poses.py

Writes every bone's armature-space (= component; the armature object is identity)
head matrix at the sampled Hold/Walk/Run frames ART-28 reviewed, plus the R02 body
component matrix H(hand_r) @ S from the frozen assembly contract. The UE import
compares all bones with the native skeleton: if every bone and the rifle match under
the measured basis, ART-28's sampled surface-contact results apply unchanged.
Output: docs/assets/knife-rifle-acceptance/rifle02-source-poses.json.
"""

import hashlib
import json
from pathlib import Path

import bpy
from mathutils import Matrix

ROOT = Path.cwd()
CONTRACT = ROOT / "docs/assets/urban-entry01/rifle02-pilot/assembly-contract.json"
OUT = ROOT / "docs/assets/knife-rifle-acceptance/rifle02-source-poses.json"
FRAMES = {"A_UE01_R02_Hold": (1, 31, 61), "A_UE01_R02_Walk": (1, 16, 32, 47, 63), "A_UE01_R02_Run": (1, 10, 20, 30, 39)}


def rows(m):
    return [[round(v, 6) for v in row] for row in m]


def main():
    contract = json.loads(CONTRACT.read_text())
    socket = Matrix(contract["socket"]["matrix"])
    arm = bpy.data.objects["SKEL_UE01"]
    assert arm.matrix_world == Matrix.Identity(4), "Armature object is not identity"
    ad = arm.animation_data or arm.animation_data_create()
    blend = Path(bpy.data.filepath)
    report = {"blend": str(blend.relative_to(ROOT)), "blend_sha256": hashlib.sha256(blend.read_bytes()).hexdigest(),
              "contract_sha256": hashlib.sha256(CONTRACT.read_bytes()).hexdigest(),
              "space": "Blender armature space, cm, column vectors, bone head", "clips": {}}
    for name, frames in FRAMES.items():
        action = bpy.data.actions[name]
        ad.action = action
        if getattr(ad, "action_slot", None) is None and getattr(action, "slots", None):
            ad.action_slot = action.slots[0]
        report["clips"][name] = {}
        for frame in frames:
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            bones = {b.name: rows(b.matrix) for b in arm.pose.bones}
            report["clips"][name][str(frame)] = {"bones": bones,
                                                 "R02_component": rows(arm.pose.bones["hand_r"].matrix @ socket)}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report) + "\n")
    print(f"VALSTRIKE_R02_SOURCE_POSES: {OUT}")


main()
