"""ART-02: author and verify the first visual rifle assets in Unreal 5.8.4.

This creates visual static meshes and editor sockets, not weapon gameplay.
Existing native assets are never overwritten. Use -VSInspectOnly to reopen.
"""

import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / "Art/Source/Weapons/Rifle01/Models"
BASE = "/Game/Art/Weapons/Rifle01"
REPORTS = ROOT / "docs/assets/rifle01"
BODY = "SM_VS_Rifle01_Body"
MAGAZINE = "SM_VS_Rifle01_Magazine"
NAMES = (BODY, MAGAZINE)
SHARED_PALETTE = Path(__file__).resolve().parents[1] / "Shared/weapon_palette.json"
_FINISH = json.loads(SHARED_PALETTE.read_text())
_ROLES = _FINISH["variants"][_FINISH["default"]]["roles"]
PALETTE = {name: (_ROLES[role]["srgb_hex"], _ROLES[role]["roughness"], _ROLES[role]["metallic"])
           for name, role in _FINISH["rifle_material_roles"].items()}

SOCKETS = {
    "Muzzle": (56, 0, 8.2),
    "Grip": (0, 0, 0),
    "SupportHand": (31, 0, 2.4),
    "Magazine": (14, 0, 2),
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def vector_values(vector):
    return [round(getattr(vector, axis), 5) for axis in ("x", "y", "z")]


def close(actual, expected, tolerance=.05):
    return len(actual) == len(expected) and all(abs(a - b) < tolerance for a, b in zip(actual, expected))


def make_material(name, hex_color, roughness, metallic):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, f"{BASE}/Materials", unreal.Material, unreal.MaterialFactoryNew())
    require(material, f"Could not create {name}")
    lib = unreal.MaterialEditingLibrary
    rgb = [int(hex_color[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    linear = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in rgb]
    color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -260, 0)
    color.set_editor_property("constant", unreal.LinearColor(*linear, 1))
    require(lib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR), name)
    for row, (value, prop) in enumerate(((roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                                        (metallic, unreal.MaterialProperty.MP_METALLIC)), 1):
        node = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -260, row * 100)
        node.set_editor_property("r", value)
        require(lib.connect_material_property(node, "", prop), name)
    require(not lib.recompile_material(material), f"Material compile failed: {name}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False), f"Material save failed: {name}")
    return material


def import_mesh(name, materials):
    options = unreal.FbxImportUI()
    for key, value in {
        "import_mesh": True, "import_as_skeletal": False, "import_animations": False,
        "import_materials": False, "import_textures": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH,
        "automated_import_should_detect_type": False,
    }.items():
        options.set_editor_property(key, value)
    data = options.get_editor_property("static_mesh_import_data")
    for key, value in {
        "combine_meshes": True, "auto_generate_collision": False,
        "build_nanite": False, "generate_lightmap_u_vs": False,
        "import_uniform_scale": 1.0, "convert_scene": True,
        "convert_scene_unit": True, "force_front_x_axis": False,
        "transform_vertex_to_absolute": True, "bake_pivot_in_vertex": False,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
    }.items():
        data.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    for key, value in {
        "filename": str(SOURCE / "Exports" / f"{name}.fbx"),
        "destination_path": f"{BASE}/Meshes", "destination_name": name,
        "automated": True, "replace_existing": False, "save": False,
        "factory": unreal.FbxFactory(), "options": options,
    }.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(f"{BASE}/Meshes/{name}")
    require(isinstance(mesh, unreal.StaticMesh), f"Mesh import failed: {name}")
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        require(slot_name in materials, f"Unknown material: {slot_name}")
        mesh.set_material(index, materials[slot_name])
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    for section in range(mesh.get_num_sections(0)):
        subsystem.enable_section_collision(mesh, False, 0, section)
    body_setup = mesh.get_editor_property("body_setup")
    instance = body_setup.get_editor_property("default_instance")
    instance.set_editor_property("collision_profile_name", "NoCollision")
    instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    body_setup.set_editor_property("default_instance", instance)
    mesh.set_editor_property("light_map_coordinate_index", 1)
    mesh.set_editor_property("light_map_resolution", 64)
    if name == BODY:
        for socket_name, location in SOCKETS.items():
            socket = unreal.StaticMeshSocket(outer=mesh)
            socket.set_editor_property("socket_name", socket_name)
            socket.set_editor_property("relative_location", unreal.Vector(*location))
            socket.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=0))
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
            socket.set_editor_property("tag", "ART-02 visual attachment")
            mesh.add_socket(socket)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), f"Mesh save failed: {name}")


def inspect(name, source):
    mesh = unreal.load_asset(f"{BASE}/Meshes/{name}")
    require(isinstance(mesh, unreal.StaticMesh), f"Missing native mesh: {name}")
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    bounds = mesh.get_bounding_box()
    low, high = vector_values(bounds.min), vector_values(bounds.max)
    source_low, source_high = source["bounds_min_cm"], source["bounds_max_cm"]
    expected_low = [source_low[0], -source_high[1], source_low[2]]
    expected_high = [source_high[0], -source_low[1], source_high[2]]
    require(close(low, expected_low) and close(high, expected_high), f"Incorrect bounds/pivot: {name}: {low}/{high}")
    require(mesh.get_num_triangles(0) == source["triangles"], f"Source/native triangle mismatch: {name}")
    materials = [mesh.get_material(i).get_path_name() for i in range(len(mesh.get_editor_property("static_materials")))]
    require([path.rsplit(".", 1)[-1] for path in materials] == source["material_slots"], f"Material mismatch: {name}")
    require(all(path.startswith(f"{BASE}/Materials/") for path in materials), f"External material: {name}")
    sections = [subsystem.is_section_collision_enabled(mesh, 0, i) for i in range(mesh.get_num_sections(0))]
    simple = subsystem.get_simple_collision_count(mesh)
    convex = subsystem.get_convex_collision_count(mesh)
    require(simple == convex == 0 and not any(sections), f"Equipped visual mesh must not collide: {name}")
    instance = mesh.get_editor_property("body_setup").get_editor_property("default_instance")
    profile = str(instance.get_editor_property("collision_profile_name"))
    enabled = str(instance.get_editor_property("collision_enabled"))
    require(profile == "NoCollision" and "NO_COLLISION" in enabled, f"Bad collision defaults: {name}")
    uv_count = subsystem.get_num_uv_channels(mesh, 0)
    require(uv_count == 2, f"Expected two UV channels: {name}")
    nanite = subsystem.get_nanite_settings(mesh).get_editor_property("enabled")
    require(not nanite, f"Unexpected Nanite: {name}")
    sockets = {}
    for socket_name, expected_location in (SOCKETS.items() if name == BODY else []):
        socket = mesh.find_socket(socket_name)
        require(socket is not None, f"Missing socket: {socket_name}")
        location = vector_values(socket.get_editor_property("relative_location"))
        scale = vector_values(socket.get_editor_property("relative_scale"))
        rotation = socket.get_editor_property("relative_rotation")
        angles = [rotation.roll, rotation.pitch, rotation.yaw]
        require(close(location, expected_location) and close(scale, [1, 1, 1]) and close(angles, [0, 0, 0]),
                f"Socket transform mismatch: {socket_name}")
        sockets[socket_name] = {"location_cm": location, "roll_pitch_yaw_degrees": angles, "scale": scale}
    # The mesh's raw Sockets array is protected; query through the public
    # component API without registering or saving a component in a world.
    probe_component = unreal.StaticMeshComponent()
    require(probe_component.set_static_mesh(mesh), f"Socket inspection component: {name}")
    require({str(n) for n in probe_component.get_all_socket_names()} == set(sockets),
            f"Unexpected socket names: {name}")
    return {
        "asset": mesh.get_path_name(),
        "source_sha256": hashlib.sha256((SOURCE / "Exports" / f"{name}.fbx").read_bytes()).hexdigest(),
        "bounds_min_cm": low, "bounds_max_cm": high,
        "size_cm": [round(b - a, 5) for a, b in zip(low, high)],
        "lod0_triangles": mesh.get_num_triangles(0), "lod0_vertices": mesh.get_num_vertices(0),
        "lods": mesh.get_num_lods(), "materials": materials, "uv_channels": uv_count,
        "simple_collision_count": simple, "convex_collision_count": convex,
        "section_collision_enabled": sections, "collision_profile": profile,
        "collision_enabled": enabled, "nanite_enabled": nanite, "sockets": sockets,
    }


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the rifle asset worktree")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "End PIE first")
    source_report = json.loads((SOURCE / "mesh_validation.json").read_text())
    sources = {item["name"]: item for item in source_report["assets"]}
    require(set(sources) == set(NAMES), "Source report must contain exactly the rifle body and magazine")
    for name, location in SOCKETS.items():
        require(close(source_report["sockets"][name]["location_cm"], location), f"Source socket mismatch: {name}")
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    if not inspect_only:
        paths = [f"{BASE}/Materials/{n}" for n in PALETTE] + [f"{BASE}/Meshes/{n}" for n in NAMES]
        require(not any(unreal.EditorAssetLibrary.does_asset_exist(p) for p in paths),
                "Rifle batch already exists or is partial; inspect before retrying. No overwrite allowed.")
        require(all((SOURCE / "Exports" / f"{n}.fbx").is_file() for n in NAMES), "Missing FBX source")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        materials = {name: make_material(name, *settings) for name, settings in PALETTE.items()}
        for name in NAMES:
            import_mesh(name, materials)
            inspect(name, sources[name])
    report = {"engine": unreal.SystemLibrary.get_engine_version(),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "assets": {name: inspect(name, sources[name]) for name in NAMES},
              "forward_axis": "+X", "up_axis": "+Z", "units": "cm",
              "gameplay_verified": False, "animation_verified": False}
    if inspect_only:
        prior = json.loads((REPORTS / "unreal-import-report.json").read_text())
        require(report["assets"] == prior["assets"], "Saved asset/source differs from import-time evidence")
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("unreal-reopen-report.json" if inspect_only else "unreal-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"VALSTRIKE_RIFLE_IMPORT_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_RIFLE_IMPORT_FAILED: {error}")
        raise
