"""INT-ART-05: import the accepted ART-28 R02 Hold/Walk/Run onto the existing UrbanEntry
skeleton and measure the new hand_r attachment on the native skeleton.

Allowlist: the three animation-only FBXs, SHA-256 checked against ART-28's frozen
artifact manifest (source 7b307cf). They go to /Game/Art/Characters/UrbanEntry01/
Animations/Rifle02/ (60 fps ranges, root motion off). Mesh, skeleton, rifle and
other assets are never saved and are checked byte-identical.

Verification against read_source_poses.py output (every bone, 13 sampled frames):
every native bone component transform must equal C * B * C (C = diag(1,-1,1)) in
position and axes. Then the R02 placement C * (H_hand_r S) * C gives the hand_r
offset, required constant over all frames. If every bone and the rifle match, the
unchanged geometry reproduces ART-28's sampled stock/face/hand contact exactly.
Also measures the stance-foot ground speed of the in-place walk/run.

Run through Scripts/with_ue_slot.py in a fresh editor; -VSInspectOnly reopens.
"""

import hashlib
import json
import math
from pathlib import Path
import statistics
import traceback

import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "docs/assets/knife-rifle-acceptance"
MANIFEST = ROOT / "docs/assets/urban-entry01/rifle02-pilot/artifact-manifest.json"
SRC = "Art/Source/Characters/UrbanEntry01/Rifle02Pilot/Exports"
UE01 = "/Game/Art/Characters/UrbanEntry01"
DEST = f"{UE01}/Animations/Rifle02"
CLIPS = {"A_UE01_R02_Hold": 1.0, "A_UE01_R02_Walk": 62 / 60, "A_UE01_R02_Run": 38 / 60}
UNCHANGED = ["Content/Art/Characters/UrbanEntry01", "Content/Art/Weapons/Rifle02", "Content/Art/Weapons/Knife01",
             "Content/Art/Characters/FPArms03"]
EXPECTED = {"location_cm": [5.177231, -12.704880, -3.049062], "quat_xyzw": [-0.772515, 0.63491, 0.009242, 0.005007]}


def require(c, m):
    if not c:
        raise RuntimeError(m)


def digest(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def xyz(v, d=4):
    return [round(getattr(v, a), d) for a in ("x", "y", "z")]


def matrix_to_transform(m):
    r = [row[:3] for row in m[:3]]
    trace = r[0][0] + r[1][1] + r[2][2]
    if trace > 0:
        s = math.sqrt(trace + 1.0) * 2
        w, x, y, z = .25 * s, (r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s
    elif r[0][0] > r[1][1] and r[0][0] > r[2][2]:
        s = math.sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2
        w, x, y, z = (r[2][1] - r[1][2]) / s, .25 * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s
    elif r[1][1] > r[2][2]:
        s = math.sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2
        w, x, y, z = (r[0][2] - r[2][0]) / s, (r[0][1] + r[1][0]) / s, .25 * s, (r[1][2] + r[2][1]) / s
    else:
        s = math.sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2
        w, x, y, z = (r[1][0] - r[0][1]) / s, (r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, .25 * s
    return unreal.Transform(unreal.Vector(m[0][3], m[1][3], m[2][3]), unreal.Quat(x, y, z, w).rotator(), unreal.Vector(1, 1, 1))


def reflect(m):
    c = [1, -1, 1, 1]
    return [[c[i] * m[i][j] * c[j] for j in range(4)] for i in range(4)]


def source_files():
    known = {e["path"]: e["sha256"] for e in json.loads(MANIFEST.read_text())["files"]}
    files = {}
    for clip in CLIPS:
        rel = f"{SRC}/{clip}.fbx"
        require(rel in known and digest(ROOT / rel) == known[rel], f"Missing/changed/unlisted source: {rel}")
        files[clip] = rel
    return files


def import_clip(rel, name, skeleton):
    options = unreal.FbxImportUI()
    for k, v in {"import_mesh": False, "import_as_skeletal": True, "import_animations": True, "import_materials": False,
                 "import_textures": False, "create_physics_asset": False, "automated_import_should_detect_type": False,
                 "mesh_type_to_import": unreal.FBXImportType.FBXIT_ANIMATION, "skeleton": skeleton}.items():
        options.set_editor_property(k, v)
    data = options.get_editor_property("anim_sequence_import_data")
    for k, v in {"convert_scene": True, "convert_scene_unit": True, "force_front_x_axis": False,
                 "import_meshes_in_bone_hierarchy": False,
                 "animation_length": unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME}.items():
        data.set_editor_property(k, v)
    task = unreal.AssetImportTask()
    for k, v in {"filename": str(ROOT / rel), "destination_path": DEST, "destination_name": name, "automated": True,
                 "replace_existing": False, "save": False, "factory": unreal.FbxFactory(), "options": options}.items():
        task.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    anim = unreal.load_asset(f"{DEST}/{name}")
    require(isinstance(anim, unreal.AnimSequence), f"Import failed: {name}")
    anim.set_editor_property("enable_root_motion", False)
    require(unreal.EditorAssetLibrary.save_loaded_asset(anim, False), f"Save failed: {name}")


def pose_all(probe, anim, frame, names, parents):
    """Component transforms of every bone at one frame (parents before children)."""
    out = {}
    for name in names:
        local = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, name, frame, False)
        parent = parents[name]
        out[name] = unreal.MathLibrary.compose_transforms(local, out[parent]) if parent else local
    return out


def axis_error(t, m):
    c = reflect(m)
    cols = [[c[r][k] for r in range(3)] for k in range(3)]
    got = [t.rotation.get_axis_x(), t.rotation.get_axis_y(), t.rotation.get_axis_z()]
    return max(abs(getattr(g, a) - v) for g, col in zip(got, cols) for a, v in zip("xyz", col))


def stance_speed(probe_poses):
    speeds = []
    for a, b in zip(probe_poses, probe_poses[1:]):
        lower = "foot_l" if a["foot_l"].translation.z < a["foot_r"].translation.z else "foot_r"
        speeds.append(-(b[lower].translation.x - a[lower].translation.x) * 60.0)
    return round(statistics.median(speeds), 1)


def measure():
    mesh = unreal.load_asset(f"{UE01}/SK_VS_UrbanEntry01")
    probe = unreal.SkeletalMeshComponent()
    probe.set_skeletal_mesh_asset(mesh)
    names = [str(probe.get_bone_name(i)) for i in range(probe.get_num_bones())]
    parents = {n: (str(probe.get_parent_bone(n)) if str(probe.get_parent_bone(n)) != "None" else None) for n in names}
    source = json.loads((OUT / "rifle02-source-poses.json").read_text())
    first_src = next(iter(next(iter(source["clips"].values())).values()))["bones"]
    source_only = sorted(set(first_src) - set(names))
    native_only = sorted(set(names) - set(first_src))
    frames, offsets, worst_t, worst_a = {}, [], 0.0, 0.0
    for clip, by_frame in source["clips"].items():
        anim = unreal.load_asset(f"{DEST}/{clip}")
        for frame, src in by_frame.items():
            native = pose_all(probe, anim, int(frame) - 1, names, parents)
            require(not source_only, f"Source bones missing natively: {source_only}")
            t_err = max(math.dist(xyz(native[b].translation, 6), [m[0][3], -m[1][3], m[2][3]]) for b, m in src["bones"].items())
            a_err = max(axis_error(native[b], m) for b, m in src["bones"].items())
            worst_t, worst_a = max(worst_t, t_err), max(worst_a, a_err)
            rifle = matrix_to_transform(reflect(src["R02_component"]))
            local = unreal.MathLibrary.compose_transforms(rifle, unreal.MathLibrary.invert_transform(native["hand_r"]))
            offsets.append(local)
            frames[f"{clip}@{frame}"] = {"max_bone_translation_error_cm": round(t_err, 5), "max_bone_axis_error": round(a_err, 6),
                                         "R02_component_cm": xyz(rifle.translation), "hand_r_offset_cm": xyz(local.translation)}
    first = offsets[0]
    spread = max(max(math.dist(xyz(o.translation), xyz(first.translation)) for o in offsets),
                 max(1 - abs(sum(getattr(o.rotation, a) * getattr(first.rotation, a) for a in "xyzw")) for o in offsets))
    q = first.rotation
    walk = unreal.load_asset(f"{DEST}/A_UE01_R02_Walk")
    run = unreal.load_asset(f"{DEST}/A_UE01_R02_Run")
    gait = {}
    for label, anim in (("A_UE01_R02_Walk", walk), ("A_UE01_R02_Run", run)):
        n = unreal.AnimationLibrary.get_num_frames(anim)
        seq = [pose_all(probe, anim, i, names, parents) for i in range(n + 1)]
        gait[label] = stance_speed(seq)
    return {"basis": "y_negated", "bones_compared": len(first_src), "native_only_bones": native_only, "frames": frames,
            "max_bone_translation_error_cm": round(worst_t, 5), "max_bone_axis_error": round(worst_a, 6),
            "hand_r_offset": {"location_cm": xyz(first.translation),
                              "quat_xyzw": [round(q.x, 6), round(q.y, 6), round(q.z, 6), round(q.w, 6)]},
            "hand_r_offset_max_spread": round(spread, 6), "art28_expected": EXPECTED, "gait_stance_speed_cm_s": gait}


def main():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8.4-"), "Pinned Unreal 5.8.4 required")
    require(Path(unreal.Paths.project_dir()).resolve() == ROOT.resolve(), "Run in the INT-ART-05 worktree")
    files = source_files()
    inspect_only = "-VSInspectOnly" in unreal.SystemLibrary.get_command_line()
    before = {str(p.relative_to(ROOT)): digest(p) for d in UNCHANGED for p in sorted((ROOT / d).rglob("*")) if p.is_file()}
    if not inspect_only:
        require(not unreal.EditorAssetLibrary.does_directory_have_assets(DEST), f"{DEST} exists; inspect, do not overwrite")
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
        skeleton = unreal.load_asset(f"{UE01}/SK_VS_UrbanEntry01").get_editor_property("skeleton")
        for clip, rel in files.items():
            import_clip(rel, clip, skeleton)
    clips = {}
    for clip, length in CLIPS.items():
        a = unreal.load_asset(f"{DEST}/{clip}")
        clips[clip] = {"length_s": round(a.get_play_length(), 4), "expected_s": round(length, 4),
                       "root_motion": a.get_editor_property("enable_root_motion"),
                       "skeleton": a.get_editor_property("skeleton").get_path_name()}
    report = {"engine": unreal.SystemLibrary.get_engine_version(), "mode": "fresh_process_reopen" if inspect_only else "import_and_save",
              "source_sha256": {rel: digest(ROOT / rel) for rel in files.values()}, "clips": clips, "fit": measure()}
    report["unchanged_existing_assets"] = {p: digest(ROOT / p) for p in before} == before
    target = OUT / ("rifle02-fit-reopen-report.json" if inspect_only else "rifle02-fit-import-report.json")
    target.write_text(json.dumps(report, indent=2) + "\n")
    fit = report["fit"]
    require(report["unchanged_existing_assets"], "Existing assets changed")
    for clip, c in clips.items():
        require(abs(c["length_s"] - c["expected_s"]) < 0.002 and not c["root_motion"], f"{clip}: length/root motion")
    require(fit["max_bone_translation_error_cm"] < 0.01, "Native bones differ from ART-28 source")
    require(fit["hand_r_offset_max_spread"] < 0.01, "hand_r offset not constant")
    require(math.dist(fit["hand_r_offset"]["location_cm"], EXPECTED["location_cm"]) < 0.01, "Offset differs from ART-28 prediction")
    if inspect_only:
        prior = json.loads((OUT / "rifle02-fit-import-report.json").read_text())
        require(report["clips"] == prior["clips"] and report["fit"] == prior["fit"], "Reopen differs")
    unreal.log(f"VALSTRIKE_R02_FIT_SUCCESS: {target}")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"VALSTRIKE_R02_FIT_FAILED: {error}\n{traceback.format_exc()}")
    finally:
        unreal.SystemLibrary.quit_editor()
