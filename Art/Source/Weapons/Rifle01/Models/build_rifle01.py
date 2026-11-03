"""R01 Kestrel: original fictional ValStrike rifle art, with no gameplay logic.

Blender 5.2.2, centimeters, +X muzzle, +Z up. Rebuild with Blender factory
startup. Pass -- --render-only to render the saved scene without rewriting
the model, FBX exports, or validation evidence.
"""

import json
import math
import sys
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
EXPORTS = ROOT / "Exports"
PREVIEWS = ROOT / "Previews"
BLEND = ROOT / "R01_Kestrel.blend"
SHARED_PALETTE = Path(__file__).resolve().parents[2] / "Shared/weapon_palette.json"
_FINISH = json.loads(SHARED_PALETTE.read_text())
_ROLES = _FINISH["variants"][_FINISH["default"]]["roles"]
PALETTE = {name: (_ROLES[role]["srgb_hex"], _ROLES[role]["roughness"], _ROLES[role]["metallic"])
           for name, role in _FINISH["rifle_material_roles"].items()}

SOCKETS = {
    "Muzzle": {"location_cm": [56, 0, 8.2], "rotation_degrees": [0, 0, 0]},
    "Grip": {"location_cm": [0, 0, 0], "rotation_degrees": [0, 0, 0]},
    "SupportHand": {"location_cm": [31, 0, 2.4], "rotation_degrees": [0, 0, 0]},
    "Magazine": {"location_cm": [14, 0, 2], "rotation_degrees": [0, 0, 0]},
}
MATERIALS = {}
BODY_PARTS, MAG_PARTS, ASSETS, RECORDS = [], [], [], []


def rgb(hexcode):
    def linear(v):
        return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
    return tuple(linear(int(hexcode[i:i + 2], 16) / 255) for i in (0, 2, 4)) + (1,)


def material(name, hexcode, roughness, metallic):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = rgb(hexcode)
    mat.use_nodes = True
    node = mat.node_tree.nodes.get("Principled BSDF")
    node.inputs["Base Color"].default_value = rgb(hexcode)
    node.inputs["Roughness"].default_value = roughness
    node.inputs["Metallic"].default_value = metallic
    return mat


def recollect(obj, collection):
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    collection.objects.link(obj)


def finish(obj, label, mat, bevel, minsize):
    obj.name = label
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new("Stylized edge chamfer", "BEVEL")
        modifier.width = min(bevel, minsize * .23)
        modifier.segments = 1
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.data.materials.append(MATERIALS["M_VS_R01_" + mat])
    obj.select_set(False)
    return obj


def box(label, center, size, mat="Graphite", bevel=.2):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.dimensions = size
    return finish(obj, label, mat, bevel, min(size))


def prism(label, profile_xz, width, mat="Graphite", bevel=.22, y=0):
    n = len(profile_xz)
    verts = [(x, y + side * width / 2, z) for side in (-1, 1) for x, z in profile_xz]
    faces = [tuple(range(n - 1, -1, -1)), tuple(range(n, 2 * n))]
    faces += [(i, (i + 1) % n, (i + 1) % n + n, i + n) for i in range(n)]
    mesh = bpy.data.meshes.new(label + "_editable")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(label, mesh)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    return finish(obj, label, mat, bevel, width)


def cylinder_x(label, x0, x1, radius, z=8.2, mat="Graphite", segments=12, bevel=.12):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=radius, depth=x1 - x0,
                                      end_fill_type="NGON", location=((x0 + x1) / 2, 0, z),
                                      rotation=(0, math.pi / 2, 0))
    obj = bpy.context.object
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return finish(obj, label, mat, bevel, min(radius * 2, x1 - x0))


def ring_x(label, x0, x1, outer, inner, z=8.2, mat="Graphite", segments=12):
    verts = []
    for x, radius in ((x0, outer), (x1, outer), (x0, inner), (x1, inner)):
        for i in range(segments):
            angle = 2 * math.pi * i / segments
            verts.append((x, radius * math.cos(angle), z + radius * math.sin(angle)))
    faces = []
    for i in range(segments):
        j = (i + 1) % segments
        faces += [(i, j, segments + j, segments + i),
                  (2 * segments + i, 3 * segments + i, 3 * segments + j, 2 * segments + j),
                  (i, 2 * segments + i, 2 * segments + j, j),
                  (segments + i, segments + j, 3 * segments + j, 3 * segments + i)]
    mesh = bpy.data.meshes.new(label + "_editable")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(label, mesh)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    return finish(obj, label, mat, .1, outer - inner)


def make_body():
    parts = [
        prism("receiver_primary", [(-11, 5), (-7, 12.2), (19, 12.2), (25, 10),
                                  (25, 4), (18, 1.5), (1, 1.5), (-2, 4)], 7.4, bevel=.45),
        prism("receiver_lower", [(-8, 4), (-5, 5.5), (21, 5.5), (21, 1),
                                (17, .4), (10, .4), (8, 2), (-7, 2)], 6.8, bevel=.25),
        prism("stock_upper_spine", [(-28, 7), (-28, 11), (-11, 11), (-8, 8), (-10, 6.8)], 5.4, bevel=.35),
        prism("stock_buttpad", [(-30, -1), (-30, 11.2), (-27, 12), (-25.8, 9.5), (-26.7, -1)], 6.6, bevel=.35),
        prism("stock_lower_brace", [(-27, -.2), (-23, -.2), (-10, 6.7), (-11, 9), (-17, 6)], 3.6, bevel=.25),
        prism("pistol_grip", [(-6, 3), (.8, 2), (-2.5, -12), (-4.8, -13), (-10, -10.5)], 4.8, bevel=.55),
        prism("grip_heel", [(-10, -10), (-4, -11.3), (-2.5, -12), (-4.8, -13), (-10.4, -11.4)], 5.1, bevel=.2),
        # Three closed profile pieces form a genuinely open trigger-guard loop.
        prism("guard_front", [(9, 2.3), (11, 2.3), (11, -3), (9.2, -4), (8.5, -2.5)], 2.8, bevel=.2),
        prism("guard_bottom", [(-1, -3.5), (9.2, -4), (10, -2.6), (-.5, -2)], 2.8, bevel=.2),
        prism("guard_rear", [(-2, 2), (0, 2), (-.5, -3), (-2, -3.5)], 2.8, bevel=.2),
        prism("trigger_visual", [(3, 2.3), (4.1, 2.3), (4, -.4), (3, -1.6), (2.4, -1.2), (3, .2)], 1.1, bevel=.1),
        prism("magazine_well", [(8, 3.3), (20.3, 3.3), (20, 0), (9.4, -.4), (7.5, 1)], 7.2, bevel=.3),
        prism("handguard_inner", [(22, 4), (41, 4), (42, 7), (39, 12), (22, 12)], 5.8, bevel=.3),
        prism("handguard_lower_spine", [(23, 2.4), (36, 2.4), (41, 5), (24, 6)], 5.8, bevel=.32),
        box("top_sight_bridge", (11.5, 0, 12.8), (48, 3.3, 1.2), bevel=.2),
        cylinder_x("barrel_visible", 39, 53, 1.28, bevel=.15),
        cylinder_x("barrel_shoulder", 41.5, 44, 1.8, bevel=.2),
        ring_x("muzzle_crown_recess", 51.5, 56, 2.15, 1.02),
        cylinder_x("muzzle_recess_floor", 51.5, 52.5, 1.01, bevel=.02),
        cylinder_x("muzzle_collar", 50, 51.5, 2.35, mat="Teal", bevel=.15),
        box("rear_sight_base", (-9, 0, 13.65), (4.5, 4.2, .9), bevel=.18),
        box("rear_sight_left", (-9, -1.55, 14.65), (2.6, .8, 1.5), bevel=.1),
        box("rear_sight_right", (-9, 1.55, 14.65), (2.6, .8, 1.5), bevel=.1),
        box("front_sight_base", (37.5, 0, 13.3), (3.4, 4, .8), bevel=.15),
        box("front_sight_post", (37.5, 0, 14.3), (.8, .7, 1.2), bevel=.1),
        box("front_sight_bead", (37.5, 0, 15.05), (.75, .65, .3), "Accent", .04),
    ]
    # Shaped side panels and broad recessed vent gaps, mirrored with no texture.
    for side in (-1, 1):
        y = side * 3.85
        parts += [
            prism("receiver_ceramic_panel", [(-5.5, 7), (-5.5, 10.5), (13, 10.5),
                                             (17, 8.5), (14, 6.8), (-2.5, 6.8)], .36, "Ceramic", .12, y),
            prism("receiver_teal_rear", [(-9, 6), (-7, 9.7), (-6.4, 9.7),
                                       (-6.4, 5.8), (-2, 4.5), (-3, 3.2)], .34, "Teal", .12, y),
            prism("handguard_lower_plate", [(23, 3.5), (36.8, 3.5), (41, 5.7),
                                            (40.8, 7.4), (23, 7.4)], .7, "Teal", .16, side * 4.05),
            prism("handguard_upper_plate", [(23, 9.8), (40.1, 9.8), (38.4, 12.4),
                                            (22, 12.4)], .7, "Teal", .16, side * 4.05),
            prism("stock_cheek_panel", [(-27, 8), (-27, 10.3), (-13, 10.3),
                                       (-10.8, 8.3), (-14.2, 7.7)], .5, "Teal", .14, side * 2.8),
            prism("stock_brace_panel", [(-24.5, 1.3), (-21.5, 1.6), (-13.3, 6),
                                       (-15, 6.8)], .3, "Teal", .1, side * 1.95),
            prism("grip_side_insert", [(-5.8, .3), (-1.2, -.3), (-4.2, -10),
                                      (-7.7, -8.8)], .36, "Teal", .13, side * 2.48),
            box("receiver_amber_mark", (11.7, side * 4.09, 9.2), (3.2, .14, .65), "Accent", .03),
            box("handguard_amber_mark", (25.5, side * 4.46, 11.1), (2.8, .12, .6), "Accent", .025),
            box("receiver_fastener", (-2.3, side * 4.1, 8.65), (1.4, .14, 1.1), bevel=.03),
        ]
        for x in (26, 31, 36):
            parts.append(prism("handguard_vent_rib", [(x, 7.3), (x + 1.1, 7.3),
                                                    (x + 2.2, 10), (x + 1.1, 10)],
                               .72, "Graphite", .1, side * 4.05))
    # Shallow separated top ridges make the first-person view readable.
    for x in (-3, 2, 7, 12, 17, 22, 27, 32):
        parts.append(box("top_bridge_ridge", (x, 0, 13.6), (2.4, 3.8, .6), bevel=.12))
    return parts


def make_magazine():
    # Local zero is the insertion anchor. At-rest assembly translation is
    # SOCKETS['Magazine']; this model is a visual solid, without internals.
    parts = [
        prism("magazine_shell", [(-4.5, 0), (4.5, 0), (5.4, -9.5),
                                 (9.2, -16), (8.3, -18), (-.8, -18), (-4, -10)],
              5.8, "Graphite", .35),
        prism("magazine_baseplate", [(-1.2, -17.2), (8.8, -17.2), (9, -18.8),
                                     (-1.6, -18.8)], 6.6, "Teal", .22),
        box("magazine_insertion_band", (0, 0, -1.1), (9.2, 6, 1.8), "Graphite", .2),
    ]
    for side in (-1, 1):
        parts += [
            prism("magazine_side_panel", [(-3.5, -3), (3.5, -3), (4.4, -10),
                                          (7.2, -15.9), (.1, -15.9), (-2.8, -9.5)],
                  .32, "Teal", .1, side * 2.98),
            prism("magazine_inset_strip", [(-1.5, -4.3), (-.2, -4.3), (1, -10),
                                           (2.8, -14.5), (1.4, -14.5), (-.4, -10)],
                  .16, "Graphite", .035, side * 3.19),
            box("magazine_identification", (1.3, side * 3.2, -5.5), (2.4, .14, .75), "Accent", .03),
        ]
    return parts


def uv_tiles(mesh):
    for layer in list(mesh.uv_layers):
        mesh.uv_layers.remove(layer)
    grid = math.ceil(math.sqrt(len(mesh.polygons)))
    for name in ("UV0", "UV1_Lightmap"):
        layer = mesh.uv_layers.new(name=name)
        for face in mesh.polygons:
            axes = [i for i in range(3) if i != max(range(3), key=lambda a: abs(face.normal[a]))]
            points = [[mesh.vertices[mesh.loops[loop].vertex_index].co[a] for a in axes]
                      for loop in face.loop_indices]
            minima = [min(p[a] for p in points) for a in (0, 1)]
            span = max(max(p[a] for p in points) - minima[a] for a in (0, 1))
            assert span > 1e-9
            col, row = face.index % grid, face.index // grid
            for loop, point in zip(face.loop_indices, points):
                layer.data[loop].uv = tuple(((col if a == 0 else row) + .08 +
                                            .84 * (point[a] - minima[a]) / span) / grid
                                           for a in (0, 1))
    mesh.uv_layers.active_index = 0


def bounds(obj):
    vertices = [obj.matrix_world @ v.co for v in obj.data.vertices]
    low = [min(v[a] for v in vertices) for a in range(3)]
    high = [max(v[a] for v in vertices) for a in range(3)]
    return low, high, [high[a] - low[a] for a in range(3)]


def make_asset(name, parts, pivot):
    collection = bpy.data.collections.new(name + "_EDITABLE_PARTS")
    bpy.context.scene.collection.children.link(collection)
    bpy.ops.object.select_all(action="DESELECT")
    joined = []
    for part in parts:
        recollect(part, collection)
        copy = part.copy()
        copy.data = part.data.copy()
        bpy.context.scene.collection.objects.link(copy)
        copy.select_set(True)
        joined.append(copy)
    bpy.context.view_layer.objects.active = joined[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    assert all(abs(v) < 1e-7 for v in obj.location), name
    assert all(abs(v) < 1e-7 for v in obj.rotation_euler), name
    assert all(abs(v - 1) < 1e-7 for v in obj.scale), name
    obj.data.update()
    uv_tiles(obj.data)
    triangulate = obj.modifiers.new("Deterministic triangulation", "TRIANGULATE")
    bpy.ops.object.modifier_apply(modifier=triangulate.name)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    assert all(edge.is_manifold for edge in bm.edges), name
    assert all(face.calc_area() > 1e-9 for face in bm.faces), name
    volume = bm.calc_volume(signed=True)
    assert volume > 0
    bm.free()
    for layer in obj.data.uv_layers:
        for face in obj.data.polygons:
            uvs = [layer.data[i].uv for i in face.loop_indices]
            assert all(0 <= v <= 1 for uv in uvs for v in uv)
            area = ((uvs[1].x - uvs[0].x) * (uvs[2].y - uvs[0].y)
                    - (uvs[1].y - uvs[0].y) * (uvs[2].x - uvs[0].x))
            assert abs(area) > 1e-14, (name, face.index)
    low, high, dimensions = bounds(obj)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(EXPORTS / (name + ".fbx")), use_selection=True,
        object_types={"MESH"}, global_scale=1, apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
        use_space_transform=True, bake_space_transform=False, mesh_smooth_type="FACE",
        use_mesh_modifiers=True, use_tspace=False, bake_anim=False,
        add_leaf_bones=False, path_mode="AUTO",
    )
    for part in parts:
        part.hide_render = True
        part.hide_set(True)
    ASSETS.append(obj)
    RECORDS.append({
        "name": name, "file": "Exports/" + name + ".fbx", "pivot": pivot,
        "bounds_min_cm": low, "bounds_max_cm": high, "dimensions_cm": dimensions,
        "vertices": len(obj.data.vertices), "triangles": len(obj.data.polygons),
        "material_slots": [m.name for m in obj.data.materials],
        "uv_channels": [layer.name for layer in obj.data.uv_layers], "textures": [],
        "collision": "none; presentation asset",
        "source_closed_manifold_shells": True, "source_signed_volume_cm3": volume,
        "source_transforms_identity": True, "uv_bounds_and_triangle_areas_passed": True,
    })
    return obj


def roundtrip():
    for record in RECORDS:
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(ROOT / record["file"]), use_anim=False)
        added = [obj for obj in bpy.data.objects if obj not in before]
        meshes = [obj for obj in added if obj.type == "MESH"]
        assert len(meshes) == 1
        obj = meshes[0]
        low, high, dimensions = bounds(obj)
        assert all(abs(low[a] - record["bounds_min_cm"][a]) < .002 for a in range(3)), low
        assert all(abs(high[a] - record["bounds_max_cm"][a]) < .002 for a in range(3)), high
        assert len(obj.data.polygons) == record["triangles"]
        assert len(obj.data.uv_layers) == 2
        assert len(obj.data.materials) == len(record["material_slots"])
        record["fbx_roundtrip"] = {"passed": True, "bounds_min_cm": low,
                                   "bounds_max_cm": high, "dimensions_cm": dimensions,
                                   "triangles": len(obj.data.polygons), "uv_channels": 2,
                                   "collision_meshes": 0}
        for obj in added:
            bpy.data.objects.remove(obj, do_unlink=True)


def aim(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat("-Z", "Y").to_euler()


def presentation():
    scene = bpy.context.scene
    collection = bpy.data.collections.new("PREVIEW_ONLY_linked_meshes")
    scene.collection.children.link(collection)
    for source in ASSETS:
        source.hide_render = True
        source.hide_set(True)
        copy = bpy.data.objects.new("DISPLAY_" + source.name, source.data)
        collection.objects.link(copy)
        if "Magazine" in source.name:
            copy.location = SOCKETS["Magazine"]["location_cm"]
    # Empty markers document intended presentation sockets in the source file.
    for name, spec in SOCKETS.items():
        empty = bpy.data.objects.new("SOCKET_" + name, None)
        collection.objects.link(empty)
        empty.empty_display_type = "ARROWS"
        empty.empty_display_size = 3
        empty.location = spec["location_cm"]
    ground_mat = material("PREVIEW_Ground", "DDE0DE", .9, 0)
    bpy.ops.mesh.primitive_plane_add(size=2000, location=(0, 0, -17))
    ground = bpy.context.object
    ground.name = "PREVIEW_Ground"
    ground.data.materials.append(ground_mat)
    recollect(ground, collection)
    scene.world = bpy.data.worlds.new("PREVIEW_World")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (.56, .63, .7, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .45
    for name, location, energy, size in (
        ("Key", (-20, -65, 120), 450000, 90),
        ("Fill", (75, 35, 80), 300000, 70),
    ):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.object
        light.name = "PREVIEW_" + name
        light.data.energy = energy
        light.data.shape = "DISK"
        light.data.size = size
        aim(light, (10, 0, 0))
        recollect(light, collection)
    bpy.ops.object.camera_add(location=(76, -130, 55))
    hero = bpy.context.object
    hero.name = "PREVIEW_HeroCamera"
    hero.data.type = "ORTHO"
    hero.data.ortho_scale = 104
    hero.data.clip_start = .1
    hero.data.clip_end = 10000
    aim(hero, (11, 0, -.5))
    recollect(hero, collection)
    bpy.ops.object.camera_add(location=(-30, 16, 21))
    first = bpy.context.object
    first.name = "PREVIEW_FirstPersonCamera"
    first.data.type = "PERSP"
    first.data.lens = 22
    first.data.clip_start = .15
    first.data.clip_end = 10000
    aim(first, (100, 7, 17))
    recollect(first, collection)
    scene.camera = hero
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 64
    scene.cycles.use_denoising = True
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))


def render_previews():
    scene = bpy.context.scene
    scene.camera = bpy.data.objects["PREVIEW_HeroCamera"]
    scene.render.resolution_x = 1800
    scene.render.resolution_y = 1100
    scene.render.filepath = str(PREVIEWS / "R01_Kestrel_Actual_Meshes.png")
    bpy.ops.render.render(write_still=True)
    scene.camera = bpy.data.objects["PREVIEW_FirstPersonCamera"]
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.filepath = str(PREVIEWS / "R01_Kestrel_FirstPerson_Composition.png")
    bpy.data.objects["PREVIEW_Ground"].hide_render = True
    bpy.ops.render.render(write_still=True)
    print("VALSTRIKE_R01_PREVIEWS_SUCCESS")


def main():
    global MATERIALS
    if "--render-only" in sys.argv:
        bpy.ops.wm.open_mainfile(filepath=str(BLEND))
        render_previews()
        return
    EXPORTS.mkdir(exist_ok=True)
    PREVIEWS.mkdir(exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = .01
    scene.unit_settings.length_unit = "CENTIMETERS"
    MATERIALS = {name: material(name, *values) for name, values in PALETTE.items()}
    make_asset("SM_VS_Rifle01_Body", make_body(), "Grip attachment at local zero")
    make_asset("SM_VS_Rifle01_Magazine", make_magazine(), "Insertion anchor at local zero")
    roundtrip()
    report = {
        "name": "R01 Kestrel", "author": "Codex, original fictional ValStrike weapon art",
        "date_europe_berlin": "2026-11-03", "tool": "Blender " + bpy.app.version_string,
        "external_inputs": [], "units": "centimeters", "forward": "+X", "up": "+Z",
        "palette": {name: {"srgb_hex": values[0], "roughness": values[1], "metallic": values[2]}
                    for name, values in PALETTE.items()},
        "assets": RECORDS, "sockets": SOCKETS,
        "assembly": {"magazine_location_cm": SOCKETS["Magazine"]["location_cm"],
                     "magazine_rotation_degrees": [0, 0, 0], "magazine_scale": [1, 1, 1]},
        "fbx_export": {"axis_forward": "-Y", "axis_up": "Z", "global_scale": 1,
                       "scene_scale_length": .01, "apply_scale_options": "FBX_SCALE_UNITS",
                       "apply_unit_scale": True, "bake_space_transform": False},
        "limitations": ["Presentation-only static meshes; no firing, animation, rig, hands, or gameplay wiring.",
                        "Disconnected closed chamfered shells intentionally overlap inside the silhouette.",
                        "No collision is supplied; this mesh is not authoritative hit or shot geometry.",
                        "Both UV channels are isolated face tiles; no textures or light bake are used.",
                        "Source/Blender export verification only; native Unreal and runtime checks are separate."],
    }
    (ROOT / "mesh_validation.json").write_text(json.dumps(report, indent=2) + "\n")
    presentation()
    render_previews()
    print("VALSTRIKE_R01_MESH_SUCCESS: body and magazine source + FBX checks passed")


if __name__ == "__main__":
    main()
