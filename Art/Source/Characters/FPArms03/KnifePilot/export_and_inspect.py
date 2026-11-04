"""ART-26 adaptation of the focused ART-22/24 source/FBX inspection."""
import bpy,json,hashlib,array,struct,math,sys
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[4]/'docs/assets/fparms03/knife-pilot'
C1='--c1' in sys.argv
contract=json.loads((DOC/'animation-contract.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'KnifePilot.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms03'];meshes=[bpy.data.objects['SK_FPArms03_'+s] for s in ['R','L']]
def signature(o,weights=True):
    h=hashlib.sha256()
    for collection,attribute,width,code in [(o.data.vertices,'co',3,'f'),(o.data.loops,'vertex_index',1,'i'),(o.data.uv_layers[0].data,'uv',2,'f'),(o.data.corner_normals,'vector',3,'f')]:
        values=array.array(code,[0])*(len(collection)*width);collection.foreach_get(attribute,values);h.update(values.tobytes())
    if weights:
        for v in o.data.vertices:
            for g in v.groups:h.update(struct.pack('if',g.group,g.weight))
    return h.hexdigest()
def rest(a):return [{'name':b.name,'parent':b.parent.name if b.parent else None,'matrix':[list(row) for row in b.matrix_local]} for b in a.data.bones]
def bounds(o):
    p=[o.matrix_world@v.co for v in o.data.vertices];return [[min(v[i] for v in p),max(v[i] for v in p)] for i in range(3)]
def reset():
    arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()
def active(action,frame):
    reset();arm.animation_data.action=action;arm.animation_data.action_slot=action.slots[0];scene.frame_set(frame);bpy.context.view_layer.update()
def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
reset();original_rest=rest(arm);original_bounds={o.name:bounds(o) for o in meshes}
names=['SKEL_FPArms03','SK_FPArms03_R','SK_FPArms03_L']
with bpy.data.libraries.load(str(ROOT/'Reference/Frozen/ART24_FPArms03.blend'),link=False) as (src,dst):dst.objects=names[:]
old=dict(zip(names,dst.objects));preserved={o.name:signature(o,not C1)==signature(old[o.name],not C1) for o in meshes};rest_same=rest(arm)==rest(old['SKEL_FPArms03'])
weight_report={}
if C1:
    changes=json.loads((DOC/'c1-weight-changes.json').read_text());expected={v['vertex']:v for v in changes['changes']}
    for o in meshes:
        found=[]
        for v,original in zip(o.data.vertices,old[o.name].data.vertices):
            now={o.vertex_groups[g.group].name:g.weight for g in v.groups};before={old[o.name].vertex_groups[g.group].name:g.weight for g in original.groups}
            if now==before:continue
            assert o.name=='SK_FPArms03_R' and v.index in expected
            e=expected[v.index]
            assert set(now).issubset({'hand_r','thumb_01_r','thumb_02_r','thumb_03_r','index_01_r','index_02_r','index_03_r'})
            assert max(abs(now.get(k,0)-e['after'].get(k,0)) for k in set(now)|set(e['after']))<.000001
            assert before==e['before']
            found.append(v.index)
        weight_report[o.name]={'changed_vertices':len(found),'allowed_groups':['hand_r','thumb_01_r','thumb_02_r','thumb_03_r','index_01_r','index_02_r','index_03_r'] if found else [],'evidence':'c1-weight-changes.json' if found else None}
        if o.name=='SK_FPArms03_R':assert set(found)==set(expected)
assert all(preserved.values()) and rest_same
for o in dst.objects:bpy.data.objects.remove(o,do_unlink=True)
report={'blender':bpy.app.version_string,'fps':scene.render.fps,'preserved_surface_uv_normals_weights':preserved,'rest_names_hierarchy_unchanged':rest_same,'clips':{},'exports':{},'source_meshes':{o.name:{'vertices':len(o.data.vertices),'triangles':len(o.data.polygons),'unweighted':sum(not v.groups for v in o.data.vertices)} for o in meshes}}
if C1:
    report['preserved_surface_uv_normals']=report.pop('preserved_surface_uv_normals_weights');report['local_weight_changes']=weight_report
samples={};pose_samples={};actions={}
for kind,clip in contract['clips'].items():
    a=bpy.data.actions[clip['action']];actions[kind]=a;end=clip['end'];frames=sorted(set([1,end,(end+1)//2,clip.get('impact_frame',1)]+({'Slash':[5,17],'Stab':[10,26]}.get(kind,[]) if C1 else [])))
    samples[kind]={};pose_samples[kind]={};root_error=0
    for frame in (frames if C1 else range(1,end+1)):
        active(a,frame);root_error=max(root_error,max(abs(arm.pose.bones['root'].matrix_basis[i][j]-(1 if i==j else 0)) for i in range(4) for j in range(4)))
        if frame in frames:
            samples[kind][frame]={b.name:[list(row) for row in arm.matrix_world@b.matrix] for b in arm.pose.bones}
    assert root_error<.00001
    assert abs((end-1)/60-clip['duration'])<.000001
    if 'impact_frame' in clip:
        assert a.pose_markers['Impact'].frame==clip['impact_frame'] and abs((clip['impact_frame']-1)/60-clip['impact_seconds'])<.000001
    report['clips'][kind]={'frames':frames,'frame_range':[1,end],'duration_seconds':(end-1)/60,'root_basis_max_error':root_error,'impact_frame':clip.get('impact_frame'),'impact_seconds':clip.get('impact_seconds')}
for kind,clip in contract['clips'].items():
    target=samples[kind][1] if kind in ['Idle','Locomotion'] else samples['Idle'][1]
    end=samples[kind][clip['end']]
    error=max(abs(end[name][i][j]-target[name][i][j]) for name in end for i in range(4) for j in range(4))
    assert error<.001,(kind,'endpoint',error)
    report['clips'][kind]['loop_or_idle_endpoint_matrix_error']=error
reset();select([arm]+meshes)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'Exports/SK_FPArms03_KnifePilot.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',path_mode='RELATIVE')
for kind,a in actions.items():
    scene.frame_start=1;scene.frame_end=contract['clips'][kind]['end'];temporary=a.copy()
    for layer in temporary.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for c in bag.fcurves:
                    value=1 if c.data_path.endswith('.scale') or (c.data_path.endswith('.rotation_quaternion') and c.array_index==0) else 0
                    c.keyframe_points.insert(0,value,options={'FAST'});c.update()
    active(temporary,0);select([arm])
    bpy.ops.export_scene.fbx(filepath=str(ROOT/'Exports'/(a.name+'.fbx')),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS')
    reset();bpy.data.actions.remove(temporary)
rest_heads={b.name:Vector(b.head_local) for b in arm.data.bones}
for path in sorted((ROOT/'Exports').glob('*.fbx')):
    bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01;scene.render.fps=60
    bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
    arm=next(o for o in scene.objects if o.type=='ARMATURE')
    assert set(arm.data.bones.keys())==set(rest_heads)
    parent_names={b.name:b.parent.name if b.parent else None for b in arm.data.bones}
    assert all(parent_names[b['name']]==b['parent'] for b in original_rest)
    rest_error=max(((arm.matrix_world@b.head_local)-rest_heads[b.name]).length for b in arm.data.bones);assert rest_error<.01
    entry={'bones':len(arm.data.bones),'rest_head_max_error_cm':rest_error,'fps':scene.render.fps}
    if path.stem.startswith('SK_'):
        entry['meshes']={}
        for o in [o for o in scene.objects if o.type=='MESH']:
            assert o.name in original_bounds,'Unexpected reference mesh exported'
            error=max(abs(v-original_bounds[o.name][i][j]) for i,pair in enumerate(bounds(o)) for j,v in enumerate(pair));assert error<.01
            entry['meshes'][o.name]={'bounds_max_error_cm':error,'unweighted':sum(not v.groups for v in o.data.vertices)}
    else:
        kind=path.stem.removeprefix('A_FPArms03_Knife');error=0
        for frame,bones in samples[kind].items():
            scene.frame_set(frame);bpy.context.view_layer.update()
            for b in arm.pose.bones:
                matrix=arm.matrix_world@b.matrix;error=max(error,max(abs(matrix[i][j]-bones[b.name][i][j]) for i in range(4) for j in range(4)))
        assert error<.03,(path.name,error)
        entry['sampled_bone_matrix_max_error']=error
    report['exports'][path.name]=entry
report['passed']=True
if C1:report['reused_evidence']={'commit':'2b944b314f9c20d8b64ee9892b1048302701992c','report':'export-inspection.json','checks':['Unchanged action ranges/impact times and all-frame identity root'],'current_scope':'Changed source preservation and sampled actual animation FBX poses, including windup/follow-through; root is sampled here.'}
(DOC/('c1-export-inspection.json' if C1 else 'export-inspection.json')).write_text(json.dumps(report,indent=2)+'\n')
print('ART26_SOURCE_AND_EXPORT_INSPECTION_PASSED')
