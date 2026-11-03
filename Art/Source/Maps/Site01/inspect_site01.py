"""PLAY-01: fresh-process native map inspection and actual Unreal viewport captures.

Run through -ExecutePythonScript in the assigned project with ordinary rendering.
This script never saves a map or edits a shared asset. Editor physics queries and
camera captures are evidence of authored geometry, not a runtime/human playtest.
"""

import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import time
import traceback
import uuid

import unreal


ROOT = Path(__file__).resolve().parents[4]
OUTPUT = ROOT / "docs/maps/site01"
REPORT = OUTPUT / "editor-report.json"
RESOLUTION = (1600, 1000)
TIMEOUT_SECONDS = 180.0
STATE = {"handle": None, "task": None, "finished": False, "busy": False,
         "checks": [], "pending": [], "view_index": 0}
spec = importlib.util.spec_from_file_location("site01_design", Path(__file__).with_name("design.py"))
design = importlib.util.module_from_spec(spec)
spec.loader.exec_module(design)
VIEWS = design.VIEWS + [{
    "id": "08_transformer_exit", "location": [-150, -1450, 152], "target": [1000, -650, 152],
    "caption": "Transformer exit after the full-height island; actual standing-eye site approach angle.",
}]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def xyz(vector):
    return [round(getattr(vector, axis), 4) for axis in ("x", "y", "z")]


def close(actual, expected, tolerance=0.05):
    return len(actual) == len(expected) and all(abs(a - b) <= tolerance for a, b in zip(actual, expected))


def tags(actor):
    return [str(tag) for tag in actor.get_editor_property("tags")]


def check(name, passed, **evidence):
    STATE["checks"].append({"name": name, "passed": bool(passed), **evidence})
    require(passed, f"{name}: {evidence}")


def label_actor(label):
    # Exact labels are preferred; the generator may prefix geometry with VS_.
    matches = [actor for actor in STATE["saved_actors"]
               if actor.get_actor_label() in (label, "VS_" + label)]
    require(len(matches) == 1, f"Expected one saved actor named {label}, found {len(matches)}")
    return matches[0]


def hit_data(hit):
    if hit is None:
        return {"blocking_hit": False}
    # HitResult's native BreakHitResult tuple in the pinned 5.8.4 Python plugin:
    # BlockingHit, InitialOverlap, Time, Distance, Location, ImpactPoint,
    # Normal, ImpactNormal, PhysMat, HitActor, HitComponent, ...
    fields = hit.to_tuple()
    require(len(fields) >= 11, "Unexpected HitResult layout")
    return {"blocking_hit": bool(fields[0]), "initial_overlap": bool(fields[1]),
            "distance_cm": round(fields[3], 4), "impact_point_cm": xyz(fields[5]),
            "actor_label": fields[9].get_actor_label() if fields[9] else None}


def capsule(start, end, half_height, ignored=None):
    return unreal.SystemLibrary.capsule_trace_single_by_profile(
        STATE["world"], unreal.Vector(*start), unreal.Vector(*end), 34.0, half_height,
        "Pawn", False, STATE["ignored"] if ignored is None else ignored,
        unreal.DrawDebugTrace.NONE, True)


def line(start, end, ignored=None, pawn=False):
    ignored = STATE["ignored"] if ignored is None else ignored
    if pawn:
        return unreal.SystemLibrary.line_trace_single_by_profile(
            STATE["world"], unreal.Vector(*start), unreal.Vector(*end), "Pawn", False,
            ignored, unreal.DrawDebugTrace.NONE, True)
    return unreal.SystemLibrary.line_trace_single(
        STATE["world"], unreal.Vector(*start), unreal.Vector(*end),
        unreal.TraceTypeQuery.ECC_VISIBILITY, False, ignored, unreal.DrawDebugTrace.NONE, True)


def metadata_checks():
    actual = STATE["saved_actors"]
    labels = [actor.get_actor_label() for actor in actual]
    check("unique_saved_actor_labels", len(labels) == len(set(labels)), actor_count=len(labels))
    starts = [actor for actor in actual if isinstance(actor, unreal.PlayerStart)]
    check("exactly_four_player_starts", len(starts) == 4, observed_count=len(starts))
    expected_starts = STATE["contract"]["spawns"]
    check("player_start_label_set", {a.get_actor_label() for a in starts} == {s["actor_label"] for s in expected_starts},
          observed_labels=sorted(a.get_actor_label() for a in starts))
    role_tags = {"VS_AttackerSpawn", "VS_DefenderSpawn"}
    for expected in expected_starts:
        actor = label_actor(expected["actor_label"])
        actor_tags = tags(actor)
        role_tag = "VS_AttackerSpawn" if expected["role"] == "attacker" else "VS_DefenderSpawn"
        rotation = actor.get_actor_rotation()
        yaw_error = (rotation.yaw - expected["yaw_degrees"] + 180) % 360 - 180
        check(f"spawn_metadata/{expected['id']}",
              isinstance(actor, unreal.PlayerStart)
              and close(xyz(actor.get_actor_location()), expected["location_cm"])
              and abs(yaw_error) < 0.05 and abs(rotation.pitch) < 0.05 and abs(rotation.roll) < 0.05
              and close(xyz(actor.get_actor_scale3d()), [1, 1, 1])
              and [tag for tag in actor_tags if tag in role_tags] == [role_tag],
              location_cm=xyz(actor.get_actor_location()), yaw_degrees=rotation.yaw,
              actor_tags=actor_tags, player_start_tag=str(actor.get_editor_property("player_start_tag")))
    site = STATE["contract"]["site"]
    actor = label_actor(site["actor_label"])
    box = actor.get_component_by_class(unreal.BoxComponent)
    require(box, "Site marker has no BoxComponent")
    rotation = actor.get_actor_rotation()
    check("site_bounds_and_nonblocking_marker",
          isinstance(actor, unreal.TriggerBox) and tags(actor).count("VS_Site_A") == 1
          and close(xyz(actor.get_actor_location()), site["center_cm"])
          and close(xyz(box.get_editor_property("box_extent")), site["half_extents_cm"])
          and close(xyz(actor.get_actor_scale3d()), [1, 1, 1])
          and max(abs(rotation.pitch), abs(rotation.yaw), abs(rotation.roll)) < 0.05
          and str(box.get_collision_profile_name()) == "NoCollision"
          and box.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION,
          center_cm=xyz(actor.get_actor_location()), half_extents_cm=xyz(box.get_editor_property("box_extent")),
          actor_tags=tags(actor), collision_profile=str(box.get_collision_profile_name()))
    recovery = STATE["contract"]["device_recovery"]
    actor = label_actor(recovery["actor_label"])
    primitives = actor.get_components_by_class(unreal.PrimitiveComponent)
    nonblocking = all(c.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION for c in primitives)
    check("recovery_marker", isinstance(actor, unreal.TargetPoint)
          and tags(actor).count("VS_DeviceRecovery") == 1
          and close(xyz(actor.get_actor_location()), recovery["location_cm"]) and nonblocking,
          location_cm=xyz(actor.get_actor_location()), actor_tags=tags(actor), nonblocking=nonblocking)
    STATE["report"]["saved_actor_inventory"] = [
        {"label": a.get_actor_label(), "class": a.get_class().get_name(),
         "location_cm": xyz(a.get_actor_location()), "actor_tags": tags(a)} for a in actual]


def floor_check(name, point):
    start, end = [point[0], point[1], 10], [point[0], point[1], -10]
    hit = hit_data(line(start, end, pawn=True))
    check(name, hit["blocking_hit"] and hit.get("actor_label") == STATE["floor_label"]
          and abs(hit.get("impact_point_cm", [0, 0, 100])[2]) <= 0.1,
          start_cm=start, end_cm=end, profile="Pawn", **hit)


def route_checks():
    measurements = []
    for route in design.ROUTES:
        points = route["points_cm"]
        distance = sum(math.dist(a, b) for a, b in zip(points, points[1:]))
        measurements.append({"id": route["id"], "points_cm": points, "distance_cm": round(distance, 3),
                             "estimated_seconds_at_450_cm_s": round(distance / 450.0, 3),
                             "observed_traversal_seconds": None,
                             "timing_kind": "calculated polyline distance / constant speed; no acceleration or human input"})
        for segment, (a, b) in enumerate(zip(points, points[1:])):
            for stance, half in (("standing", 88.0), ("crouched", 56.0)):
                for direction, (start, end) in enumerate(((a, b), (b, a))):
                    s, e = [*start, half + 2], [*end, half + 2]
                    hit = hit_data(capsule(s, e, half))
                    check(f"route/{route['id']}/{segment}/{stance}/{direction}", not hit["blocking_hit"],
                          start_cm=s, end_cm=e, radius_cm=34, half_height_cm=half,
                          profile="Pawn", simple_collision=True, **hit)
            samples = max(1, math.ceil(math.dist(a, b) / 100.0))
            for sample in range(samples + 1):
                point = [a[i] + (b[i] - a[i]) * sample / samples for i in (0, 1)]
                floor_check(f"route_floor/{route['id']}/{segment}/{sample}", point)
    STATE["report"]["routes"] = measurements


def spawn_and_recovery_checks():
    for expected in STATE["contract"]["spawns"]:
        point = expected["location_cm"]
        end = [point[0] + 0.1, point[1], point[2]]
        hit = hit_data(capsule(point, end, 88.0))
        check(f"spawn_capsule/{expected['id']}", not hit["blocking_hit"],
              start_cm=point, end_cm=end, radius_cm=34, half_height_cm=88, profile="Pawn", **hit)
        floor_check(f"spawn_floor/{expected['id']}", point)
    point = STATE["contract"]["device_recovery"]["location_cm"]
    floor_check("recovery_floor", point)
    for stance, half in (("standing", 88.0), ("crouched", 56.0)):
        start, end = [point[0], point[1], half + 2], [point[0] + 0.1, point[1], half + 2]
        hit = hit_data(capsule(start, end, half))
        check(f"recovery_capsule/{stance}", not hit["blocking_hit"],
              start_cm=start, end_cm=end, radius_cm=34, half_height_cm=half, profile="Pawn", **hit)
    # Every authored route terminates at the same recovery XY: swept route
    # clearance plus floor samples supplies connected editor geometry evidence.
    check("routes_reach_recovery_xy", all(close(route["points_cm"][-1], point[:2]) for route in design.ROUTES),
          recovery_xy_cm=point[:2])


def spawn_sightline_checks():
    spawns = STATE["contract"]["spawns"]
    site = STATE["contract"]["site"]
    cx, cy = site["center_cm"][:2]
    hx, hy = site["half_extents_cm"][:2]
    targets = [("center", [cx, cy])] + [
        (f"corner_{x}_{y}", [cx + x * (hx - 20), cy + y * (hy - 20)]) for x in (-1, 1) for y in (-1, 1)]
    for height in (88, 152):
        for a in [s for s in spawns if s["role"] == "attacker"]:
            for b in [s for s in spawns if s["role"] == "defender"]:
                start, end = [*a["location_cm"][:2], height], [*b["location_cm"][:2], height]
                hit = hit_data(line(start, end))
                check(f"opposing_spawn_occlusion/{a['id']}/{b['id']}/{height}", hit["blocking_hit"],
                      start_cm=start, end_cm=end, channel="Visibility", **hit)
        for spawn in spawns:
            for name, point in targets:
                start, end = [*spawn["location_cm"][:2], height], [*point, height]
                hit = hit_data(line(start, end))
                check(f"spawn_site_occlusion/{spawn['id']}/{name}/{height}", hit["blocking_hit"],
                      start_cm=start, end_cm=end, channel="Visibility", **hit)


def cover_checks():
    for label, suffix, xy, yaw, base_z in design.PROPS:
        if suffix != "Cover_200x120":
            continue
        actor = label_actor(label)
        ignored = [other for other in STATE["saved_actors"] if other != actor]
        angle = math.radians(yaw)
        normal = [-math.sin(angle), math.cos(angle)]
        for height in (88, 152):
            for side in (-1, 1):
                start = [xy[i] + normal[i] * side * 110 for i in (0, 1)] + [base_z + height]
                end = [xy[i] - normal[i] * side * 110 for i in (0, 1)] + [base_z + height]
                hit = hit_data(line(start, end, ignored))
                expected = height == 88
                check(f"cover_visibility/{label}/{height}/{side}", hit["blocking_hit"] == expected
                      and (not expected or hit["actor_label"] == actor.get_actor_label()),
                      start_cm=start, end_cm=end, channel="Visibility", expected_blocking=expected,
                      isolated_actor=actor.get_actor_label(), **hit)
        for side in (-1, 1):
            start = [xy[i] + normal[i] * side * 160 for i in (0, 1)] + [90]
            end = [xy[i] - normal[i] * side * 160 for i in (0, 1)] + [90]
            hit = hit_data(capsule(start, end, 88.0, ignored))
            check(f"cover_capsule/{label}/{side}", hit["blocking_hit"]
                  and hit["actor_label"] == actor.get_actor_label() and not hit["initial_overlap"],
                  start_cm=start, end_cm=end, radius_cm=34, half_height_cm=88, profile="Pawn", **hit)


def clear_sightline_checks():
    """Probe the intended long engagement rays, separate from spawn occlusion."""
    observed = []
    for name, a, b in (
        ("gallery", [-1050, 850, 152], [800, 850, 152]),
        ("transformer_exit", [-150, -1450, 152], [1000, -650, 152]),
        ("service_flank", [-1000, 1750, 152], [1900, 1750, 152]),
    ):
        hit = hit_data(line(a, b))
        evidence = {"start_cm": a, "end_cm": b, "ray_distance_cm": round(math.dist(a, b), 3),
                    "channel": "Visibility", "simple_collision": True, "expected_clear": True, **hit}
        check(f"clear_sightline/{name}", not hit["blocking_hit"], **evidence)
        observed.append({"id": name, **evidence})
    STATE["report"]["clear_sightlines"] = observed


def create_camera(view):
    position, target = unreal.Vector(*view["location"]), unreal.Vector(*view["target"])
    rotation = unreal.MathLibrary.find_look_at_rotation(position, target)
    camera = STATE["actors"].spawn_actor_from_class(unreal.CameraActor, position, rotation)
    require(camera, f"Could not create camera {view['id']}")
    camera.set_actor_label("Inspection_" + view["id"] + "_UNSAVED")
    component = camera.get_component_by_class(unreal.CameraComponent)
    component.set_field_of_view(70.0 if view["id"] != "07_overview" else 55.0)
    component.set_editor_property("aspect_ratio", RESOLUTION[0] / RESOLUTION[1])
    component.set_editor_property("constrain_aspect_ratio", False)
    settings = component.get_editor_property("post_process_settings")
    for key, value in (("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", 1.0),
                       ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 1.0),
                       ("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.0)):
        settings.set_editor_property(key, value)
    component.set_editor_property("post_process_settings", settings)
    component.set_editor_property("post_process_blend_weight", 1.0)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(position, rotation)
    return camera


def finish(error=None):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    if STATE["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(STATE["handle"])
    if error:
        unreal.log_error(f"VALSTRIKE_SITE01_INSPECTION_FAILED: {error}")
        failure_report = STATE.get("report", {"run_id": STATE.get("run_id", uuid.uuid4().hex),
                                              "engine": unreal.SystemLibrary.get_engine_version()})
        failure_report.update(passed=False, failure=error, checks=STATE["checks"])
        OUTPUT.mkdir(parents=True, exist_ok=True)
        failure_text = json.dumps(failure_report, indent=2) + "\n"
        (OUTPUT / "editor-failure.json").write_text(failure_text, encoding="utf-8")
        # Also invalidate an older successful report on early startup failures.
        REPORT.write_text(failure_text, encoding="utf-8")
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def complete():
    if STATE.get("completing") or STATE["finished"]:
        return
    STATE["completing"] = True
    check("map_file_unchanged_by_inspection", digest(STATE["map_file"]) == STATE["map_hash"],
          sha256=STATE["map_hash"])
    STATE["report"].update(passed=True, checks=STATE["checks"], check_count=len(STATE["checks"]),
                           elapsed_seconds=round(time.monotonic() - STATE["started"], 3))
    # Publish only after every native capture is present and every check passes.
    for pending, final in STATE["pending"]:
        pending.replace(final)
    report_pending = OUTPUT / f".editor-report-{STATE['run_id']}.json"
    report_pending.write_text(json.dumps(STATE["report"], indent=2) + "\n", encoding="utf-8")
    report_pending.replace(REPORT)
    (OUTPUT / "editor-failure.json").unlink(missing_ok=True)
    unreal.log(f"VALSTRIKE_SITE01_INSPECTION_SUCCESS: {len(STATE['checks'])} checks; {len(STATE['pending'])} captures; {REPORT}")
    finish()


def tick(_delta_time):
    # Actor transactions and capture calls can pump Slate messages recursively.
    # Only the outer callback owns state transitions and screenshot publication.
    if STATE["busy"] or STATE["finished"]:
        return
    STATE["busy"] = True
    try:
        require(time.monotonic() - STATE["started"] < TIMEOUT_SECONDS, "Site01 inspection exceeded 180 seconds")
        if not STATE.get("queries_done"):
            if time.monotonic() - STATE["started"] < 3.0:
                return
            metadata_checks()
            route_checks()
            spawn_and_recovery_checks()
            spawn_sightline_checks()
            clear_sightline_checks()
            cover_checks()
            STATE["queries_done"] = True
        if STATE["task"] is None:
            if STATE["view_index"] >= len(VIEWS):
                complete()
                return
            view = VIEWS[STATE["view_index"]]
            camera = create_camera(view)
            STATE["camera"] = camera
            pending = OUTPUT / "previews" / f".capture-{STATE['run_id']}-{view['id']}.png"
            final = OUTPUT / "previews" / f"{view['id']}.png"
            STATE["current_pending"] = pending
            STATE["pending"].append((pending, final))
            STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(
                *RESOLUTION, str(pending), camera=camera, delay=2.0, force_game_view=True)
            require(STATE["task"] and STATE["task"].is_valid_task(), "No active screenshot viewport/task")
            return
        if not STATE["task"].is_task_done():
            return
        pending = STATE["current_pending"]
        require(pending.is_file(), f"Screenshot completed without file: {pending}")
        data = pending.read_bytes()
        require(len(data) > 1000 and data[:8] == b"\x89PNG\r\n\x1a\n", "Invalid captured PNG")
        require(struct.unpack(">II", data[16:24]) == RESOLUTION, "Unexpected capture resolution")
        view = VIEWS[STATE["view_index"]]
        STATE["report"]["captures"].append({**view, "file": str(STATE["pending"][-1][1].relative_to(ROOT)),
                                            "resolution": list(RESOLUTION), "sha256": hashlib.sha256(data).hexdigest(),
                                            "horizontal_fov_degrees": 55 if view["id"] == "07_overview" else 70,
                                            "capture_kind": "Unreal Editor native viewport; not player-controlled runtime"})
        STATE["actors"].destroy_actor(STATE["camera"])
        STATE["view_index"] += 1
        STATE["task"] = None
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the assigned tactical-map checkout")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    require(not levels.is_in_play_in_editor(), "End PIE first")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Use a fresh editor without unsaved maps")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / "previews").mkdir(exist_ok=True)
    STATE["contract"] = json.loads((OUTPUT / "layout.json").read_text(encoding="utf-8"))
    require(STATE["contract"] == design.contract(), "layout.json does not match current authoring data")
    STATE["map_file"] = ROOT / "Content/Maps/Site01/VS_Site01.umap"
    STATE["map_hash"] = digest(STATE["map_file"])
    require(levels.load_level(design.MAP), "Could not reopen saved Site01 map")
    STATE["world"] = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    STATE["actors"] = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    STATE["saved_actors"] = list(STATE["actors"].get_all_level_actors())
    STATE["ignored"] = [a for a in STATE["saved_actors"] if isinstance(a, unreal.PlayerStart)]
    STATE["floor_label"] = label_actor("Floor").get_actor_label()
    STATE["run_id"] = uuid.uuid4().hex
    STATE["started"] = time.monotonic()
    sources = [Path(__file__), Path(__file__).with_name("design.py"), OUTPUT / "layout.json",
               ROOT / "Source/ValStrike/VSCharacter.cpp", ROOT / "Source/ValStrike/VSGameMode.cpp"]
    STATE["report"] = {"engine": unreal.SystemLibrary.get_engine_version(), "run_id": STATE["run_id"],
                       "map_asset": design.MAP, "map_sha256": STATE["map_hash"],
                       "mode": "fresh-process saved map reopen; no map save; native editor queries and captures",
                       "source_sha256": {str(p.relative_to(ROOT)): digest(p) for p in sources},
                       "runtime_verified": False, "multiplayer_verified": False, "human_playtest": False,
                       "query_policy": {"capsule_radius_cm": 34, "standing_half_height_cm": 88,
                                        "crouched_half_height_cm": 56, "route_floor_clearance_cm": 2,
                                        "movement_profile": "Pawn", "simple_collision": True,
                                        "floor_sample_max_spacing_cm": 100,
                                        "ignored_editor_player_start_markers": [a.get_actor_label() for a in STATE["ignored"]]},
                       "limitations": ["Capsule sweeps do not exercise CharacterMovement, acceleration, jumping, input or network replication.",
                                       "Floor samples and authored routes do not prove every free point in the playable AABB is reachable.",
                                       "Spawn rays test authored standing/crouched eyes and five site points, not all possible combat angles.",
                                       "Estimated travel uses constant 450 cm/s; observed runtime traversal is not measured by this script."],
                       "captures": []}
    for command in ("r.EyeAdaptationQuality 0", "r.DefaultFeature.AutoExposure 0",
                    "r.DefaultFeature.MotionBlur 0", "r.DefaultFeature.Bloom 0"):
        unreal.SystemLibrary.execute_console_command(STATE["world"], command)
    unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        finish(traceback.format_exc())
