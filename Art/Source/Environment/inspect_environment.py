"""ART-01: inspect saved imports in an unsaved Unreal Editor 5.8.4 scene.

Run with -ExecutePythonScript, ordinary rendering, and -unattended in a fresh
editor process. This script never saves a map, material, or mesh. It produces
an actual native viewport PNG and editor collision-query evidence, not gameplay
or packaged-client verification. See docs/assets/IMPORT.md for the invocation.
"""

import json
from pathlib import Path
import struct
import time
import traceback
import uuid

import unreal


ROOT = Path(__file__).resolve().parents[3]
BASE = "/Game/Art/Environment"
OUTPUT = ROOT / "docs/assets/Previews/VS_Environment01_Unreal.png"
REPORT = ROOT / "docs/assets/unreal-inspection-report.json"
NAMES = (
    "SM_VS_Wall_400x300", "SM_VS_Pillar_40x300", "SM_VS_Crate_100",
    "SM_VS_Cover_200x120", "SM_VS_Vent_120x80",
)
RESOLUTION = (1600, 1000)
TIMEOUT_SECONDS = 120.0
STATE = {"handle": None, "task": None, "finished": False}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def xyz(vector):
    return [round(getattr(vector, axis), 4) for axis in ("x", "y", "z")]


def spawn(actor_class, name, location, rotation=None):
    actor = STATE["actors"].spawn_actor_from_class(
        actor_class, unreal.Vector(*location), rotation or unreal.Rotator())
    require(actor, f"Could not spawn inspection actor: {name}")
    actor.set_actor_label(name)
    return actor


def mesh_actor(name, location):
    actor = spawn(unreal.StaticMeshActor, name, location)
    component = actor.static_mesh_component
    require(component.set_static_mesh(STATE["meshes"][name]), name)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    # Placement explicitly uses the documented collision policy. These checks
    # supplement the separate importer report of the saved asset defaults.
    component.set_collision_profile_name("NoCollision" if "Vent" in name else "BlockAll")
    return actor


def directional_light(label, intensity, rotation, cast_shadows):
    actor = spawn(unreal.DirectionalLight, label, (0, 0, 900), rotation)
    component = actor.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_intensity(intensity)
    component.set_editor_property("cast_shadows", cast_shadows)
    component.set_editor_property("light_source_angle", 3.0)
    return actor


def create_scene():
    STATE["meshes"] = {name: unreal.load_asset(f"{BASE}/Meshes/{name}") for name in NAMES}
    require(all(isinstance(mesh, unreal.StaticMesh) for mesh in STATE["meshes"].values()),
            "All five saved native static meshes are required")
    plaster = unreal.load_asset(f"{BASE}/Materials/M_VS_Plaster")
    require(plaster, "Saved plaster material is required")
    wall_bounds = STATE["meshes"][NAMES[0]].get_bounding_box()
    # FBX imports reflect source Y. Determine the front from the documented
    # base-corner wall pivot so the camera also handles a preserved-Y import.
    if abs(wall_bounds.max.y) < 0.1 and wall_bounds.min.y < -19.9:
        front = 1
    elif abs(wall_bounds.min.y) < 0.1 and wall_bounds.max.y > 19.9:
        front = -1
    else:
        raise RuntimeError(f"Unexpected wall bounds: {wall_bounds}")
    STATE["front"] = front
    STATE["world"] = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    require(STATE["world"], "Could not create unsaved inspection world")
    STATE["actors"] = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    locations = (
        (-260, -100 * front, 0), (180, -100 * front, 0),
        (-255, 145 * front, 0), (30, 145 * front, 0),
        (-60, -85 * front, 170),
    )
    STATE["display"] = {
        name: mesh_actor(name, location) for name, location in zip(NAMES, locations)
    }
    floor = spawn(unreal.StaticMeshActor, "Inspection floor - unsaved", (0, 0, -10.1))
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
    floor.set_actor_scale3d(unreal.Vector(120, 120, 0.2))
    floor.static_mesh_component.set_material(0, plaster)
    floor.static_mesh_component.set_collision_profile_name("NoCollision")
    # One sun plus a captured sky gives readable fronts and ambient fill on
    # the Mac rendering path without Lumen, baked lightmaps, or external HDRIs.
    # Python's Rotator positional order is roll/pitch/yaw. Name the axes.
    sun = directional_light("Inspection key", 3.0,
                            unreal.Rotator(pitch=-45, yaw=-55 * front, roll=0), True)
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("atmosphere_sun_light", True)
    spawn(unreal.SkyAtmosphere, "Inspection atmosphere - unsaved", (0, 0, 0))
    sky = spawn(unreal.SkyLight, "Inspection ambient sky - unsaved", (0, 0, 800))
    sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky_component.set_editor_property("real_time_capture", True)
    sky_component.set_editor_property("lower_hemisphere_is_black", False)
    sky_component.set_intensity(1.0)
    for command in ("r.EyeAdaptationQuality 0", "r.DefaultFeature.AutoExposure 0",
                    "r.DefaultFeature.MotionBlur 0", "r.DefaultFeature.Bloom 0"):
        unreal.SystemLibrary.execute_console_command(STATE["world"], command)
    camera_location = unreal.Vector(550, 1050 * front, 650)
    target = unreal.Vector(-20, 0, 145)
    rotation = unreal.MathLibrary.find_look_at_rotation(camera_location, target)
    camera = spawn(unreal.CameraActor, "Inspection camera - unsaved", xyz(camera_location), rotation)
    camera_component = camera.get_component_by_class(unreal.CameraComponent)
    camera_component.set_field_of_view(42.0)
    camera_component.set_editor_property("aspect_ratio", RESOLUTION[0] / RESOLUTION[1])
    camera_component.set_editor_property("constrain_aspect_ratio", False)
    post_process = camera_component.get_editor_property("post_process_settings")
    post_process.set_editor_property("override_auto_exposure_min_brightness", True)
    post_process.set_editor_property("auto_exposure_min_brightness", 1.0)
    post_process.set_editor_property("override_auto_exposure_max_brightness", True)
    post_process.set_editor_property("auto_exposure_max_brightness", 1.0)
    post_process.set_editor_property("override_auto_exposure_bias", True)
    post_process.set_editor_property("auto_exposure_bias", 0.0)
    camera_component.set_editor_property("post_process_settings", post_process)
    camera_component.set_editor_property("post_process_blend_weight", 1.0)
    STATE["camera"] = camera
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        camera_location, rotation)
    unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
    STATE["report"] = {
        "engine": unreal.SystemLibrary.get_engine_version(),
        "scene": "fresh unsaved editor world; no map package saved",
        "native_front": "+Y" if front == 1 else "-Y",
        "camera_location_cm": xyz(camera_location), "camera_rotation": str(rotation),
        "horizontal_fov_degrees": 42.0, "resolution": list(RESOLUTION),
        "lighting": {"key_directional_intensity": 3.0, "captured_sky_intensity": 1.0,
                     "automatic_exposure": False, "exposure_bias": 0.0},
        "placements": {name: xyz(actor.get_actor_location()) for name, actor in STATE["display"].items()},
        "editor_collision_queries": {}, "runtime_verified": False,
        "gameplay_collision_verified": False, "map_placement_verified": False,
    }


def collision_queries():
    all_actors = STATE["actors"].get_all_level_actors()
    for name, actor in STATE["display"].items():
        component = actor.static_mesh_component
        bounds = STATE["meshes"][name].get_bounding_box()
        position = actor.get_actor_location()
        center = unreal.Vector(*[
            (getattr(bounds.min, axis) + getattr(bounds.max, axis)) / 2 + getattr(position, axis)
            for axis in ("x", "y", "z")
        ])
        depth = bounds.max.y - bounds.min.y
        start = unreal.Vector(center.x, center.y + STATE["front"] * (depth / 2 + 50), center.z)
        end = unreal.Vector(center.x, center.y - STATE["front"] * (depth / 2 + 50), center.z)
        expected_hit = "Vent" not in name
        profile = str(component.get_collision_profile_name())
        require(profile == ("BlockAll" if expected_hit else "NoCollision"), f"Bad placement profile: {name}")
        result = {
            "assigned_component_profile": profile,
            "component_collision_enabled": str(component.get_collision_enabled()),
            "trace_start_cm": xyz(start), "trace_end_cm": xyz(end),
            "trace_channel": "Visibility", "expected_blocking_hit": expected_hit,
        }
        ignored = [other for other in all_actors if other != actor]

        def query(trace_start, trace_end, complex_trace):
            return unreal.SystemLibrary.line_trace_single(
                STATE["world"], trace_start, trace_end, unreal.TraceTypeQuery.ECC_VISIBILITY,
                complex_trace, ignored, unreal.DrawDebugTrace.NONE, True)

        for complex_trace in (False, True):
            hit = query(start, end, complex_trace)
            actual_hit = hit is not None
            require(actual_hit == expected_hit,
                    f"Editor {'complex' if complex_trace else 'simple'} trace mismatch: {name}: {actual_hit}")
            result["complex_blocking_hit" if complex_trace else "simple_blocking_hit"] = actual_hit
        if expected_hit:
            # Probe the center and four inset corners of the declared envelope.
            # This verifies the imported hull's placement and front face at
            # several positions; it is not a complete sweep/clearance test.
            inset = 2.0
            low_x, high_x = bounds.min.x + position.x + inset, bounds.max.x + position.x - inset
            low_z, high_z = bounds.min.z + position.z + inset, bounds.max.z + position.z - inset
            probes = (("center", center.x, center.z),
                      ("lower_left", low_x, low_z), ("lower_right", high_x, low_z),
                      ("upper_left", low_x, high_z), ("upper_right", high_x, high_z))
            expected_y = position.y + (bounds.max.y if STATE["front"] == 1 else bounds.min.y)
            result["simple_front_face_probes"] = []
            result["front_face_y_world_cm"] = round(expected_y, 4)
            result["front_face_tolerance_cm"] = 0.2
            result["corner_probe_inset_cm"] = inset
            for label, x, z in probes:
                probe_start = unreal.Vector(x, start.y, z)
                probe_end = unreal.Vector(x, end.y, z)
                hit = query(probe_start, probe_end, False)
                require(hit is not None, f"Simple hull missed {name}/{label}")
                # HitResult declares HasNativeBreak=GameplayStatics.BreakHitResult.
                # StructBase.to_tuple uses that function in the installed 5.8.4
                # Python plugin; fields 0/1/5/9 are BlockingHit/InitialOverlap/
                # ImpactPoint/HitActor. Raw internal properties are not exposed.
                fields = hit.to_tuple()
                require(len(fields) >= 11 and fields[0] and not fields[1],
                        f"Invalid blocking-hit result for {name}/{label}")
                impact = fields[5]
                require(fields[9] == actor, f"Unexpected hit actor for {name}/{label}")
                deviation = abs(impact.y - expected_y)
                require(deviation <= 0.2,
                        f"Simple hull front face differs for {name}/{label}: {impact.y} vs {expected_y} cm")
                result["simple_front_face_probes"].append({
                    "position": label, "start_cm": xyz(probe_start), "end_cm": xyz(probe_end),
                    "blocking_hit": True, "impact_point_cm": xyz(impact),
                    "front_face_deviation_cm": round(deviation, 5),
                })
        STATE["report"]["editor_collision_queries"][name] = result


def finish(error=None):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    if STATE["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(STATE["handle"])
        STATE["handle"] = None
    if error:
        unreal.log_error(f"VALSTRIKE_INSPECTION_FAILED: {error}")
    # Let the -ExecutePythonScript runner perform an unattended exit next tick.
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta_time):
    try:
        elapsed = time.monotonic() - STATE["started"]
        require(elapsed < TIMEOUT_SECONDS, f"Inspection exceeded {TIMEOUT_SECONDS:g} seconds")
        if STATE["task"] is None:
            if elapsed < 3.0:
                return  # Allow physics/render state to register in the new world.
            collision_queries()
            STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(
                *RESOLUTION, str(STATE["pending_png"]), camera=STATE["camera"],
                delay=2.0, force_game_view=True)
            require(STATE["task"] and STATE["task"].is_valid_task(), "No active screenshot viewport/task")
            return
        if not STATE["task"].is_task_done():
            return
        pending = STATE["pending_png"]
        require(pending.is_file(), f"Screenshot task completed without image: {pending}")
        data = pending.read_bytes()
        require(len(data) > 1000 and data[:8] == b"\x89PNG\r\n\x1a\n", "Invalid screenshot PNG")
        size = struct.unpack(">II", data[16:24])
        require(size == RESOLUTION, f"Unexpected screenshot resolution: {size}")
        STATE["report"]["screenshot"] = str(OUTPUT.relative_to(ROOT))
        STATE["report"]["capture_completed"] = True
        STATE["report"]["elapsed_seconds"] = round(elapsed, 2)
        report_pending = REPORT.with_name(f".inspection-{STATE['run_id']}.json")
        report_pending.write_text(json.dumps(STATE["report"], indent=2) + "\n", encoding="utf-8")
        # A unique pending filename prevents a failed run from validating an old
        # PNG. Previous review artifacts are replaced only after a complete run.
        pending.replace(OUTPUT)
        report_pending.replace(REPORT)
        unreal.log(f"VALSTRIKE_INSPECTION_SUCCESS: {OUTPUT}; {REPORT}")
        finish()
    except Exception:
        finish(traceback.format_exc())


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the isolated asset worktree")
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(), "End PIE first")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Use a fresh editor without unsaved maps")
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["started"] = time.monotonic()
    STATE["run_id"] = uuid.uuid4().hex
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    STATE["pending_png"] = OUTPUT.parent / f".inspection-{STATE['run_id']}.png"
    create_scene()
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        finish(traceback.format_exc())
