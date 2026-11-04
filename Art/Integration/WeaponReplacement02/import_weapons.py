"""INT-ART-04: import the frozen R02 rifle and P02 pistol as native UE 5.8.4 visual meshes.

Sources (read-only, SHA-256 checked against their own records):
  R02 #89 Art/Source/Weapons/Rifle02/Exports + Textures/GLB (exact GLB PBR bytes)
  P02 #81 Art/Source/Weapons/Pistol02/Models/Exports + Originals maps
Each part is a separate static mesh (no combine, no collision, no Nanite, no LOD
generation, no reduction). Authored normals are imported (FBX tangents are absent;
MikkTSpace tangents are built from those normals). Materials recreate the source
graphs: base colour sRGB, data maps linear, normal strength 1, OpenGL normal maps
green-flipped for UE. The cut/seam interior slot gets the recorded flat colour.

Sockets (body only, centimetres, zero rotation; FBX import negates Y, the
centreline anchors are unchanged):
  R02: the recipe anchors exactly; the magazine is insertion-local and attaches to
       Magazine at identity.
  P02: Grip and Muzzle as recorded. Body, Slide and Magazine share the Grip datum
       and assemble at identity, so the runtime attachment sockets Slide/Magazine
       sit at (0,0,0); the recorded markers (0,0,3.9)/(-2.5,0,-1) are reported,
       not applied a second time.

Third-person grips are measured on the native skeleton (no asset edit), local =
inverse(H) * (C * W * C) with the verified basis C:
  R02 on the frozen ART-15 rifle hold: W = H_src(hold) * R02 hand_r-relative matrix
      (ART-17 measured it on the same source frame and hand orientation).
  P02 on the #103 ART-23 pistol hold: W = ART-23's P02 weapon component matrix.
Support/right-hand bone positions are reported in weapon space against the source
fit so contact differences are visible, not hidden.

Run through Scripts/with_ue_slot.py in a fresh editor; -VSInspectOnly reopens and
compares. Existing assets (including R01/P01) are never overwritten.
"""

import hashlib
import json
import math
from pathlib import Path
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
REPORTS = ROOT / "docs/assets/weapon-replacement02"
CHAR = "/Game/Art/Characters/UrbanEntry01"
RIFLE_HOLD = f"{CHAR}/Animations/A_UE01_Pilot_RifleHold"
PISTOL_HOLD = f"{CHAR}/Animations/A_UE01_Pilot_PistolHold"
UNCHANGED_DIRS = ["Content/Art/Weapons/Rifle01", "Content/Art/Weapons/Pistol01", "Content/Art/Characters/UrbanEntry01"]
R02_SRC = "Art/Source/Weapons/Rifle02"
P02_SRC = "Art/Source/Weapons/Pistol02"

WEAPONS = {
    "R02": {
        "base": "/Game/Art/Weapons/Rifle02",
        "hash_record": "docs/assets/rifle02/export-verification.json",
        "measurements": "docs/assets/rifle02/source-measurements.json",
        "parts": {"Body": f"{R02_SRC}/Exports/SM_VS_Rifle02_Body.fbx",
                  "Magazine": f"{R02_SRC}/Exports/SM_VS_Rifle02_Magazine.fbx"},
        "mesh_names": {"Body": "SM_VS_Rifle02_Body", "Magazine": "SM_VS_Rifle02_Magazine"},
        "textures": {"BaseColor": f"{R02_SRC}/Textures/GLB/T_R02_base_color.jpg",
                     "ORM": f"{R02_SRC}/Textures/GLB/T_R02_orm.jpg",
                     "Normal": f"{R02_SRC}/Textures/GLB/T_R02_normal.jpg"},
        "interior_slot": "M_R02_SeamInterior",
        "interior": {"base_color_linear": (0.035, 0.043, 0.055), "roughness": 0.8, "metallic": 0.0},
        "sockets": {"Grip": (0, 0, 0), "Muzzle": (52, 0, 7.05), "SupportHand": (29, 0, 2.4), "Magazine": (15, 0, -2.5)},
        "source_markers": {},
        "assembled_cm": [75.0, 7.2512, 25.4552],
        "triangles": {"Body": 448486, "Magazine": 11860},
    },
    "P02": {
        "base": "/Game/Art/Weapons/Pistol02",
        "hash_record": "docs/assets/pistol02/source-hashes.json",
        "measurements": "docs/assets/pistol02/source-measurements.json",
        "parts": {"Body": f"{P02_SRC}/Models/Exports/SM_VS_Pistol02_Body.fbx",
                  "Slide": f"{P02_SRC}/Models/Exports/SM_VS_Pistol02_Slide.fbx",
                  "Magazine": f"{P02_SRC}/Models/Exports/SM_VS_Pistol02_Magazine.fbx"},
        "mesh_names": {"Body": "SM_VS_Pistol02_Body", "Slide": "SM_VS_Pistol02_Slide", "Magazine": "SM_VS_Pistol02_Magazine"},
        "textures": {"BaseColor": f"{P02_SRC}/Originals/texture_0_base_color.png",
                     "Roughness": f"{P02_SRC}/Originals/texture_0_roughness.png",
                     "Metallic": f"{P02_SRC}/Originals/texture_0_metallic.png",
                     "Normal": f"{P02_SRC}/Originals/texture_0_normal.png"},
        "interior_slot": "P02_CutInterior",
        "interior": {"base_color_linear": (0.035, 0.045, 0.060), "roughness": 0.7, "metallic": 0.0},
        "sockets": {"Grip": (0, 0, 0), "Muzzle": (15.5, 0, 5.0), "Slide": (0, 0, 0), "Magazine": (0, 0, 0)},
        "source_markers": {"Slide": [0, 0, 3.9], "Magazine": [-2.5, 0, -1.0]},
        "assembled_cm": [21.0, 3.3521, 13.8327],
        "triangles": {"Body": 211980, "Slide": 241968, "Magazine": 7464},
    },
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def xyz(vector, digits=4):
    return [round(getattr(vector, axis), digits) for axis in ("x", "y", "z")]


def close(a, b, tolerance):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


def tree_hashes(folders):
    return {str(p.relative_to(ROOT)): digest(p) for folder in folders
            for p in sorted((ROOT / folder).rglob("*")) if p.is_file()}


def recorded_hash(weapon, rel):
    record = json.loads((ROOT / WEAPONS[weapon]["hash_record"]).read_text())
    if weapon == "R02":
        return record["files"][rel[len(R02_SRC) + 1:]]["sha256"]
    return record[rel]["sha256"]


def check_sources():
    checked = {}
    for weapon, spec in WEAPONS.items():
        for rel in list(spec["parts"].values()) + list(spec["textures"].values()):
            actual = digest(ROOT / rel)
            require(actual == recorded_hash(weapon, rel), f"Source differs from its frozen record: {rel}")
            checked[rel] = actual
    return checked


def asset_paths(weapon):
    spec = WEAPONS[weapon]
    base = spec["base"]
    return {"meshes": {part: f"{base}/Meshes/{name}" for part, name in spec["mesh_names"].items()},
            "material": f"{base}/Materials/M_VS_{weapon}_Original",
            "interior": f"{base}/Materials/M_VS_{weapon}_Interior",
            "textures": {role: f"{base}/Textures/T_VS_{weapon}_{role}" for role in spec["textures"]}}


def run_import(filename, asset_path, options=None, factory=None):
    folder, name = asset_path.rsplit("/", 1)
    task = unreal.AssetImportTask()
    settings = {"filename": str(filename), "destination_path": folder, "destination_name": name,
                "automated": True, "replace_existing": False, "save": False}
    if options is not None:
        settings.update(options=options, factory=factory)
    for key, value in settings.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(asset_path)


def import_texture(rel, asset_path, role):
    texture = run_import(ROOT / rel, asset_path)
    require(isinstance(texture, unreal.Texture2D), f"Texture import failed: {asset_path}")
    texture.set_editor_property("srgb", role == "BaseColor")
    if role == "Normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", True)  # OpenGL source, DirectX engine
    elif role != "BaseColor":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    require(unreal.EditorAssetLibrary.save_loaded_asset(texture, False), f"Save failed: {asset_path}")
    return texture


def new_material(asset_path):
    folder, name = asset_path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    require(material, f"Could not create {asset_path}")
    return material


def finish_material(material, asset_path):
    require(not unreal.MaterialEditingLibrary.recompile_material(material), f"Material compile failed: {asset_path}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False), f"Save failed: {asset_path}")
    return material


def sample(material, texture, sampler, row):
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSample, -480, row * 260)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    return node


def make_original_material(weapon, textures):
    lib, MP, ST = unreal.MaterialEditingLibrary, unreal.MaterialProperty, unreal.MaterialSamplerType
    path = asset_paths(weapon)["material"]
    material = new_material(path)
    base = sample(material, textures["BaseColor"], ST.SAMPLERTYPE_COLOR, 0)
    require(lib.connect_material_property(base, "RGB", MP.MP_BASE_COLOR), "base colour")
    if weapon == "R02":  # packed ORM from the original GLB: G roughness, B metallic, used directly
        orm = sample(material, textures["ORM"], ST.SAMPLERTYPE_MASKS, 1)
        require(lib.connect_material_property(orm, "G", MP.MP_ROUGHNESS), "roughness")
        require(lib.connect_material_property(orm, "B", MP.MP_METALLIC), "metallic")
    else:
        rough = sample(material, textures["Roughness"], ST.SAMPLERTYPE_MASKS, 1)
        metal = sample(material, textures["Metallic"], ST.SAMPLERTYPE_MASKS, 2)
        require(lib.connect_material_property(rough, "R", MP.MP_ROUGHNESS), "roughness")
        require(lib.connect_material_property(metal, "R", MP.MP_METALLIC), "metallic")
    normal = sample(material, textures["Normal"], ST.SAMPLERTYPE_NORMAL, 3)
    require(lib.connect_material_property(normal, "RGB", MP.MP_NORMAL), "normal")
    return finish_material(material, path)


def make_interior_material(weapon):
    lib, MP = unreal.MaterialEditingLibrary, unreal.MaterialProperty
    spec = WEAPONS[weapon]["interior"]
    path = asset_paths(weapon)["interior"]
    material = new_material(path)
    color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -420, 0)
    color.set_editor_property("constant", unreal.LinearColor(*spec["base_color_linear"], 1.0))
    require(lib.connect_material_property(color, "", MP.MP_BASE_COLOR), "interior colour")
    for row, (key, prop) in enumerate((("roughness", MP.MP_ROUGHNESS), ("metallic", MP.MP_METALLIC)), start=1):
        node = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -420, row * 160)
        node.set_editor_property("r", spec[key])
        require(lib.connect_material_property(node, "", prop), f"interior {key}")
    return finish_material(material, path)


def import_mesh(weapon, part, rel, asset_path, original, interior):
    options = unreal.FbxImportUI()
    for key, value in {"import_mesh": True, "import_as_skeletal": False, "import_animations": False,
                       "import_materials": False, "import_textures": False,
                       "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH,
                       "automated_import_should_detect_type": False}.items():
        options.set_editor_property(key, value)
    data = options.get_editor_property("static_mesh_import_data")
    for key, value in {"combine_meshes": True, "auto_generate_collision": False, "build_nanite": False,
                       "generate_lightmap_u_vs": False, "import_uniform_scale": 1.0, "convert_scene": True,
                       "convert_scene_unit": True, "force_front_x_axis": False,
                       "transform_vertex_to_absolute": True, "bake_pivot_in_vertex": False,
                       "remove_degenerates": False,
                       "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():
        data.set_editor_property(key, value)
    mesh = run_import(ROOT / rel, asset_path, options, unreal.FbxFactory())
    require(isinstance(mesh, unreal.StaticMesh), f"Mesh import failed: {asset_path}")
    slots = mesh.get_editor_property("static_materials")
    names = [str(s.get_editor_property("material_slot_name")) for s in slots]
    inner = WEAPONS[weapon]["interior_slot"]
    require(len(names) == 2 and names.count(inner) == 1, f"{asset_path}: unexpected slots {names}")
    for index, name in enumerate(names):
        mesh.set_material(index, interior if name == inner else original)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    for section in range(mesh.get_num_sections(0)):
        subsystem.enable_section_collision(mesh, False, 0, section)
    body = mesh.get_editor_property("body_setup")
    if body:
        instance = body.get_editor_property("default_instance")
        instance.set_editor_property("collision_profile_name", "NoCollision")
        instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
        body.set_editor_property("default_instance", instance)
    if part == "Body":
        for socket_name, location in WEAPONS[weapon]["sockets"].items():
            socket = unreal.StaticMeshSocket(outer=mesh)
            socket.set_editor_property("socket_name", socket_name)
            socket.set_editor_property("relative_location", unreal.Vector(*location))
            socket.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=0))
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
            socket.set_editor_property("tag", f"INT-ART-04 {weapon} visual attachment")
            mesh.add_socket(socket)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), f"Save failed: {asset_path}")


def source_part_bounds(weapon, part):
    record = json.loads((ROOT / WEAPONS[weapon]["measurements"]).read_text())
    name = WEAPONS[weapon]["mesh_names"][part]
    if weapon == "R02":
        b = record["parts"][name]["local_bounds"]
        return b["min"], b["max"]
    b = json.loads((ROOT / "docs/assets/pistol02/export-verification.json").read_text())["parts"][name]["fbx"]["bounds"]
    return b[0], b[1]


def inspect_weapon(weapon):
    spec, p = WEAPONS[weapon], asset_paths(weapon)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    parts, lo, hi = {}, [1e9] * 3, [-1e9] * 3
    for part, path in p["meshes"].items():
        mesh = unreal.load_asset(path)
        require(isinstance(mesh, unreal.StaticMesh), f"Missing {path}")
        box = mesh.get_bounding_box()
        low, high = xyz(box.min), xyz(box.max)
        smin, smax = source_part_bounds(weapon, part)
        expected = ([smin[0], -smax[1], smin[2]], [smax[0], -smin[1], smax[2]])  # FBX import negates Y
        lo = [min(a, b) for a, b in zip(lo, low)]
        hi = [max(a, b) for a, b in zip(hi, high)]
        slots = mesh.get_editor_property("static_materials")
        sockets = {}
        for socket_name in spec["sockets"] if part == "Body" else []:
            socket = mesh.find_socket(socket_name)
            require(socket is not None, f"{path}: missing socket {socket_name}")
            r = socket.get_editor_property("relative_rotation")
            sockets[socket_name] = {"location_cm": xyz(socket.get_editor_property("relative_location")),
                                    "roll_pitch_yaw": [round(r.roll, 4), round(r.pitch, 4), round(r.yaw, 4)]}
        parts[part] = {
            "asset": mesh.get_path_name(), "bounds_min_cm": low, "bounds_max_cm": high,
            "expected_bounds_cm": {"min": [round(v, 4) for v in expected[0]], "max": [round(v, 4) for v in expected[1]]},
            "bounds_match_source": close(low, expected[0], 0.01) and close(high, expected[1], 0.01),
            "lod_count": mesh.get_num_lods(), "lod0_triangles": mesh.get_num_triangles(0),
            "source_triangles": spec["triangles"][part], "lod0_vertices": mesh.get_num_vertices(0), "uv_channels": subsystem.get_num_uv_channels(mesh, 0),
            "nanite_enabled": subsystem.get_nanite_settings(mesh).get_editor_property("enabled"),
            "slots": {str(s.get_editor_property("material_slot_name")): s.get_editor_property("material_interface").get_path_name()
                      for s in slots},
            "collision": {"simple": subsystem.get_simple_collision_count(mesh),
                          "convex": subsystem.get_convex_collision_count(mesh),
                          "sections": [subsystem.is_section_collision_enabled(mesh, 0, i) for i in range(mesh.get_num_sections(0))]},
            "sockets": sockets,
        }
    textures = {}
    for role, path in p["textures"].items():
        t = unreal.load_asset(path)
        require(isinstance(t, unreal.Texture2D), f"Missing {path}")
        textures[role] = {"asset": path, "srgb": t.get_editor_property("srgb"),
                          "compression": str(t.get_editor_property("compression_settings")),
                          "flip_green_channel": t.get_editor_property("flip_green_channel")}
    dims = [round(h - l, 4) for l, h in zip(lo, hi)]
    return {"parts": parts, "textures": textures, "assembled_dimensions_cm": dims,
            "assembled_matches_source": close(dims, spec["assembled_cm"], 0.01),
            "source_markers_not_applied_cm": spec["source_markers"]}


# ---- third-person fit on the native skeleton -------------------------------------

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


def matmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def reflect(m, flip_y):
    c = [1, -1 if flip_y else 1, 1, 1]
    return [[c[i] * m[i][j] * c[j] for j in range(4)] for i in range(4)]


def component_pose(mesh, anim, bone, frame=0):
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    chain, current = [], unreal.Name(bone)
    while str(current) != "None":
        chain.append(current)
        current = probe.get_parent_bone(current)
    pose = unreal.Transform()
    for name in chain:
        pose = unreal.MathLibrary.compose_transforms(pose, unreal.AnimationLibrary.get_bone_pose_for_frame(anim, name, frame, False))
    return pose


def fit(mesh, anim, source_hand, source_weapon, landmarks_source):
    """local = inverse(H) * (C W C); landmarks are source weapon-space references."""
    hand = component_pose(mesh, anim, "hand_r")
    native, src = xyz(hand.translation), [row[3] for row in source_hand[:3]]
    basis = None
    for flip_y in (True, False):
        if close(native, [src[0], -src[1] if flip_y else src[1], src[2]], 0.5):
            basis = flip_y
            break
    require(basis is not None, f"Native hand_r {native} matches neither basis of source {src}")
    weapon = matrix_to_transform(reflect(source_weapon, basis))
    local = unreal.MathLibrary.compose_transforms(weapon, unreal.MathLibrary.invert_transform(hand))
    rot, quat = local.rotation.rotator(), local.rotation
    contacts = {}
    for bone, ref in landmarks_source.items():
        actual = xyz(unreal.MathLibrary.inverse_transform_location(weapon, component_pose(mesh, anim, bone).translation), 3)
        expected = [ref[0], -ref[1] if basis else ref[1], ref[2]]
        contacts[bone] = {"native_weapon_cm": actual, "source_fit_weapon_cm": [round(v, 3) for v in expected],
                          "delta_cm": round(math.dist(actual, expected), 3)}
    return {"native_hand_r_cm": native, "source_hand_r_cm": [round(v, 4) for v in src],
            "basis": "y_negated" if basis else "identity",
            "weapon_component_cm": xyz(weapon.translation),
            "weapon_component_roll_pitch_yaw": [round(v, 4) for v in (weapon.rotation.rotator().roll,
                                                weapon.rotation.rotator().pitch, weapon.rotation.rotator().yaw)],
            "hand_r_offset_location_cm": xyz(local.translation),
            "hand_r_offset_roll_pitch_yaw": [round(rot.roll, 4), round(rot.pitch, 4), round(rot.yaw, 4)],
            "hand_r_offset_quat_xyzw": [round(quat.x, 6), round(quat.y, 6), round(quat.z, 6), round(quat.w, 6)],
            "bone_positions_in_weapon_space": contacts}


def derive_grips():
    mesh = unreal.load_asset(f"{CHAR}/SK_VS_UrbanEntry01")
    rifle_hold, pistol_hold = unreal.load_asset(RIFLE_HOLD), unreal.load_asset(PISTOL_HOLD)
    require(isinstance(mesh, unreal.SkeletalMesh) and rifle_hold and pistol_hold, "UrbanEntry mesh/holds missing")
    hold = json.loads((ROOT / "docs/assets/urban-entry01/pilot-readiness.json").read_text())["idle_and_weapon_socket"]
    r02 = json.loads((ROOT / "docs/assets/rifle02/character-fit.json").read_text())
    h_src = hold["hold_hand_r_component_matrix"]
    rel = r02["hand_r_bone_relative_matrix_rows"]
    # ART-17 fitted R02 on the unmodified source frame; ART-15's hold keeps that hand
    # orientation (wrists only translated), so the same hand-relative matrix applies.
    hand_rot_src = [row[:3] for row in h_src[:3]]
    rel_rot_t = [[rel[j][i] for j in range(3)] for i in range(3)]
    orientation_error = max(abs(hand_rot_src[i][j] - rel_rot_t[i][j]) for i in range(3) for j in range(3))
    w_r02 = matmul(h_src, rel)
    loc = r02["weapon_world_location_cm"]
    r02_refs = {b: [v - o for v, o in zip(r02["landmarks"][b]["world_cm"], loc)]
                for b in ("hand_l", "middle_01_l", "hand_r", "middle_01_r")}
    rifle = fit(mesh, rifle_hold, h_src, w_r02, r02_refs)
    rifle["hand_orientation_vs_r02_fit_max_abs"] = round(orientation_error, 6)
    rifle["r01_support_reference_r01_space_cm"] = hold["support_hand_reference_in_rifle_cm"]["hand_l"]
    p02 = json.loads((ROOT / "docs/assets/urban-entry01/pistol-pilot/source-validation.json").read_text())["weapons"]["P02"]
    pistol = fit(mesh, pistol_hold, p02["hand_r_component_matrix"], p02["weapon_component_matrix"],
                 {b: p02["bone_references_weapon_cm"][b] for b in ("hand_r", "hand_l", "index_01_r")})
    return {"R02_on_rifle_hold": rifle, "P02_on_pistol_hold": pistol}


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-04 worktree")
    sources = check_sources()
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    before = tree_hashes(UNCHANGED_DIRS)
    if not inspect_only:
        existing = [x for w in WEAPONS for x in [*asset_paths(w)["meshes"].values(), asset_paths(w)["material"],
                    asset_paths(w)["interior"], *asset_paths(w)["textures"].values()]
                    if unreal.EditorAssetLibrary.does_asset_exist(x)]
        require(not existing, f"Weapon batch exists or is partial; inspect, do not overwrite: {existing}")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        for weapon, spec in WEAPONS.items():
            p = asset_paths(weapon)
            textures = {role: import_texture(rel, p["textures"][role], role) for role, rel in spec["textures"].items()}
            original, interior = make_original_material(weapon, textures), make_interior_material(weapon)
            for part, rel in spec["parts"].items():
                import_mesh(weapon, part, rel, p["meshes"][part], original, interior)
    report = {"engine": unreal.SystemLibrary.get_engine_version(),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "source_sha256": sources, "weapons": {w: inspect_weapon(w) for w in WEAPONS},
              "grips": derive_grips()}
    after = tree_hashes(UNCHANGED_DIRS)
    report["unchanged_r01_p01_character_assets"] = before == after
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("weapons-reopen-report.json" if inspect_only else "weapons-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    require(report["unchanged_r01_p01_character_assets"], "R01/P01/character assets changed")
    for weapon, item in report["weapons"].items():
        require(item["assembled_matches_source"], f"{weapon}: assembled size {item['assembled_dimensions_cm']}")
        for part, data in item["parts"].items():
            require(data["bounds_match_source"], f"{weapon} {part}: bounds differ from source")
            c = data["collision"]
            require(c["simple"] == c["convex"] == 0 and not any(c["sections"]), f"{weapon} {part}: collision present")
            require(not data["nanite_enabled"] and data["lod_count"] == 1, f"{weapon} {part}: unexpected Nanite/LOD")
            require(data["lod0_triangles"] == data["source_triangles"], f"{weapon} {part}: triangle count changed")
    if inspect_only:
        prior = json.loads((REPORTS / "weapons-import-report.json").read_text())
        require(report["weapons"] == prior["weapons"] and report["grips"] == prior["grips"], "Reopen differs from import")
    unreal.log(f"VALSTRIKE_WEAPON_REPLACEMENT_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_WEAPON_REPLACEMENT_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        unreal.SystemLibrary.quit_editor()
