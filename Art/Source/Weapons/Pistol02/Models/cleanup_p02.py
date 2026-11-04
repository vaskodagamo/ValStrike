"""ART-18 correction: preserve the original Meshy surface through part separation.
No decimation, surface smoothing, material overrides or paid API calls.
Exports are high-detail authoring/reference meshes, not a runtime LOD claim.
"""
import bpy, bmesh, json, math
from pathlib import Path
from mathutils import Vector
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[4]
DOC = REPO / 'docs/assets/pistol02'
SRC = ROOT.parent
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.unit_settings.system = 'METRIC'
sc.unit_settings.scale_length = .01
bpy.ops.import_scene.gltf(filepath=str(SRC / 'Originals/model.glb'))
o = next(o for o in sc.objects if o.type == 'MESH')
bpy.context.view_layer.objects.active = o
bpy.ops.object.select_all(action='DESELECT')
o.select_set(True)
scale = 21 / o.dimensions.x
# Apply the coordinate-frame change to geometry AND custom normals.
o.rotation_mode = 'XYZ'
o.rotation_euler.z = math.pi
o.scale = (scale,) * 3
o.location = (5, 0, .2)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
o.data.calc_loop_triangles()
original_triangles = len(o.data.loop_triangles)
assert original_triangles == 454988 and o.data.has_custom_normals
# BMesh does not directly preserve encoded custom split normals through cuts.
# Keep explicit per-corner vectors; bisect interpolates these only at new edges.
normal_attribute = o.data.attributes.new('P02_OriginalCornerNormal', 'FLOAT_VECTOR', 'CORNER')
normal_attribute.data.foreach_set('vector', [c for n in o.data.corner_normals for c in n.vector])
for mat in o.data.materials:
    mat.name = 'P02_Original_Meshy_PBR'
cap = bpy.data.materials.new('P02_CutInterior')
cap.diffuse_color = (.035, .045, .060, 1)
cap.use_nodes = True
bs = cap.node_tree.nodes.get('Principled BSDF')
bs.inputs['Base Color'].default_value = cap.diffuse_color
bs.inputs['Roughness'].default_value = .7
o.data.materials.append(cap)

def cutcopy(name, planes):
    n = o.copy()
    n.data = o.data.copy()
    sc.collection.objects.link(n)
    n.name = name
    bm = bmesh.new()
    bm.from_mesh(n.data)
    # Join exactly coincident UV-island borders for closed cut loops. UVs and
    # normals stay per-corner; no relaxation or average-normal pass is used.
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.000001)
    for point, normal, inside, outside in planes:
        result = bmesh.ops.bisect_plane(
            bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
            dist=.000001, plane_co=point, plane_no=normal,
            clear_inner=inside, clear_outer=outside)
        edges = [e for e in result['geom_cut']
                 if isinstance(e, bmesh.types.BMEdge) and e.is_boundary]
        if edges:
            filled = bmesh.ops.holes_fill(bm, edges=edges, sides=0)
            for face in filled.get('faces', []):
                face.material_index = 1
                face.smooth = False
    bm.normal_update()
    bm.to_mesh(n.data)
    bm.free()
    n.data.update()
    attr = n.data.attributes['P02_OriginalCornerNormal']
    normals = [None] * len(n.data.loops)
    for poly in n.data.polygons:
        for idx in poly.loop_indices:
            normals[idx] = (poly.normal if poly.material_index == 1
                            else attr.data[idx].vector.normalized())
    n.data.normals_split_custom_set(normals)
    n.data.attributes.remove(attr)
    return n

body = cutcopy('SM_VS_Pistol02_Body', [
    ((0, 0, 3.9), (0, 0, 1), False, True),
    ((0, 0, -6.05), (0, 0, 1), True, False)])
slide = cutcopy('SM_VS_Pistol02_Slide', [
    ((0, 0, 3.9), (0, 0, 1), True, False)])
mag = cutcopy('SM_VS_Pistol02_Magazine', [
    ((0, 0, -6.05), (0, 0, 1), False, True)])
bpy.data.objects.remove(o, do_unlink=True)
parts = [body, slide, mag]
# Concealed visual stem only; the visible magazine floor remains Meshy geometry.
bpy.ops.mesh.primitive_cube_add(size=1, location=(-2.5, 0, -3.5))
stem = bpy.context.object
stem.dimensions = (3.2, 2.1, 5)
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
stem.data.materials.append(cap)
bevel = stem.modifiers.new('Visual stem edges', 'BEVEL')
bevel.width = .15
bevel.segments = 2
bpy.ops.object.modifier_apply(modifier=bevel.name)
bpy.ops.object.select_all(action='DESELECT')
mag.select_set(True)
stem.select_set(True)
bpy.context.view_layer.objects.active = mag
bpy.ops.object.join()
for img in bpy.data.images:
    if img.source == 'FILE' and img.has_data:
        img.pack()
anchors = {'Grip': (0, 0, 0), 'Muzzle': (15.5, 0, 5),
           'Slide': (0, 0, 3.9), 'Magazine': (-2.5, 0, -1)}
for name, p in anchors.items():
    empty = bpy.data.objects.new(name, None)
    sc.collection.objects.link(empty)
    empty.location = p
    empty.empty_display_size = .65
(ROOT / 'Exports').mkdir(exist_ok=True)
report = {
    'revision': '2026-11-04 quality correction after user rejection',
    'tool': bpy.app.version_string, 'units': 'cm', 'forward': '+X', 'up': '+Z',
    'uniform_scale_from_meshy': scale, 'axis_flip_xy': -1,
    'source_meshy_triangles': original_triangles,
    'surface_processing': 'No decimation or smoothing; original corner normals and PBR preserved; cut-plane caps use flat interior material.',
    'runtime_optimization': 'Deferred; these are high-detail source/reference exports.',
    'anchors': {k: {'location_cm': v, 'rotation_xyz_degrees': [0, 0, 0],
                    'scale': [1, 1, 1]} for k, v in anchors.items()}, 'parts': []}
allverts = []
for part in parts:
    bpy.context.view_layer.update()
    part.data.calc_loop_triangles()
    verts = [part.matrix_world @ v.co for v in part.data.vertices]
    allverts += verts
    report['parts'].append({
        'name': part.name, 'triangles': len(part.data.loop_triangles),
        'vertices': len(part.data.vertices),
        'materials': [m.name for m in part.data.materials],
        'custom_normals': part.data.has_custom_normals,
        'dimensions_cm': [max(v[i] for v in verts) - min(v[i] for v in verts)
                          for i in range(3)],
        'transform_identity': all(abs(part.matrix_world[i][j] - (i == j)) < 1e-5
                                  for i in range(4) for j in range(4))})
    bpy.ops.object.select_all(action='DESELECT')
    part.select_set(True)
    bpy.context.view_layer.objects.active = part
    bpy.ops.export_scene.fbx(
        filepath=str(ROOT / 'Exports' / (part.name + '.fbx')),
        use_selection=True, object_types={'MESH'}, axis_forward='-Y', axis_up='Z',
        global_scale=1, apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
        bake_anim=False, path_mode='COPY', embed_textures=True)
report['bounds_cm'] = [[min(v[i] for v in allverts) for i in range(3)],
                       [max(v[i] for v in allverts) for i in range(3)]]
report['dimensions_cm'] = [report['bounds_cm'][1][i] - report['bounds_cm'][0][i]
                           for i in range(3)]
(DOC / 'source-measurements.json').write_text(json.dumps(report, indent=2) + '\n')
bpy.ops.object.select_all(action='DESELECT')
for part in parts:
    part.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'P02_Kestrel.blend'))
print('P02_CORRECTED_SOURCE', json.dumps(report), flush=True)
