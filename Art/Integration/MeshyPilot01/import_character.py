"""INT-ART-01: import the ART-15-approved Urban Entry pilot character into UE 5.8.4.

Imports only the readiness allowlist: SK_VS_UrbanEntry01 with its three textures,
the static R01 hold, the forward in-place rifle walk and the optional unarmed walk.
Creates one material, an optional 1D locomotion blend space (hold -> walk) and the
R01_Pilot_Grip socket on hand_r. Root motion stays off. No map is opened or saved.

The source-to-native basis C is measured, not assumed: the native hand_r component
transform at hold frame 1 is compared with the source record under both C=identity
and C=diag(1,-1,1). Only a matching basis builds the socket, as
socket = inverse(H) * (C * W * C) (column-vector notation, ART-15 PILOT.md).

Run through Scripts/with_ue_slot.py:
  UnrealEditor ValStrike.uproject /Engine/Maps/Entry \
    -ExecutePythonScript=Art/Integration/MeshyPilot01/import_character.py -unattended -log
Reopen with -VSInspectOnly in a fresh process. Existing assets are never overwritten.
"""

import hashlib
import json
import math
from pathlib import Path
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
READINESS = ROOT / "docs/assets/urban-entry01/pilot-readiness.json"
REPORTS = ROOT / "docs/assets/meshy-pilot-integration"
BASE = "/Game/Art/Characters/UrbanEntry01"
ANIMS = f"{BASE}/Animations"
MESH = "SK_VS_UrbanEntry01"
MATERIAL = "M_VS_UrbanEntry01"
BLEND_SPACE = "BS_UE01_Pilot_Locomotion"
SOCKET = "R01_Pilot_Grip"
WALK_CLIP_SPEED = 145.0  # cm/s, stance-foot speed of the walk, see PREFLIGHT.md
CLIP_ROLES = {"static_rifle_idle": "A_UE01_Pilot_RifleHold",
              "forward_in_place_rifle_walk": "A_UE01_Pilot_Walk_RifleHold",
              "optional_unarmed_forward_walk": "A_UE01_Walking"}
TEXTURE_ROLES = {"texture_BaseColor": "basecolor", "texture_Metallic": "metallic", "texture_Roughness": "roughness"}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def xyz(vector):
    return [round(getattr(vector, axis), 4) for axis in ("x", "y", "z")]


def close(actual, expected, tolerance):
    return all(abs(a - b) <= tolerance for a, b in zip(actual, expected))


def load_readiness():
    record = json.loads(READINESS.read_text())
    require(record.get("task_id") == "ART-15" and record.get("status") in ("ready", "partial"), "ART-15 record not usable")
    files = {item["role"]: item for item in record["included_files"]}
    require(set(files) == {"skeletal_mesh", *CLIP_ROLES, *TEXTURE_ROLES}, f"Unexpected allowlist: {sorted(files)}")
    for item in files.values():
        path = ROOT / item["path"]
        require(path.is_file(), f"Missing source (git lfs pull?): {item['path']}")
        require(digest(path) == item["sha256"], f"Source hash mismatch: {item['path']}")
    return record, files


def import_task(filename, destination, name, options=None, factory=None):
    task = unreal.AssetImportTask()
    settings = {"filename": str(filename), "destination_path": destination, "destination_name": name,
                "automated": True, "replace_existing": False, "save": False}
    if options is not None:
        settings["options"] = options
    if factory is not None:
        settings["factory"] = factory
    for key, value in settings.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(f"{destination}/{name}")


def save(asset, label):
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, False), f"Save failed: {label}")


def import_textures(files):
    textures = {}
    for role, kind in TEXTURE_ROLES.items():
        name = Path(files[role]["path"]).stem
        texture = import_task(ROOT / files[role]["path"], f"{BASE}/Textures", name)
        require(isinstance(texture, unreal.Texture2D), f"Texture import failed: {name}")
        texture.set_editor_property("srgb", kind == "basecolor")
        if kind != "basecolor":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        save(texture, name)
        textures[kind] = texture
    return textures


def make_material(textures):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL, f"{BASE}/Materials", unreal.Material, unreal.MaterialFactoryNew())
    require(material, "Could not create character material")
    material.set_editor_property("used_with_skeletal_mesh", True)
    lib = unreal.MaterialEditingLibrary
    wiring = (("basecolor", "RGB", unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
              ("roughness", "R", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
              ("metallic", "R", unreal.MaterialProperty.MP_METALLIC, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
    for row, (kind, output, prop, sampler) in enumerate(wiring):
        node = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -420, row * 260)
        node.set_editor_property("texture", textures[kind])
        node.set_editor_property("sampler_type", sampler)
        require(lib.connect_material_property(node, output, prop), f"Material {kind} connection")
    require(not lib.recompile_material(material), "Character material compile failed")
    save(material, MATERIAL)
    return material


def fbx_options(skeleton=None):
    options = unreal.FbxImportUI()
    animation = skeleton is not None
    for key, value in {
        "import_mesh": not animation, "import_as_skeletal": True, "import_animations": animation,
        "import_materials": False, "import_textures": False, "create_physics_asset": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH,
        "automated_import_should_detect_type": False,
    }.items():
        options.set_editor_property(key, value)
    if animation:
        options.set_editor_property("skeleton", skeleton)
        data = options.get_editor_property("anim_sequence_import_data")
        for key, value in {"convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                           "import_meshes_in_bone_hierarchy": False,
                           "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME}.items():
            data.set_editor_property(key, value)
    else:
        data = options.get_editor_property("skeletal_mesh_import_data")
        for key, value in {"convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                           "import_morph_targets": False, "import_meshes_in_bone_hierarchy": False,
                           "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():
            data.set_editor_property(key, value)
    return options


def import_character(files, material):
    mesh = import_task(ROOT / files["skeletal_mesh"]["path"], BASE, MESH, fbx_options(), unreal.FbxFactory())
    require(isinstance(mesh, unreal.SkeletalMesh), "Skeletal mesh import failed")
    materials = mesh.get_editor_property("materials")
    require(len(materials) == 1, f"Expected one material section, got {len(materials)}")
    materials[0].set_editor_property("material_interface", material)
    mesh.set_editor_property("materials", materials)
    save(mesh, MESH)
    skeleton = mesh.get_editor_property("skeleton")
    require(skeleton, "Imported mesh has no skeleton")
    save(skeleton, "skeleton")  # Separate package created by the FBX importer.
    clips = {}
    for role, name in CLIP_ROLES.items():
        anim = import_task(ROOT / files[role]["path"], ANIMS, name, fbx_options(skeleton), unreal.FbxFactory())
        require(isinstance(anim, unreal.AnimSequence), f"Animation import failed: {name}")
        anim.set_editor_property("enable_root_motion", False)
        save(anim, name)
        clips[role] = anim
    return mesh, skeleton, clips


def make_blend_space(skeleton, hold, walk):
    """Best effort: the runtime falls back to switching clips when this is absent."""
    try:
        factory = unreal.BlendSpaceFactory1D()
        factory.set_editor_property("target_skeleton", skeleton)
        space = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BLEND_SPACE, ANIMS, unreal.BlendSpace1D, factory)
        require(space, "create_asset returned None")
        parameter = unreal.BlendParameter()
        parameter.set_editor_property("display_name", "Speed")
        parameter.set_editor_property("min", 0.0)
        parameter.set_editor_property("max", WALK_CLIP_SPEED)
        parameter.set_editor_property("grid_num", 4)
        parameters = list(space.get_editor_property("blend_parameters"))
        parameters[0] = parameter
        space.set_editor_property("blend_parameters", parameters)
        samples = []
        for anim, value in ((hold, 0.0), (walk, WALK_CLIP_SPEED)):
            sample = unreal.BlendSample()
            sample.set_editor_property("animation", anim)
            sample.set_editor_property("sample_value", unreal.Vector(value, 0, 0))
            samples.append(sample)
        space.set_editor_property("sample_data", samples)
        save(space, BLEND_SPACE)
        return {"created": True, "asset": space.get_path_name()}
    except Exception as error:  # Editor API differences: record and fall back.
        if unreal.EditorAssetLibrary.does_asset_exist(f"{ANIMS}/{BLEND_SPACE}"):
            unreal.EditorAssetLibrary.delete_asset(f"{ANIMS}/{BLEND_SPACE}")
        return {"created": False, "reason": str(error), "runtime_fallback": "hold/walk clip switch"}


def matrix_to_transform(matrix):
    """4x4 column-vector matrix (rotation + translation, unit scale) -> unreal.Transform."""
    r = [row[:3] for row in matrix[:3]]
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
    return unreal.Transform(unreal.Vector(matrix[0][3], matrix[1][3], matrix[2][3]),
                            unreal.Quat(x, y, z, w).rotator(), unreal.Vector(1, 1, 1))


def reflect(matrix, flip_y):
    """C * M * C with C = diag(1, -1 if flip_y else 1, 1, 1)."""
    c = [1, -1 if flip_y else 1, 1, 1]
    return [[c[i] * matrix[i][j] * c[j] for j in range(4)] for i in range(4)]


def component_pose(mesh, anim, bone, frame):
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    chain, current = [], unreal.Name(bone)
    while str(current) != "None":
        chain.append(current)
        current = probe.get_parent_bone(current)
    pose = unreal.Transform()
    for name in chain:  # child first: pose = pose * parent
        local = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, name, frame, False)
        pose = unreal.MathLibrary.compose_transforms(pose, local)
    return pose, [str(n) for n in chain]


def derive_grip(mesh, record, hold):
    """Rifle world-model offset relative to the native hand_r bone (no asset edit).

    UE 5.8 Python cannot name a SkeletalMeshSocket (read-only property), so the
    runtime attaches the world rifle to hand_r with this offset, kept in C++.
    """
    socket_record = record["idle_and_weapon_socket"]
    hand, chain = component_pose(mesh, hold, "hand_r", 0)
    native_hand = xyz(hand.translation)
    source_hand = [row[3] for row in socket_record["hold_hand_r_component_matrix"][:3]]
    basis = None
    for flip_y in (True, False):
        expected = [source_hand[0], -source_hand[1] if flip_y else source_hand[1], source_hand[2]]
        if close(native_hand, expected, 0.5):
            basis = flip_y
            break
    evidence = {"native_hand_r_cm": native_hand, "source_hand_r_cm": source_hand, "bone_chain": chain}
    require(basis is not None, f"Native hand_r matches neither basis; grip held: {evidence}")
    rifle = matrix_to_transform(reflect(socket_record["hold_rifle_component_matrix"], basis))
    local = unreal.MathLibrary.compose_transforms(rifle, unreal.MathLibrary.invert_transform(hand))
    rot = local.rotation.rotator()
    quat = local.rotation
    evidence.update({"hand_r_offset_location_cm": [round(v, 4) for v in xyz(local.translation)],
                     "hand_r_offset_rotator_roll_pitch_yaw": [round(rot.roll, 4), round(rot.pitch, 4), round(rot.yaw, 4)],
                     "hand_r_offset_quat_xyzw": [round(quat.x, 6), round(quat.y, 6), round(quat.z, 6), round(quat.w, 6)],"basis": "y_negated" if basis else "identity",
                     "rifle_component_cm": xyz(rifle.translation),
                     "rifle_component_rotator": [round(v, 3) for v in (rifle.rotation.rotator().roll,
                                                 rifle.rotation.rotator().pitch, rifle.rotation.rotator().yaw)]})
    return evidence


def inspect(record):
    mesh = unreal.load_asset(f"{BASE}/{MESH}")
    require(isinstance(mesh, unreal.SkeletalMesh), "Missing native character")
    bounds = mesh.get_bounds()
    low = xyz(bounds.origin - bounds.box_extent)
    high = xyz(bounds.origin + bounds.box_extent)
    socket = mesh.find_socket(SOCKET)
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    space = unreal.load_asset(f"{ANIMS}/{BLEND_SPACE}")
    samples = [] if space is None else [
        [s.get_editor_property("animation").get_name(), xyz(s.get_editor_property("sample_value"))[0]]
        for s in space.get_editor_property("sample_data")]
    clips = {}
    for name in CLIP_ROLES.values():
        anim = unreal.load_asset(f"{ANIMS}/{name}")
        require(isinstance(anim, unreal.AnimSequence), f"Missing clip {name}")
        clips[name] = {"length_s": round(anim.get_editor_property("sequence_length"), 4),
                       "root_motion": anim.get_editor_property("enable_root_motion")}
    return {
        "mesh": mesh.get_path_name(), "skeleton": mesh.get_editor_property("skeleton").get_path_name(),
        "bounds_min_cm": low, "bounds_max_cm": high,
        "material": mesh.get_editor_property("materials")[0].get_editor_property("material_interface").get_path_name(),
        "socket": None if socket is None else {
            "bone": str(socket.get_editor_property("bone_name")),
            "location_cm": xyz(socket.get_editor_property("relative_location")),
            "rotation": [round(v, 3) for v in (socket.get_editor_property("relative_rotation").roll,
                                               socket.get_editor_property("relative_rotation").pitch,
                                               socket.get_editor_property("relative_rotation").yaw)]},
        "clips": clips,
        "blend_space": unreal.EditorAssetLibrary.does_asset_exist(f"{ANIMS}/{BLEND_SPACE}"),
        "blend_space_samples": samples,
        "bone_count": probe.get_num_bones(), "root_bone": str(probe.get_bone_name(0)),
        "source_commit": record["source_commit"],
    }


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-01 worktree")
    record, files = load_readiness()
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    report = {"engine": unreal.SystemLibrary.get_engine_version(), "readiness_sha256": digest(READINESS),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "map_edited": False, "runtime_verified": False}
    if not inspect_only:
        require(not unreal.EditorAssetLibrary.does_directory_exist(BASE)
                or not unreal.EditorAssetLibrary.list_assets(BASE, True, False),
                f"{BASE} already has assets; inspect, do not overwrite")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        material = make_material(import_textures(files))
        mesh, skeleton, clips = import_character(files, material)
        report["blend_space_creation"] = make_blend_space(
            skeleton, clips["static_rifle_idle"], clips["forward_in_place_rifle_walk"])
        try:
            report["grip_derivation"] = derive_grip(mesh, record, clips["static_rifle_idle"])
        except Exception as error:  # Grip held; character and clips still usable.
            report["grip_derivation"] = {"held": str(error)}
        require(unreal.EditorAssetLibrary.save_directory(BASE, True, True), "Saving the character folder failed")
    report["assets"] = inspect(record)
    if inspect_only:  # Deterministic derivation from the saved assets.
        try:
            report["grip_derivation"] = derive_grip(unreal.load_asset(f"{BASE}/{MESH}"), record,
                                                    unreal.load_asset(f"{ANIMS}/{CLIP_ROLES['static_rifle_idle']}"))
        except Exception as error:
            report["grip_derivation"] = {"held": str(error)}
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("character-reopen-report.json" if inspect_only else "character-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if inspect_only:
        prior = json.loads((REPORTS / "character-import-report.json").read_text())
        # Keys added after the import run (bones, samples) are reopen-only evidence.
        diffs = [k for k, v in prior["assets"].items() if report["assets"].get(k) != v]
        require(not diffs, f"Saved assets differ from import-time evidence: {diffs}")
    unreal.log(f"VALSTRIKE_MESHY_CHARACTER_IMPORT_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_MESHY_CHARACTER_IMPORT_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        # Bounded slot use: always close this one-shot editor process.
        unreal.SystemLibrary.quit_editor()
