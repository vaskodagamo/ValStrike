"""INT-ART-03: editor views of the native third-person knife assembly (not runtime).

Opens VS_Graybox without saving, places temporary UrbanEntry actors posed on the
imported ART-27 knife clips (single-node pose at a fixed time: hold, equip, slash
impact/follow-through, stab impact, walk, run), attaches K01 to hand_r with the
measured runtime offset (UVSKnifePresentationComponent::WorldKnifeOffset) and
captures side and three-quarter views at a fixed exposure. Stationary actions have
a neutral lower body, so these equal the runtime layered pose when standing.
The map file is checked unchanged. Output: docs/assets/knife-integration/third-person/.
"""

import hashlib
import json
from pathlib import Path
import struct
import time
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "docs/assets/knife-integration/third-person"
MAP = "/Game/Maps/VS_Graybox"
MAP_FILE = ROOT / "Content/Maps/VS_Graybox.umap"
UE01 = "/Game/Art/Characters/UrbanEntry01"
RESOLUTION = (1400, 900)
OFFSET = ((2.109, -12.1092, 0.6281), (-0.565402, -0.404277, 0.316081, -0.645735))  # knife-import-report.json
POSES = [("hold", "A_UE01_Knife_Hold", 0.0), ("equip_mid", "A_UE01_Knife_Equip", 0.2),
         ("slash_impact", "A_UE01_Knife_Slash", 0.15), ("slash_follow", "A_UE01_Knife_Slash", 0.25),
         ("stab_impact", "A_UE01_Knife_Stab", 0.30), ("walk", "A_UE01_Knife_Walk", 0.25), ("run", "A_UE01_Knife_Run", 0.2)]
STATE = {"diag": [], "i": 0, "task": None, "busy": False, "done": False, "captures": [], "views": [], "spawned": []}


def require(c, m):
    if not c:
        raise RuntimeError(m)


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def setup():
    found = [a for a in actors().get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    require(found, "No PlayerStart in graybox")
    origin = found[0].get_actor_location()
    mesh = unreal.load_asset(f"{UE01}/SK_VS_UrbanEntry01")
    knife = unreal.load_asset("/Game/Art/Weapons/Knife01/Meshes/SM_VS_Knife01")
    loc, q = OFFSET
    offset = unreal.Transform(unreal.Vector(*loc), unreal.Quat(*q).rotator(), unreal.Vector(1, 1, 1))
    for n, (pid, clip, t) in enumerate(POSES):
        at = unreal.Vector(origin.x + n * 220.0, origin.y, origin.z - 90.0)
        body = actors().spawn_actor_from_class(unreal.SkeletalMeshActor, at, unreal.Rotator(0, 0, 0))
        comp = body.skeletal_mesh_component
        comp.set_skeletal_mesh_asset(mesh)
        comp.set_update_animation_in_editor(True)
        comp.set_editor_property("visibility_based_anim_tick_option",
                                 unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
        comp.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        anim = unreal.load_asset(f"{UE01}/Animations/Knife/{clip}")
        comp.play_animation(anim, False)
        comp.set_position(t, False)
        comp.set_play_rate(0.0)
        k = actors().spawn_actor_from_class(unreal.StaticMeshActor, at, unreal.Rotator(0, 0, 0))
        k.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        k.static_mesh_component.set_static_mesh(knife)
        attached = k.root_component.attach_to_component(comp, "hand_r", unreal.AttachmentRule.SNAP_TO_TARGET,
                                             unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET, False)
        k.root_component.set_relative_transform(offset, False, False)
        STATE["diag"].append({"pose": pid, "anim": str(anim), "mode": str(comp.get_editor_property("animation_mode")),
                              "attached": attached, "hand_r": [round(v, 1) for v in (comp.get_socket_location("hand_r").x,
                              comp.get_socket_location("hand_r").y, comp.get_socket_location("hand_r").z)],
                              "knife": [round(v, 1) for v in (k.get_actor_location().x, k.get_actor_location().y, k.get_actor_location().z)],
                              "knife_parent": str(k.root_component.get_attach_parent())})
        STATE["spawned"] += [body, k]
        target = (at.x + 15, at.y, at.z + 125)
        STATE["views"] += [(f"{pid}_side", (at.x + 10, at.y - 230, at.z + 140), target),
                           (f"{pid}_front", (at.x + 210, at.y - 110, at.z + 150), target)]


def camera(location, target):
    pos = unreal.Vector(*location)
    cam = actors().spawn_actor_from_class(unreal.CameraActor, pos, unreal.MathLibrary.find_look_at_rotation(pos, unreal.Vector(*target)))
    c = cam.get_component_by_class(unreal.CameraComponent)
    c.set_field_of_view(55.0)
    c.set_editor_property("aspect_ratio", RESOLUTION[0] / RESOLUTION[1])
    pp = c.get_editor_property("post_process_settings")
    for key, value in (("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", 1.0),
                       ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 1.0)):
        pp.set_editor_property(key, value)
    c.set_editor_property("post_process_settings", pp)
    return cam


def finish(error=None):
    if STATE["done"]:
        return
    STATE["done"] = True
    unreal.unregister_slate_post_tick_callback(STATE["handle"])
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)
    for a in STATE["spawned"]:
        actors().destroy_actor(a)
    report = {"engine": unreal.SystemLibrary.get_engine_version(), "map": MAP, "runtime": False,
              "offset_on_hand_r": {"location_cm": OFFSET[0], "quat_xyzw": OFFSET[1]}, "poses": POSES,
              "captures": STATE["captures"], "diag": STATE["diag"], "map_unchanged": hashlib.sha256(MAP_FILE.read_bytes()).hexdigest() == STATE["map"],
              "passed": error is None, "error": error}
    (OUT / "preview-report.json").write_text(json.dumps(report, indent=1) + "\n")
    if error or not report["map_unchanged"]:
        unreal.log_error(f"VALSTRIKE_KNIFE_TP_PREVIEW_FAILED: {error or 'map changed'}")
    else:
        unreal.log(f"VALSTRIKE_KNIFE_TP_PREVIEW_SUCCESS: {OUT}")
    unreal.SystemLibrary.quit_editor()


def tick(_dt):
    if STATE["busy"] or STATE["done"]:
        return
    STATE["busy"] = True
    try:
        require(time.monotonic() - STATE["started"] < 300, "Preview exceeded 300 s")
        if time.monotonic() - STATE["started"] < 4.0:
            return
        if not STATE["spawned"]:
            setup()
            return
        if STATE["task"] is None:
            if STATE["i"] >= len(STATE["views"]):
                finish()
                return
            vid, loc, tgt = STATE["views"][STATE["i"]]
            STATE["cam"], STATE["file"] = camera(loc, tgt), OUT / f"{vid}.png"
            STATE["task"] = unreal.AutomationLibrary.take_high_res_screenshot(*RESOLUTION, str(STATE["file"]), camera=STATE["cam"],
                                                                             delay=1.5, force_game_view=True)
            return
        if not STATE["task"].is_task_done():
            return
        data = STATE["file"].read_bytes()
        require(data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == RESOLUTION, "Bad capture")
        STATE["captures"].append({"file": STATE["file"].name, "sha256": hashlib.sha256(data).hexdigest()})
        actors().destroy_actor(STATE["cam"])
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
        unreal.log_error(f"VALSTRIKE_KNIFE_TP_PREVIEW_FAILED: {traceback.format_exc()}")
        unreal.SystemLibrary.quit_editor()
