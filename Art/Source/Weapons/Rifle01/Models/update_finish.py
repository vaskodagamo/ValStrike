"""Material-only ART-03 migration. Keeps canonical and editable geometry exact."""
import hashlib
import importlib.util
import json
import re
import sys
from pathlib import Path
import bpy

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('rifle_builder', ROOT / 'build_rifle01.py')
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)

def fingerprint(obj):
    mesh = obj.data
    data = {'vertices': [list(v.co) for v in mesh.vertices],
            'faces': [(list(p.vertices), p.material_index) for p in mesh.polygons],
            'uv': {uv.name: [list(item.uv) for item in uv.data] for uv in mesh.uv_layers},
            'matrix': [list(row) for row in obj.matrix_world],
            'slots': [m.name for m in mesh.materials]}
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()

def read_material(mat, expected_name, check_viewport=False):
    """Verify serialized shader inputs, rather than reporting intended constants."""
    assert mat is not None and mat.use_nodes, f'Missing node material: {expected_name}'
    # FBX import allocates fresh material datablocks and Blender adds a numeric
    # suffix when the canonical name already exists in the source scene.
    assert re.fullmatch(re.escape(expected_name) + r'(?:\.\d{3,})?', mat.name), mat.name
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    assert bsdf is not None, f'Missing Principled BSDF: {mat.name}'
    values = r.PALETTE[expected_name]
    expected_color = list(r.rgb(values[0]))
    inputs = {name: bsdf.inputs[name] for name in ('Base Color', 'Roughness', 'Metallic')}
    assert not any(socket.is_linked for socket in inputs.values()), f'Unexpected linked input: {mat.name}'
    color = list(inputs['Base Color'].default_value)
    roughness = float(inputs['Roughness'].default_value)
    metallic = float(inputs['Metallic'].default_value)
    assert all(abs(a - b) < 1e-6 for a, b in zip(color, expected_color)), f'BaseColor differs: {mat.name}'
    assert abs(roughness - values[1]) < 1e-6, f'Roughness differs: {mat.name}'
    assert abs(metallic - values[2]) < 1e-6, f'Metallic differs: {mat.name}'
    result = {'material_name': mat.name, 'canonical_material_name': expected_name,
              'expected_srgb_hex': values[0], 'linear_base_color_rgba': color,
              'roughness': roughness, 'metallic': metallic, 'unlinked_principled_inputs': True}
    if check_viewport:
        viewport = list(mat.diffuse_color)
        assert all(abs(a - b) < 1e-6 for a, b in zip(viewport, expected_color)), f'Viewport color differs: {mat.name}'
        result['linear_viewport_color_rgba'] = viewport
    return result

def roundtrip_with_materials():
    """Read real FBX files and retain geometry plus imported material evidence."""
    for record in r.RECORDS:
        before_objects = set(bpy.data.objects)
        before_meshes = set(bpy.data.meshes)
        before_materials = set(bpy.data.materials)
        bpy.ops.import_scene.fbx(filepath=str(ROOT / record['file']), use_anim=False)
        added = [obj for obj in bpy.data.objects if obj not in before_objects]
        meshes = [obj for obj in added if obj.type == 'MESH']
        assert len(meshes) == 1, record['name']
        obj = meshes[0]
        low, high, dimensions = r.bounds(obj)
        assert all(abs(low[a] - record['bounds_min_cm'][a]) < .002 for a in range(3)), low
        assert all(abs(high[a] - record['bounds_max_cm'][a]) < .002 for a in range(3)), high
        assert len(obj.data.polygons) == record['triangles'], record['name']
        assert len(obj.data.uv_layers) == 2, record['name']
        assert len(obj.data.materials) == len(record['material_slots']), record['name']
        materials = [read_material(mat, expected_name)
                     for mat, expected_name in zip(obj.data.materials, record['material_slots'])]
        record['fbx_roundtrip'] = {'passed': True, 'bounds_min_cm': low,
                                   'bounds_max_cm': high, 'dimensions_cm': dimensions,
                                   'triangles': len(obj.data.polygons), 'uv_channels': 2,
                                   'collision_meshes': 0, 'materials': materials,
                                   'material_names_and_shader_values_passed': True}
        for obj in added:
            bpy.data.objects.remove(obj, do_unlink=True)
        for mesh in set(bpy.data.meshes) - before_meshes:
            assert mesh.users == 0, 'Unexpected surviving roundtrip mesh user'
            bpy.data.meshes.remove(mesh)
        for mat in set(bpy.data.materials) - before_materials:
            assert mat.users == 0, 'Unexpected surviving roundtrip material user'
            bpy.data.materials.remove(mat)

def main():
    bpy.ops.wm.open_mainfile(filepath=str(r.BLEND))
    bpy.context.preferences.filepaths.save_version = 0
    objects = {o.name: o for o in bpy.data.objects if o.type == 'MESH' and not o.name.startswith('PREVIEW_')}
    before = {n: fingerprint(o) for n, o in objects.items()}
    for name, values in r.PALETTE.items():
        mat = bpy.data.materials[name]
        mat.diffuse_color = r.rgb(values[0])
        bsdf = mat.node_tree.nodes.get('Principled BSDF')
        bsdf.inputs['Base Color'].default_value = r.rgb(values[0])
        bsdf.inputs['Roughness'].default_value = values[1]
        bsdf.inputs['Metallic'].default_value = values[2]
    r.RECORDS = json.loads((ROOT / 'mesh_validation.json').read_text())['assets']
    for record in r.RECORDS:
        obj = bpy.data.objects[record['name']]
        obj.hide_set(False)
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.export_scene.fbx(filepath=str(ROOT / record['file']), use_selection=True,
            object_types={'MESH'}, global_scale=1, apply_unit_scale=True,
            apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
            use_space_transform=True, bake_space_transform=False, mesh_smooth_type='FACE',
            use_mesh_modifiers=True, use_tspace=False, bake_anim=False, add_leaf_bones=False, path_mode='AUTO')
        obj.hide_set(True)
    roundtrip_with_materials()
    assert before == {n: fingerprint(o) for n,o in objects.items()}, 'Material migration changed geometry'
    scene = bpy.context.scene
    scene.camera = bpy.data.objects['PREVIEW_HeroCamera']
    bpy.data.objects['PREVIEW_Ground'].hide_render = False
    scene.cycles.samples = 32
    bpy.ops.wm.save_as_mainfile(filepath=str(r.BLEND))
    bpy.ops.wm.open_mainfile(filepath=str(r.BLEND))
    after = {n: fingerprint(bpy.data.objects[n]) for n in before}
    assert before == after, 'Saved/reopened source differs'
    reopened_materials = {name: read_material(bpy.data.materials.get(name), name, check_viewport=True)
                          for name in r.PALETTE}
    report = json.loads((ROOT / 'mesh_validation.json').read_text())
    report['palette'] = {n: {'srgb_hex':v[0], 'roughness':v[1], 'metallic':v[2]} for n,v in r.PALETTE.items()}
    report['assets'] = r.RECORDS
    report['finish_revision'] = 'ART-03 crimson; geometry preserved'
    (ROOT/'mesh_validation.json').write_text(json.dumps(report,indent=2)+'\n')
    result = {'date_europe_berlin':'2026-11-03','tool':bpy.app.version_string,
              'geometry_hashes_before':before,'geometry_hashes_after_reopen':after,
              'all_geometry_unchanged':before == after, 'materials':reopened_materials,
              'saved_reopened_material_values_passed':True,
              'palette_sha256':hashlib.sha256(r.SHARED_PALETTE.read_bytes()).hexdigest(),
              'fbx_sha256':{v['name']:hashlib.sha256((ROOT/v['file']).read_bytes()).hexdigest() for v in r.RECORDS}}
    (ROOT/'finish_validation.json').write_text(json.dumps(result,indent=2)+'\n')
    if '--skip-render' not in sys.argv:
        r.render_previews()
    print('VALSTRIKE_RIFLE_FINISH_SOURCE_SUCCESS')

if __name__ == '__main__': main()
