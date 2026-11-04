"""Changed-asset inspection: reopen source, export, read back FBX scale/rig/poses.
No Unreal invocation or general test suite. Run after fit_weapons.py.
"""
import bpy,bmesh,json,math,hashlib,sys
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[3]/'docs/assets/fparms02'
CHECK_ONLY='--inspect-only' in sys.argv
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'FPArms02.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms02'];meshes=[bpy.data.objects['SK_FPArms02_'+s] for s in ['R','L']]
def reset():
    if arm.animation_data:arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()
def set_action(a,frame):
    reset()
    arm.animation_data_create();arm.animation_data.action=a;arm.animation_data.action_slot=a.slots[0];scene.frame_set(frame);bpy.context.view_layer.update()
def bounds(o,evaluated=False):
    obj=o.evaluated_get(bpy.context.evaluated_depsgraph_get()) if evaluated else o
    pts=[obj.matrix_world@v.co for v in obj.data.vertices]
    return [[min(p[i] for p in pts),max(p[i] for p in pts)] for i in range(3)]
def select(objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0]
def export(path,action=None):
    reset();temporary=None
    if action:
        temporary=action.copy()
        for layer in temporary.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    for c in bag.fcurves:c.keyframe_points.insert(0,1 if c.data_path.endswith('.scale') else 0,options={'FAST'});c.update()
        set_action(temporary,0)
    select([arm]+([] if action else meshes))
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'ARMATURE'} if action else {'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=bool(action),bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',colors_type='LINEAR',path_mode='RELATIVE')
    reset()
    if temporary:bpy.data.actions.remove(temporary)
reset();scene.frame_start=1;scene.frame_end=30
rest={b.name:{'head':list(b.head_local),'parent':b.parent.name if b.parent else None} for b in arm.data.bones}
report={'blender':bpy.app.version_string,'units':'cm, +X forward, Z up; FBX converter -Y/Z matching project','fps':30,'source_reopened':True,'source_meshes':{},'poses':{},'fbx_roundtrip':{},'limitations':['Source inspections do not prove native material, skeleton, sockets, clipping or runtime correctness.']}
source_bounds={}
for mesh in meshes:
    bm=bmesh.new();bm.from_mesh(mesh.data)
    source_bounds[mesh.name]=bounds(mesh)
    report['source_meshes'][mesh.name]={'vertices':len(mesh.data.vertices),'triangles':len(mesh.data.polygons),'bounds_cm':source_bounds[mesh.name],'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'uv_layers':len(mesh.data.uv_layers),'vertex_color':list(mesh.data.color_attributes.keys()),'weight_sum_max_error':max(abs(sum(g.weight for g in v.groups)-1) for v in mesh.data.vertices),'unweighted':sum(not v.groups for v in mesh.data.vertices)};bm.free()
    assert max(b-a for a,b in source_bounds[mesh.name])<45,'Centimeter conversion failed'
    assert report['source_meshes'][mesh.name]['unweighted']==0
actions=[a for a in bpy.data.actions if a.name.startswith('A_FPArms02_')]
# Source Action Editor switching must not inherit a previous hold translation.
for a in actions:
    if not CHECK_ONLY and a.name in ['A_FPArms02_Open','A_FPArms02_Grip','A_FPArms02_Wrist','A_FPArms02_KnifePlaceholder']:
        set_action(a,1)
        for b in arm.pose.bones:
            b.location=(0,0,0);b.scale=(1,1,1)
            for f in [1,30]:
                b.keyframe_insert(data_path='location',frame=f,group=b.name);b.keyframe_insert(data_path='scale',frame=f,group=b.name)
samples={}
for a in actions:
    samples[a.name]={}
    for f in [1,15,30]:
        set_action(a,f);samples[a.name][f]={b.name:list(arm.matrix_world@b.head) for b in arm.pose.bones}
    set_action(a,15);dg=bpy.context.evaluated_depsgraph_get();o=meshes[0];evaluated=o.evaluated_get(dg).data
    stretches=[]
    for e in o.data.edges:
        i,j=e.vertices;l=(o.data.vertices[i].co-o.data.vertices[j].co).length
        if l>.015:stretches.append((evaluated.vertices[i].co-evaluated.vertices[j].co).length/l)
    stretches.sort()
    report['poses'][a.name]={'frames':[1,15,30],'right_bounds_mid_cm':bounds(o,True),'edge_stretch_p99':stretches[int(len(stretches)*.99)],'edge_stretch_max':max(stretches)}
if not CHECK_ONLY:
    export(ROOT/'Exports/SK_FPArms02.fbx')
    for a in actions:export(ROOT/'Exports'/(a.name+'.fbx'),a)
    reset();scene.frame_set(1);select([arm]+meshes);bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'FPArms02.blend'))
# Reimport each actual export in a clean scene, at centimeter scene units.
for path in sorted((ROOT/'Exports').glob('*.fbx')):
    bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
    bpy.ops.import_scene.fbx(filepath=str(path),anim_offset=0)
    arm=next(o for o in scene.objects if o.type=='ARMATURE')
    observed=set(arm.data.bones.keys());assert observed==set(rest),(path.name,'bone names')
    parent_errors=[b.name for b in arm.data.bones if (b.parent.name if b.parent else None)!=rest[b.name]['parent']]
    max_rest=max(((arm.matrix_world@b.head_local)-Vector(rest[b.name]['head'])).length for b in arm.data.bones)
    entry={'bones':len(observed),'parents_match':not parent_errors,'max_rest_head_error_cm':max_rest}
    assert not parent_errors
    assert max_rest<.01,(path.name,'rest mismatch',max_rest)
    if path.stem.startswith('SK_'):
        entry['meshes']={}
        for o in [o for o in scene.objects if o.type=='MESH']:
            error=max(abs(v-source_bounds[o.name][i][j]) for i,axis in enumerate(bounds(o)) for j,v in enumerate(axis))
            entry['meshes'][o.name]={'bounds_max_error_cm':error,'uv_layers':len(o.data.uv_layers),'vertex_color':list(o.data.color_attributes.keys()),'unweighted':sum(not v.groups for v in o.data.vertices)}
            assert error<.01 and entry['meshes'][o.name]['unweighted']==0
    else:
        max_pose=0
        for f in [1,15,30]:
            scene.frame_set(f);bpy.context.view_layer.update()
            max_pose=max(max_pose,max(((arm.matrix_world@b.head)-Vector(samples[path.stem][f][b.name])).length for b in arm.pose.bones))
        entry['sampled_pose_head_max_error_cm']=max_pose
        assert max_pose<.03,(path.name,'pose mismatch',max_pose)
    report['fbx_roundtrip'][path.name]=entry
report['passed']=True
(DOC/'export-inspection.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART19_SOURCE_AND_FBX_INSPECTION_PASSED')
