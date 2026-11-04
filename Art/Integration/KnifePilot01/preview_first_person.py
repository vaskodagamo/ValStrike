"""INT-ART-05: compare first-person knife viewmodel placements in the editor (not runtime).

Opens VS_Graybox without saving. A camera with the runtime first-person settings
(90 degree horizontal FOV, 1000x700 client aspect) looks +X at a target UrbanEntry in
the R02 hold, standing at knife reach (125 cm). The FPArms03 knife hands, posed on the
imported ART-26-C1 clips (single-node, fixed time), are placed at each candidate
arm-from-camera offset with K01 on weapon_socket_r at the measured runtime offset
(UVSKnifePresentationComponent::FirstPersonKnifeOffset). Physical scale, skeleton,
clips and timings are unchanged; only the arm component's offset from the camera
differs. A reference frame without arms lets coverage be measured from pixels.
Output: docs/assets/knife-rifle-acceptance/fp-framing/ (map checked unchanged).
"""

import hashlib
import json
from pathlib import Path
import struct
import time
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "docs/assets/knife-rifle-acceptance/fp-framing"
MAP = "/Game/Maps/VS_Graybox"
MAP_FILE = ROOT / "Content/Maps/VS_Graybox.umap"
ARMS = "/Game/Art/Characters/FPArms03"
RESOLUTION = (1000, 700)
KNIFE_OFFSET = ((-1.5, -6.88, -1.16), (0.0, -0.948683, -0.316228, 0.0))
CANDIDATES = {"A_current": (20, 0, -5), "B": (24, 0, -7), "C": (26, 2, -9), "D": (28, 3, -10)}
POSES = [("idle", "A_FPArms03_KnifeIdle", 0.5), ("slash_impact", "A_FPArms03_KnifeSlash", 0.15),
         ("slash_follow", "A_FPArms03_KnifeSlash", 0.25), ("stab_impact", "A_FPArms03_KnifeStab", 0.30)]
EYE = 64.0  # cm above the pawn origin, as BaseEyeHeight for the first-person camera
STATE = {"jobs": [], "i": 0, "task": None, "busy": False, "done": False, "captures": []}


def require(c, m):
    if not c:
        raise RuntimeError(m)


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def setup():
    start = [a for a in actors().get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    require(start, "No PlayerStart")
    base = start[0].get_actor_location()
    cam_loc = unreal.Vector(base.x, base.y + 600, base.z + EYE)
    STATE["cam_loc"] = cam_loc
    cam = actors().spawn_actor_from_class(unreal.CameraActor, cam_loc, unreal.Rotator(0, 0, 0))
    c = cam.get_component_by_class(unreal.CameraComponent)
    c.set_field_of_view(90.0)
    c.set_editor_property("aspect_ratio", RESOLUTION[0] / RESOLUTION[1])
    c.set_editor_property("constrain_aspect_ratio", True)
    STATE["camera"] = cam
    target = actors().spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(cam_loc.x + 125, cam_loc.y, base.z - 90),
                                             unreal.Rotator(0, 180, 0))
    tc = target.skeletal_mesh_component
    tc.set_skeletal_mesh_asset(unreal.load_asset("/Game/Art/Characters/UrbanEntry01/SK_VS_UrbanEntry01"))
    pose(tc, unreal.load_asset("/Game/Art/Characters/UrbanEntry01/Animations/Rifle02/A_UE01_R02_Hold"), 0.0)
    arms = actors().spawn_actor_from_class(unreal.SkeletalMeshActor, cam_loc, unreal.Rotator(0, 0, 0))
    ac = arms.skeletal_mesh_component
    ac.set_skeletal_mesh_asset(unreal.load_asset(f"{ARMS}/SK_FPArms03_KnifePilot"))
    ac.set_cast_shadow(False)
    knife = actors().spawn_actor_from_class(unreal.StaticMeshActor, cam_loc, unreal.Rotator(0, 0, 0))
    knife.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    knife.static_mesh_component.set_static_mesh(unreal.load_asset("/Game/Art/Weapons/Knife01/Meshes/SM_VS_Knife01"))
    knife.static_mesh_component.set_cast_shadow(False)
    rule = unreal.AttachmentRule.SNAP_TO_TARGET
    require(knife.root_component.attach_to_component(ac, "weapon_socket_r", rule, rule, rule, False), "Knife attach failed")
    loc, q = KNIFE_OFFSET
    knife.root_component.set_relative_transform(unreal.Transform(unreal.Vector(*loc), unreal.Quat(*q).rotator(), unreal.Vector(1, 1, 1)), False, False)
    STATE.update(target=target, arms=arms, knife=knife)
    STATE["jobs"] = [("reference", None, None)] + [(f"{cid}_{pid}", cid, (clip, t)) for cid in CANDIDATES for pid, clip, t in POSES]


def pose(component, anim, t):
    component.set_update_animation_in_editor(True)
    component.set_editor_property("visibility_based_anim_tick_option",
                                  unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    component.play_animation(anim, False)
    component.set_position(t, False)
    component.set_play_rate(0.0)


def arrange(job):
    _, cid, clip = job
    arms, knife = STATE["arms"], STATE["knife"]
    hidden = cid is None
    arms.set_actor_hidden_in_game(hidden)
    knife.set_actor_hidden_in_game(hidden)
    if hidden:
        return
    off = CANDIDATES[cid]
    cam = STATE["cam_loc"]
    arms.set_actor_location(unreal.Vector(cam.x + off[0], cam.y + off[1], cam.z + off[2]), False, False)
    pose(arms.skeletal_mesh_component, unreal.load_asset(f"{ARMS}/Animations/{clip[0]}"), clip[1])


def finish(error=None):
    if STATE["done"]:
        return
    STATE["done"] = True
    unreal.unregister_slate_post_tick_callback(STATE["handle"])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    for key in ("camera", "target", "knife", "arms"):
        if STATE.get(key):
            actors().destroy_actor(STATE[key])
    report = {"engine": unreal.SystemLibrary.get_engine_version(), "runtime": False, "candidates_arm_from_camera_cm": CANDIDATES,
              "poses": POSES, "fov": 90, "resolution": RESOLUTION, "target_distance_cm": 125, "captures": STATE["captures"],
              "map_unchanged": hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() == STATE["map"], "passed": error is None, "error": error}
    (OUT / "framing-report.json").write_text(json.dumps(report, indent=1) + "\n")
    if error or not report["map_unchanged"]:
        unreal.log_error(f"VALSTRIKE_KNIFE_FP_FRAMING_FAILED: {error or 'map changed'}")
    else:
        unreal.log(f"VALSTRIKE_KNIFE_FP_FRAMING_SUCCESS: {OUT}")
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    if STATE["busy"] or STATE["done"]:
        return
    STATE["busy"] = True
    try:
        require(time.monotonic() - STATE["started"] < 400, "Framing preview exceeded 400 s")
        if time.monotonic() - STATE["started"] < 4.0:
            return
        if "camera" not in STATE:
            setup()
            return
        if STATE["task"] is None:
            if STATE["i"] >= len(STATE["jobs"]):
                finish()
                return
            job = STATE["jobs"][STATE["i"]]
            arrange(job)
            STATE["file"] = OUT / f"{job[0]}.png"
            STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(*RESOLUTION, str(STATE["file"]), camera=STATE["camera"],
                                                                             delay=1.0, force_game_view=True)
            return
        if not STATE["task"].is_task_done():
            return
        data = STATE["file"].read_bytes()
        require(data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == RESOLUTION, "Bad capture")
        STATE["captures"].append({"file": STATE["file"].name, "sha256": hashlib.sha256(data).hexdigest()})
        STATE["task"], STATE["i"] = None, STATE["i"] + 1
    except Exception:
        finish(traceback.format_exc())
    finally:
        STATE["busy"] = False


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    STATE["map"] = hashlib.sha256(MAP_FILE.read_bytes()).hexdigest()
    require(unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP), "Could not open graybox")
    OUT.mkdir(parents=True, exist_ok=True)
    for command in ("r.EyeAdaptationQuality 0", "r.DefaultFeature.AutoExposure 0", "r.DefaultFeature.MotionBlur 0"):
        unreal.SystemLibrary.execute_console_command(None, command)
    STATE["started"] = time.monotonic()
    # UE 5.8's -ExecutePythonScript runner quits after the script unless kept alive.
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE["handle"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error(f"VALSTRIKE_KNIFE_FP_FRAMING_FAILED: {traceback.format_exc()}")
        unreal.SystemLibrary.quit_editor()
