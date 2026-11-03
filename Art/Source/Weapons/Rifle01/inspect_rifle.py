"""ART-02: native rifle inspection and composition captures in Unreal 5.8.4.

Run with -ExecutePythonScript in a fresh, unattended, rendering editor process.
The scene and its neutral presentation material remain transient: no Unreal
asset or map is saved. A first-person composition is not gameplay verification.
"""

import hashlib
import json
import math
from pathlib import Path
import struct
import time
import traceback
import uuid

import unreal


ROOT = Path(__file__).resolve().parents[4]
BASE = "/Game/Art/Weapons/Rifle01"
BODY = "SM_VS_Rifle01_Body"
MAGAZINE = "SM_VS_Rifle01_Magazine"
SOCKETS = {"Grip": (0, 0, 0), "Magazine": (14, 0, 2),
           "Muzzle": (56, 0, 8.2), "SupportHand": (31, 0, 2.4)}
REPORT_DIR = ROOT / "docs/assets/rifle01"
RESOLUTION = (1600, 1000)
TIMEOUT_SECONDS = 120.0
STATE = {"handle": None, "task": None, "finished": False, "capture_index": 0}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def xyz(vector):
    return [round(getattr(vector, axis), 5) for axis in ("x", "y", "z")]


def distance(a, b):
    return math.sqrt(sum((getattr(a, axis) - getattr(b, axis)) ** 2 for axis in ("x", "y", "z")))


def transform_data(transform):
    rotation = transform.get_editor_property("rotation")
    return {
        "translation_cm": xyz(transform.get_editor_property("translation")),
        "rotation_quaternion_xyzw": [round(getattr(rotation, axis), 6) for axis in ("x", "y", "z", "w")],
        "scale": xyz(transform.get_editor_property("scale3d")),
    }


def spawn(actor_class, label, location, rotation=None):
    actor = STATE["actors"].spawn_actor_from_class(
        actor_class, unreal.Vector(*location), rotation or unreal.Rotator())
    require(actor, f"Could not create inspection actor: {label}")
    actor.set_actor_label(label)
    return actor


def load_rifle():
    STATE["meshes"] = {name: unreal.load_asset(f"{BASE}/Meshes/{name}") for name in (BODY, MAGAZINE)}
    require(all(isinstance(mesh, unreal.StaticMesh) for mesh in STATE["meshes"].values()),
            "The saved native rifle body and magazine must exist")
    body_bounds = STATE["meshes"][BODY].get_bounding_box()
    require(abs(body_bounds.min.x + 30) < 0.1 and abs(body_bounds.max.x - 56) < 0.1,
            "Expected original 86 cm body with +X muzzle and grip origin")
    body_mesh = STATE["meshes"][BODY]
    native_sockets = {}
    for name, expected_location in SOCKETS.items():
        socket = body_mesh.find_socket(name)
        require(socket, f"Missing saved native socket: {name}")
        location = socket.get_editor_property("relative_location")
        scale = socket.get_editor_property("relative_scale")
        rotation = socket.get_editor_property("relative_rotation")
        require(distance(location, unreal.Vector(*expected_location)) < 0.01, f"Socket location mismatch: {name}")
        require(distance(scale, unreal.Vector(1, 1, 1)) < 0.001, f"Socket scale mismatch: {name}")
        require(all(abs(getattr(rotation, axis)) < 0.001 for axis in ("pitch", "yaw", "roll")),
                f"Expected identity socket rotation: {name}")
        native_sockets[name] = {"location_cm": xyz(location), "rotation_degrees": {
            axis: round(getattr(rotation, axis), 6) for axis in ("pitch", "yaw", "roll")}, "scale": xyz(scale)}
    STATE["report"]["saved_sockets_component_space"] = native_sockets
    STATE["report"]["native_body_bounds_cm"] = {"min": xyz(body_bounds.min), "max": xyz(body_bounds.max)}


def equipped_mesh(name):
    actor = spawn(unreal.StaticMeshActor, name + " - inspection", (0, 0, 0))
    component = actor.static_mesh_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    require(component.set_static_mesh(STATE["meshes"][name]), f"Could not assign {name}")
    component.set_collision_profile_name("NoCollision")
    require(component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION,
            f"Equipped presentation must not collide: {name}")
    return actor


def create_stage():
    STATE["world"] = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    require(STATE["world"], "Could not create an unsaved editor world")
    STATE["actors"] = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    STATE["body"] = equipped_mesh(BODY)
    STATE["magazine"] = equipped_mesh(MAGAZINE)
    body_component = STATE["body"].static_mesh_component
    rule = unreal.AttachmentRule.SNAP_TO_TARGET
    require(STATE["magazine"].attach_to_component(body_component, "Magazine", rule, rule, rule, False),
            "Could not attach magazine to saved body socket")

    # A temporary neutral material makes the capture independent of unrelated
    # environment assets. It belongs to /Engine/Transient and is never saved.
    ground_material = unreal.Material(name="RifleInspectionGround_" + STATE["run_id"])
    require(ground_material.get_path_name().startswith("/Engine/Transient"), "Ground material must be transient")
    library = unreal.MaterialEditingLibrary
    color = library.create_material_expression(ground_material, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property("constant", unreal.LinearColor(0.32, 0.34, 0.35, 1))
    roughness = library.create_material_expression(ground_material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property("r", 0.9)
    require(library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR), "Ground base color")
    require(library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS), "Ground roughness")
    require(not library.recompile_material(ground_material), "Transient ground shader compilation failed")
    STATE["ground_material"] = ground_material
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    require(cube, "Engine presentation cube unavailable")
    floor = spawn(unreal.StaticMeshActor, "Studio ground - unsaved", (0, 0, -1.1))
    floor.static_mesh_component.set_static_mesh(cube)
    floor.static_mesh_component.set_material(0, ground_material)
    floor.static_mesh_component.set_collision_profile_name("NoCollision")
    floor.set_actor_scale3d(unreal.Vector(40, 40, 0.02))
    STATE["floor"] = floor
    # Backdrop is used only for the first-person composition, without fake HUD
    # or game-world context. A distant neutral wall fills its field of view.
    backdrop = spawn(unreal.StaticMeshActor, "First-person backdrop - unsaved", (700, 0, 160))
    backdrop.static_mesh_component.set_static_mesh(cube)
    backdrop.static_mesh_component.set_material(0, ground_material)
    backdrop.static_mesh_component.set_collision_profile_name("NoCollision")
    backdrop.set_actor_scale3d(unreal.Vector(0.1, 30, 25))
    STATE["backdrop"] = backdrop

    key_rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(-200, -350, 450), unreal.Vector(0, 0, 10))
    sun = spawn(unreal.DirectionalLight, "Inspection sun - unsaved", (-200, -350, 450), key_rotation)
    sun_component = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sun_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun_component.set_intensity(3.0)
    sun_component.set_editor_property("atmosphere_sun_light", True)
    sun_component.set_editor_property("light_source_angle", 3.0)
    spawn(unreal.SkyAtmosphere, "Inspection atmosphere - unsaved", (0, 0, 0))
    sky = spawn(unreal.SkyLight, "Inspection sky - unsaved", (0, 0, 400))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_editor_property("lower_hemisphere_is_black", False)
    sky_component.set_intensity(1.0)
    for command in ("r.EyeAdaptationQuality 0", "r.DefaultFeature.AutoExposure 0",
                    "r.DefaultFeature.MotionBlur 0", "r.DefaultFeature.Bloom 0"):
        unreal.SystemLibrary.execute_console_command(STATE["world"], command)
    camera = spawn(unreal.CameraActor, "Inspection camera - unsaved", (100, -180, 85))
    camera_component = camera.get_component_by_class(unreal.CameraComponent)
    camera_component.set_aspect_ratio(RESOLUTION[0] / RESOLUTION[1])
    camera_component.set_constraint_aspect_ratio(True)
    post_process = camera_component.get_editor_property("post_process_settings")
    for property_name, value in {
        "override_auto_exposure_min_brightness": True, "auto_exposure_min_brightness": 1.0,
        "override_auto_exposure_max_brightness": True, "auto_exposure_max_brightness": 1.0,
        "override_auto_exposure_bias": True, "auto_exposure_bias": 0.0,
    }.items():
        post_process.set_editor_property(property_name, value)
    camera_component.set_editor_property("post_process_settings", post_process)
    camera_component.set_editor_property("post_process_blend_weight", 1.0)
    STATE["camera"] = camera
    STATE["camera_component"] = camera_component
    unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
    STATE["report"]["lighting"] = {"sun_intensity": 3.0, "captured_sky_intensity": 1.0,
                                   "key_ray_direction": xyz(key_rotation.get_forward_vector()),
                                   "auto_exposure": False, "exposure_bias": 0.0}


def check_assembly():
    body = STATE["body"]
    magazine = STATE["magazine"]
    component = body.static_mesh_component
    magazine_component = magazine.static_mesh_component
    require(magazine_component.get_attach_parent() == component, "Magazine parent component differs")
    require(str(magazine_component.get_attach_socket_name()) == "Magazine", "Magazine socket differs")
    socket_world = component.get_socket_transform("Magazine", unreal.RelativeTransformSpace.RTS_WORLD)
    magazine_world = magazine_component.get_world_transform()
    socket_position = socket_world.get_editor_property("translation")
    magazine_position = magazine_world.get_editor_property("translation")
    position_error = distance(socket_position, magazine_position)
    require(position_error < 0.01, f"Magazine attachment position error: {position_error} cm")
    relative = magazine_component.get_relative_transform()
    relative_data = transform_data(relative)
    require(max(abs(value) for value in relative_data["translation_cm"]) < 0.001, "Magazine relative location is not zero")
    require(max(abs(value - 1) for value in relative_data["scale"]) < 0.001, "Magazine attachment is scaled")
    require(max(abs(value) for value in relative_data["rotation_quaternion_xyzw"][:3]) < 0.001,
            "Magazine attachment rotated away from its socket")
    for actor in (body, magazine):
        require(distance(actor.get_actor_scale3d(), unreal.Vector(1, 1, 1)) < 0.001, "Inspection must use scale 1")
        require(actor.static_mesh_component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION,
                "Equipped actor unexpectedly collides")
    forward = component.get_socket_rotation("Muzzle").get_forward_vector()
    require(distance(forward, body.get_actor_forward_vector()) < 0.001, "Muzzle does not point along native +X")
    return {
        "body_world_transform": transform_data(component.get_world_transform()),
        "magazine_world_transform": transform_data(magazine_world),
        "magazine_socket_world_transform": transform_data(socket_world),
        "magazine_relative_transform": relative_data,
        "attachment_position_error_cm": round(position_error, 6),
        "attachment_socket": "Magazine", "weld_simulated_bodies": False,
        "muzzle_forward_world": xyz(forward),
        "socket_world_transforms": {name: transform_data(component.get_socket_transform(
            name, unreal.RelativeTransformSpace.RTS_WORLD)) for name in SOCKETS},
        "equipped_component_collision": {name: {
            "profile": str(actor.static_mesh_component.get_collision_profile_name()),
            "enabled": str(actor.static_mesh_component.get_collision_enabled()),
        } for name, actor in ((BODY, body), (MAGAZINE, magazine))},
    }


def pose_capture():
    index = STATE["capture_index"]
    body = STATE["body"]
    if index == 0:
        # Place the assembly just above the studio floor at true centimeter scale.
        bottom = min(STATE["meshes"][BODY].get_bounding_box().min.z,
                     STATE["meshes"][MAGAZINE].get_bounding_box().min.z + SOCKETS["Magazine"][2])
        body.set_actor_location_and_rotation(unreal.Vector(0, 0, 0.1 - bottom), unreal.Rotator(), False, True)
        target = unreal.Vector(10, 0, 0.1 - bottom + 1)
        camera_location = unreal.Vector(90, -180, 85)
        camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, target)
        fov = 32.0
        kind = "Studio"
        composition = "Native assembled body and socket-attached magazine; scale 1; no gameplay"
    else:
        # UE +X is forward and +Y is camera-right. This is an art composition,
        # with no player, hands, firing logic, HUD, or first-person mesh pass.
        camera_location = unreal.Vector(0, 0, 160)
        camera_rotation = unreal.Rotator(pitch=0, yaw=0, roll=0)
        offset = unreal.Vector(32, 16, -18)
        body.set_actor_location_and_rotation(
            unreal.Vector(camera_location.x + offset.x, camera_location.y + offset.y,
                          camera_location.z + offset.z), unreal.Rotator(), False, True)
        fov = 85.0
        kind = "FirstPerson"
        composition = "First-person visual composition only; grip offset (32,16,-18) cm; no gameplay or arms"
    STATE["camera"].set_actor_location_and_rotation(camera_location, camera_rotation, False, True)
    STATE["camera_component"].set_field_of_view(fov)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera_location, camera_rotation)
    assembly = check_assembly()
    capture = {"kind": kind, "composition": composition, "resolution": list(RESOLUTION),
               "camera_location_cm": xyz(camera_location),
               "camera_rotation_degrees": {axis: round(getattr(camera_rotation, axis), 6)
                                           for axis in ("pitch", "yaw", "roll")},
               "horizontal_fov_degrees": fov, "assembly": assembly}
    if index == 1:
        muzzle = STATE["body"].static_mesh_component.get_socket_location("Muzzle")
        x, y, z = muzzle.x - camera_location.x, muzzle.y - camera_location.y, muzzle.z - camera_location.z
        half_width = x * math.tan(math.radians(fov / 2))
        half_height = half_width * RESOLUTION[1] / RESOLUTION[0]
        uv = (0.5 + y / (2 * half_width), 0.5 - z / (2 * half_height))
        require(0.53 < uv[0] < 0.85 and 0.52 < uv[1] < 0.85, f"Muzzle outside lower-right composition: {uv}")
        capture["muzzle_expected_normalized_image_xy"] = [round(value, 5) for value in uv]
        capture["grip_offset_from_camera_cm"] = [32, 16, -18]
        capture["rear_stock_may_be_cropped_by_camera_near_plane"] = True
    capture["output"] = f"docs/assets/rifle01/Previews/VS_Rifle01_Unreal_{kind}.png"
    STATE["current_capture"] = capture
    STATE["pending_png"] = REPORT_DIR / "Previews" / f".rifle-{STATE['run_id']}-{kind}.png"
    STATE["pose_started"] = time.monotonic()


def finish(error=None):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    if STATE["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(STATE["handle"])
        STATE["handle"] = None
    if error:
        unreal.log_error(f"VALSTRIKE_RIFLE_INSPECTION_FAILED: {error}")
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta_time):
    try:
        require(time.monotonic() - STATE["started"] < TIMEOUT_SECONDS,
                f"Rifle inspection exceeded {TIMEOUT_SECONDS:g} seconds")
        if STATE["task"] is None:
            if time.monotonic() - STATE["pose_started"] < 3:
                return
            STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(
                *RESOLUTION, str(STATE["pending_png"]), camera=STATE["camera"], delay=2.0, force_game_view=True)
            require(STATE["task"] and STATE["task"].is_valid_task(), "No valid editor screenshot task")
            return
        if not STATE["task"].is_task_done():
            return
        pending = STATE["pending_png"]
        require(pending.is_file(), f"Screenshot completed without file: {pending}")
        data = pending.read_bytes()
        require(len(data) > 1000 and data[:8] == b"\x89PNG\r\n\x1a\n", "Invalid screenshot PNG")
        require(struct.unpack(">II", data[16:24]) == RESOLUTION, "Unexpected screenshot resolution")
        capture = STATE["current_capture"]
        capture["sha256"] = hashlib.sha256(data).hexdigest()
        capture["capture_completed"] = True
        STATE["report"]["captures"].append(capture)
        STATE["completed_files"].append((pending, ROOT / capture["output"]))
        STATE["capture_index"] += 1
        STATE["task"] = None
        if STATE["capture_index"] < 2:
            pose_capture()
            return
        # Commit evidence only after BOTH unique screenshots completed. An old
        # preview can never count as the result of this invocation.
        STATE["report"]["elapsed_seconds"] = round(time.monotonic() - STATE["started"], 3)
        report = REPORT_DIR / "unreal-inspection-report.json"
        temporary_report = REPORT_DIR / f".rifle-{STATE['run_id']}-report.json"
        temporary_report.write_text(json.dumps(STATE["report"], indent=2) + "\n", encoding="utf-8")
        for temporary, final in STATE["completed_files"]:
            temporary.replace(final)
        temporary_report.replace(report)
        unreal.log(f"VALSTRIKE_RIFLE_INSPECTION_SUCCESS: {report}; two native screenshots")
        finish()
    except Exception:
        finish(traceback.format_exc())


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the isolated rifle asset worktree")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "End PIE first")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Use a fresh editor without unsaved maps")
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["started"] = time.monotonic()
    STATE["run_id"] = uuid.uuid4().hex
    STATE["completed_files"] = []
    STATE["report"] = {"engine": unreal.SystemLibrary.get_engine_version(), "run_id": STATE["run_id"],
                       "scene": "fresh unsaved editor world; no packages saved", "captures": [],
                       "native_coordinate_convention": "+X forward; +Y camera-right; +Z up; centimeters",
                       "source_to_native_convention": "FBX reflects source Y; socket Y positions are all zero",
                       "runtime_verified": False, "gameplay_verified": False,
                       "first_person_integration_verified": False, "animation_verified": False}
    (REPORT_DIR / "Previews").mkdir(parents=True, exist_ok=True)
    load_rifle()
    create_stage()
    pose_capture()
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        finish(traceback.format_exc())
