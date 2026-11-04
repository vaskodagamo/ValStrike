"""INT-ART-03: import K01, the ART-26-C1 first-person knife hands and ART-27 knife motion.

Exact allowlisted inputs (SHA-256 checked against each source's own frozen record):
  K01 #85        Art/Source/Weapons/Knife01/Exports/SM_VS_Knife01.fbx + Textures/Embedded
  ART-26-C1 #115 Art/Source/Characters/FPArms03/KnifePilot/Exports/*.fbx + ../Textures/Embedded
  ART-27 #110    Art/Source/Characters/UrbanEntry01/KnifePilot/Exports/A_UE01_Knife_*.fbx
Geometry, UVs, normals and PBR are preserved: no reduction, collision, Nanite or LODs.
Materials recreate the GLB graphs (base colour sRGB; packed G roughness / B metallic;
OpenGL normal maps green-flipped for UE). Clips: 60 fps ranges, root motion off, the
artists' Impact markers recreated as sync markers at the contract times.

Measured attachments (no asset edit), C = verified source-to-native basis (bone
translations must match under C; bone-axis agreement is reported, not assumed):
  first person: K01 on weapon_socket_r, local = inverse(H_U) * C (H_B * L) C, checked
    constant over every recorded frame (source poses: read_source_poses.py output);
  third person: K01 on hand_r, local = inverse(H_U) * C W_K C on Knife_Hold frame 1.
Also measures the in-place walk/run stance-foot ground speed for cadence matching.

Run through Scripts/with_ue_slot.py in a fresh editor; -VSInspectOnly reopens.
Existing assets are never overwritten; UrbanEntry/R01/P01/R02/P02 stay byte-identical.
"""

import hashlib
import json
import math
from pathlib import Path
import statistics
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
REPORTS = ROOT / "docs/assets/knife-integration"
K01_SRC = "Art/Source/Weapons/Knife01"
FP_SRC = "Art/Source/Characters/FPArms03"
TP_SRC = "Art/Source/Characters/UrbanEntry01/KnifePilot/Exports"
KNIFE = "/Game/Art/Weapons/Knife01"
ARMS = "/Game/Art/Characters/FPArms03"
UE01 = "/Game/Art/Characters/UrbanEntry01"
TP_ANIMS = f"{UE01}/Animations/Knife"
FP_CLIPS = ("Equip", "Idle", "Locomotion", "Slash", "Stab")
TP_CLIPS = ("Hold", "Equip", "Walk", "Run", "Slash", "Stab")
UNCHANGED_DIRS = ["Content/Art/Characters/UrbanEntry01", "Content/Art/Weapons/Rifle01", "Content/Art/Weapons/Pistol01",
                  "Content/Art/Weapons/Rifle02", "Content/Art/Weapons/Pistol02"]
SOCKETS = {"Grip": (0, 0, 0)}  # Tip added from the measured source frame (Y negated).


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def xyz(v, digits=4):
    return [round(getattr(v, a), digits) for a in ("x", "y", "z")]


def close(a, b, tol):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def tree_hashes(folders):
    return {str(p.relative_to(ROOT)): digest(p) for f in folders for p in sorted((ROOT / f).rglob("*")) if p.is_file()}


# ---- sources ----------------------------------------------------------------------

def records():
    k01 = {e["path"]: e["sha256"] for e in json.loads((ROOT / "docs/assets/knife01/manifest.json").read_text())["files"]}
    fp = {e["path"]: e["sha256"] for e in json.loads((ROOT / "docs/assets/fparms03/knife-pilot/c1-source-files.json").read_text())["files"]}
    fp.update({e["path"]: e["sha256"] for e in json.loads((ROOT / "docs/assets/fparms03/source-files.json").read_text())})
    tp = {e["path"]: e["sha256"] for e in json.loads((ROOT / "docs/assets/urban-entry01/knife-pilot/artifact-manifest.json").read_text())["files"]}
    return {**k01, **fp, **tp}


def sources():
    s = {"k01_fbx": f"{K01_SRC}/Exports/SM_VS_Knife01.fbx",
         "k01_tex": {r: f"{K01_SRC}/Textures/Embedded/Image_{i}.jpg" for i, r in enumerate(("BaseColor", "ORM", "Normal"))},
         "arms_fbx": f"{FP_SRC}/KnifePilot/Exports/SK_FPArms03_KnifePilot.fbx",
         "arms_tex": {r: f"{FP_SRC}/Textures/Embedded/Image_{i}.jpg" for i, r in enumerate(("BaseColor", "ORM", "Normal"))},
         "fp_clips": {c: f"{FP_SRC}/KnifePilot/Exports/A_FPArms03_Knife{c}.fbx" for c in FP_CLIPS},
         "tp_clips": {c: f"{TP_SRC}/A_UE01_Knife_{c}.fbx" for c in TP_CLIPS}}
    known = records()
    flat = [s["k01_fbx"], s["arms_fbx"], *s["k01_tex"].values(), *s["arms_tex"].values(), *s["fp_clips"].values(), *s["tp_clips"].values()]
    checked = {}
    for rel in flat:
        require(rel in known, f"Not in any frozen record (not allowlisted): {rel}")
        actual = digest(ROOT / rel)
        require(actual == known[rel], f"Source differs from its frozen record: {rel}")
        checked[rel] = actual
    return s, checked


# ---- import helpers ---------------------------------------------------------------

def run_import(filename, asset_path, options=None, factory=None):
    folder, name = asset_path.rsplit("/", 1)
    task = unreal.AssetImportTask()
    settings = {"filename": str(ROOT / filename), "destination_path": folder, "destination_name": name,
                "automated": True, "replace_existing": False, "save": False}
    if options is not None:
        settings.update(options=options, factory=factory)
    for k, v in settings.items():
        task.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(asset_path)


def save(asset, label):
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, False), f"Save failed: {label}")


def import_textures(files, base, tag):
    out = {}
    for role, rel in files.items():
        path = f"{base}/Textures/T_VS_{tag}_{role}"
        t = run_import(rel, path)
        require(isinstance(t, unreal.Texture2D), f"Texture import failed: {path}")
        t.set_editor_property("srgb", role == "BaseColor")
        if role == "Normal":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("flip_green_channel", True)
        elif role == "ORM":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        save(t, path)
        out[role] = t
    return out


def make_material(path, textures, two_sided):
    lib, MP, ST = unreal.MaterialEditingLibrary, unreal.MaterialProperty, unreal.MaterialSamplerType
    folder, name = path.rsplit("/", 1)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    require(m, f"Could not create {path}")
    m.set_editor_property("two_sided", two_sided)

    def sample(role, sampler, row):
        node = lib.create_material_expression(m, unreal.MaterialExpressionTextureSample, -480, row * 260)
        node.set_editor_property("texture", textures[role])
        node.set_editor_property("sampler_type", sampler)
        return node

    base, orm, normal = sample("BaseColor", ST.SAMPLERTYPE_COLOR, 0), sample("ORM", ST.SAMPLERTYPE_MASKS, 1), sample("Normal", ST.SAMPLERTYPE_NORMAL, 2)
    for node, out, prop in ((base, "RGB", MP.MP_BASE_COLOR), (orm, "G", MP.MP_ROUGHNESS), (orm, "B", MP.MP_METALLIC), (normal, "RGB", MP.MP_NORMAL)):
        require(lib.connect_material_property(node, out, prop), f"{path}: {prop}")
    require(not lib.recompile_material(m), f"Material compile failed: {path}")
    save(m, path)
    return m


def import_k01(src, material):
    options = unreal.FbxImportUI()
    for k, v in {"import_mesh": True, "import_as_skeletal": False, "import_animations": False, "import_materials": False,
                 "import_textures": False, "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH,
                 "automated_import_should_detect_type": False}.items():
        options.set_editor_property(k, v)
    data = options.get_editor_property("static_mesh_import_data")
    for k, v in {"combine_meshes": True, "auto_generate_collision": False, "build_nanite": False, "generate_lightmap_u_vs": False,
                 "import_uniform_scale": 1.0, "convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                 "transform_vertex_to_absolute": True, "bake_pivot_in_vertex": False, "remove_degenerates": False,
                 "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():
        data.set_editor_property(k, v)
    path = f"{KNIFE}/Meshes/SM_VS_Knife01"
    mesh = run_import(src["k01_fbx"], path, options, unreal.FbxFactory())
    require(isinstance(mesh, unreal.StaticMesh), "K01 import failed")
    require(len(mesh.get_editor_property("static_materials")) == 1, "K01: expected one material slot")
    mesh.set_material(0, material)
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    sub.remove_collisions(mesh)
    for section in range(mesh.get_num_sections(0)):
        sub.enable_section_collision(mesh, False, 0, section)
    tip = json.loads((ROOT / "docs/assets/knife01/source-inspection.json").read_text())
    tip = _find(tip, "Tip")["location_cm"]
    for name, loc in {**SOCKETS, "Tip": (tip[0], -tip[1], tip[2])}.items():
        socket = unreal.StaticMeshSocket(outer=mesh)
        socket.set_editor_property("socket_name", name)
        socket.set_editor_property("relative_location", unreal.Vector(*loc))
        socket.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=0))
        socket.set_editor_property("tag", "INT-ART-03 K01 visual landmark")
        mesh.add_socket(socket)
    save(mesh, path)


def _find(node, key):
    if isinstance(node, dict):
        if key in node and isinstance(node[key], dict) and "location_cm" in node[key]:
            return node[key]
        for v in node.values():
            r = _find(v, key)
            if r:
                return r
    elif isinstance(node, list):
        for v in node:
            r = _find(v, key)
            if r:
                return r
    return None


def skeletal_options(skeleton=None):
    options = unreal.FbxImportUI()
    anim = skeleton is not None
    for k, v in {"import_mesh": not anim, "import_as_skeletal": True, "import_animations": anim, "import_materials": False,
                 "import_textures": False, "create_physics_asset": False, "automated_import_should_detect_type": False,
                 "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION if anim else unreal.FBXImportType.FBXIT_SKELETAL_MESH}.items():
        options.set_editor_property(k, v)
    if anim:
        options.set_editor_property("skeleton", skeleton)
        data = options.get_editor_property("anim_sequence_import_data")
        for k, v in {"convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                     "import_meshes_in_bone_hierarchy": False,
                     "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME}.items():
            data.set_editor_property(k, v)
    else:
        data = options.get_editor_property("skeletal_mesh_import_data")
        for k, v in {"convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                     "import_morph_targets": False, "import_meshes_in_bone_hierarchy": False,
                     "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():
            data.set_editor_property(k, v)
    return options


def import_clip(rel, path, skeleton, impact_s):
    anim = run_import(rel, path, skeletal_options(skeleton), unreal.FbxFactory())
    require(isinstance(anim, unreal.AnimSequence), f"Animation import failed: {path}")
    anim.set_editor_property("enable_root_motion", False)
    if impact_s is not None:
        unreal.AnimationLibrary.add_animation_notify_track(anim, "Impact")
        unreal.AnimationLibrary.add_animation_sync_marker(anim, "Impact", impact_s, "Impact")
    save(anim, path)
    return anim


def import_arms(src, material, contract):
    path = f"{ARMS}/SK_FPArms03_KnifePilot"
    mesh = run_import(src["arms_fbx"], path, skeletal_options(), unreal.FbxFactory())
    require(isinstance(mesh, unreal.SkeletalMesh), "FPArms03 import failed")
    materials = mesh.get_editor_property("materials")
    require(len(materials) >= 1, "FPArms03: no material slots")
    for index in range(len(materials)):
        slot = materials[index]  # UE 5.8 returns a struct copy; write it back.
        slot.set_editor_property("material_interface", material)
        materials[index] = slot
    mesh.set_editor_property("materials", materials)
    applied = [(str(m.get_editor_property("material_slot_name")), m.get_editor_property("material_interface"))
               for m in mesh.get_editor_property("materials")]
    require(all(i is not None and i.get_path_name() == material.get_path_name() for _, i in applied),
            f"FPArms03: material assignment did not apply: {applied}")
    save(mesh, path)
    skeleton = mesh.get_editor_property("skeleton")
    require(skeleton, "FPArms03 has no skeleton")
    save(skeleton, "FPArms03 skeleton")
    for clip in FP_CLIPS:
        import_clip(src["fp_clips"][clip], f"{ARMS}/Animations/A_FPArms03_Knife{clip}", skeleton,
                    contract["clips"][clip].get("impact_seconds"))


def import_tp(src, contract):
    mesh = unreal.load_asset(f"{UE01}/SK_VS_UrbanEntry01")
    require(isinstance(mesh, unreal.SkeletalMesh), "UrbanEntry mesh missing")
    skeleton = mesh.get_editor_property("skeleton")
    for clip in TP_CLIPS:
        import_clip(src["tp_clips"][clip], f"{TP_ANIMS}/A_UE01_Knife_{clip}", skeleton,
                    contract["clips"][f"A_UE01_Knife_{clip}"]["impact_seconds"])


# ---- measurement ------------------------------------------------------------------

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
    return unreal.Transform(unreal.Vector(m[0][3], m[1][3], m[2][3]), unreal.Quat(x, y, z, w).rotator(), unreal.Vector(1, 1, 1))


def reflect(m):
    c = [1, -1, 1, 1]
    return [[c[i] * m[i][j] * c[j] for j in range(4)] for i in range(4)]


def component_pose(mesh, anim, bone, frame):
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    chain, current = [], unreal.Name(bone)
    while str(current) != "None":
        chain.append(current)
        current = probe.get_parent_bone(current)
    require(len(chain) > 0 and str(chain[0]) == bone, f"Bone missing: {bone}")
    pose = unreal.Transform()
    for name in chain:
        pose = unreal.MathLibrary.compose_transforms(pose, unreal.AnimationLibrary.get_bone_pose_for_frame(anim, name, frame, False))
    return pose


def axes(t):
    return [xyz(t.rotation.get_axis_x(), 5), xyz(t.rotation.get_axis_y(), 5), xyz(t.rotation.get_axis_z(), 5)]


def source_axes(m):
    m = reflect(m)
    return [[m[r][c] for r in range(3)] for c in range(3)]  # columns = local X/Y/Z in component space


def axis_error(t, m):
    return max(abs(a - b) for u, v in zip(axes(t), source_axes(m)) for a, b in zip(u, v))


def offset_report(local):
    q, r = local.rotation, local.rotation.rotator()
    return {"location_cm": xyz(local.translation), "quat_xyzw": [round(q.x, 6), round(q.y, 6), round(q.z, 6), round(q.w, 6)],
            "roll_pitch_yaw": [round(r.roll, 4), round(r.pitch, 4), round(r.yaw, 4)]}


def measure_fp():
    poses = json.loads((REPORTS / "source-poses.json").read_text())
    mesh = unreal.load_asset(f"{ARMS}/SK_FPArms03_KnifePilot")
    frames, offsets = {}, []
    for clip, by_frame in poses["clips"].items():
        anim = unreal.load_asset(f"{ARMS}/Animations/A_FPArms03_Knife{clip}")
        for frame, src in by_frame.items():
            h = component_pose(mesh, anim, "weapon_socket_r", int(frame) - 1)
            hb = src["weapon_socket_r"]
            knife = matrix_to_transform(reflect(src["K01_component"]))
            local = unreal.MathLibrary.compose_transforms(knife, unreal.MathLibrary.invert_transform(h))
            offsets.append(local)
            frames[f"{clip}@{frame}"] = {
                "native_socket_cm": xyz(h.translation),
                "socket_translation_error_cm": round(math.dist(xyz(h.translation), [hb[0][3], -hb[1][3], hb[2][3]]), 4),
                "socket_axis_error": round(axis_error(h, hb), 5),
                "knife_component_cm": xyz(knife.translation), "offset": offset_report(local)}
    first = offsets[0]
    spread = max(max(math.dist(xyz(o.translation), xyz(first.translation)) for o in offsets),
                 max(1 - abs(sum(getattr(o.rotation, a) * getattr(first.rotation, a) for a in "xyzw")) for o in offsets))
    contract = json.loads((ROOT / "docs/assets/fparms03/knife-pilot/animation-contract.json").read_text())["camera"]
    cam = contract["position_cm"]
    arms_from_camera = [-cam[0], cam[1], -cam[2]]  # Y = 0 either way
    return {"basis": "y_negated", "frames": frames, "offset_on_weapon_socket_r": offset_report(first),
            "offset_max_spread": round(spread, 6), "arms_component_from_camera_cm": arms_from_camera,
            "camera_horizontal_fov": contract["horizontal_fov_degrees"]}


def measure_tp():
    sock = json.loads((ROOT / "docs/assets/urban-entry01/knife-pilot/animation-contract.json").read_text())["socket"]
    mesh = unreal.load_asset(f"{UE01}/SK_VS_UrbanEntry01")
    hold = unreal.load_asset(f"{TP_ANIMS}/A_UE01_Knife_Hold")
    h = component_pose(mesh, hold, "hand_r", 0)
    hb = sock["hold_hand_component_matrix"]
    knife = matrix_to_transform(reflect(sock["hold_knife_component_matrix"]))
    local = unreal.MathLibrary.compose_transforms(knife, unreal.MathLibrary.invert_transform(h))
    slash, stab = unreal.load_asset(f"{TP_ANIMS}/A_UE01_Knife_Slash"), unreal.load_asset(f"{TP_ANIMS}/A_UE01_Knife_Stab")
    tip = unreal.load_asset(f"{KNIFE}/Meshes/SM_VS_Knife01").find_socket("Tip").get_editor_property("relative_location")

    def tip_at(anim, frame):
        world = unreal.MathLibrary.compose_transforms(local, component_pose(mesh, anim, "hand_r", frame))
        return xyz(unreal.MathLibrary.transform_location(world, tip), 2)

    return {"basis": "y_negated", "native_hand_r_cm": xyz(h.translation),
            "hand_translation_error_cm": round(math.dist(xyz(h.translation), [hb[0][3], -hb[1][3], hb[2][3]]), 4),
            "hand_axis_error": round(axis_error(h, hb), 5), "knife_component_cm": xyz(knife.translation),
            "offset_on_hand_r": offset_report(local),
            "k01_tip_component_cm": {"hold": tip_at(hold, 0), "slash_impact_f10": tip_at(slash, 9), "stab_impact_f19": tip_at(stab, 18)}}


def stance_speed(mesh, anim):
    """Median backward speed (cm/s) of the lower foot of an in-place gait clip."""
    n = unreal.AnimationLibrary.get_num_frames(anim)
    feet = {f: [component_pose(mesh, anim, f, i).translation for i in range(n + 1)] for f in ("foot_l", "foot_r")}
    speeds = []
    for i in range(n):
        lower = min(feet, key=lambda f: feet[f][i].z)
        speeds.append(-(feet[lower][i + 1].x - feet[lower][i].x) * 60.0)
    return round(statistics.median(speeds), 1)


def inspect_assets():
    k01 = unreal.load_asset(f"{KNIFE}/Meshes/SM_VS_Knife01")
    sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    box = k01.get_bounding_box()
    arms = unreal.load_asset(f"{ARMS}/SK_FPArms03_KnifePilot")
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(arms)
    clips = {}
    for clip in FP_CLIPS:
        a = unreal.load_asset(f"{ARMS}/Animations/A_FPArms03_Knife{clip}")
        clips[f"FP_{clip}"] = {"length_s": round(a.get_play_length(), 4), "frames": unreal.AnimationLibrary.get_num_frames(a),
                               "root_motion": a.get_editor_property("enable_root_motion"),
                               "sync_markers": [round(m.get_editor_property("time"), 4) for m in unreal.AnimationLibrary.get_animation_sync_markers(a)]}
    ue01 = unreal.load_asset(f"{UE01}/SK_VS_UrbanEntry01")
    for clip in TP_CLIPS:
        a = unreal.load_asset(f"{TP_ANIMS}/A_UE01_Knife_{clip}")
        clips[f"TP_{clip}"] = {"length_s": round(a.get_play_length(), 4), "frames": unreal.AnimationLibrary.get_num_frames(a),
                               "root_motion": a.get_editor_property("enable_root_motion"),
                               "skeleton": a.get_editor_property("skeleton").get_path_name(),
                               "sync_markers": [round(m.get_editor_property("time"), 4) for m in unreal.AnimationLibrary.get_animation_sync_markers(a)]}
    textures = {}
    for base, tag in ((KNIFE, "K01"), (ARMS, "FPArms03")):
        for role in ("BaseColor", "ORM", "Normal"):
            t = unreal.load_asset(f"{base}/Textures/T_VS_{tag}_{role}")
            textures[f"{tag}_{role}"] = {"srgb": t.get_editor_property("srgb"), "compression": str(t.get_editor_property("compression_settings")),
                                         "flip_green": t.get_editor_property("flip_green_channel")}
    abox = arms.get_bounds()
    return {
        "k01": {"asset": k01.get_path_name(), "bounds_min_cm": xyz(box.min), "bounds_max_cm": xyz(box.max),
                "triangles": k01.get_num_triangles(0), "lods": k01.get_num_lods(),
                "nanite": sub.get_nanite_settings(k01).get_editor_property("enabled"),
                "collision": [sub.get_simple_collision_count(k01), sub.get_convex_collision_count(k01)],
                "sockets": {s: xyz(k01.find_socket(s).get_editor_property("relative_location")) for s in ("Grip", "Tip")},
                "material": k01.get_material(0).get_path_name()},
        "arms": {"asset": arms.get_path_name(), "bones": probe.get_num_bones(),
                 "has_weapon_socket_r": probe.get_bone_index("weapon_socket_r") >= 0,
                 "bounds_origin_cm": xyz(abox.origin), "bounds_extent_cm": xyz(abox.box_extent),
                 "material_slots": [m.get_editor_property("material_interface").get_path_name() for m in arms.get_editor_property("materials")],
                 "skeleton": arms.get_editor_property("skeleton").get_path_name()},
        "clips": clips, "textures": textures,
        "gait_stance_speed_cm_s": {
            "A_UE01_Knife_Walk": stance_speed(ue01, unreal.load_asset(f"{TP_ANIMS}/A_UE01_Knife_Walk")),
            "A_UE01_Knife_Run": stance_speed(ue01, unreal.load_asset(f"{TP_ANIMS}/A_UE01_Knife_Run")),
            "A_UE01_Pilot_Walk_RifleHold (existing, 145 in C++)": stance_speed(ue01, unreal.load_asset(f"{UE01}/Animations/A_UE01_Pilot_Walk_RifleHold"))}}


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-03 worktree")
    src, checked = sources()
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    before = tree_hashes(UNCHANGED_DIRS)
    fp_contract = json.loads((ROOT / "docs/assets/fparms03/knife-pilot/animation-contract.json").read_text())
    tp_contract = json.loads((ROOT / "docs/assets/urban-entry01/knife-pilot/animation-contract.json").read_text())
    if not inspect_only:
        for folder in (KNIFE, ARMS, TP_ANIMS):
            require(not unreal.EditorAssetLibrary.does_directory_have_assets(folder), f"{folder} exists; inspect, do not overwrite")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        import_k01(src, make_material(f"{KNIFE}/Materials/M_VS_K01", import_textures(src["k01_tex"], KNIFE, "K01"), False))
        import_arms(src, make_material(f"{ARMS}/Materials/M_VS_FPArms03", import_textures(src["arms_tex"], ARMS, "FPArms03"), True), fp_contract)
        import_tp(src, tp_contract)
    report = {"engine": unreal.SystemLibrary.get_engine_version(), "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "source_sha256": checked, "assets": inspect_assets(), "first_person": measure_fp(), "third_person": measure_tp()}
    report["unchanged_existing_assets"] = {p: digest(ROOT / p) for p in before} == before  # pre-existing files only
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("knife-reopen-report.json" if inspect_only else "knife-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    a, fp, tp = report["assets"], report["first_person"], report["third_person"]
    require(report["unchanged_existing_assets"], "Existing character/weapon assets changed")
    require(a["k01"]["triangles"] == 70640 and a["k01"]["collision"] == [0, 0] and not a["k01"]["nanite"], "K01 import contract")
    require(close([h - l for l, h in zip(a["k01"]["bounds_min_cm"], a["k01"]["bounds_max_cm"])], [27.8, 2.3992, 4.6513], 0.01), "K01 size")
    # 39 source bones + the armature node, which the FBX importer adds as the root bone.
    require(a["arms"]["bones"] == 40 and a["arms"]["has_weapon_socket_r"], "FPArms03 rig")
    for name, c in a["clips"].items():
        require(not c["root_motion"], f"{name}: root motion on")
    for clip, spec in fp_contract["clips"].items():
        require(abs(a["clips"][f"FP_{clip}"]["length_s"] - spec["duration"]) < 0.002, f"FP {clip} length")
    for clip in TP_CLIPS:
        require(abs(a["clips"][f"TP_{clip}"]["length_s"] - tp_contract["clips"][f"A_UE01_Knife_{clip}"]["duration_seconds"]) < 0.002, f"TP {clip} length")
    require(max(f["socket_translation_error_cm"] for f in fp["frames"].values()) < 0.05, "FP socket basis mismatch")
    require(fp["offset_max_spread"] < 0.01, "FP knife offset is not constant across clips")
    require(tp["hand_translation_error_cm"] < 0.05, "TP hand basis mismatch")
    if inspect_only:
        prior = json.loads((REPORTS / "knife-import-report.json").read_text())
        for key in ("assets", "first_person", "third_person"):
            require(report[key] == prior[key], f"Reopen differs: {key}")
    unreal.log(f"VALSTRIKE_KNIFE_IMPORT_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_KNIFE_IMPORT_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        unreal.SystemLibrary.quit_editor()
