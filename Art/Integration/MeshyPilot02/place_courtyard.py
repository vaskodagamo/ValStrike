"""INT-ART-02 phase B: north service facade section + roof parapets in Site01.

Places exactly ART-25's six NoCollision visuals (docs/assets/meshy-site01/courtyard-pilot/
placement.md) on the north face of NorthService, captures before/after editor views from
the actual north service route, and sets VS_Sun as the explicit forward-shading light
(the engine's own brightness fallback already chose it; this only removes the conflict).
No existing actor is moved, hidden or deleted; every pre-existing actor's transform, tags
and collision (and every light's settings except that one priority) must be unchanged
after save + reopen. -VSInspectOnly: reopen-verify and capture "after" only.
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
BASE = "/Game/Art/Environment/MeshySite01/CourtyardPilot"
REPORTS = ROOT / "docs/assets/meshy-site01-followups"
PREVIEWS = REPORTS / "courtyard-previews"
FOLDER = "Site01/CourtyardPilot"
RESOLUTION = (1600, 1000)
MESHES = {"W01": f"{BASE}/W01/SM_VS_MSH_W01_StraightWall", "W08": f"{BASE}/W08/SM_VS_MSH_W08_PedestrianDoorway",
          "W09": f"{BASE}/W09/SM_VS_MSH_W09_StraightParapet"}
PLACEMENTS = [  # label, asset, native pivot, expected world AABB (ART-25 placement.md)
    ("VS_ART25_WallWest", "W01", (400, 1510, 0), (400, 1491.7933, 0), (594.2425, 1510, 300)),
    ("VS_ART25_DoorRecess", "W08", (595.2425, 1510, 0), (595.2425, 1461.2572, 0), (850.4555, 1510, 300)),
    ("VS_ART25_WallEast", "W01", (851.4555, 1510, 0), (851.4555, 1491.7933, 0), (1045.6980, 1510, 300)),
    ("VS_ART25_RoofParapet1", "W09", (421.8490, 1500, 394), (421.8490, 1462.6270, 394), (621.8490, 1500, 457.5910)),
    ("VS_ART25_RoofParapet2", "W09", (622.8490, 1500, 394), (622.8490, 1462.6270, 394), (822.8490, 1500, 457.5910)),
    ("VS_ART25_RoofParapet3", "W09", (823.8490, 1500, 394), (823.8490, 1462.6270, 394), (1023.8490, 1500, 457.5910)),
]
BACKING = ("VS_ServicePanel_1_2", "VS_ServicePanel_1_3", "VS_NorthService", "VS_ServiceRoofBand")
SUN, FILL = "VS_Sun", "VS_StylizedFill"
VIEWS = [  # id, camera, target
    ("approach_service_route", (100, 1750, 152), (690, 1510, 185)),
    ("route_facing_facade", (722.849, 1750, 152), (722.849, 1500, 170)),
    ("route_crouched", (722.849, 1750, 62), (722.849, 1500, 120)),
    ("oblique_from_west", (300, 1820, 152), (720, 1505, 190)),
    ("oblique_from_east", (1150, 1820, 152), (720, 1505, 190)),
    ("close_west_joint", (600, 1590, 150), (595, 1500, 150)),
    ("close_blind_recess", (722.849, 1620, 165), (722.849, 1470, 150)),
    ("overhead_diagnostic", (722.849, 1760, 1500), (722.849, 1755, 0)),
    ("roof_diagnostic", (722.849, 2000, 650), (722.849, 1480, 420)),
]
STATE = {"phase": "start", "view": 0, "task": None, "busy": False, "done": False, "report": {}, "captures": []}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def xyz(v):
    return [round(getattr(v, a), 3) for a in ("x", "y", "z")]


def close(a, b, tol=0.1):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


def sub(cls):
    return unreal.get_editor_subsystem(cls)


def labels():
    result = {}
    for actor in sub(unreal.EditorActorSubsystem).get_all_level_actors():
        label = actor.get_actor_label()
        require(label not in result, f"Duplicate label {label}")
        result[label] = actor
    return result


def light_state(actor):
    c = actor.get_component_by_class(unreal.DirectionalLightComponent)
    return {"intensity": c.get_editor_property("intensity"), "cast_shadows": c.get_editor_property("cast_shadows"),
            "atmosphere_sun_light": c.get_editor_property("atmosphere_sun_light"),
            "forward_shading_priority": c.get_editor_property("forward_shading_priority"),
            "mobility": str(c.get_editor_property("mobility")), "light_color_rgba": [getattr(c.get_editor_property("light_color"), k) for k in ("r", "g", "b", "a")],
            "rotation": [round(actor.get_actor_rotation().roll, 3), round(actor.get_actor_rotation().pitch, 3),
                         round(actor.get_actor_rotation().yaw, 3)]}


def snapshot(actor):
    r = actor.get_actor_rotation()
    prims = sorted(({"name": c.get_name(), "profile": str(c.get_collision_profile_name()),
                     "enabled": str(c.get_collision_enabled()),
                     "visible": c.get_editor_property("visible"), "hidden_in_game": c.get_editor_property("hidden_in_game")}
                    for c in actor.get_components_by_class(unreal.PrimitiveComponent)), key=lambda p: p["name"])
    data = {"class": actor.get_class().get_name(), "location": xyz(actor.get_actor_location()),
            "rotation": [round(r.roll, 3), round(r.pitch, 3), round(r.yaw, 3)], "scale": xyz(actor.get_actor_scale3d()),
            "tags": [str(t) for t in actor.get_editor_property("tags")], "primitives": prims}
    if actor.get_component_by_class(unreal.DirectionalLightComponent):
        data["light"] = light_state(actor)
    return data


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return [round(origin.x - extent.x, 3), round(origin.y - extent.y, 3), round(origin.z - extent.z, 3)], \
           [round(origin.x + extent.x, 3), round(origin.y + extent.y, 3), round(origin.z + extent.z, 3)]


def preflight(found):
    lights = [a for a in found.values() if a.get_component_by_class(unreal.DirectionalLightComponent)]
    info = {a.get_actor_label(): light_state(a) for a in lights}
    require(sorted(info) == sorted([SUN, FILL]), f"Expected exactly {SUN} and {FILL} directional lights: {sorted(info)}")
    sun, fill = info[SUN], info[FILL]
    # The engine falls back to the brightest light on equal priority; act only if that is the shadowing sun.
    require(sun["forward_shading_priority"] == fill["forward_shading_priority"] and sun["intensity"] > fill["intensity"]
            and sun["cast_shadows"] and not fill["cast_shadows"] and sun["atmosphere_sun_light"],
            f"Light arrangement differs from the inspected Site01 setup: {info}")
    backing = {}
    for label in BACKING:
        require(label in found, f"Missing backing actor {label}")
        backing[label] = bounds(found[label])
    for label in ("VS_ServicePanel_1_2", "VS_ServicePanel_1_3"):
        require(backing[label][1][1] <= 1510.0, f"{label} protrudes through the new skin plane: {backing[label]}")
    return {"directional_lights_before": info, "backing_bounds_cm": backing}


def place(found):
    actors = sub(unreal.EditorActorSubsystem)
    placed = []
    for label, key, pivot, want_min, want_max in PLACEMENTS:
        require(label not in found, f"{label} already exists; inspect, do not duplicate")
        mesh = unreal.load_asset(MESHES[key])
        require(isinstance(mesh, unreal.StaticMesh), f"Missing native {key}")
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*pivot), unreal.Rotator())
        actor.set_actor_label(label)
        actor.set_folder_path(FOLDER)
        c = actor.static_mesh_component
        c.set_static_mesh(mesh)
        c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_collision_profile_name("NoCollision")
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("can_ever_affect_navigation", False)
        low, high = bounds(actor)
        require(close(low, want_min) and close(high, want_max), f"{label}: {low}/{high} vs {want_min}/{want_max}")
        placed.append({"label": label, "asset": MESHES[key], "pivot_cm": list(pivot), "world_bounds_cm": [low, high]})
    sun = found[SUN].get_component_by_class(unreal.DirectionalLightComponent)
    sun.set_editor_property("forward_shading_priority", 1)
    return placed


def trace(start, end, profile="Pawn"):
    world = sub(unreal.UnrealEditorSubsystem).get_editor_world()
    hit = unreal.SystemLibrary.line_trace_single_by_profile(world, unreal.Vector(*start), unreal.Vector(*end), profile,
                                                            False, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return {"hit": False}
    f = hit.to_tuple()
    return {"hit": bool(f[0]), "impact_cm": xyz(f[5]), "actor": f[9].get_actor_label() if f[9] else None}


def verify(before):
    found = labels()
    changed = []
    for label, prior in before.items():
        require(label in found, f"Pre-existing actor missing: {label}")
        now = snapshot(found[label])
        if label == SUN:  # The one intended edit.
            require(now["light"]["forward_shading_priority"] == 1, "Sun priority not saved")
            now["light"]["forward_shading_priority"] = prior["light"]["forward_shading_priority"]
        if now != prior:
            changed.append(label)
    require(not changed, f"Pre-existing actors changed: {changed}")
    placed = []
    for label, key, pivot, want_min, want_max in PLACEMENTS:
        require(label in found, f"Missing after reopen: {label}")
        low, high = bounds(found[label])
        c = found[label].static_mesh_component
        require(close(low, want_min) and close(high, want_max)
                and c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION, f"{label} wrong after reopen")
        placed.append({"label": label, "world_bounds_cm": [low, high], "collision": "NoCollision"})
    # Collision boundary unchanged: from the route, pawn traces stop on the retained facade at Y=1500.
    walls = []
    for x in (450, 600, 722.849, 845, 1000):
        hit = trace((x, 1750, 90), (x, 1300, 90))
        walls.append({"x": x, **hit})
        require(hit["hit"] and abs(hit["impact_cm"][1] - 1500) <= 0.6 and hit["actor"] in BACKING,
                f"Service facade collision changed at X={x}: {hit}")
    # Visibility from the route also stops at the retained wall, never at a new visual.
    world = sub(unreal.UnrealEditorSubsystem).get_editor_world()
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(722.849, 1750, 152), unreal.Vector(722.849, 1300, 152),
                                                 unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
    f = hit.to_tuple() if hit else None
    vis = {"hit": bool(f[0]), "impact_cm": xyz(f[5]), "actor": f[9].get_actor_label() if f[9] else None} if f else {"hit": False}
    require(vis["hit"] and vis["actor"] in BACKING, f"Visibility from the route must stop at the retained wall: {vis}")
    return {"preexisting_actors_unchanged": len(before), "placed": placed, "facade_pawn_traces": walls,
            "visibility_trace": vis, "directional_lights_after": {SUN: light_state(found[SUN]), FILL: light_state(found[FILL])}}


def camera(view_id, location, target):
    pos = unreal.Vector(*location)
    rot = unreal.MathLibrary.find_look_at_rotation(pos, unreal.Vector(*target))
    cam = sub(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.CameraActor, pos, rot)
    cam.set_actor_label(f"INTART02_{view_id}_UNSAVED")
    c = cam.get_component_by_class(unreal.CameraComponent)
    c.set_field_of_view(70.0)
    c.set_editor_property("aspect_ratio", RESOLUTION[0] / RESOLUTION[1])
    pp = c.get_editor_property("post_process_settings")
    for key, value in (("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", 1.0),
                       ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 1.0),
                       ("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.0)):
        pp.set_editor_property(key, value)
    c.set_editor_property("post_process_settings", pp)
    sub(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(pos, rot)
    return cam


def capture_step(prefix):
    """Returns True when every view for this prefix is captured."""
    if STATE["task"] is None:
        if STATE["view"] >= len(VIEWS):
            STATE["view"] = 0
            return True
        view_id, loc, tgt = VIEWS[STATE["view"]]
        STATE["camera"] = camera(view_id, loc, tgt)
        STATE["file"] = PREVIEWS / f"{prefix}_{view_id}.png"
        STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(
            *RESOLUTION, str(STATE["file"]), camera=STATE["camera"], delay=2.0, force_game_view=True)
        require(STATE["task"] and STATE["task"].is_valid_task(), "No screenshot task")
        return False
    if not STATE["task"].is_task_done():
        return False
    data = STATE["file"].read_bytes()
    require(data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == RESOLUTION, "Bad capture")
    view_id, loc, tgt = VIEWS[STATE["view"]]
    STATE["captures"].append({"id": f"{prefix}_{view_id}", "camera_cm": list(loc), "target_cm": list(tgt),
                              "file": str(STATE["file"].relative_to(ROOT)), "sha256": hashlib.sha256(data).hexdigest()})
    sub(unreal.EditorActorSubsystem).destroy_actor(STATE["camera"])
    STATE["view"] += 1
    STATE["task"] = None
    return False


def finish(error=None):
    if STATE["done"]:
        return
    STATE["done"] = True
    unreal.unregister_slate_post_tick_callback(STATE["handle"])
    report = STATE["report"]
    report.update(passed=error is None, error=error, captures=STATE["captures"],
                  map_sha256=hashlib.sha256(MAP_FILE.read_bytes()).hexdigest())
    name = "courtyard-placement-reopen-report.json" if STATE["inspect_only"] else "courtyard-placement-report.json"
    (REPORTS / name).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log_error(f"VALSTRIKE_MESHY_COURTYARD_PLACE_FAILED: {error}") if error else \
        unreal.log(f"VALSTRIKE_MESHY_COURTYARD_PLACE_SUCCESS: {REPORTS / name}")
    unreal.SystemLibrary.quit_editor()


def tick(_delta):
    if STATE["busy"] or STATE["done"]:
        return
    STATE["busy"] = True
    try:
        require(time.monotonic() - STATE["started"] < 420, "Courtyard placement exceeded 420 s")
        levels = sub(unreal.LevelEditorSubsystem)
        phase = STATE["phase"]
        if phase == "start":
            if time.monotonic() - STATE["started"] < 3.0:
                return
            found = labels()
            STATE["report"]["preflight"] = preflight(found) if not STATE["inspect_only"] else None
            if not STATE["inspect_only"]:
                before = {label: snapshot(actor) for label, actor in found.items()}
                (REPORTS / "courtyard-preexisting-snapshot.json").write_text(json.dumps(before, indent=1) + "\n")
            STATE["phase"] = "after_verify" if STATE["inspect_only"] else "capture_before"
        elif phase == "capture_before":
            if capture_step("before"):
                STATE["report"]["placed"] = place(labels())
                require(levels.save_current_level(), "Map save failed")
                require(levels.load_level(MAP), "Map reopen failed")
                STATE["phase"] = "after_verify"
                STATE["settle"] = time.monotonic()
        elif phase == "after_verify":
            if time.monotonic() - STATE.get("settle", 0) < 3.0:
                return
            before = json.loads((REPORTS / "courtyard-preexisting-snapshot.json").read_text())
            STATE["report"]["verification"] = verify(before)
            STATE["phase"] = "capture_after"
        elif phase == "capture_after":
            if capture_step("after"):
                finish()
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-02 worktree")
    levels = sub(unreal.LevelEditorSubsystem)
    require(not levels.is_in_play_in_editor(), "End PIE first")
    require(levels.load_level(MAP), "Could not open Site01")
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    STATE["inspect_only"] = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    STATE["report"] = {"engine": unreal.SystemLibrary.get_engine_version(), "map": MAP,
                       "mode": "reopen_inspect" if STATE["inspect_only"] else "capture_place_save_reopen",
                       "recipe": "docs/assets/meshy-site01/courtyard-pilot/placement.md", "runtime_verified": False}
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
        unreal.log_error(f"VALSTRIKE_MESHY_COURTYARD_PLACE_FAILED: {traceback.format_exc()}")
        unreal.SystemLibrary.quit_editor()
