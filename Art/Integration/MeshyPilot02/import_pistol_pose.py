"""INT-ART-02 phase A: import ART-23's stationary pistol hold and derive the P01 grip.

Imports only A_UE01_Pilot_PistolHold.fbx (allowlist + SHA-256 from the frozen ART-23
readiness record) onto the existing UrbanEntry skeleton, root motion off. The mesh,
skeleton and other clips are not re-saved; their file hashes are checked unchanged.

The P01 world-model offset relative to native hand_r is measured, not copied: the
native hand_r component transform H at frame 1 is compared with the source record
under C=identity and C=diag(1,-1,1); the matching basis gives
local = inverse(H) * (C * W * C), W = source P01 weapon component matrix.

Run through Scripts/with_ue_slot.py (fresh editor process). -VSInspectOnly reopens.
"""

import hashlib
import json
import math
from pathlib import Path
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
READINESS = ROOT / "docs/assets/urban-entry01/pistol-pilot/readiness.json"
VALIDATION = ROOT / "docs/assets/urban-entry01/pistol-pilot/source-validation.json"
REPORTS = ROOT / "docs/assets/meshy-site01-followups"
BASE = "/Game/Art/Characters/UrbanEntry01"
ANIMS = f"{BASE}/Animations"
NAME = "A_UE01_Pilot_PistolHold"
MESH = f"{BASE}/SK_VS_UrbanEntry01"
UNCHANGED = ["Content/Art/Characters/UrbanEntry01/SK_VS_UrbanEntry01.uasset",
             "Content/Art/Characters/UrbanEntry01/SK_VS_UrbanEntry01_Skeleton.uasset",
             "Content/Art/Characters/UrbanEntry01/Animations/A_UE01_Pilot_RifleHold.uasset",
             "Content/Art/Characters/UrbanEntry01/Animations/A_UE01_Pilot_Walk_RifleHold.uasset",
             "Content/Art/Characters/UrbanEntry01/Animations/A_UE01_Walking.uasset"]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def xyz(vector):
    return [round(getattr(vector, axis), 4) for axis in ("x", "y", "z")]


def source_fbx():
    record = json.loads(READINESS.read_text())
    require(record.get("task_id") == "ART-23" and record.get("status") == "source_ready", "ART-23 not ready")
    items = [i for i in record["included_files"] if i["path"].endswith(f"{NAME}.fbx")]
    require(len(items) == 1, "Allowlist must contain exactly the pistol hold FBX")
    path = ROOT / items[0]["path"]
    require(path.is_file() and digest(path) == items[0]["sha256"], f"Missing or changed source: {path}")
    return record, path


def import_animation(path, skeleton):
    options = unreal.FbxImportUI()
    for key, value in {"import_mesh": False, "import_as_skeletal": True, "import_animations": True,
                       "import_materials": False, "import_textures": False, "create_physics_asset": False,
                       "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION,
                       "automated_import_should_detect_type": False, "skeleton": skeleton}.items():
        options.set_editor_property(key, value)
    data = options.get_editor_property("anim_sequence_import_data")
    for key, value in {"convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                       "import_meshes_in_bone_hierarchy": False,
                       "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME}.items():
        data.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(path), "destination_path": ANIMS, "destination_name": NAME,
                       "automated": True, "replace_existing": False, "save": False,
                       "factory": unreal.FbxFactory(), "options": options}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    anim = unreal.load_asset(f"{ANIMS}/{NAME}")
    require(isinstance(anim, unreal.AnimSequence), "Pistol hold import failed")
    anim.set_editor_property("enable_root_motion", False)
    require(unreal.EditorAssetLibrary.save_loaded_asset(anim, False), "Pistol hold save failed")
    return anim


def matrix_to_transform(m):
    r = [row[:3] for row in m[:3]]
    trace = r[0][0] + r[1][1] + r[2][2]
    if trace > 0:
        s = math.sqrt(trace + 1.0) * 2
        w, x, y, z = .25 * s, (r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s
    elif r[0][0] > r[1][1] and r[0][0] > r[2][2]:
        s = math.sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2
        w, x, y, z = (r[2][1] - r[1][2]) / s, .25 * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s
    elif r[1][1] > r[2][2]:
        s = math.sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2
        w, x, y, z = (r[0][2] - r[2][0]) / s, (r[0][1] + r[1][0]) / s, .25 * s, (r[1][2] + r[2][1]) / s
    else:
        s = math.sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2
        w, x, y, z = (r[1][0] - r[0][1]) / s, (r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, .25 * s
    return unreal.Transform(unreal.Vector(m[0][3], m[1][3], m[2][3]),
                            unreal.Quat(x, y, z, w).rotator(), unreal.Vector(1, 1, 1))


def reflect(m, flip_y):
    c = [1, -1 if flip_y else 1, 1, 1]
    return [[c[i] * m[i][j] * c[j] for j in range(4)] for i in range(4)]


def component_pose(mesh, anim, bone, frame):
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    chain, current = [], unreal.Name(bone)
    while str(current) != "None":
        chain.append(current)
        current = probe.get_parent_bone(current)
    pose = unreal.Transform()
    for name in chain:
        pose = unreal.MathLibrary.compose_transforms(pose, unreal.AnimationLibrary.get_bone_pose_for_frame(anim, name, frame, False))
    return pose, [str(n) for n in chain]


def derive_p01(mesh, anim):
    source = json.loads(VALIDATION.read_text())["weapons"]["P01"]
    hand, chain = component_pose(mesh, anim, "hand_r", 0)
    native = xyz(hand.translation)
    src = [row[3] for row in source["hand_r_component_matrix"][:3]]
    basis = None
    for flip_y in (True, False):
        expected = [src[0], -src[1] if flip_y else src[1], src[2]]
        if all(abs(a - b) <= 0.5 for a, b in zip(native, expected)):
            basis = flip_y
            break
    evidence = {"native_hand_r_cm": native, "source_hand_r_cm": src, "bone_chain": chain}
    require(basis is not None, f"Native hand_r matches neither basis: {evidence}")
    weapon = matrix_to_transform(reflect(source["weapon_component_matrix"], basis))
    local = unreal.MathLibrary.compose_transforms(weapon, unreal.MathLibrary.invert_transform(hand))
    rot, quat = local.rotation.rotator(), local.rotation
    evidence.update({
        "basis": "y_negated" if basis else "identity",
        "p01_component_cm": xyz(weapon.translation),
        "p01_component_rotator_roll_pitch_yaw": [round(v, 4) for v in (weapon.rotation.rotator().roll,
                                                 weapon.rotation.rotator().pitch, weapon.rotation.rotator().yaw)],
        "hand_r_offset_location_cm": xyz(local.translation),
        "hand_r_offset_rotator_roll_pitch_yaw": [round(rot.roll, 4), round(rot.pitch, 4), round(rot.yaw, 4)],
        "hand_r_offset_quat_xyzw": [round(quat.x, 6), round(quat.y, 6), round(quat.z, 6), round(quat.w, 6)]})
    return evidence


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-02 worktree")
    record, fbx = source_fbx()
    before = {p: digest(ROOT / p) for p in UNCHANGED}
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    mesh = unreal.load_asset(MESH)
    require(isinstance(mesh, unreal.SkeletalMesh), "Existing UrbanEntry mesh missing")
    if not inspect_only:
        require(not unreal.EditorAssetLibrary.does_asset_exist(f"{ANIMS}/{NAME}"), "Pistol hold exists; inspect, do not overwrite")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        import_animation(fbx, mesh.get_editor_property("skeleton"))
    anim = unreal.load_asset(f"{ANIMS}/{NAME}")
    require(isinstance(anim, unreal.AnimSequence), "Pistol hold missing")
    report = {"engine": unreal.SystemLibrary.get_engine_version(),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "readiness_sha256": digest(READINESS), "source_commit": record["source_commit"],
              "source_fbx_sha256": digest(fbx), "asset": anim.get_path_name(),
              "skeleton": anim.get_editor_property("skeleton").get_path_name(),
              "length_s": round(anim.get_editor_property("sequence_length"), 4),
              "root_motion": anim.get_editor_property("enable_root_motion"),
              "p01_grip_derivation": derive_p01(mesh, anim),
              "unchanged_existing_assets": before, "runtime_verified": False}
    after = {p: digest(ROOT / p) for p in UNCHANGED}
    require(after == before, "Existing UrbanEntry assets changed during pistol import")
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("pistol-reopen-report.json" if inspect_only else "pistol-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if inspect_only:
        prior = json.loads((REPORTS / "pistol-import-report.json").read_text())
        diffs = [k for k in ("asset", "skeleton", "length_s", "root_motion", "p01_grip_derivation",
                             "unchanged_existing_assets") if report[k] != prior[k]]
        require(not diffs, f"Reopen differs from import: {diffs}")
    unreal.log(f"VALSTRIKE_MESHY_PISTOL_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_MESHY_PISTOL_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        unreal.SystemLibrary.quit_editor()
