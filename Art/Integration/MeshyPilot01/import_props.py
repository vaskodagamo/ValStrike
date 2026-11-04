"""INT-ART-01: import the four ART-16-approved Site01 props as native UE 5.8.4 assets.

Asset import only: static meshes, textures and one material per prop under
/Game/Art/Environment/MeshySite01/PropsSurfaces. This script never opens, edits or
saves a map; placement is a separate, inspected step. The allowlist, file paths and
SHA-256 values come from ART-16's frozen readiness record, so held assets (W02,
ART-11 structures) can never be imported by this script.

Run through Scripts/with_ue_slot.py in the integration worktree:
  UnrealEditor ValStrike.uproject /Engine/Maps/Entry \
    -ExecutePythonScript=Art/Integration/MeshyPilot01/import_props.py -unattended -log
Reopen in a fresh process with -VSInspectOnly to compare saved assets with the
import-time report. Existing native assets are never overwritten.
"""

import hashlib
import json
from pathlib import Path
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
READINESS = ROOT / "docs/assets/meshy-site01/props-surfaces/pilot-readiness.json"
REPORTS = ROOT / "docs/assets/meshy-pilot-integration"
BASE = "/Game/Art/Environment/MeshySite01/PropsSurfaces"
ROLE_ORDER = ("basecolor", "roughness", "metallic", "normal")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def xyz(vector):
    return [round(getattr(vector, axis), 4) for axis in ("x", "y", "z")]


def close(actual, expected, tolerance=0.05):
    return len(actual) == len(expected) and all(abs(a - b) <= tolerance for a, b in zip(actual, expected))


def short(asset_id):
    return asset_id.replace("MSH-", "")  # MSH-W03 -> W03


def asset_paths(asset):
    tag = short(asset["asset_id"])
    mesh = Path(asset["fbx"]["path"]).stem
    textures = {t["role"]: f"{BASE}/{tag}/Textures/{Path(t['path']).stem}" for t in asset["textures"]}
    return {"mesh": f"{BASE}/{tag}/{mesh}", "material": f"{BASE}/{tag}/M_VS_MSH_{tag}",
            "textures": textures}


def load_readiness():
    record = json.loads(READINESS.read_text())
    require(record.get("task_id") == "ART-16" and record.get("status") == "ready", "ART-16 record is not ready")
    assets = record["assets"]
    ids = [a["asset_id"] for a in assets]
    require(ids == record["approved_asset_ids"] and set(ids) == {"MSH-W03", "MSH-W04", "MSH-W07", "MSH-W10"},
            f"Unexpected allowlist: {ids}")
    for asset in assets:
        require(asset["verdict"] == "ready", f"{asset['asset_id']} is not ready")
        require(sorted(t["role"] for t in asset["textures"]) == sorted(ROLE_ORDER),
                f"{asset['asset_id']} needs exactly four material maps")
        for item in [asset["fbx"]] + asset["textures"]:
            path = ROOT / item["path"]
            require(path.is_file(), f"Missing source (git lfs pull?): {item['path']}")
            require(digest(path) == item["sha256"], f"Source hash mismatch: {item['path']}")
    return record


def run_import(task_filename, destination, name, options=None, factory=None):
    task = unreal.AssetImportTask()
    settings = {"filename": str(task_filename), "destination_path": destination, "destination_name": name,
                "automated": True, "replace_existing": False, "save": False}
    if options is not None:
        settings["options"] = options
    if factory is not None:
        settings["factory"] = factory
    for key, value in settings.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.load_asset(f"{destination}/{name}")


def import_texture(source, asset_path, role, flip_green):
    folder, name = asset_path.rsplit("/", 1)
    texture = run_import(ROOT / source["path"], folder, name)
    require(isinstance(texture, unreal.Texture2D), f"Texture import failed: {asset_path}")
    require(bool(source["srgb"]) == (role == "basecolor"), f"Unexpected colour space in record: {asset_path}")
    texture.set_editor_property("srgb", role == "basecolor")
    if role == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", bool(flip_green))
    elif role in ("roughness", "metallic"):
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    require(unreal.EditorAssetLibrary.save_loaded_asset(texture, False), f"Texture save failed: {asset_path}")
    return texture


def make_material(asset_path, textures):
    folder, name = asset_path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, folder, unreal.Material, unreal.MaterialFactoryNew())
    require(material, f"Could not create {asset_path}")
    lib = unreal.MaterialEditingLibrary
    wiring = {
        "basecolor": ("RGB", unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        "roughness": ("R", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
        "metallic": ("R", unreal.MaterialProperty.MP_METALLIC, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
        "normal": ("RGB", unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
    }
    for row, role in enumerate(ROLE_ORDER):
        output, prop, sampler = wiring[role]
        node = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -420, row * 260)
        node.set_editor_property("texture", textures[role])
        node.set_editor_property("sampler_type", sampler)
        require(lib.connect_material_property(node, output, prop), f"{asset_path}: {role} connection")
    require(not lib.recompile_material(material), f"Material compile failed: {asset_path}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False), f"Material save failed: {asset_path}")
    return material


def import_mesh(asset, mesh_path, material):
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
        "combine_meshes": True, "auto_generate_collision": False, "build_nanite": False,
        "generate_lightmap_u_vs": False,  # Site01 lighting is fully movable; keep UV0 only.
        "import_uniform_scale": 1.0, "convert_scene": True, "convert_scene_unit": True,
        "force_front_x_axis": False, "transform_vertex_to_absolute": True, "bake_pivot_in_vertex": False,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
    }.items():
        data.set_editor_property(key, value)
    folder, name = mesh_path.rsplit("/", 1)
    mesh = run_import(ROOT / asset["fbx"]["path"], folder, name, options, unreal.FbxFactory())
    require(isinstance(mesh, unreal.StaticMesh), f"Mesh import failed: {mesh_path}")
    require(len(mesh.get_editor_property("static_materials")) == 1, f"Expected one material slot: {mesh_path}")
    mesh.set_material(0, material)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    for section in range(mesh.get_num_sections(0)):
        subsystem.enable_section_collision(mesh, False, 0, section)
    body_setup = mesh.get_editor_property("body_setup")
    instance = body_setup.get_editor_property("default_instance")
    instance.set_editor_property("collision_profile_name", "NoCollision")
    instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    body_setup.set_editor_property("default_instance", instance)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), f"Mesh save failed: {mesh_path}")


def inspect(asset):
    paths = asset_paths(asset)
    mesh = unreal.load_asset(paths["mesh"])
    require(isinstance(mesh, unreal.StaticMesh), f"Missing native mesh: {paths['mesh']}")
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    bounds = mesh.get_bounding_box()
    low, high = xyz(bounds.min), xyz(bounds.max)
    src_low, src_high = asset["local_bounds_cm"]["min"], asset["local_bounds_cm"]["max"]
    # ART-16's placement table assumes native local bounds equal the source record.
    # The FBX handedness conversion may instead negate Y; report which one is true
    # and fail, so a mirrored result holds that placement instead of being guessed.
    mirrored = ([src_low[0], -src_high[1], src_low[2]], [src_high[0], -src_low[1], src_high[2]])
    if close(low, src_low) and close(high, src_high):
        frame = "matches_source_record"
    elif close(low, mirrored[0]) and close(high, mirrored[1]):
        frame = "y_negated"
    else:
        frame = "mismatch"
    sections = [subsystem.is_section_collision_enabled(mesh, 0, i) for i in range(mesh.get_num_sections(0))]
    simple, convex = subsystem.get_simple_collision_count(mesh), subsystem.get_convex_collision_count(mesh)
    require(simple == convex == 0 and not any(sections), f"Visual prop must not collide: {paths['mesh']}")
    textures = {}
    for role, path in paths["textures"].items():
        texture = unreal.load_asset(path)
        require(isinstance(texture, unreal.Texture2D), f"Missing texture: {path}")
        textures[role] = {"asset": path, "srgb": texture.get_editor_property("srgb"),
                          "compression": str(texture.get_editor_property("compression_settings")),
                          "flip_green_channel": texture.get_editor_property("flip_green_channel"),
                          # Streamed size; a fresh process may still show the 32 px
                          # compile placeholder, so the reopen comparison ignores it.
                          "size_px": [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]}
    return {
        "asset_id": asset["asset_id"], "mesh": mesh.get_path_name(), "source_fbx_sha256": asset["fbx"]["sha256"],
        "bounds_min_cm": low, "bounds_max_cm": high, "size_cm": [round(b - a, 4) for a, b in zip(low, high)],
        "record_local_bounds_cm": asset["local_bounds_cm"], "native_frame_vs_record": frame,
        "lod0_triangles": mesh.get_num_triangles(0), "record_triangles": asset["triangles"],
        "uv_channels": subsystem.get_num_uv_channels(mesh, 0),
        "nanite_enabled": subsystem.get_nanite_settings(mesh).get_editor_property("enabled"),
        "material": mesh.get_material(0).get_path_name(), "textures": textures,
        "simple_collision_count": simple, "convex_collision_count": convex,
        "section_collision_enabled": sections,
    }


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-01 worktree")
    record = load_readiness()
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    if not inspect_only:
        existing = [p for a in record["assets"] for p in
                    [asset_paths(a)["mesh"], asset_paths(a)["material"], *asset_paths(a)["textures"].values()]
                    if unreal.EditorAssetLibrary.does_asset_exist(p)]
        require(not existing, f"Props batch exists or is partial; inspect, do not overwrite: {existing}")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        for asset in record["assets"]:
            paths = asset_paths(asset)
            flip = asset["normal_convention"]["unreal_flip_green_channel"]
            textures = {t["role"]: import_texture(t, paths["textures"][t["role"]], t["role"], flip)
                        for t in asset["textures"]}
            import_mesh(asset, paths["mesh"], make_material(paths["material"], textures))
    report = {"engine": unreal.SystemLibrary.get_engine_version(),
              "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "readiness_sha256": digest(READINESS), "readiness_source_commit": record["source_commit"],
              "assets": [inspect(a) for a in record["assets"]],
              "map_edited": False, "runtime_verified": False}
    REPORTS.mkdir(parents=True, exist_ok=True)
    target = REPORTS / ("props-reopen-report.json" if inspect_only else "props-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if inspect_only:
        prior = json.loads((REPORTS / "props-import-report.json").read_text())
        def stable(item):  # Source dimensions are fixed by the verified PNG hashes.
            return {**item, "textures": {r: {k: v for k, v in t.items() if k != "size_px"}
                                         for r, t in item["textures"].items()}}
        diffs = [(a["asset_id"], k) for a, b in zip(map(stable, report["assets"]), map(stable, prior["assets"]))
                 for k in b if a.get(k) != b[k]]
        require(not diffs, f"Saved assets differ from import-time evidence: {diffs}")
    for item in report["assets"]:
        require(item["native_frame_vs_record"] != "mismatch", f"{item['asset_id']}: unexpected native bounds")
        require(item["lod0_triangles"] == item["record_triangles"], f"{item['asset_id']}: triangle mismatch")
    unreal.log(f"VALSTRIKE_MESHY_PROPS_IMPORT_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_MESHY_PROPS_IMPORT_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        # Bounded slot use: always close this one-shot editor process.
        unreal.SystemLibrary.quit_editor()
