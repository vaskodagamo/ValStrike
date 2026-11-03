"""ART-01: import the original five-piece kit in Unreal Editor 5.8.4.

Run in a fresh editor, never a gameplay session. Native assets are authored only
through Unreal APIs. Existing assets are inspected, never replaced by this script.
See docs/assets/IMPORT.md for the exact invocation and acceptance limits.
"""

import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
EXPORTS = ROOT / "Art/Source/Environment/Models/Exports"
BASE = "/Game/Art/Environment"
SPECS = {
    "SM_VS_Wall_400x300": (400, 20, 300),
    "SM_VS_Pillar_40x300": (40, 40, 300),
    "SM_VS_Crate_100": (100, 100, 100),
    "SM_VS_Cover_200x120": (200, 100, 120),
    "SM_VS_Vent_120x80": (120, 20, 80),
}
PALETTE = {
    "M_VS_Plaster": ("C8B89E", 0.86, 0.0),
    "M_VS_PaintedMetal": ("416F70", 0.70, 0.0),
    "M_VS_Trim": ("303C41", 0.78, 0.0),
    "M_VS_Accent": ("D39B58", 0.74, 0.0),
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def linear_color(hex_color):
    def decode(value):
        value = int(value, 16) / 255.0
        return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(*(decode(hex_color[i:i + 2]) for i in (0, 2, 4)), 1.0)


def material(name, hex_color, roughness, metallic):
    path = f"{BASE}/Materials/{name}"
    require(not unreal.EditorAssetLibrary.does_asset_exist(path), f"Refusing overwrite: {path}")
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, f"{BASE}/Materials", unreal.Material, unreal.MaterialFactoryNew())
    require(asset, f"Material creation failed: {name}")
    lib = unreal.MaterialEditingLibrary
    color = lib.create_material_expression(asset, unreal.MaterialExpressionConstant3Vector, -260, 0)
    color.set_editor_property("constant", linear_color(hex_color))
    require(lib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR), name)
    for row, (value, prop) in enumerate(((roughness, unreal.MaterialProperty.MP_ROUGHNESS),
                                        (metallic, unreal.MaterialProperty.MP_METALLIC)), 1):
        node = lib.create_material_expression(asset, unreal.MaterialExpressionConstant, -260, row * 100)
        node.set_editor_property("r", value)
        require(lib.connect_material_property(node, "", prop), name)
    require(not lib.recompile_material(asset), f"Shader compile failed: {name}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, False), f"Save failed: {name}")
    return asset


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
        "one_convex_hull_per_ucx": True, "build_nanite": False,
        "generate_lightmap_u_vs": False, "import_uniform_scale": 1.0,
        "convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
        "transform_vertex_to_absolute": True, "bake_pivot_in_vertex": False,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
    }.items():
        data.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    for key, value in {
        "filename": str(EXPORTS / f"{name}.fbx"), "destination_path": f"{BASE}/Meshes",
        "destination_name": name, "automated": True, "replace_existing": False,
        "save": False, "factory": unreal.FbxFactory(), "options": options,
    }.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(f"{BASE}/Meshes/{name}")
    require(isinstance(asset, unreal.StaticMesh), f"Import failed: {name}")
    for index, slot in enumerate(asset.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        require(slot_name in materials, f"Unknown material {slot_name} in {name}")
        asset.set_material(index, materials[slot_name])
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if name == "SM_VS_Vent_120x80":
        subsystem.remove_collisions(asset)
        for section in range(asset.get_num_sections(0)):
            subsystem.enable_section_collision(asset, False, 0, section)
        body = asset.get_editor_property("body_setup")
        instance = body.get_editor_property("default_instance")
        instance.set_editor_property("collision_profile_name", "NoCollision")
        instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
        body.set_editor_property("default_instance", instance)
    asset.set_editor_property("light_map_coordinate_index", 1)
    asset.set_editor_property("light_map_resolution", 64)
    require(unreal.EditorAssetLibrary.save_loaded_asset(asset, False), f"Save failed: {name}")
    return asset


def xyz(vector):
    return [round(getattr(vector, component), 4) for component in ("x", "y", "z")]


def inspect(name):
    asset = unreal.load_asset(f"{BASE}/Meshes/{name}")
    require(isinstance(asset, unreal.StaticMesh), f"Missing native mesh: {name}")
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    bounds = asset.get_bounding_box()
    low, high = xyz(bounds.min), xyz(bounds.max)
    size = [round(b - a, 3) for a, b in zip(low, high)]
    require(all(abs(actual - expected) < 0.1 for actual, expected in zip(size, SPECS[name])),
            f"Wrong centimeter bounds for {name}: {size}, expected {SPECS[name]}")
    require(abs(low[2]) < 0.1, f"Pivot is not on base: {name}: {low}")
    if name in ("SM_VS_Wall_400x300", "SM_VS_Pillar_40x300"):
        require(all(min(abs(low[i]), abs(high[i])) < 0.1 for i in (0, 1)),
                f"Architectural pivot is not at base corner: {name}")
    else:
        require(all(abs(low[i] + high[i]) < 0.1 for i in (0, 1)),
                f"Prop pivot is not at bottom center: {name}")
    source = next(item for item in json.loads((EXPORTS.parent / "mesh_validation.json").read_text())["assets"]
                  if item["name"] == name)
    require(asset.get_num_triangles(0) == source["triangles"], f"Source/import triangle mismatch: {name}")
    materials = [asset.get_material(i).get_path_name()
                 for i in range(len(asset.get_editor_property("static_materials")))]
    require(all(path.startswith(f"{BASE}/Materials/") for path in materials), name)
    require([path.rsplit(".", 1)[-1] for path in materials] == source["material_slots"],
            f"Source/import material mismatch: {name}")
    simple = subsystem.get_simple_collision_count(asset)
    convex = subsystem.get_convex_collision_count(asset)
    sections = [subsystem.is_section_collision_enabled(asset, 0, index)
                for index in range(asset.get_num_sections(0))]
    vent = name == "SM_VS_Vent_120x80"
    require((simple == 0 and convex == 0 and not any(sections)) if vent else convex == 1,
            f"Collision mismatch: {name}: simple={simple}, convex={convex}, sections={sections}")
    uv_count = subsystem.get_num_uv_channels(asset, 0)
    require(uv_count == 2, f"Expected two UV sets: {name}: {uv_count}")
    nanite = subsystem.get_nanite_settings(asset).get_editor_property("enabled")
    require(not nanite, f"Unexpected Nanite dependency: {name}")
    body = asset.get_editor_property("body_setup")
    instance = body.get_editor_property("default_instance")
    profile = str(instance.get_editor_property("collision_profile_name"))
    enabled = str(instance.get_editor_property("collision_enabled"))
    if vent:
        require(profile == "NoCollision" and "NO_COLLISION" in enabled, f"Vent collision profile: {profile}/{enabled}")
    else:
        require(profile == "BlockAll" and "QUERY_AND_PHYSICS" in enabled,
                f"Solid collision profile: {name}: {profile}/{enabled}")
    return {
        "asset": asset.get_path_name(), "source_sha256": hashlib.sha256((EXPORTS / f"{name}.fbx").read_bytes()).hexdigest(),
        "bounds_min_cm": low, "bounds_max_cm": high, "size_cm": size,
        "lod0_triangles": asset.get_num_triangles(0), "lod0_vertices": asset.get_num_vertices(0),
        "lods": asset.get_num_lods(), "materials": materials, "sections": asset.get_num_sections(0),
        "uv_channels": uv_count, "simple_collision_count": simple, "convex_collision_count": convex,
        "section_collision_enabled": sections, "collision_profile": profile,
        "collision_enabled": enabled, "collision_complexity": str(subsystem.get_collision_complexity(asset)),
        "nanite_enabled": nanite, "textures": [],
    }


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the isolated asset worktree")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "End PIE first")
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    if not inspect_only:
        paths = [f"{BASE}/Materials/{n}" for n in PALETTE] + [f"{BASE}/Meshes/{n}" for n in SPECS]
        require(not any(unreal.EditorAssetLibrary.does_asset_exist(p) for p in paths),
                "Asset batch already exists or is partial. Inspect before a manual retry; no overwrite permitted.")
        require(all((EXPORTS / f"{name}.fbx").is_file() for name in SPECS), "Missing FBX source exports")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        materials = {name: material(name, *values) for name, values in PALETTE.items()}
        for name in SPECS:
            import_mesh(name, materials)
            inspect(name)  # Stop immediately on an incorrect pilot scale or collision import.
    result = {"engine": unreal.SystemLibrary.get_engine_version(),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "assets": {name: inspect(name) for name in SPECS},
              "runtime_verified": False, "map_placement_verified": False}
    if inspect_only:
        previous = json.loads((ROOT / "docs/assets/unreal-import-report.json").read_text())
        require(result["assets"] == previous["assets"],
                "Saved asset or source export differs from import-time report; investigate stale exports or edited assets.")
    output = ROOT / "docs/assets" / ("unreal-reopen-report.json" if inspect_only else "unreal-import-report.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"VALSTRIKE_ASSET_SUCCESS: {output}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_ASSET_FAILED: {error}")
        raise
