"""INT-ART-01: place the ART-16 pilot props in /Game/Maps/Site01/VS_Site01.

Adds exactly the nine visuals of docs/assets/meshy-site01/props-surfaces/pilot-placement.md
(NoCollision, Static), hides only the rendering of the three replaced crate actors
(their collision/transform stay), and gives VS_Floor / VS_GalleryPaving map-local
masked material copies that open the 400x400 cm tile patch. Does not rerun the
whole-map builder. Every pre-existing actor's transform, tags and collision are
snapshotted and must be unchanged after save + reopen.

Native transform adjustment (coordinator-authorized, INT-ART-01-RESUME.md): the FBX
import negates Y, so each pivot is solved from the measured native bounds to reach
the recipe's exact world envelope; W07's native front is +Y, so it is yawed 180
degrees to face the -Y route with its back on the existing wall. No scale/mirroring.

Modes: default = place, save, reopen-verify, capture. -VSInspectOnly = reopen-verify
and capture without saving. Captures are Unreal editor viewport images, not runtime.
"""

import hashlib
import json
from pathlib import Path
import struct
import time
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
MAP = "/Game/Maps/Site01/VS_Site01"
MAP_FILE = ROOT / "Content/Maps/Site01/VS_Site01.umap"
PROPS = "/Game/Art/Environment/MeshySite01/PropsSurfaces"
MATS = "/Game/Maps/Site01/Materials"
REPORTS = ROOT / "docs/assets/meshy-pilot-integration"
PREVIEWS = REPORTS / "previews"
FOLDER = "Site01/MeshyPilot01"
RESOLUTION = (1600, 1000)
PATCH = (-100.0, 300.0, 750.0, 1150.0)  # world X min/max, Y min/max of the tile patch

MESHES = {"W03": f"{PROPS}/W03/SM_VS_MSH_W03_ConcreteTile", "W04": f"{PROPS}/W04/SM_VS_MSH_W04_LongCargoCrate",
          "W07": f"{PROPS}/W07/SM_VS_MSH_W07_InfillPanel", "W10": f"{PROPS}/W10/SM_VS_MSH_W10_CargoCrate"}
# label, asset, recipe pivot, recipe world bounds min, max, yaw needed for the intended facing
PLACEMENTS = [
    ("VS_MSH_W03_Gallery_00", "W03", (-100, 750, 0), (-100, 750, -10), (100, 950, 0), 0),
    ("VS_MSH_W03_Gallery_10", "W03", (100, 750, 0), (100, 750, -10), (300, 950, 0), 0),
    ("VS_MSH_W03_Gallery_01", "W03", (-100, 950, 0), (-100, 950, -10), (100, 1150, 0), 0),
    ("VS_MSH_W03_Gallery_11", "W03", (100, 950, 0), (100, 950, -10), (300, 1150, 0), 0),
    ("VS_MSH_W04_ServiceRoof", "W04", (-200, 1375, 394), (-300, 1325, 394), (-100, 1425, 494), 0),
    ("VS_MSH_W07_ServiceInfill", "W07", (-300, 1230.5, 0), (-300, 1230.5, 0), (-100, 1250.5, 300), 180),
    ("VS_MSH_W10_North", "W10", (1150, 950, 0), (1100, 900, 0), (1200, 1000, 100), 0),
    ("VS_MSH_W10_SiteBase", "W10", (1460, -300, 0), (1410, -350, 0), (1510, -250, 100), 0),
    ("VS_MSH_W10_SiteTop", "W10", (1460, -300, 100), (1410, -350, 100), (1510, -250, 200), 0),
]
HIDDEN_CRATES = ("VS_NorthCrate", "VS_SiteEastCrate", "VS_SiteEastCrateTop")
MASKED = {"VS_Floor": ("M_Site01_Paving", "M_Site01_Paving_PilotTileMask"),
          "VS_GalleryPaving": ("M_Site01_Lane", "M_Site01_Lane_PilotTileMask")}
VIEWS = [
    ("tiles_standing", (650, 950, 152), (100, 950, 0)),
    ("tiles_crouched_grazing", (700, 820, 62), (0, 1000, 0)),
    ("tiles_top_down", (100, 950, 700), (100, 951, 0)),
    ("service_panel", (-200, 700, 152), (-200, 1250, 150)),
    ("service_panel_close", (-60, 1150, 140), (-200, 1240, 150)),
    ("service_roof_crate", (-200, 250, 152), (-200, 1375, 430)),
    ("north_crate", (1150, 600, 152), (1150, 950, 50)),
    ("site_crate_stack", (1150, -300, 152), (1460, -300, 100)),
]
STATE = {"phase": "start", "view": 0, "task": None, "busy": False, "done": False, "report": {}}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def xyz(vector):
    return [round(getattr(vector, axis), 3) for axis in ("x", "y", "z")]


def close(a, b, tolerance=0.05):
    return all(abs(x - y) <= tolerance for x, y in zip(a, b))


def subsystem(cls):
    return unreal.get_editor_subsystem(cls)


def actors_by_label():
    result = {}
    for actor in subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        label = actor.get_actor_label()
        require(label not in result, f"Duplicate label {label}")
        result[label] = actor
    return result


def snapshot(actor):
    rotation = actor.get_actor_rotation()
    primitives = []
    for component in actor.get_components_by_class(unreal.PrimitiveComponent):
        primitives.append({"name": component.get_name(), "profile": str(component.get_collision_profile_name()),
                           "enabled": str(component.get_collision_enabled())})
    return {"class": actor.get_class().get_name(), "location": xyz(actor.get_actor_location()),
            "rotation": [round(rotation.roll, 3), round(rotation.pitch, 3), round(rotation.yaw, 3)],
            "scale": xyz(actor.get_actor_scale3d()), "tags": [str(t) for t in actor.get_editor_property("tags")],
            "primitives": sorted(primitives, key=lambda p: p["name"])}


def masked_copy(source, target):
    src, dst = f"{MATS}/{source}", f"{MATS}/{target}"
    require(not unreal.EditorAssetLibrary.does_asset_exist(dst), f"{dst} exists; inspect, do not overwrite")
    material = unreal.EditorAssetLibrary.duplicate_asset(src, dst)
    require(isinstance(material, unreal.Material), f"Could not duplicate {src}")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    lib = unreal.MaterialEditingLibrary
    position = lib.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -700, 400)
    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -420, 400)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property("description", "INT-ART-01 W03 tile patch mask")
    pin = unreal.CustomInput()
    pin.set_editor_property("input_name", "WP")
    custom.set_editor_property("inputs", [pin])
    x0, x1, y0, y1 = PATCH
    custom.set_editor_property("code", (
        f"float inside = step({x0}, WP.x) * step(WP.x, {x1}) * step({y0}, WP.y) * step(WP.y, {y1});"
        " return 1.0 - inside;"))
    require(lib.connect_material_expressions(position, "", custom, "WP"), "Mask input connection")
    require(lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_OPACITY_MASK), "Mask output")
    require(not lib.recompile_material(material), f"Masked material compile failed: {dst}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False), f"Save failed: {dst}")
    return material


def world_bounds(mesh, location, yaw):
    box = mesh.get_bounding_box()
    rotation = unreal.Rotator(yaw=yaw)
    corners = [unreal.MathLibrary.greater_greater_vector_rotator(unreal.Vector(x, y, z), rotation)
               for x in (box.min.x, box.max.x) for y in (box.min.y, box.max.y) for z in (box.min.z, box.max.z)]
    low = [location[i] + min(getattr(c, a) for c in corners) for i, a in enumerate("xyz")]
    high = [location[i] + max(getattr(c, a) for c in corners) for i, a in enumerate("xyz")]
    return low, high


def solve_pivot(mesh, wanted_min, yaw):
    low, _ = world_bounds(mesh, (0, 0, 0), yaw)
    return [wanted_min[i] - low[i] for i in range(3)]


def place(labels):
    actors = subsystem(unreal.EditorActorSubsystem)
    conversion = []
    for label, key, recipe_pivot, wanted_min, wanted_max, yaw in PLACEMENTS:
        require(label not in labels, f"{label} already placed; inspect, do not duplicate")
        mesh = unreal.load_asset(MESHES[key])
        require(isinstance(mesh, unreal.StaticMesh), f"Missing native {key}")
        pivot = solve_pivot(mesh, wanted_min, yaw)
        low, high = world_bounds(mesh, pivot, yaw)
        require(close(low, wanted_min, 0.1) and close(high, wanted_max, 0.1),
                f"{label}: native envelope {low}/{high} cannot match recipe {wanted_min}/{wanted_max}")
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*pivot), unreal.Rotator(yaw=yaw))
        require(actor, f"Spawn failed: {label}")
        actor.set_actor_label(label)
        actor.set_folder_path(FOLDER)
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_mobility(unreal.ComponentMobility.STATIC)
        component.set_collision_profile_name("NoCollision")
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
        conversion.append({"label": label, "asset": MESHES[key], "recipe_pivot_cm": list(recipe_pivot),
                           "native_pivot_cm": [round(v, 3) for v in pivot], "yaw_degrees": yaw,
                           "native_local_bounds_cm": [xyz(mesh.get_bounding_box().min), xyz(mesh.get_bounding_box().max)],
                           "world_bounds_cm": [[round(v, 3) for v in low], [round(v, 3) for v in high]]})
    for label in HIDDEN_CRATES:
        component = labels[label].static_mesh_component
        component.set_editor_property("hidden_in_game", True)
        component.set_editor_property("cast_hidden_shadow", False)
    for label, (source, target) in MASKED.items():
        labels[label].static_mesh_component.set_material(0, masked_copy(source, target))
    return conversion


def trace(start, end):
    world = subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    hit = unreal.SystemLibrary.line_trace_single_by_profile(
        world, unreal.Vector(*start), unreal.Vector(*end), "Pawn", False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    fields = hit.to_tuple()
    return {"hit": bool(fields[0]), "impact_cm": xyz(fields[5]),
            "actor": fields[9].get_actor_label() if fields[9] else None}


def crate_collision_checks():
    """Original crate collision must not exceed the new 100 cm visual envelope."""
    results = []
    for label, center, top in (("VS_NorthCrate", (1150, 950), 100), ("VS_SiteEastCrateTop", (1460, -300), 200)):
        down = trace((center[0], center[1], top + 200), (center[0], center[1], 0))
        results.append({"check": f"{label}/top", **(down or {})})
        require(down and down["hit"] and down["impact_cm"][2] <= top + 0.6, f"{label} collision above visual: {down}")
    for label, center, z in (("VS_NorthCrate", (1150, 950), 50), ("VS_SiteEastCrate", (1460, -300), 50),
                             ("VS_SiteEastCrateTop", (1460, -300), 150)):
        for axis, sign in ((0, 1), (0, -1), (1, 1), (1, -1)):
            start = [center[0], center[1], z]
            start[axis] += sign * 150
            hit = trace(start, (center[0], center[1], z))
            results.append({"check": f"{label}/side/{'xy'[axis]}{'+' if sign > 0 else '-'}", **(hit or {})})
            require(hit and hit["hit"] and abs(hit["impact_cm"][axis] - center[axis]) <= 50.6,
                    f"{label} collision outside visual envelope: {hit}")
    return results


def verify(before):
    labels = actors_by_label()
    changed = []
    for label, prior in before.items():
        require(label in labels, f"Pre-existing actor missing after reopen: {label}")
        if snapshot(labels[label]) != prior:
            changed.append(label)
    require(not changed, f"Pre-existing actor transform/tags/collision changed: {changed}")
    placed = []
    for label, key, _, wanted_min, wanted_max, yaw in PLACEMENTS:
        require(label in labels, f"Missing placed actor after reopen: {label}")
        actor = labels[label]
        component = actor.static_mesh_component
        low, high = world_bounds(component.static_mesh, xyz(actor.get_actor_location()), actor.get_actor_rotation().yaw)
        ok = (close(low, wanted_min, 0.1) and close(high, wanted_max, 0.1)
              and component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
              and component.static_mesh.get_path_name().startswith(MESHES[key]))
        require(ok, f"{label} wrong after reopen: {low}/{high}")
        placed.append({"label": label, "world_bounds_cm": [low, high], "collision": "NoCollision"})
    for label in HIDDEN_CRATES:
        component = labels[label].static_mesh_component
        require(component.get_editor_property("hidden_in_game") and str(component.get_collision_profile_name()) != "NoCollision",
                f"{label} must be hidden in game but keep collision")
    for label, (_, target) in MASKED.items():
        require(labels[label].static_mesh_component.get_material(0).get_name() == target, f"{label} mask material")
    return {"preexisting_actors_unchanged": len(before), "placed": placed, "crate_collision": crate_collision_checks(),
            "hidden_in_game": list(HIDDEN_CRATES), "masked_materials": {k: v[1] for k, v in MASKED.items()}}


def create_camera(view_id, location, target):
    position = unreal.Vector(*location)
    rotation = unreal.MathLibrary.find_look_at_rotation(position, unreal.Vector(*target))
    camera = subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.CameraActor, position, rotation)
    camera.set_actor_label(f"INTART01_{view_id}_UNSAVED")
    component = camera.get_component_by_class(unreal.CameraComponent)
    component.set_field_of_view(70.0)
    component.set_editor_property("aspect_ratio", RESOLUTION[0] / RESOLUTION[1])
    settings = component.get_editor_property("post_process_settings")
    for key, value in (("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", 1.0),
                       ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 1.0),
                       ("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.0)):
        settings.set_editor_property(key, value)
    component.set_editor_property("post_process_settings", settings)
    subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(position, rotation)
    return camera


def finish(error=None):
    if STATE["done"]:
        return
    STATE["done"] = True
    unreal.unregister_slate_post_tick_callback(STATE["handle"])
    report = STATE["report"]
    report.update(passed=error is None, error=error,
                  map_sha256=hashlib.sha256(MAP_FILE.read_bytes()).hexdigest())
    name = "site01-placement-reopen-report.json" if STATE["inspect_only"] else "site01-placement-report.json"
    (REPORTS / name).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if error:
        unreal.log_error(f"VALSTRIKE_MESHY_SITE01_FAILED: {error}")
    else:
        unreal.log(f"VALSTRIKE_MESHY_SITE01_SUCCESS: {REPORTS / name}")
    unreal.SystemLibrary.quit_editor()


def tick(_delta):
    if STATE["busy"] or STATE["done"]:
        return
    STATE["busy"] = True
    try:
        require(time.monotonic() - STATE["started"] < 300, "Site01 placement exceeded 300 s")
        if STATE["phase"] == "start":
            if time.monotonic() - STATE["started"] < 3.0:
                return
            levels = subsystem(unreal.LevelEditorSubsystem)
            if not STATE["inspect_only"]:
                labels = actors_by_label()
                before = {label: snapshot(actor) for label, actor in labels.items()}
                (REPORTS / "site01-preexisting-snapshot.json").write_text(json.dumps(before, indent=1) + "\n")
                STATE["report"]["native_conversion"] = place(labels)
                require(levels.save_current_level(), "Map save failed")
                require(levels.load_level(MAP), "Map reopen failed")
            before = json.loads((REPORTS / "site01-preexisting-snapshot.json").read_text())
            STATE["report"]["verification"] = verify(before)
            STATE["phase"] = "capture"
            return
        if STATE["task"] is None:
            if STATE["view"] >= len(VIEWS):
                finish()
                return
            view_id, location, target = VIEWS[STATE["view"]]
            STATE["camera"] = create_camera(view_id, location, target)
            STATE["file"] = PREVIEWS / f"{view_id}.png"
            STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(
                *RESOLUTION, str(STATE["file"]), camera=STATE["camera"], delay=2.0, force_game_view=True)
            require(STATE["task"] and STATE["task"].is_valid_task(), "No screenshot task")
            return
        if not STATE["task"].is_task_done():
            return
        data = STATE["file"].read_bytes()
        require(data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == RESOLUTION, "Bad capture")
        view_id, location, target = VIEWS[STATE["view"]]
        STATE["report"].setdefault("captures", []).append({
            "id": view_id, "camera_cm": list(location), "target_cm": list(target),
            "file": str(STATE["file"].relative_to(ROOT)), "sha256": hashlib.sha256(data).hexdigest(),
            "kind": "Unreal editor viewport, game view, fixed exposure; not runtime"})
        subsystem(unreal.EditorActorSubsystem).destroy_actor(STATE["camera"])
        STATE["view"] += 1
        STATE["task"] = None
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-01 worktree")
    levels = subsystem(unreal.LevelEditorSubsystem)
    require(not levels.is_in_play_in_editor(), "End PIE first")
    require(levels.load_level(MAP), "Could not open Site01")
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    STATE["inspect_only"] = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    STATE["report"] = {"engine": unreal.SystemLibrary.get_engine_version(), "map": MAP,
                       "mode": "reopen_inspect" if STATE["inspect_only"] else "place_save_reopen",
                       "runtime_verified": False,
                       "recipe": "docs/assets/meshy-site01/props-surfaces/pilot-placement.md"}
    for command in ("r.EyeAdaptationQuality 0", "r.DefaultFeature.AutoExposure 0", "r.DefaultFeature.MotionBlur 0"):
        unreal.SystemLibrary.execute_console_command(None, command)
    unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
    STATE["started"] = time.monotonic()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error(f"VALSTRIKE_MESHY_SITE01_FAILED: {traceback.format_exc()}")
        unreal.SystemLibrary.quit_editor()
