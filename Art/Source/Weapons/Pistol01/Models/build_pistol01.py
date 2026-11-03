"""P01 Shrike: original stylized ValStrike pistol visual asset, ART-03.

Blender 5.2.2; centimeters, +X muzzle/+Z up. Editable source parts and three
canonical static meshes. No internal firearm mechanism, rig, or gameplay.
Run with -- --skip-render to export only; -- --render-only preserves exports.
-- --update-finish updates saved material values and exports without rebuilding geometry.
"""
import hashlib
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
BLEND = ROOT / "P01_Shrike.blend"
SHARED_PALETTE = Path(__file__).resolve().parents[2] / "Shared/weapon_palette.json"
_FINISH = json.loads(SHARED_PALETTE.read_text())
_ROLES = _FINISH["variants"][_FINISH["default"]]["roles"]
PALETTE = {"M_VS_P01_" + role: (spec["srgb_hex"].lstrip("#"), spec["roughness"], spec["metallic"])
           for role, spec in _ROLES.items()}
SOCKETS = {
    "Grip": {"location_cm": [0, 0, 0], "rotation_degrees": [0, 0, 0]},
    "Slide": {"location_cm": [0, 0, 3], "rotation_degrees": [0, 0, 0]},
    "Magazine": {"location_cm": [0, 0, 0], "rotation_degrees": [0, 0, 0]},
    "Muzzle": {"location_cm": [16.7, 0, 4.75], "rotation_degrees": [0, 0, 0]},
}
ASSEMBLY = {"SM_VS_Pistol01_Body": [0, 0, 0], "SM_VS_Pistol01_Slide": [0, 0, 3],
            "SM_VS_Pistol01_Magazine": [0, 0, 0]}
MATERIALS, ASSETS, RECORDS = {}, [], []

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
    obj.data.materials.append(MATERIALS["M_VS_P01_" + mat])
    obj.select_set(False)
    return obj


def box(label, center, size, mat="Structure", bevel=.2):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.dimensions = size
    return finish(obj, label, mat, bevel, min(size))


def prism(label, profile_xz, width, mat="Structure", bevel=.22, y=0):
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


def cylinder_x(label, x0, x1, radius, z=1.75, mat="Structure", segments=12, bevel=.12):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.mesh.primitive_cylinder_add(vertices=segments, radius=radius, depth=x1 - x0,
                                      end_fill_type="NGON", location=((x0 + x1) / 2, 0, z),
                                      rotation=(0, math.pi / 2, 0))
    obj = bpy.context.object
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return finish(obj, label, mat, bevel, min(radius * 2, x1 - x0))


def ring_x(label, x0, x1, outer, inner, z=1.75, mat="Structure", segments=12):
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


def guard_ring():
    """One closed extruded ring avoids coplanar overlap at visible guard joints."""
    outer = [(.3, .8), (8.8, .8), (9.6, -2.25), (8.5, -3.7), (.2, -3.35)]
    inner = [(1.55, -.15), (7.85, -.15), (8.35, -2.0), (7.8, -2.65), (1.5, -2.25)]
    n = len(outer)
    vertices = [(x, y, z) for loop, y in ((outer,-.95),(outer,.95),(inner,-.95),(inner,.95))
                for x,z in loop]
    faces = []
    for i in range(n):
        j = (i+1) % n
        faces += [(i,j,n+j,n+i), (2*n+i,3*n+i,3*n+j,2*n+j),
                  (i,2*n+i,2*n+j,j), (n+i,n+j,3*n+j,3*n+i)]
    mesh = bpy.data.meshes.new("guard_ring_editable")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("guard_ring", mesh)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action="DESELECT")
    return finish(obj, "guard_ring", "Structure", .12, .8)


def make_body():
    parts = [
        prism("frame_upper", [(-6.6, 1.1), (-5.8, 2.9), (14.9, 2.9), (15.7, 1.9),
                              (14.4, .35), (5.6, .35), (3.6, -.8), (-2.3, -.8)],
              3.7, "Structure", .23),
        prism("grip_core", [(-4.7, 1.3), (1.2, .7), (-1.15, -9.8), (-2.3, -10.4),
                            (-6.8, -9.7), (-6.9, -8.5)], 3.55, "Structure", .32),
        prism("grip_backstrap", [(-5.1, .3), (-6.1, -.2), (-7.25, -8.9),
                                 (-6.75, -9.7), (-5.7, -9.6)], 3.25, "Structure", .16),
        guard_ring(),
        prism("trigger_visual", [(4.25, .6), (5.1, .6), (4.8, -1.4), (4.1, -1.9),
                                  (3.65, -1.6), (4.2, -.6)], .75, "Steel", .06),
        prism("dustcover_lower", [(8.3, .7), (14.3, .7), (14.6, 1.4), (8.5, 1.5)],
              3.3, "Gunmetal", .14),
    ]
    for side in (-1, 1):
        parts += [
            prism("frame_chevron", [(-5.1, 1.35), (-4.4, 2.55), (5.3, 2.55),
                                    (6.2, 1.8), (4.7, .85), (-1.9, .85), (-2.6, .1)],
                  .18, "Gunmetal", .045, side * 1.86),
            prism("grip_inset", [(-4.4, -.6), (-.15, -.6), (-2.3, -8.75),
                                 (-3.0, -9.1), (-5.8, -8.65)], .19, "Structure", .045, side * 1.82),
            prism("grip_heel_insert", [(-6.1, -8.75), (-2.3, -9.3), (-2.6, -9.7),
                                      (-6.35, -9.1)], .2, "Gunmetal", .035, side * 1.84),
            box("slide_stop_visual", (-.6, side * 2.03, 1.6), (2.0, .22, .5), "Steel", .055),
            box("frame_crimson_mark", (9.8, side * 1.9, 1.85), (2.4, .15, .55), "Accent", .035),
            prism("grip_accent_chevron", [(-3.65, -1), (-2.7, -1), (-2.96, -2.1),
                                          (-3.75, -2.5), (-3.55, -1.8)],
                  .15, "Accent", .025, side * 1.95),
        ]
        # Five broad, regular raised bars are matte geometry, not a noisy texture.
        for i in range(5):
            z = -3.35 - i * .95
            x = -2.7 - i * .25
            parts.append(prism("grip_ridge", [(x-1.0,z), (x+1.1,z), (x+.95,z-.27),
                                               (x-1.12,z-.27)], .16, "Structure", .025, side * 1.96))
    return parts


def make_slide():
    # All coordinates below are local to the Slide anchor (body Z = 3 cm).
    parts = [
        prism("slide_main", [(-6.6, .1), (-6.8, 2.65), (-5.8, 3.45), (12.8, 3.45),
                             (15.6, 2.75), (15.6, .35), (14.4, .1)], 3.85, "Gunmetal", .25),
        prism("slide_top_spine", [(-5.5, 3.4), (12.7, 3.4), (14.4, 2.99),
                                  (14.3, 3.3), (12.5, 3.68), (-5.2, 3.68)],
              2.15, "Structure", .07),
        # A short octagonal recessed muzzle is a visual shell only.
        ring_x("muzzle_recess", 15.55, 16.7, 1.27, .66, z=1.75, mat="Structure", segments=12),
        ring_x("muzzle_rim", 16.47, 16.7, 1.28, .66, z=1.75, mat="Steel", segments=12),
        cylinder_x("muzzle_recess_floor", 15.56, 15.65, .655, z=1.75, mat="Structure", bevel=.015),
        box("rear_sight_base", (-4.9, 0, 3.77), (1.8, 3.1, .35), "Structure", .075),
        box("rear_sight_left", (-4.9, -1.15, 4.1), (1.05, .6, .75), "Structure", .07),
        box("rear_sight_right", (-4.9, 1.15, 4.1), (1.05, .6, .75), "Structure", .07),
        box("front_sight_base", (12.9, 0, 3.62), (1.45, 1.5, .3), "Structure", .06),
        box("front_sight_post", (12.9, 0, 4.02), (.55, .55, .65), "Structure", .055),
        box("front_sight_bead", (12.62, 0, 4.17), (.12, .42, .21), "Accent", .02),
    ]
    for side in (-1, 1):
        parts += [
            prism("slide_steel_plane", [(-1.0, 1.15), (-.55, 2.75), (11.75, 2.75),
                                        (13.8, 2.22), (12.8, 1.45), (2.1, 1.45), (.7, 1.0)],
                  .16, "Steel", .035, side * 1.97),
            prism("slide_front_inset", [(10.2, .55), (14.35, .55), (14.7, 1.2),
                                         (11.0, 1.2)], .14, "Structure", .03, side * 1.96),
            box("slide_identity_mark", (7.7, side * 2.09, 2.12), (2.4, .12, .5), "Accent", .025),
            box("rear_sight_index", (-5.445, side * 1.15, 4.15), (.08, .34, .15), "Steel", .015),
        ]
        for x in (-5.2, -3.8, -2.4):
            parts.append(prism("slide_grasp_rib", [(x-.15,.5), (x+.3,.5), (x+1.0,2.8),
                                                    (x+.55,2.8)], .2, "Structure", .035, side * 1.99))
    return parts


def make_magazine():
    return [
        prism("magazine_shell", [(-4.15, 0), (.5, 0), (-1.7, -9.65), (-5.85, -9.15)],
              2.85, "Gunmetal", .15),
        prism("magazine_baseplate", [(-6.3,-9.35), (-1.5,-9.8), (-1.4,-10.65),
                                     (-2.2,-10.85), (-6.7,-10.05)], 4.05, "Structure", .16),
        prism("magazine_heel", [(-6.4,-9.8), (-2.0,-10.45), (-2.15,-10.77),
                                 (-6.4,-10.15)], 3.45, "Gunmetal", .075),
        box("magazine_base_mark", (-1.58,0,-10.13), (.28,2.0,.4), "Accent", .035),
    ]


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
        material_checks = {}
        for imported, expected in zip(obj.data.materials, record["material_slots"]):
            # Blender adds a numeric suffix when imported materials coexist with
            # the source scene. FBX's actual material base name is still checked.
            assert imported.name.split(".")[0] == expected, imported.name
            node = imported.node_tree.nodes["Principled BSDF"]
            values = PALETTE[expected]
            base_color = list(node.inputs["Base Color"].default_value)
            roughness = node.inputs["Roughness"].default_value
            metallic = node.inputs["Metallic"].default_value
            assert max(abs(a-b) for a,b in zip(base_color, rgb(values[0]))) < 1e-6
            assert abs(roughness-values[1]) < 1e-6
            assert abs(metallic-values[2]) < 1e-6
            material_checks[expected] = {"base_color_linear_rgba": base_color,
                                         "roughness": roughness, "metallic": metallic}
        record["fbx_roundtrip"] = {"passed": True, "bounds_min_cm": low,
                                   "bounds_max_cm": high, "dimensions_cm": dimensions,
                                   "triangles": len(obj.data.polygons), "uv_channels": 2,
                                   "collision_meshes": 0, "material_constants": material_checks}
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
        copy.location = ASSEMBLY[source.name]
    for name, spec in SOCKETS.items():
        empty = bpy.data.objects.new("SOCKET_" + name, None)
        collection.objects.link(empty)
        empty.empty_display_type = "ARROWS"
        empty.empty_display_size = 1.5
        empty.location = spec["location_cm"]
    ground_mat = material("PREVIEW_Ground", "A6A9AD", .9, 0)
    bpy.ops.mesh.primitive_plane_add(size=1000, location=(0, 0, -10.9))
    ground = bpy.context.object
    ground.name = "PREVIEW_Ground"
    ground.data.materials.append(ground_mat)
    recollect(ground, collection)
    scene.world = bpy.data.worlds.new("PREVIEW_World")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (.56, .63, .7, 1)
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = .45
    for name, location, energy, size in (("Key", (5,-35,50), 70000, 45), ("Fill", (-30,25,35), 40000, 40)):
        bpy.ops.object.light_add(type="AREA", location=location)
        light = bpy.context.object
        light.name = "PREVIEW_" + name
        light.data.energy = energy
        light.data.shape = "DISK"
        light.data.size = size
        aim(light, (4,0,-1))
        recollect(light, collection)
    bpy.ops.object.camera_add(location=(30,-62,23))
    hero = bpy.context.object
    hero.name = "PREVIEW_HeroCamera"
    hero.data.type = "ORTHO"
    hero.data.ortho_scale = 34
    hero.data.clip_start = .1
    hero.data.clip_end = 10000
    aim(hero, (3,0,-1.5))
    recollect(hero, collection)
    # Equivalent optical composition to UE +X camera with grip (27,12,-13).
    # Blender source Y is reflected by FBX, so source camera is at Y=+12.
    bpy.ops.object.camera_add(location=(-27,12,13))
    first = bpy.context.object
    first.name = "PREVIEW_FirstPersonCamera"
    first.data.type = "PERSP"
    first.data.lens = 19.65
    first.data.sensor_fit = "HORIZONTAL"
    first.data.clip_start = .1
    first.data.clip_end = 10000
    aim(first, (100,12,13))
    recollect(first, collection)
    scene.camera = hero
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.render.threads_mode = "FIXED"
    scene.render.threads = 4
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))


def render_previews():
    scene = bpy.context.scene
    scene.camera = bpy.data.objects["PREVIEW_HeroCamera"]
    bpy.data.objects["PREVIEW_Ground"].hide_render = False
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1000
    scene.render.filepath = str(PREVIEWS / "P01_Shrike_Actual_Meshes.png")
    bpy.ops.render.render(write_still=True)
    scene.camera = bpy.data.objects["PREVIEW_FirstPersonCamera"]
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1000
    scene.render.filepath = str(PREVIEWS / "P01_Shrike_FirstPerson_Composition.png")
    bpy.data.objects["PREVIEW_Ground"].hide_render = True
    bpy.ops.render.render(write_still=True)
    print("VALSTRIKE_P01_PREVIEWS_SUCCESS")


def geometry_fingerprint(obj):
    """Includes topology, UVs, material assignments and transforms, excludes shaders."""
    data = {"vertices": [list(v.co) for v in obj.data.vertices],
            "edges": [list(e.vertices) for e in obj.data.edges],
            "faces": [{"vertices": list(f.vertices), "material": f.material_index,
                       "smooth": f.use_smooth} for f in obj.data.polygons],
            "uvs": {layer.name: [list(item.uv) for item in layer.data] for layer in obj.data.uv_layers},
            "materials": [mat.name for mat in obj.data.materials],
            "location": list(obj.location), "rotation": list(obj.rotation_euler), "scale": list(obj.scale),
            "modifiers": [modifier.type for modifier in obj.modifiers]}
    return hashlib.sha256(json.dumps(data, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def update_finish():
    """Revise the saved sources/exported materials while proving geometry unchanged."""
    global RECORDS
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    bpy.context.preferences.filepaths.save_version = 0
    report_path = ROOT / "mesh_validation.json"
    report = json.loads(report_path.read_text())
    RECORDS = report["assets"]
    before = {record["name"]: geometry_fingerprint(bpy.data.objects[record["name"]]) for record in RECORDS}
    for name, values in PALETTE.items():
        mat = bpy.data.materials[name]
        mat.diffuse_color = rgb(values[0])
        node = mat.node_tree.nodes["Principled BSDF"]
        node.inputs["Base Color"].default_value = rgb(values[0])
        node.inputs["Roughness"].default_value = values[1]
        node.inputs["Metallic"].default_value = values[2]
    for record in RECORDS:
        obj = bpy.data.objects[record["name"]]
        bpy.ops.object.select_all(action="DESELECT")
        obj.hide_set(False)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(
            filepath=str(EXPORTS / (obj.name + ".fbx")), use_selection=True,
            object_types={"MESH"}, global_scale=1, apply_unit_scale=True,
            apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
            use_space_transform=True, bake_space_transform=False, mesh_smooth_type="FACE",
            use_mesh_modifiers=True, use_tspace=False, bake_anim=False,
            add_leaf_bones=False, path_mode="AUTO")
        obj.select_set(False)
        obj.hide_set(True)
        low, high, dimensions = bounds(obj)
        assert all(abs(low[i]-record["bounds_min_cm"][i]) < 1e-6 for i in range(3))
        assert all(abs(high[i]-record["bounds_max_cm"][i]) < 1e-6 for i in range(3))
        assert len(obj.data.polygons) == record["triangles"]
    roundtrip()
    after = {record["name"]: geometry_fingerprint(bpy.data.objects[record["name"]]) for record in RECORDS}
    assert before == after, "Finish revision changed mesh geometry, UVs, transforms or assignments"
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    reopened = {record["name"]: geometry_fingerprint(bpy.data.objects[record["name"]]) for record in RECORDS}
    assert before == reopened, "Saved material revision changed canonical geometry"
    for name, values in PALETTE.items():
        node = bpy.data.materials[name].node_tree.nodes["Principled BSDF"]
        assert max(abs(a-b) for a,b in zip(node.inputs["Base Color"].default_value, rgb(values[0]))) < 1e-6
        assert abs(node.inputs["Roughness"].default_value-values[1]) < 1e-6
        assert abs(node.inputs["Metallic"].default_value-values[2]) < 1e-6
    report["palette"] = {name: {"srgb_hex": values[0], "roughness": values[1], "metallic": values[2]}
                         for name, values in PALETTE.items()}
    report["shared_palette_sha256"] = hashlib.sha256(SHARED_PALETTE.read_bytes()).hexdigest()
    report["finish_revision"] = {"method": "saved-source material constants only; no geometry regeneration",
                                 "geometry_before_sha256": before, "geometry_after_sha256": after,
                                 "geometry_saved_reopen_sha256": reopened,
                                 "geometry_uv_assignments_transforms_unchanged": True}
    report["saved_blend_reopen"] = {"passed": True, "canonical_geometry_transforms_material_constants": True}
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    if "--skip-render" not in sys.argv:
        render_previews()
    print("VALSTRIKE_P01_FINISH_SUCCESS: saved source, FBX and reopen values agree; geometry unchanged")


def main():
    global MATERIALS
    if "--update-finish" in sys.argv:
        update_finish()
        return
    if "--render-only" in sys.argv:
        bpy.ops.wm.open_mainfile(filepath=str(BLEND))
        render_previews()
        return
    assert bpy.app.version[:3] == (5, 2, 2), bpy.app.version_string
    EXPORTS.mkdir(parents=True, exist_ok=True)
    PREVIEWS.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = .01
    scene.unit_settings.length_unit = "CENTIMETERS"
    MATERIALS = {name: material(name, *values) for name, values in PALETTE.items()}
    make_asset("SM_VS_Pistol01_Body", make_body(), "Grip attachment at local zero")
    make_asset("SM_VS_Pistol01_Slide", make_slide(), "Slide anchor; rest at body (0,0,3) cm")
    make_asset("SM_VS_Pistol01_Magazine", make_magazine(), "Insertion anchor; rest at body (0,0,0) cm")
    roundtrip()
    low = [min(r["bounds_min_cm"][a] + ASSEMBLY[r["name"]][a] for r in RECORDS) for a in range(3)]
    high = [max(r["bounds_max_cm"][a] + ASSEMBLY[r["name"]][a] for r in RECORDS) for a in range(3)]
    report = {
        "name": "P01 Shrike", "author": "Codex, original fictional ValStrike weapon art",
        "date_europe_berlin": "2026-11-03", "tool": "Blender " + bpy.app.version_string,
        "external_inputs": [], "units": "centimeters", "forward": "+X", "up": "+Z",
        "palette": {name: {"srgb_hex": values[0], "roughness": values[1], "metallic": values[2]}
                    for name, values in PALETTE.items()},
        "shared_palette_sha256": hashlib.sha256(SHARED_PALETTE.read_bytes()).hexdigest(),
        "assets": RECORDS, "sockets": SOCKETS,
        "assembly": {name: {"location_cm": pos, "rotation_degrees": [0,0,0], "scale": [1,1,1]}
                     for name, pos in ASSEMBLY.items()},
        "assembled_bounds_min_cm": low, "assembled_bounds_max_cm": high,
        "assembled_dimensions_cm": [high[a]-low[a] for a in range(3)],
        "total_triangles": sum(r["triangles"] for r in RECORDS),
        "fbx_export": {"axis_forward": "-Y", "axis_up": "Z", "global_scale": 1,
                       "scene_scale_length": .01, "apply_scale_options": "FBX_SCALE_UNITS",
                       "apply_unit_scale": True, "bake_space_transform": False},
        "limitations": ["Static visual meshes only; no functional internals, rig, hands, animation, or gameplay.",
                        "Closed chamfered component shells intentionally overlap inside the silhouette.",
                        "No collision supplied; visual meshes must use NoCollision in Unreal.",
                        "UV channels are isolated face tiles; no texture-density/light-bake acceptance.",
                        "Blender previews are actual models, not native Unreal or gameplay captures."],
    }
    (ROOT / "mesh_validation.json").write_text(json.dumps(report, indent=2) + "\n")
    presentation()
    # Save/reopen validates persisted canonical geometry and palette, not just RAM.
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    for record in RECORDS:
        obj = bpy.data.objects[record["name"]]
        assert all(abs(v) < 1e-7 for v in obj.location)
        assert all(abs(v) < 1e-7 for v in obj.rotation_euler)
        assert all(abs(v-1) < 1e-7 for v in obj.scale)
        assert len(obj.data.polygons) == record["triangles"]
        actual_low, actual_high, _ = bounds(obj)
        assert all(abs(actual_low[i]-record["bounds_min_cm"][i]) < 1e-6 for i in range(3))
        assert all(abs(actual_high[i]-record["bounds_max_cm"][i]) < 1e-6 for i in range(3))
    for name, values in PALETTE.items():
        bsdf = bpy.data.materials[name].node_tree.nodes["Principled BSDF"]
        assert max(abs(a-b) for a,b in zip(bsdf.inputs["Base Color"].default_value, rgb(values[0]))) < 1e-6
        assert abs(bsdf.inputs["Roughness"].default_value-values[1]) < 1e-6
        assert abs(bsdf.inputs["Metallic"].default_value-values[2]) < 1e-6
    report["saved_blend_reopen"] = {"passed": True, "canonical_geometry_transforms_material_constants": True}
    (ROOT / "mesh_validation.json").write_text(json.dumps(report, indent=2) + "\n")
    if "--skip-render" not in sys.argv:
        render_previews()
    print("VALSTRIKE_P01_MESH_SUCCESS: body, slide, magazine source/FBX/saved-Blender checks passed")


if __name__ == "__main__":
    main()
