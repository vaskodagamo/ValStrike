"""Author Switchyard through Unreal Editor only. Fresh process; refuses an existing map.

Deliberate revisions use -VSSiteRebuild after reviewing the existing map. Only the
Site01 package is replaced; shared assets and VS_Graybox are never saved.
"""
import hashlib
import json
from pathlib import Path
import sys

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import design as d


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong checkout")
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned engine required")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not levels.is_in_play_in_editor(), "Use a fresh editor without PIE")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Unsaved map present")
    exists = unreal.EditorAssetLibrary.does_asset_exist(d.MAP)
    require(not exists or "-VSSiteRebuild" in unreal.SystemLibrary.get_command_line(),
            "Map already exists; inspect it before deliberate -VSSiteRebuild")
    if exists:
        # Editor-owned deletion/recreation of our own explicitly requested map.
        require(unreal.EditorAssetLibrary.delete_asset(d.MAP), "Could not replace Site01 map")
    require(levels.new_level(d.MAP, False), "Cannot create Site01")
    world = worlds.get_editor_world()
    mode = unreal.load_class(None, "/Script/ValStrike.VSGameMode")
    require(mode, "Build ValStrikeEditor first")
    world.get_world_settings().set_editor_property("default_game_mode", mode)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    mats = {name: unreal.load_asset(f"/Game/Art/Environment/Materials/M_VS_{name}")
            for name in ("Plaster", "PaintedMetal", "Trim", "Accent")}
    require(all(mats.values()), "Missing completed environment kit")
    # Map-local matte paving, authored from original constant colors.
    for name, color in {"Paving": (0.155, 0.185, 0.195), "Lane": (0.235, 0.270, 0.275),
                        "Site": (0.39, 0.245, 0.10)}.items():
        path = f"/Game/Maps/Site01/Materials/M_Site01_{name}"
        mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if not mat:
            mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                f"M_Site01_{name}", "/Game/Maps/Site01/Materials", unreal.Material, unreal.MaterialFactoryNew())
            lib = unreal.MaterialEditingLibrary
            c = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -200, 0)
            c.set_editor_property("constant", unreal.LinearColor(*color, 1))
            r = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 140)
            r.set_editor_property("r", 0.9)
            lib.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
            lib.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
            require(not lib.recompile_material(mat), "Material compile failed")
            require(unreal.EditorAssetLibrary.save_loaded_asset(mat, False), "Material save failed")
        mats[name] = mat
    labels = set()

    def spawn(cls, label, pos, yaw=0):
        require(label not in labels, f"Duplicate label {label}")
        labels.add(label)
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*pos), unreal.Rotator(yaw=yaw))
        require(actor, f"Spawn failed: {label}")
        actor.set_actor_label(label)
        actor.set_folder_path("Site01")
        return actor

    def box(label, pos, size, finish, collide=True):
        a = spawn(unreal.StaticMeshActor, "VS_" + label, pos)
        c = a.static_mesh_component
        c.set_static_mesh(cube)
        a.set_actor_scale3d(unreal.Vector(*(n / 100 for n in size)))
        c.set_material(0, mats[finish])
        c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_collision_profile_name("BlockAll" if collide else "NoCollision")
        return a

    def kit(label, suffix, center, yaw=0, scale=(1, 1, 1)):
        mesh = unreal.load_asset(f"/Game/Art/Environment/Meshes/SM_VS_{suffix}")
        require(mesh, f"Missing kit mesh {suffix}")
        b = mesh.get_bounding_box()
        # Center imported corner pivots accurately; props also use this path.
        local = unreal.Vector(*[(getattr(b.min, ax) + getattr(b.max, ax)) * s / 2
                                for ax, s in zip(("x", "y", "z"), scale)])
        rot = unreal.Rotator(yaw=yaw)
        offset = unreal.MathLibrary.greater_greater_vector_rotator(local, rot)
        a = spawn(unreal.StaticMeshActor, "VS_" + label,
                  [center[0] - offset.x, center[1] - offset.y, center[2] - offset.z], yaw)
        a.set_actor_scale3d(unreal.Vector(*scale))
        c = a.static_mesh_component
        c.set_static_mesh(mesh)
        c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_collision_profile_name("NoCollision" if suffix.startswith("Vent") else "BlockAll")
        return a

    for label, pos, size, finish in d.BLOCKS:
        # Kit facade shells occupy the outer 20 cm; inset the core to avoid
        # coplanar surfaces fighting the detailed wall faces.
        core_size = (size[0] - 40, size[1] - 40, size[2]) if label in ("RelayHouse", "NorthService") else size
        box(label, pos, core_size, finish)
    for label, suffix, xy, yaw, z in d.PROPS:
        height = 120 if suffix.startswith("Cover") else 100
        kit(label, suffix, (*xy, z + height / 2), yaw)
    # Flush facade modules: simple massing remains the collision boundary.
    for side in (-1, 1):
        for index in range(4):
            kit(f"RelayPanel_{side}_{index}", "Wall_400x300",
                (-1100 + index * 400, side * 690, 210), 0 if side == 1 else 180, (1, 1, 1.4))
        for index in range(3):
            kit(f"RelayEnd_{side}_{index}", "Wall_400x300",
                (-500 + side * 790, -450 + index * 450, 210), -90 if side == 1 else 90, (1.125, 1, 1.4))
        for index in range(5):
            kit(f"ServicePanel_{side}_{index}", "Wall_400x300",
                (-200 + index * 400, 1375 + side * 115, 190), 0 if side == 1 else 180, (1, 1, 380 / 300))
    for x in (-1280, 280):
        for y in (-680, 680):
            kit(f"RelayPillar_{x}_{y}", "Pillar_40x300", (x, y, 210), 0, (1, 1, 1.4))
    # Inset roof detail has collision disabled; no inaccessible rooftop route is advertised.
    box("RelayRoofBand", (-500, 0, 426), (1600, 1400, 12), "PaintedMetal", False)
    box("RelayRoofPlant", (-550, 0, 475), (650, 650, 85), "Trim", False)
    box("ServiceRoofBand", (600, 1375, 387), (2000, 250, 14), "PaintedMetal", False)
    kit("RelayVent", "Vent_120x80", (310, 280, 255), 90)
    for x in (-2600, 2600):
        box(f"BoundaryCapX{x}", (x, 0, 505), (110, 4300, 10), "PaintedMetal", False)
    for y in (-2100, 2100):
        box(f"BoundaryCapY{y}", (0, y, 505), (5200, 110, 10), "PaintedMetal", False)
    # Flush no-collision graphic surfaces: there are no movement steps.
    box("GalleryPaving", (0, 950, 0.35), (3000, 450, 0.5), "Lane", False)
    box("SouthPaving", (-600, -1650, 0.35), (2900, 400, 0.5), "Lane", False)
    box("SitePad", (1050, 0, 0.5), (800, 1000, 0.6), "Site", False)
    for x in (650, 1450):
        box(f"SiteLineX{x}", (x, 0, 1), (8, 1000, 1), "Accent", False)
    for y in (-500, 500):
        box(f"SiteLineY{y}", (1050, y, 1), (800, 8, 1), "Accent", False)
    for entry in d.SPAWNS:
        a = spawn(unreal.PlayerStart, entry["actor_label"], entry["location_cm"], entry["yaw_degrees"])
        a.set_editor_property("tags", ["VS_AttackerSpawn" if entry["role"] == "attacker" else "VS_DefenderSpawn"])
        box(entry["id"] + "Pad", (*entry["location_cm"][:2], 0.5), (140, 140, 0.5), "PaintedMetal", False)
    site = spawn(unreal.TriggerBox, d.SITE["actor_label"], d.SITE["center_cm"])
    site.set_editor_property("tags", ["VS_Site_A"])
    site.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(*d.SITE["half_extents_cm"]), False)
    site.get_component_by_class(unreal.BoxComponent).set_collision_profile_name("NoCollision")
    recovery = spawn(unreal.TargetPoint, d.RECOVERY["actor_label"], d.RECOVERY["location_cm"])
    recovery.set_editor_property("tags", ["VS_DeviceRecovery"])
    recovery.set_actor_enable_collision(False)

    def lettering(label, text, pos, yaw, size, color):
        a = spawn(unreal.TextRenderActor, label, pos, yaw)
        c = a.get_component_by_class(unreal.TextRenderComponent)
        c.set_text(text)
        c.set_world_size(size)
        c.set_text_render_color(unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
        c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        c.set_collision_profile_name("NoCollision")
    lettering("VS_SiteSign", "A", (316, -230, 210), 0, 170, (226, 176, 91))
    lettering("VS_RelaySign", "RELAY  /  01", (-500, -711, 310), -90, 45, (56, 71, 76))
    lettering("VS_ServiceSign", "SERVICE   03", (600, 1240, 275), -90, 40, (56, 71, 76))
    sun = spawn(unreal.DirectionalLight, "VS_Sun", (0, 0, 1500))
    sun.set_actor_rotation(unreal.Rotator(pitch=-48, yaw=-35), False)
    c = sun.get_component_by_class(unreal.DirectionalLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property("atmosphere_sun_light", True)
    c.set_editor_property("light_source_angle", 3.0)
    c.set_intensity(3.0)
    fill = spawn(unreal.DirectionalLight, "VS_StylizedFill", (0, 0, 1400))
    fill.set_actor_rotation(unreal.Rotator(pitch=-45, yaw=145), False)
    c = fill.get_component_by_class(unreal.DirectionalLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property("cast_shadows", False)
    c.set_intensity(1.5)
    spawn(unreal.SkyAtmosphere, "VS_Atmosphere", (0, 0, 0))
    sky = spawn(unreal.SkyLight, "VS_Sky", (0, 0, 1200))
    c = sky.get_component_by_class(unreal.SkyLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property("real_time_capture", True)
    c.set_editor_property("lower_hemisphere_is_black", False)
    c.set_intensity(1.0)
    require(levels.save_current_level(), "Save failed")
    require(levels.load_level(d.MAP), "Reopen failed")
    saved_labels = {a.get_actor_label() for a in actors.get_all_level_actors()}
    require(labels <= saved_labels, "Actors missing after reopen")
    out = ROOT / "docs/maps/site01"
    out.mkdir(parents=True, exist_ok=True)
    (out / "layout.json").write_text(json.dumps(d.contract(), indent=2) + "\n")
    data = {"engine": unreal.SystemLibrary.get_engine_version(), "map_asset": d.MAP,
            "generated_actor_count": len(labels), "saved_reopened_actor_count": len(saved_labels),
            "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "design_sha256": hashlib.sha256((HERE / "design.py").read_bytes()).hexdigest(),
            "runtime_verified": False, "status": "saved_and_reopened"}
    (out / "generation-report.json").write_text(json.dumps(data, indent=2) + "\n")
    unreal.log("VALSTRIKE_SITE01_BUILD_SUCCESS")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_SITE01_BUILD_FAILED: {error}")
        raise
