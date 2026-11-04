"""Read-only ART-13 intake inspection; imports temporary copies, never originals."""
import bpy, json, hashlib, shutil, tempfile
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
DOC=REPO/'docs/assets/urban-entry01'
INBOX=Path('/Users/webproduktion3/Documents/ValStrike/meshy_ai_assets')
MANIFEST=json.loads((REPO/'docs/assets/meshy-character-intake.json').read_text())

def matrix(m): return [[round(v,8) for v in row] for row in m]
def inspect(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc=bpy.context.scene
    sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=.01
    sc.render.fps=60
    bpy.ops.import_scene.fbx(filepath=str(path),use_image_search=False,use_anim=True)
    arm=next(o for o in sc.objects if o.type=='ARMATURE')
    meshes=[o for o in sc.objects if o.type=='MESH']
    result={'armature':arm.name,'object_matrix':matrix(arm.matrix_world),
      'bones':{b.name:{'parent':b.parent.name if b.parent else None,'rest':matrix(b.matrix_local),'head':list(b.head_local),'tail':list(b.tail_local)} for b in arm.data.bones},'meshes':[]}
    for o in meshes:
        o.data.calc_loop_triangles()
        coords=[o.matrix_world@v.co for v in o.data.vertices]
        sums=[sum(g.weight for g in v.groups) for v in o.data.vertices]
        result['meshes'].append({'name':o.name,'vertices':len(coords),'triangles':len(o.data.loop_triangles),
          'bounds':[[min(v[i] for v in coords) for i in range(3)],[max(v[i] for v in coords) for i in range(3)]],
          'matrix':matrix(o.matrix_world),'uvs':[x.name for x in o.data.uv_layers],
          'unweighted':sum(x==0 for x in sums),'nonunit_weights':sum(abs(x-1)>1e-4 for x in sums),
          'weight_sum_range':[min(sums),max(sums)],'max_influences':max(len(v.groups) for v in o.data.vertices),
          'materials':[m.name for m in o.data.materials],
          'geometry_sha256':hashlib.sha256(str([(tuple(v.co),[(g.group,round(g.weight,7)) for g in v.groups]) for v in o.data.vertices]).encode()).hexdigest()})
    result['materials']={m.name:[{'type':n.type,'image':n.image.filepath if n.type=='TEX_IMAGE' and n.image else None} for n in m.node_tree.nodes] for m in bpy.data.materials if m.use_nodes}
    result['images']=[{'name':i.name,'size':list(i.size),'filepath':i.filepath,'colorspace':i.colorspace_settings.name,'packed':bool(i.packed_file)} for i in bpy.data.images]
    result['actions']=[{'name':a.name,'range':list(a.frame_range),'slots':[s.identifier for s in a.slots]} for a in bpy.data.actions]
    print('INTAKE',path.name, json.dumps({k:v for k,v in result.items() if k not in ['bones','materials','images']}),flush=True)
    return result

report={'inputs':{},'blender':bpy.app.version_string}
rows=MANIFEST['extracted_files']
ordered=sorted([r for r in rows if r['file'].endswith('.fbx')],key=lambda r:(0 if 'Character_output' in r['file'] else 1 if '_Walking_' in r['file'] else 2 if '_Running_' in r['file'] else 3,r['file']))
with tempfile.TemporaryDirectory(prefix='_intake_',dir=ROOT) as temp:
    for row in rows:
        p=INBOX/row['file'];data=p.read_bytes()
        assert hashlib.sha256(data).hexdigest()==row['sha256'] and len(data)==row['bytes']
        shutil.copyfile(p,Path(temp)/p.name)
    for row in ordered:
        p=Path(temp)/Path(row['file']).name
        report['inputs'][p.name]=inspect(p)
base=report['inputs']['Meshy_AI_Character_output.fbx']
base_bones=base['bones']
report['common_bones']=base_bones
for record in report['inputs'].values():
    bones=record.pop('bones')
    record['bone_count']=len(bones)
    record['bone_names_hierarchy_rest_equal_to_base']=bones==base_bones
    for image in record['images']:image['filepath']=Path(image['filepath']).name
    for material in record['materials'].values():
        for node in material:
            if node['image']:node['image']=Path(node['image']).name
(DOC/'intake_inspection.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART13_INTAKE_INSPECTED',flush=True)
