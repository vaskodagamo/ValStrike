"""Bounded Site01 review: unsaved proxies/cameras; never save production assets.

Execute with the pinned rendered Unreal Editor under Scripts/with_ue_slot.py.
These are editor compositions at gameplay eye heights, not runtime acceptance.
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

SCRIPT = Path(__file__).resolve()
ROOT = SCRIPT.parents[3]
OUT = SCRIPT.parent
SIZE = (1600, 1000)
VIEWS = [
    dict(id="09_transformer_proxy", location=[-1000, -1550, 154.15], target=[800, -750, 154.15],
         proxy=[-750, -1270, 0], caption="Dark temporary proxy against Transformer west face."),
    dict(id="10_site_proxy", location=[1550, -900, 154.15], target=[900, 100, 142.15],
         proxy=[1480, -425, 0], caption="Dark temporary proxy immediately in front of stacked site crates."),
    dict(id="11_cover_standing", location=[710, 320, 154.15], target=[300, 320, 154.15],
         proxy=None, caption="Standing eye over SiteWestCover (120 cm), facing relay facade."),
    dict(id="12_cover_crouched", location=[710, 320, 90.15], target=[300, 320, 90.15],
         proxy=None, caption="Same position and direction at crouched eye height."),
]
S = dict(handle=None, task=None, finished=False, busy=False, index=0, temporary=[], captures=[])


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def content_hashes():
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted((ROOT / "Content").rglob("*")) if p.is_file()}


def spawn(kind, label, position, rotation=None):
    actor = S["actors"].spawn_actor_from_class(kind, unreal.Vector(*position), rotation or unreal.Rotator())
    require(actor, "Spawn failed: " + label)
    actor.set_actor_label("QA_UNSAVED_" + label)
    S["temporary"].append(actor)
    return actor


def mesh(label, asset, position, scale=(1, 1, 1), rotation=None, material=None):
    actor = spawn(unreal.StaticMeshActor, label, position, rotation)
    comp = actor.static_mesh_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_static_mesh(unreal.load_asset(asset))
    require(comp.static_mesh, "Missing mesh: " + asset)
    comp.set_collision_profile_name("NoCollision")
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        comp.set_material(0, material)
    return actor


def proxy_material():
    material = unreal.Material(name="SiteReadabilityProxy_" + S["run_id"])
    require(material.get_path_name().startswith("/Engine/Transient"), "Proxy material must be transient")
    lib = unreal.MaterialEditingLibrary
    color = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property("constant", unreal.LinearColor(0.035, 0.045, 0.055, 1))
    roughness = lib.create_material_expression(material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property("r", 0.8)
    require(lib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR), "Color connection")
    require(lib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS), "Roughness connection")
    require(not lib.recompile_material(material), "Proxy material compilation failed")
    return material


def stage(view):
    for actor in S["temporary"]:
        S["actors"].destroy_actor(actor)
    S["temporary"] = []
    position = unreal.Vector(*view["location"])
    rotation = unreal.MathLibrary.find_look_at_rotation(position, unreal.Vector(*view["target"]))
    camera = spawn(unreal.CameraActor, view["id"], view["location"], rotation)
    component = camera.get_component_by_class(unreal.CameraComponent)
    component.set_field_of_view(90)
    component.set_editor_property("aspect_ratio", SIZE[0] / SIZE[1])
    component.set_editor_property("constrain_aspect_ratio", False)
    # Keep saved map lighting and project postprocess defaults. No exposure override.
    component.set_editor_property("post_process_blend_weight", 0.0)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(position, rotation)
    if view["proxy"]:
        x, y, z = view["proxy"]
        mesh("TEMPORARY_DARK_PROXY_BODY", "/Engine/BasicShapes/Cube", [x, y, z + 72],
             (0.5, 0.4, 1.44), material=S["material"])
        mesh("TEMPORARY_DARK_PROXY_HEAD", "/Engine/BasicShapes/Sphere", [x, y, z + 160],
             (0.32, 0.32, 0.32), material=S["material"])
    if view["proxy"] is None:
        return camera  # Keep cover-height pair free of cosmetic wall intersection.
    # Accepted rifle assets, staged at the earlier art-composition offset.
    # This is not the separate in-progress runtime presentation component.
    yaw, pitch = math.radians(rotation.yaw), math.radians(rotation.pitch)
    def world_offset(x, y, z):
        forward = (math.cos(pitch)*math.cos(yaw), math.cos(pitch)*math.sin(yaw), math.sin(pitch))
        right = (-math.sin(yaw), math.cos(yaw), 0)
        up = (-math.sin(pitch)*math.cos(yaw), -math.sin(pitch)*math.sin(yaw), math.cos(pitch))
        return [view["location"][i] + forward[i]*x + right[i]*y + up[i]*z for i in range(3)]
    base = "/Game/Art/Weapons/Rifle01/Meshes/"
    body = mesh("ACCEPTED_R01_COMPOSITION", base + "SM_VS_Rifle01_Body", world_offset(32, 16, -18), rotation=rotation)
    socket = body.static_mesh_component.get_socket_transform("Magazine", unreal.RelativeTransformSpace.RTS_WORLD)
    mag = mesh("ACCEPTED_R01_MAGAZINE", base + "SM_VS_Rifle01_Magazine", [0, 0, 0])
    mag.set_actor_transform(socket, False, True)
    return camera


def finish(error=None):
    if S["finished"]:
        return
    S["finished"] = True
    if S["handle"] is not None:
        unreal.unregister_slate_post_tick_callback(S["handle"])
    try:
        report = {"run_id": S.get("run_id"), "engine": unreal.SystemLibrary.get_engine_version(),
                  "passed": error is None, "error": error, "captures": S["captures"],
                  "mode": "Native editor compositions; unsaved proxy and rifle placement; no gameplay",
                  "source_revision": "a520ff20fba6bb9a94d0fc6ae2c59acff3d94606",
                  "capture_script_sha256": digest(SCRIPT),
                  "proxy": {"final_character": False, "body_dimensions_cm": [50, 40, 144],
                            "head_diameter_cm": 32, "total_height_cm": 176,
                            "base_color_linear_rgb": [0.035, 0.045, 0.055], "roughness": 0.8},
                  "weapon": "Views09/10 only: accepted R01 body/magazine; camera-relative grip (32,16,-18) cm; no arms or runtime component",
                  "camera": {"fov_degrees": 90, "standing_eye_cm_above_capsule_bottom": 152,
                             "crouched_eye_cm_above_capsule_bottom": 88, "observed_floor_gap_cm": 2.15,
                             "floor_gap_source": "docs/maps/site01/runtime-report.json; prior single-player traversal",
                             "postprocess_override": False, "lighting_changed": False},
                  "runtime_verified": False, "multiplayer_verified": False, "human_playtest": False}
        if "hashes" in S:
            after = content_hashes()
            report["content_sha256_before"] = S["hashes"]
            report["content_files_unchanged"] = after == S["hashes"]
            if after != S["hashes"]:
                report.update(passed=False, error="Content changed during review")
        (OUT / "capture-report.json").write_text(json.dumps(report, indent=2) + "\n")
        unreal.log("SITE_READABILITY_RESULT: " + str(report["passed"]))
        if error:
            unreal.log_error(error)
    except Exception:
        unreal.log_error("SITE_READABILITY_REPORT_FAILED: " + traceback.format_exc())
    finally:
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    if S["busy"] or S["finished"]:
        return
    S["busy"] = True
    try:
        require(time.monotonic() - S["started"] < 150, "Capture timeout")
        if time.monotonic() - S["started"] < 4:
            return
        if S["task"] is None:
            if S["index"] == len(VIEWS):
                finish()
                return
            view = VIEWS[S["index"]]
            camera = stage(view)
            S["pending"] = OUT / "previews" / (".pending-" + S["run_id"] + ".png")
            S["task"] = unreal.AutomationLibrary.take_high_res_screenshot(
                *SIZE, str(S["pending"]), camera=camera, delay=3.0, force_game_view=True)
            require(S["task"] and S["task"].is_valid_task(), "Screenshot task missing")
            return
        if not S["task"].is_task_done():
            return
        data = S["pending"].read_bytes()
        require(data[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", data[16:24]) == SIZE, "Invalid screenshot")
        view = VIEWS[S["index"]]
        output = OUT / "previews" / (view["id"] + ".png")
        S["pending"].replace(output)
        S["captures"].append({**view, "file": str(output.relative_to(ROOT)), "resolution": list(SIZE),
                              "sha256": digest(output), "proxy_is_temporary": bool(view["proxy"])})
        S["index"] += 1
        S["task"] = None
    except Exception:
        finish(traceback.format_exc())
    finally:
        S["busy"] = False


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned UE 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT, "Wrong project checkout")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    require(not levels.is_in_play_in_editor(), "Use fresh non-PIE editor")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "Unsaved map already open")
    S["hashes"] = content_hashes()
    require(levels.load_level("/Game/Maps/Site01/VS_Site01"), "Cannot load Site01")
    S["actors"] = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    S["run_id"] = uuid.uuid4().hex
    S["material"] = proxy_material()
    S["started"] = time.monotonic()
    (OUT / "previews").mkdir(exist_ok=True)
    unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    S["handle"] = unreal.register_slate_post_tick_callback(tick)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        finish(traceback.format_exc())
