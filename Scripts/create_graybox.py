"""SET-02 / MOVE-01: author the bootstrap arena in a fresh Unreal editor process.

Run via Scripts/ue.py generate-map after compiling the Editor target. Existing
assets are never replaced. A failed run can leave partial assets: inspect them
in the editor before retrying; this script deliberately does not delete them.
"""

import json
from pathlib import Path

import unreal

MAP = "/Game/Maps/VS_Graybox"
MATERIAL_DIR = "/Game/Materials/Bootstrap"
COLORS = {
    "M_Floor": (0.15, 0.18, 0.22),
    "M_Wall": (0.45, 0.49, 0.52),
    "M_Cover": (0.16, 0.39, 0.42),
    "M_Spawn": (0.65, 0.27, 0.12),
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def content_file(asset_path, extension):
    return Path(unreal.Paths.project_content_dir()) / (asset_path[6:] + extension)


def make_material(name, rgb):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew()
    )
    require(material, f"Could not create material {name}")
    library = unreal.MaterialEditingLibrary
    color = library.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -250, 0
    )
    roughness = library.create_material_expression(
        material, unreal.MaterialExpressionConstant, -250, 150
    )
    require(color and roughness, f"Could not create material nodes for {name}")
    color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
    roughness.set_editor_property("r", 0.9)
    require(library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR),
            f"Could not connect base color for {name}")
    require(library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS),
            f"Could not connect roughness for {name}")
    errors = library.recompile_material(material)
    require(not errors, f"Material compilation failed for {name}: {errors}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False),
            f"Could not save material {name}")
    require(content_file(f"{MATERIAL_DIR}/{name}", ".uasset").is_file(),
            f"Material save did not produce a file: {name}")
    return material


def main():
    marker = Path(unreal.Paths.project_saved_dir()) / "Bootstrap/graybox-success.json"
    marker.unlink(missing_ok=True)  # Only this generator's own completion marker.
    require(not any(content_file(MAP, ext).exists() for ext in (".umap", ".uasset")),
            f"Refusing to overwrite {MAP}; open the existing map in the editor.")
    require(not unreal.find_object(None, f"{MAP}.VS_Graybox"),
            f"Refusing to replace an already loaded map: {MAP}")
    for name in COLORS:
        asset_path = f"{MATERIAL_DIR}/{name}"
        require(not content_file(asset_path, ".uasset").exists()
                and not unreal.EditorAssetLibrary.does_asset_exist(asset_path),
                f"Refusing to overwrite material {asset_path}; inspect prior bootstrap assets.")
    game_mode = unreal.load_class(None, "/Script/ValStrike.VSGameMode")
    require(game_mode, "VSGameMode is unavailable. Compile ValStrikeEditor and restart the editor first.")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    require(cube, "The engine cube mesh is unavailable; verify the engine installation.")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not levels.is_in_play_in_editor(), "End Play In Editor before generating the arena.")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(),
            "Save unsaved maps before running the generator in a fresh editor process.")
    materials = {name: make_material(name, rgb) for name, rgb in COLORS.items()}
    require(levels.new_level(MAP, False), f"Could not create {MAP}")
    worlds.get_editor_world().get_world_settings().set_editor_property("default_game_mode", game_mode)
    labels = set()

    def spawn(actor_class, label, location, rotation=(0, 0, 0)):
        actor = actors.spawn_actor_from_class(actor_class, unreal.Vector(*location),
                                              unreal.Rotator(*rotation))
        require(actor, f"Could not spawn {label}")
        actor.set_actor_label(label)
        labels.add(label)
        return actor

    def box(label, location, size, material="M_Cover"):
        actor = spawn(unreal.StaticMeshActor, label, location)
        component = actor.static_mesh_component
        require(component.set_static_mesh(cube), f"Could not assign cube to {label}")
        actor.set_actor_scale3d(unreal.Vector(*(value / 100.0 for value in size)))
        component.set_material(0, materials[material])
        component.set_collision_profile_name("BlockAll")
        component.set_mobility(unreal.ComponentMobility.STATIC)
        return actor

    # Engine cube is 100 cm per side. The floor surface is exactly Z=0.
    box("VS_Floor", (0, 0, -50), (4000, 3000, 100), "M_Floor")
    for label, location, size in (
        ("North", (0, 1500, 200), (4100, 100, 400)),
        ("South", (0, -1500, 200), (4100, 100, 400)),
        ("East", (2000, 0, 200), (100, 3000, 400)),
        ("West", (-2000, 0, 200), (100, 3000, 400)),
    ):
        box(f"VS_Wall_{label}", location, size, "M_Wall")
    for index, (location, size) in enumerate((
        ((-500, 0, 90), (200, 500, 180)),
        ((500, 100, 60), (220, 300, 120)),
        ((0, -650, 100), (350, 150, 200)),
    )):
        box(f"VS_Cover_{index + 1}", location, size)
    # Four 20 cm risers lead onto an 80 cm platform, clear of all spawn points.
    for index in range(4):
        height = 20 * (index + 1)
        box(f"VS_Step_{index + 1}", (-300 + 80 * index, 1050, height / 2),
            (80, 300, height), "M_Wall")
    box("VS_Platform", (200, 1050, 40), (440, 500, 80), "M_Wall")
    for index, (x, y, yaw) in enumerate((
        (-1400, -600, 0), (-1400, 600, 0), (1400, -600, 180), (1400, 600, 180)
    )):
        spawn(unreal.PlayerStart, f"VS_Spawn_{index + 1}", (x, y, 100), (0, yaw, 0))
        box(f"VS_SpawnPad_{index + 1}", (x, y, 1), (160, 160, 2), "M_Spawn")
    sun = spawn(unreal.DirectionalLight, "VS_Sun", (0, 0, 1000), (-50, -30, 0))
    sun_light = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_light.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_light.set_editor_property("atmosphere_sun_light", True)
    sun_light.set_intensity(5000.0)
    spawn(unreal.SkyAtmosphere, "VS_Atmosphere", (0, 0, 0))
    sky = spawn(unreal.SkyLight, "VS_SkyLight", (0, 0, 800))
    sky_light = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_light.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_light.set_editor_property("real_time_capture", True)
    sky_light.set_intensity(1.0)

    require(levels.save_current_level(), f"Final save failed for {MAP}")
    require(content_file(MAP, ".umap").stat().st_size > 0, "Saved map is missing or empty.")
    require(levels.load_level(MAP), f"Could not reopen saved map {MAP}")
    saved_actors = actors.get_all_level_actors()
    require(labels.issubset({actor.get_actor_label() for actor in saved_actors}),
            "Saved map is missing generated actors.")
    require(sum(isinstance(actor, unreal.PlayerStart) for actor in saved_actors) >= 4,
            "Saved map has fewer than four PlayerStarts.")
    require(worlds.get_editor_world().get_world_settings().get_editor_property("default_game_mode") == game_mode,
            "Saved map did not retain the VSGameMode override.")
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.write_text(json.dumps({"map": MAP, "generator_version": 1,
                                  "actor_count": len(saved_actors)}, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"VALSTRIKE_GRAYBOX_SUCCESS: saved and reopened {MAP}; {len(saved_actors)} actors.")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_GRAYBOX_FAILED: {error}. Partial assets may remain; inspect before retrying.")
        raise
