"""INT-ART-02 phase B: import ART-25's frozen W01/W08/W09 as native UE 5.8.4 assets.

Asset import only (static mesh, four textures, one material each) under
/Game/Art/Environment/MeshySite01/CourtyardPilot/<ID>/. Allowlist, paths and SHA-256
come from the frozen ART-25 readiness record; held/later assets cannot be imported.
Native bounds must equal the record's expected_native_bounds_cm (FBX Y negation).
No map access; existing assets are never overwritten. -VSInspectOnly reopens.
"""

import hashlib
import json
from pathlib import Path
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
READINESS = ROOT / "docs/assets/meshy-site01/courtyard-pilot/readiness.json"
REPORTS = ROOT / "docs/assets/meshy-site01-followups"
BASE = "/Game/Art/Environment/MeshySite01/CourtyardPilot"
ROLES = ("basecolor", "roughness", "metallic", "normal")
IDS = {"MSH-W01", "MSH-W08", "MSH-W09"}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def xyz(vector):
    return [round(getattr(vector, axis), 4) for axis in ("x", "y", "z")]


def close(a, b, tolerance=0.05):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


def paths(asset):
    tag = asset["asset_id"].replace("MSH-", "")
    return {"mesh": f"{BASE}/{tag}/{Path(asset['fbx']['path']).stem}", "material": f"{BASE}/{tag}/M_VS_MSH_{tag}",
            "textures": {t["role"]: f"{BASE}/{tag}/Textures/T_MSH_{tag}_{t['role']}" for t in asset["textures"]}}


def load_readiness():
    record = json.loads(READINESS.read_text())
    require(record.get("task_id") == "ART-25" and record.get("status") == "ready", "ART-25 not ready")
    assets = record["assets"]
    require({a["asset_id"] for a in assets} == IDS == set(record["approved_asset_ids"]), "Unexpected allowlist")
    for asset in assets:
        require(asset["verdict"] == "ready", f"{asset['asset_id']} not ready")
        require(sorted(t["role"] for t in asset["textures"]) == sorted(ROLES), f"{asset['asset_id']} maps")
        for item in [asset["fbx"]] + asset["textures"]:
            path = ROOT / item["path"]
            require(path.is_file() and digest(path) == item["sha256"], f"Missing/changed source: {item['path']}")
    return record


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


def import_texture(item, asset_path, flip_green):
    texture = run_import(ROOT / item["path"], asset_path)
    require(isinstance(texture, unreal.Texture2D), f"Texture import failed: {asset_path}")
    role = item["role"]
    require(bool(item["srgb"]) == (role == "basecolor"), f"Unexpected colour space: {asset_path}")
    texture.set_editor_property("srgb", role == "basecolor")
    if role == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", bool(flip_green))
    elif role != "basecolor":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    require(unreal.EditorAssetLibrary.save_loaded_asset(texture, False), f"Save failed: {asset_path}")
    return texture


def make_material(asset_path, textures):
    folder, name = asset_path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    require(material, f"Could not create {asset_path}")
    lib = unreal.MaterialEditingLibrary
    wiring = {"basecolor": ("RGB", unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
              "roughness": ("R", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
              "metallic": ("R", unreal.MaterialProperty.MP_METALLIC, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
              "normal": ("RGB", unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)}
    for row, role in enumerate(ROLES):
        output, prop, sampler = wiring[role]
        node = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -420, row * 260)
        node.set_editor_property("texture", textures[role])
        node.set_editor_property("sampler_type", sampler)
        require(lib.connect_material_property(node, output, prop), f"{asset_path}: {role}")
    require(not lib.recompile_material(material), f"Material compile failed: {asset_path}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False), f"Save failed: {asset_path}")
    return material


def import_mesh(asset, mesh_path, material):
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
                       "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS}.items():
        data.set_editor_property(key, value)
    mesh = run_import(ROOT / asset["fbx"]["path"], mesh_path, options, unreal.FbxFactory())
    require(isinstance(mesh, unreal.StaticMesh), f"Mesh import failed: {mesh_path}")
    require(len(mesh.get_editor_property("static_materials")) == 1, f"Expected one slot: {mesh_path}")
    mesh.set_material(0, material)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    for section in range(mesh.get_num_sections(0)):
        subsystem.enable_section_collision(mesh, False, 0, section)
    body = mesh.get_editor_property("body_setup")
    instance = body.get_editor_property("default_instance")
    instance.set_editor_property("collision_profile_name", "NoCollision")
    instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    body.set_editor_property("default_instance", instance)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), f"Save failed: {mesh_path}")


def inspect(asset):
    p = paths(asset)
    mesh = unreal.load_asset(p["mesh"])
    require(isinstance(mesh, unreal.StaticMesh), f"Missing {p['mesh']}")
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    box = mesh.get_bounding_box()
    low, high = xyz(box.min), xyz(box.max)
    expected = asset["expected_native_bounds_cm"]
    sections = [subsystem.is_section_collision_enabled(mesh, 0, i) for i in range(mesh.get_num_sections(0))]
    simple, convex = subsystem.get_simple_collision_count(mesh), subsystem.get_convex_collision_count(mesh)
    textures = {}
    for role, path in p["textures"].items():
        t = unreal.load_asset(path)
        require(isinstance(t, unreal.Texture2D), f"Missing {path}")
        textures[role] = {"asset": path, "srgb": t.get_editor_property("srgb"),
                          "compression": str(t.get_editor_property("compression_settings")),
                          "flip_green_channel": t.get_editor_property("flip_green_channel")}
    return {"asset_id": asset["asset_id"], "mesh": mesh.get_path_name(), "bounds_min_cm": low, "bounds_max_cm": high,
            "expected_native_bounds_cm": expected,
            "bounds_match_expected": close(low, expected["min"]) and close(high, expected["max"]),
            "lod0_triangles": mesh.get_num_triangles(0), "record_triangles": asset["triangles"],
            "uv_channels": subsystem.get_num_uv_channels(mesh, 0),
            "nanite_enabled": subsystem.get_nanite_settings(mesh).get_editor_property("enabled"),
            "material": mesh.get_material(0).get_path_name(), "textures": textures,
            "collision": {"simple": simple, "convex": convex, "sections": sections}}


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-02 worktree")
    record = load_readiness()
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    if not inspect_only:
        existing = [x for a in record["assets"] for x in [paths(a)["mesh"], paths(a)["material"], *paths(a)["textures"].values()]
                    if unreal.EditorAssetLibrary.does_asset_exist(x)]
        require(not existing, f"Courtyard batch exists or is partial; inspect, do not overwrite: {existing}")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        for asset in record["assets"]:
            p = paths(asset)
            flip = asset["normal_convention"]["unreal_start_flip_green_channel"]
            textures = {t["role"]: import_texture(t, p["textures"][t["role"]], flip) for t in asset["textures"]}
            import_mesh(asset, p["mesh"], make_material(p["material"], textures))
    report = {"engine": unreal.SystemLibrary.get_engine_version(),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "readiness_sha256": digest(READINESS), "source_commit": record["source_commit"],
              "assets": [inspect(a) for a in record["assets"]], "map_edited": False, "runtime_verified": False}
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("courtyard-reopen-report.json" if inspect_only else "courtyard-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if inspect_only:
        prior = json.loads((REPORTS / "courtyard-import-report.json").read_text())
        require(report["assets"] == prior["assets"], "Reopen differs from import-time evidence")
    for item in report["assets"]:
        require(item["bounds_match_expected"], f"{item['asset_id']}: native bounds differ from ART-25 expectation")
        require(item["lod0_triangles"] == item["record_triangles"], f"{item['asset_id']}: triangle mismatch")
        c = item["collision"]
        require(c["simple"] == c["convex"] == 0 and not any(c["sections"]), f"{item['asset_id']}: collision present")
    unreal.log(f"VALSTRIKE_MESHY_COURTYARD_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_MESHY_COURTYARD_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        unreal.SystemLibrary.quit_editor()
