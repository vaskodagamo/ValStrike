"""Bounded single-player PIE CharacterMovement traversals. No multiplayer/human claim.

Starts at each route's first waypoint using a test teleport, then uses pawn
AddMovementInput each Slate tick. No teleport occurs within a measured traversal.
Timing is observed world simulation time, not an FPS benchmark or human timing.
"""
import hashlib
import json
import math
from pathlib import Path
import sys
import time
import traceback
import uuid

import unreal

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(HERE))
import design as d

STATE = {"handle": None, "finished": False, "phase": "waiting", "job": -1, "results": []}
JOBS = [(r, False) for r in d.ROUTES] + [(d.ROUTES[0], True)]
REPORT = ROOT / "docs/maps/site01/runtime-report.json"


def require(value, message):
    if not value:
        raise RuntimeError(message)


def xyz(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


def finish(error=None):
    if STATE["finished"]:
        return
    STATE["finished"] = True
    result = {"passed": error is None, "run_id": STATE["run_id"],
              "engine": unreal.SystemLibrary.get_engine_version(),
              "evidence_kind": "single-player Play In Editor, real VSCharacter/CharacterMovement, scripted AddMovementInput",
              "human_playtest": False, "multiplayer_verified": False, "packaged_verified": False,
              "timing_basis": "observed world simulation seconds; waypoint tolerance 40 cm; initial test teleport excluded",
              "map_sha256": STATE["map_sha256"], "design_sha256": STATE["design_sha256"],
              "script_sha256": hashlib.sha256((HERE / "traverse_site01.py").read_bytes()).hexdigest(),
              "wall_elapsed_seconds": round(time.monotonic() - STATE["started"], 2),
              "traversals": STATE["results"], "error": error}
    REPORT.write_text(json.dumps(result, indent=2) + "\n")
    if error:
        unreal.log_error("VALSTRIKE_SITE01_RUNTIME_FAILED: " + error)
    else:
        unreal.log("VALSTRIKE_SITE01_RUNTIME_SUCCESS")
    # End PIE on the engine tick, then let a later Slate tick exit the editor.
    # Keep PIE teardown and QUIT_EDITOR on separate ticks.
    STATE["ending"] = True
    STATE["levels"].editor_request_end_play()


def next_job(now):
    STATE["job"] += 1
    if STATE["job"] == len(JOBS):
        finish()
        return
    route, crouch = JOBS[STATE["job"]]
    pawn = STATE["pawn"]
    movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
    movement.stop_movement_immediately()
    pawn.un_crouch()
    p = route["points_cm"][0]
    pawn.set_actor_location(unreal.Vector(*p, 100), False, True)
    if crouch:
        pawn.crouch()
    STATE.update(phase="settling", route=route, crouched=crouch, waypoint=1,
                 settle_until=now + 0.6, movement=movement)


def tick(_dt):
    if STATE.get("ending"):
        if not STATE["levels"].is_in_play_in_editor():
            STATE["ending"] = False
            unreal.unregister_slate_post_tick_callback(STATE["handle"])
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        return
    if STATE["finished"] or STATE.get("busy"):
        return
    STATE["busy"] = True
    try:
        require(time.monotonic() - STATE["started"] < 240, "PIE traversal exceeded 240 wall seconds")
        world = STATE["worlds"].get_game_world()
        if not world:
            return
        now = unreal.GameplayStatics.get_time_seconds(world)
        if STATE["phase"] == "waiting":
            pawn = unreal.GameplayStatics.get_player_character(world, 0)
            if not pawn:
                return
            require(pawn.get_class().get_name() == "VSCharacter", "PIE did not spawn VSCharacter")
            controllers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController)
            require(len(controllers) == 1, "Expected one standalone PIE controller")
            STATE["pawn"] = pawn
            STATE["world"] = world
            next_job(now)
            return
        pawn = STATE["pawn"]
        pos = pawn.get_actor_location()
        if STATE["phase"] == "settling":
            if now < STATE["settle_until"]:
                return
            capsule = pawn.get_component_by_class(unreal.CapsuleComponent)
            expected = 56 if STATE["crouched"] else 88
            require(abs(capsule.get_scaled_capsule_half_height() - expected) < 0.2, "Stance did not settle")
            require(STATE["movement"].is_moving_on_ground(), "Start not grounded")
            STATE.update(phase="moving", started_sim=now, last_pos=pos, distance=0.0,
                         min_z=pos.z, max_z=pos.z, sample_count=0,
                         observed_half_height=capsule.get_scaled_capsule_half_height())
        STATE["distance"] += math.dist(xyz(pos)[:2], xyz(STATE["last_pos"])[:2])
        STATE["last_pos"] = pos
        STATE["sample_count"] += 1
        STATE["min_z"] = min(STATE["min_z"], pos.z)
        STATE["max_z"] = max(STATE["max_z"], pos.z)
        require(abs(pos.z - (56 if STATE["crouched"] else 88)) < 6, "Pawn left flat floor or climbed unintended cover")
        points = STATE["route"]["points_cm"]
        target = points[STATE["waypoint"]]
        dx, dy = target[0] - pos.x, target[1] - pos.y
        remaining = math.hypot(dx, dy)
        if remaining < 40:
            STATE["waypoint"] += 1
            if STATE["waypoint"] == len(points):
                elapsed = now - STATE["started_sim"]
                STATE["movement"].stop_movement_immediately()
                STATE["results"].append({"route_id": STATE["route"]["id"],
                    "stance": "crouched" if STATE["crouched"] else "standing",
                    "passed": True, "observed_sim_seconds": round(elapsed, 3),
                    "observed_distance_cm": round(STATE["distance"], 2),
                    "final_location_cm": xyz(pos), "endpoint_error_cm": round(remaining, 2),
                    "capsule_half_height_cm": STATE["observed_half_height"],
                    "center_z_range_cm": [round(STATE["min_z"], 3), round(STATE["max_z"], 3)],
                    "samples": STATE["sample_count"]})
                REPORT.write_text(json.dumps({"passed": False, "status": "running",
                    "run_id": STATE["run_id"], "traversals": STATE["results"]}, indent=2) + "\n")
                unreal.log("VALSTRIKE_SITE01_ROUTE_COMPLETE: " + str(STATE["results"][-1]))
                next_job(now)
                return
            target = points[STATE["waypoint"]]
            dx, dy = target[0] - pos.x, target[1] - pos.y
            remaining = math.hypot(dx, dy)
        require(now - STATE["started_sim"] < (50 if STATE["crouched"] else 30),
                f"Stalled on {STATE['route']['id']} waypoint {STATE['waypoint']} at {xyz(pos)}")
        pawn.add_movement_input(unreal.Vector(dx / remaining, dy / remaining, 0), 1.0, True)
    except Exception:
        failure = traceback.format_exc()
        if STATE["finished"]:
            REPORT.write_text(json.dumps({"passed": False, "error": failure, "traversals": STATE["results"]}, indent=2) + "\n")
            unreal.log_error("VALSTRIKE_SITE01_RUNTIME_FAILED: " + failure)
            unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        else:
            finish(failure)
    finally:
        STATE["busy"] = False


def main():
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong checkout")
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned engine required")
    STATE["levels"] = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    STATE["worlds"] = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not STATE["levels"].is_in_play_in_editor(), "Fresh process required")
    require(STATE["levels"].load_level(d.MAP), "Saved map not found")
    settings = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.LevelEditorPlaySettings"))
    settings.set_editor_property("PlayNumberOfClients", 1)
    # EPlayNetMode is not exported to Python in this installed build.
    # Require a process-local INI override, never change project configuration.
    require("PlayNetMode=PIE_Standalone" in unreal.SystemLibrary.get_command_line(),
            "Pass -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.LevelEditorPlaySettings]:PlayNetMode=PIE_Standalone")
    settings.set_editor_property("RunUnderOneProcess", True)
    settings.set_editor_property("bLaunchSeparateServer", False)
    STATE.update(started=time.monotonic(), run_id=uuid.uuid4().hex,
                 map_sha256=hashlib.sha256((ROOT / "Content/Maps/Site01/VS_Site01.umap").read_bytes()).hexdigest(),
                 design_sha256=hashlib.sha256((HERE / "design.py").read_bytes()).hexdigest())
    REPORT.write_text(json.dumps({"passed": False, "status": "running", "run_id": STATE["run_id"]}, indent=2) + "\n")
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)
    STATE["levels"].editor_request_begin_play()


if __name__ == "__main__":
    try:
        main()
    except Exception:
        error = traceback.format_exc()
        REPORT.write_text(json.dumps({"passed": False, "error": error}, indent=2) + "\n")
        unreal.log_error("VALSTRIKE_SITE01_RUNTIME_FAILED: " + error)
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
