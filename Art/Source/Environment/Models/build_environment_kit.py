"""Original ValStrike environment kit. Run with Blender 5.2.2 --background --python.

All source coordinates are centimeters; Z up, front -Y. No external geometry,
textures, fonts, or add-ons. Exports are real FBX meshes, not Unreal assets.
"""

import json
import math
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
EXPORTS = ROOT / "Exports"
PREVIEWS = ROOT / "Previews"
PALETTE = {
    "M_VS_Plaster": ("C8B89E", .86, 0.0),
    "M_VS_PaintedMetal": ("416F70", .70, 0.0),
    "M_VS_Trim": ("303C41", .78, 0.0),
    "M_VS_Accent": ("D39B58", .74, 0.0),
}


def linear_channel(value):
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def color(hexcode):
    return tuple(linear_channel(int(hexcode[i:i + 2], 16) / 255) for i in (0, 2, 4)) + (1,)


def material(name, hexcode, roughness, metal=0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = color(hexcode)
    mat.use_nodes = True
    node = mat.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = color(hexcode)
    node.inputs["Roughness"].default_value = roughness
    node.inputs["Metallic"].default_value = metal
    return mat


def move_collection(obj, collection):
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    collection.objects.link(obj)


def box(label, location, dimensions, mat, bevel=1, rotation_x=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = label
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new("Deliberate single-segment chamfer", "BEVEL")
        # Keep plate bevels below half-thickness: meeting bevels can collapse
        # narrow faces into zero-area polygons and export degenerate UVs.
        modifier.width = min(bevel, min(dimensions) * .24)
        modifier.segments = 1
        modifier.affect = "EDGES"
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.rotation_euler.x = math.radians(rotation_x)
    if mat:
        obj.data.materials.append(MATERIALS[mat])
    return obj


def uv_face_tiles(mesh):
    # Every convex polygon gets its own disjoint tile with 10% cell gutters.
    # This produces valid nonoverlapping UVs without a texture dependency.
    for old_layer in list(mesh.uv_layers):
        mesh.uv_layers.remove(old_layer)
    grid = math.ceil(math.sqrt(len(mesh.polygons)))
    for layer_name in ("UV0", "UV1_Lightmap"):
        layer = mesh.uv_layers.new(name=layer_name)
        for polygon in mesh.polygons:
            normal = polygon.normal
            dropped = max(range(3), key=lambda axis: abs(normal[axis]))
            axes = [axis for axis in range(3) if axis != dropped]
            points = [[mesh.vertices[mesh.loops[i].vertex_index].co[axis] for axis in axes]
                      for i in polygon.loop_indices]
            minima = [min(p[a] for p in points) for a in (0, 1)]
            maxima = [max(p[a] for p in points) for a in (0, 1)]
            span = max(maxima[a] - minima[a] for a in (0, 1))
            assert span > 0
            col, row = polygon.index % grid, polygon.index // grid
            for loop_index, point in zip(polygon.loop_indices, points):
                layer.data[loop_index].uv = tuple(
                    ((col if a == 0 else row) + .1 + .8 * (point[a] - minima[a]) / span) / grid
                    for a in (0, 1)
                )
    mesh.uv_layers.active_index = 0


def assemble(name, parts, expected_size, pivot, collision=True):
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    mesh_obj = bpy.context.object
    mesh_obj.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    mesh_obj.data.name = name + "_Geometry"
    mesh_obj.data.update()
    uv_face_tiles(mesh_obj.data)
    triangulate = mesh_obj.modifiers.new("Deterministic triangles", "TRIANGULATE")
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    for uv_layer in mesh_obj.data.uv_layers:
        for triangle in mesh_obj.data.polygons:
            uvs = [uv_layer.data[i].uv for i in triangle.loop_indices]
            assert all(0 <= value <= 1 for uv in uvs for value in uv), name
            area2 = ((uvs[1].x - uvs[0].x) * (uvs[2].y - uvs[0].y)
                     - (uvs[1].y - uvs[0].y) * (uvs[2].x - uvs[0].x))
            assert abs(area2) > 1e-12, (name, triangle.index, "degenerate UV")
    collection = bpy.data.collections.new(name + "_SOURCE")
    bpy.context.scene.collection.children.link(collection)
    move_collection(mesh_obj, collection)
    low = [min(v.co[a] for v in mesh_obj.data.vertices) for a in range(3)]
    high = [max(v.co[a] for v in mesh_obj.data.vertices) for a in range(3)]
    size = [high[a] - low[a] for a in range(3)]
    assert all(abs(size[a] - expected_size[a]) < .002 for a in range(3)), (name, size)
    assert all(abs(v) < 1e-7 for v in mesh_obj.location)
    assert all(abs(v - 1) < 1e-7 for v in mesh_obj.scale)
    bm = bmesh.new()
    bm.from_mesh(mesh_obj.data)
    assert all(edge.is_manifold for edge in bm.edges), name
    assert all(face.calc_area() > 1e-7 for face in bm.faces), name
    volume = bm.calc_volume(signed=True)
    assert volume > 0, name
    bm.free()
    collision_obj = None
    if collision:
        center = [(low[a] + high[a]) / 2 for a in range(3)]
        collision_obj = box("UCX_" + name + "_00", center, size, None, bevel=0)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        move_collection(collision_obj, collection)
        collision_obj.display_type = "WIRE"
        collision_obj.hide_render = True
    bpy.ops.object.select_all(action="DESELECT")
    mesh_obj.select_set(True)
    if collision_obj:
        collision_obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_obj
    bpy.ops.export_scene.fbx(
        filepath=str(EXPORTS / (name + ".fbx")), use_selection=True,
        object_types={"MESH"}, global_scale=1, apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
        use_space_transform=True, bake_space_transform=False, mesh_smooth_type="FACE",
        use_mesh_modifiers=True, use_tspace=False, bake_anim=False,
        add_leaf_bones=False, path_mode="AUTO", use_custom_props=True,
    )
    result = {
        "name": name, "file": "Exports/" + name + ".fbx",
        "bounds_min_cm": low, "bounds_max_cm": high, "dimensions_cm": size,
        "pivot": pivot, "vertices": len(mesh_obj.data.vertices),
        "triangles": len(mesh_obj.data.polygons),
        "material_slots": [m.name for m in mesh_obj.data.materials],
        "uv_channels": [layer.name for layer in mesh_obj.data.uv_layers],
        "uv_layout": "Each convex pre-triangulation face in separate grid tile with 10% cell gutters",
        "textures": [], "collision": "one full-bounds UCX convex box" if collision else "none; decorative",
        "source_closed_manifold_shells": True, "source_signed_volume_cm3": volume,
        "source_transforms_identity": True,
    }
    MESHES.append(mesh_obj)
    COLLISIONS.append(collision_obj)
    RESULTS.append(result)
    return mesh_obj


def build_models():
    # Full envelope 400 x 20 x 300. Broad plaster planes, teal lower band.
    assemble("SM_VS_Wall_400x300", [
        box("wall_plaster", (200, 11, 150), (400, 18, 300), "M_VS_Plaster", 1.5),
        box("wall_lower_band", (200, 1.4, 27), (400, 1.2, 54), "M_VS_PaintedMetal", .6),
        box("wall_foot", (200, 1, 5), (400, 2, 10), "M_VS_Trim", .5),
        box("wall_top", (200, 1, 294), (400, 2, 12), "M_VS_Trim", .5),
        box("wall_panel_joint", (200, 1.6, 171), (2, .8, 234), "M_VS_Trim", .2),
    ], (400, 20, 300), "base corner (minimum X/Y/Z)")
    assemble("SM_VS_Pillar_40x300", [
        box("pillar_body", (20, 20, 150), (36, 36, 268), "M_VS_Plaster", 2),
        box("pillar_base", (20, 20, 8), (40, 40, 16), "M_VS_Trim", 1),
        box("pillar_cap", (20, 20, 292), (40, 40, 16), "M_VS_Trim", 1),
        box("pillar_front_inlay", (20, 1.4, 150), (12, 1.2, 268), "M_VS_PaintedMetal", .7),
        box("pillar_accent", (20, .6, 259), (12, 1.2, 6), "M_VS_Accent", .3),
    ], (40, 40, 300), "base corner (minimum X/Y/Z)")
    crate = [
        box("crate_body", (0, 0, 50), (94, 94, 84), "M_VS_PaintedMetal", 3),
        box("crate_foot", (0, 0, 5), (100, 100, 10), "M_VS_Trim", 2),
        box("crate_lid", (0, 0, 94), (100, 100, 8), "M_VS_PaintedMetal", 2),
        box("crate_lid_inset", (0, 0, 99), (74, 74, 2), "M_VS_Trim", .8),
        box("crate_front_panel", (0, -47.6, 50), (70, 2, 50), "M_VS_Plaster", 2),
        box("crate_rear_panel", (0, 47.6, 50), (70, 2, 50), "M_VS_PaintedMetal", 2),
        box("crate_front_latch", (24, -49, 79), (14, 2, 12), "M_VS_Accent", 1),
    ]
    for x in (-44, 44):
        for y in (-44, 44):
            crate.append(box("crate_corner", (x, y, 50), (12, 12, 80), "M_VS_Trim", 1.5))
    assemble("SM_VS_Crate_100", crate, (100, 100, 100), "bottom center")
    assemble("SM_VS_Cover_200x120", [
        box("cover_foot", (0, 0, 7), (200, 100, 14), "M_VS_Trim", 2),
        box("cover_body", (0, 0, 58), (196, 96, 88), "M_VS_Plaster", 4),
        box("cover_cap", (0, 0, 111), (200, 100, 18), "M_VS_Trim", 3),
        box("cover_front_panel", (0, -47.8, 62), (172, 3.6, 58), "M_VS_PaintedMetal", 2),
        box("cover_rear_panel", (0, 47.8, 62), (172, 3.6, 58), "M_VS_PaintedMetal", 2),
        box("cover_front_stripe", (-65, -49.4, 87), (28, 1.2, 5), "M_VS_Accent", .4),
    ], (200, 100, 120), "bottom center")
    vent = [
        box("vent_left", (-56, 0, 40), (8, 20, 80), "M_VS_PaintedMetal", 1),
        box("vent_right", (56, 0, 40), (8, 20, 80), "M_VS_PaintedMetal", 1),
        box("vent_top", (0, 0, 76), (104, 20, 8), "M_VS_PaintedMetal", 1),
        box("vent_bottom", (0, 0, 4), (104, 20, 8), "M_VS_PaintedMetal", 1),
        box("vent_back", (0, 8, 40), (104, 4, 64), "M_VS_Trim", .5),
    ]
    for z in (17, 29, 41, 53, 65):
        vent.append(box("vent_louver", (0, -1, z), (104, 14, 4), "M_VS_Trim", .7, rotation_x=-18))
    assemble("SM_VS_Vent_120x80", vent, (120, 20, 80), "bottom center", collision=False)


def verify_exports():
    # Round-trip each real FBX into a clean temporary collection, measuring the
    # world-space vertices (not merely trusted source metadata).
    for result in RESULTS:
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(ROOT / result["file"]), use_anim=False)
        imported = [o for o in bpy.data.objects if o not in before]
        render_meshes = [o for o in imported if o.type == "MESH" and not o.name.startswith("UCX_")]
        collision_meshes = [o for o in imported if o.name.startswith("UCX_")]
        assert len(render_meshes) == 1, result["name"]
        obj = render_meshes[0]
        vertices = [obj.matrix_world @ v.co for v in obj.data.vertices]
        low = [min(v[a] for v in vertices) for a in range(3)]
        high = [max(v[a] for v in vertices) for a in range(3)]
        dimensions = [high[a] - low[a] for a in range(3)]
        assert all(abs(dimensions[a] - result["dimensions_cm"][a]) < .01 for a in range(3)), dimensions
        assert all(abs(low[a] - result["bounds_min_cm"][a]) < .01 for a in range(3)), low
        assert len(obj.data.uv_layers) == 2
        assert len(collision_meshes) == (0 if result["collision"].startswith("none") else 1)
        result["fbx_roundtrip"] = {
            "passed": True, "dimensions_cm": dimensions, "bounds_min_cm": low,
            "bounds_max_cm": high, "triangles": len(obj.data.polygons),
            "material_slots": [m.name.split(".00")[0] for m in obj.data.materials],
            "uv_channels": len(obj.data.uv_layers), "collision_meshes": len(collision_meshes),
        }
        for obj in imported:
            bpy.data.objects.remove(obj, do_unlink=True)


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def display_copy(original, label, location, rotation_z=0):
    obj = bpy.data.objects.new(label, original.data)
    PRESENTATION.objects.link(obj)
    obj.location = location
    obj.rotation_euler.z = math.radians(rotation_z)
    return obj


def text_obj(text, location, size, material_name="M_VS_Trim"):
    curve = bpy.data.curves.new("Caption", "FONT")
    curve.body = text
    curve.align_x = "CENTER"
    curve.size = size
    curve.extrude = 0
    obj = bpy.data.objects.new("Caption " + text, curve)
    PRESENTATION.objects.link(obj)
    obj.location = location
    obj.rotation_euler = CAMERA.rotation_euler
    curve.materials.append(MATERIALS[material_name])
    return obj


def create_preview():
    global PRESENTATION, CAMERA
    PRESENTATION = bpy.data.collections.new("PRESENTATION_linked_meshes_no_export")
    bpy.context.scene.collection.children.link(PRESENTATION)
    for obj in MESHES + [c for c in COLLISIONS if c]:
        obj.hide_render = True
        obj.hide_set(True)
    # A compact composition with all five assets distinct and visible.
    display_copy(MESHES[0], "DISPLAY_Wall", (-260, 100, 0))
    display_copy(MESHES[1], "DISPLAY_Pillar", (168, 100, 0))
    display_copy(MESHES[2], "DISPLAY_Crate", (-230, -130, 0), rotation_z=-6)
    display_copy(MESHES[3], "DISPLAY_Cover", (45, -130, 0))
    display_copy(MESHES[4], "DISPLAY_Vent", (-60, 89.8, 160))
    ground_mat = material("Preview_Ground", "E8E5DF", .9)
    bpy.ops.mesh.primitive_plane_add(size=20000, location=(0, 0, -.2))
    ground = bpy.context.object
    ground.name = "Preview studio ground (not an asset)"
    ground.data.materials.append(ground_mat)
    move_collection(ground, PRESENTATION)
    bpy.ops.object.camera_add(location=(690, -1170, 630))
    CAMERA = bpy.context.object
    CAMERA.name = "Preview camera"
    CAMERA.data.type = "ORTHO"
    CAMERA.data.ortho_scale = 830
    CAMERA.data.clip_start = 1
    CAMERA.data.clip_end = 100000
    aim(CAMERA, (-30, 0, 140))
    move_collection(CAMERA, PRESENTATION)
    scene = bpy.context.scene
    scene.camera = CAMERA
    scene.world = bpy.data.worlds.new("Preview studio world")
    scene.world.color = (.22, .22, .22)
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (.65, .71, .78, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .45
    for label, position, energy, size in (
        ("Key", (-350, -420, 850), 13000000, 550),
        ("Fill", (550, -50, 500), 7000000, 500),
    ):
        bpy.ops.object.light_add(type="AREA", location=position)
        light = bpy.context.object
        light.name = "Preview " + label
        light.data.energy = energy
        light.data.shape = "DISK"
        light.data.size = size
        aim(light, (0, 0, 100))
        move_collection(light, PRESENTATION)
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    scene.render.filepath = str(PREVIEWS / "VS_Environment_Kit_Actual_Meshes.png")
    # Save the editable asset plus linked presentation; canonical source objects
    # are retained in their SOURCE collections with identity transforms.
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "VS_Environment_Kit.blend"))
    bpy.ops.render.render(write_still=True)


def main():
    global MATERIALS, MESHES, COLLISIONS, RESULTS
    EXPORTS.mkdir(exist_ok=True)
    PREVIEWS.mkdir(exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = .01
    bpy.context.scene.unit_settings.length_unit = "CENTIMETERS"
    MATERIALS = {name: material(name, *values) for name, values in PALETTE.items()}
    MESHES, COLLISIONS, RESULTS = [], [], []
    build_models()
    verify_exports()
    report = {
        "author": "Codex, original procedural geometry for ValStrike",
        "date_europe_berlin": "2026-11-03",
        "tool": "Blender " + bpy.app.version_string,
        "source": "build_environment_kit.py and editable VS_Environment_Kit.blend",
        "external_inputs": [], "units": "centimeters", "up": "+Z", "front": "-Y",
        "fbx_export": {"axis_forward": "-Y", "axis_up": "Z", "global_scale": 1,
                       "scene_scale_length": .01, "apply_scale_options": "FBX_SCALE_UNITS",
                       "apply_unit_scale": True, "bake_space_transform": False},
        "palette_srgb_hex": {key: value[0] for key, value in PALETTE.items()},
        "assets": RESULTS,
        "limitations": [
            "Blender source/export verification only; Unreal import verification is separate.",
            "Disconnected overlapping closed bevelled shells are intentional hard-surface construction.",
            "One conservative full-envelope convex collision per solid piece; bevel recesses are not collision cuts.",
            "UV channels are valid isolated face tiles, not art-directed texture layouts; no textures used.",
            "No gameplay placement, movement, sightline, packaged-build, or performance acceptance.",
        ],
    }
    (ROOT / "mesh_validation.json").write_text(json.dumps(report, indent=2) + "\n")
    create_preview()
    print("VALSTRIKE_MESH_KIT_SUCCESS: five FBX round-trips and source topology checks passed")


if __name__ == "__main__":
    main()
