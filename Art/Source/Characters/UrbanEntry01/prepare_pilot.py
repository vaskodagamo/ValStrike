"""ART-15 bounded derivatives. Preserves ART-13 source and all original exports.
Run Blender 5.2.2 --background --threads 2 --python-exit-code 1 --python this_file.
Only two derived actions are exported/reimported; no Unreal or service calls.
"""
import bpy, sys, json, math, hashlib
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT))
from prepare_source import set_action, curves, reset, export_fbx, mesh_points
REPO=ROOT.parents[3]; DOC=REPO/'docs/assets/urban-entry01'
OUT=ROOT/'Exports/Pilot'; PRE=DOC/'Previews/Pilot'
OUT.mkdir(exist_ok=True); PRE.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01.blend'))
sc=bpy.context.scene; arm=bpy.data.objects['SKEL_UE01']; mesh=bpy.data.objects['SK_VS_UrbanEntry01']
set_action(arm,bpy.data.actions['A_UE01_Walk_Backward_While_Shooting']);sc.frame_set(1)
armed_spine=arm.pose.bones['spine_01'].matrix.copy()
armed_pelvis=arm.pose.bones['pelvis'].head.copy()
armed_pose={b.name:b.matrix_basis.copy() for b in arm.pose.bones}
upper={'spine_01'} | {b.name for b in arm.data.bones['spine_01'].children_recursive}
# A walking stride is not a stable idle: use the supplied neutral lower body,
# with the armed upper body rebased at the retained pelvis. No bind edits.
reset(arm)
neutral_delta=arm.pose.bones['pelvis'].head-armed_pelvis
for name in upper: arm.pose.bones[name].matrix_basis=armed_pose[name]
bpy.context.view_layer.update()
arm.pose.bones['spine_01'].matrix=Matrix.Translation(neutral_delta) @ armed_spine
bpy.context.view_layer.update()
# Lower both wrists together 10 cm for a readable low-ready rifle (the supplied
# shooting pose puts R01 across the forehead). Analytic two-link pose adjustment
# preserves limb lengths and the existing bend side; no IK bones or rerig.
def move_wrist(side,delta):
    u=arm.pose.bones['upperarm_'+side];l=arm.pose.bones['lowerarm_'+side];h=arm.pose.bones['hand_'+side]
    a=u.head.copy();e=l.head.copy();w=h.head.copy();target=w+delta;hand=h.matrix.copy();hand.translation=target
    length1=(e-a).length;length2=(w-e).length;direction=(target-a).normalized();distance=(target-a).length
    assert abs(length1-length2)<distance<length1+length2
    projected=(e-a)-direction*(e-a).dot(direction);bend=projected.normalized()
    along=(length1**2-length2**2+distance**2)/(2*distance)
    new_elbow=a+direction*along+bend*math.sqrt(max(0,length1**2-along**2))
    rot=(e-a).rotation_difference(new_elbow-a).to_matrix().to_4x4()
    u.matrix=Matrix.Translation(a) @ rot @ Matrix.Translation(-a) @ u.matrix
    bpy.context.view_layer.update()
    e=l.head.copy();w=h.head.copy();rot=(w-e).rotation_difference(target-e).to_matrix().to_4x4()
    l.matrix=Matrix.Translation(e) @ rot @ Matrix.Translation(-e) @ l.matrix
    bpy.context.view_layer.update();h.matrix=hand;bpy.context.view_layer.update()
    assert (h.head-target).length<.001
for side in ('r','l'): move_wrist(side,Vector((0,0,-10)))
head=arm.pose.bones['head'];hp=head.head.copy()
head.matrix=Matrix.Translation(hp) @ Matrix.Rotation(math.radians(-15),4,'Y') @ Matrix.Translation(-hp) @ head.matrix
bpy.context.view_layer.update()
pose={b.name:b.matrix_basis.copy() for b in arm.pose.bones}
spine_world=arm.pose.bones['spine_01'].matrix.copy();pelvis=arm.pose.bones['pelvis'].head.copy()
# Bake this single edited pose as a constant one-second pilot hold.
hold=bpy.data.actions['A_UE01_Walk_Backward_While_Shooting'].copy();hold.name='A_UE01_Pilot_RifleHold';hold.use_fake_user=True
for f in curves(hold):
    bone=f.data_path.split('"')[1];loc,quat,scale=pose[bone].decompose()
    values=loc if f.data_path.endswith('.location') else quat if f.data_path.endswith('.rotation_quaternion') else scale
    value=values[f.array_index];f.keyframe_points.clear()
    for frame in (1,61): f.keyframe_points.insert(frame,value)
    f.update()
set_action(arm,hold);sc.frame_set(1)
rifle_world=Matrix.Translation(Vector((18,-18,133))+neutral_delta) @ Matrix.Rotation(math.radians(8),4,'Z')
socket=arm.pose.bones['hand_r'].matrix.inverted() @ rifle_world
# Existing Walking is already in place. Freeze the supplied armed torso on its
# original leg/pelvis gait; compensate initial pelvis orientation once at spine_01.
walk=bpy.data.actions['A_UE01_Walking'].copy();walk.name='A_UE01_Pilot_Walk_RifleHold';walk.use_fake_user=True
set_action(arm,walk);sc.frame_set(1)
upper={'spine_01'} | {b.name for b in arm.data.bones['spine_01'].children_recursive}
arm.pose.bones['spine_01'].matrix=Matrix.Translation(arm.pose.bones['pelvis'].head-pelvis) @ spine_world
bpy.context.view_layer.update()
walkpose=dict(pose);walkpose['spine_01']=arm.pose.bones['spine_01'].matrix_basis.copy()
for f in curves(walk):
    bone=f.data_path.split('"')[1]
    if bone not in upper: continue
    loc,quat,scale=walkpose[bone].decompose()
    values=loc if f.data_path.endswith('.location') else quat if f.data_path.endswith('.rotation_quaternion') else scale
    value=values[f.array_index]
    for k in f.keyframe_points: k.co.y=value;k.handle_left.y=value;k.handle_right.y=value
    f.update()
# Bring in the existing actual weapon objects solely for source fit evidence.
rifle_path=REPO/'Art/Source/Weapons/Rifle01/Models/R01_Kestrel.blend'
with bpy.data.libraries.load(str(rifle_path),link=False) as (src,dst):
    dst.objects=['SM_VS_Rifle01_Body','SM_VS_Rifle01_Magazine']
body,mag=dst.objects
for obj in (body,mag):
    sc.collection.objects.link(obj);obj.hide_set(False);obj.hide_render=False
    obj.name='PREVIEW_ONLY_'+obj.name
body['purpose']='Actual R01 read-only geometry; preview only; never in character FBX'
mag.parent=body;mag.matrix_parent_inverse=Matrix.Identity(4);mag.matrix_basis=Matrix.Translation((14,0,2))
# Explicit update avoids Blender bone-parent tail offset ambiguity.
def fit():
    bpy.context.view_layer.update();body.matrix_world=arm.matrix_world @ arm.pose.bones['hand_r'].matrix @ socket
    bpy.context.view_layer.update()
report={'task_id':'ART-15','blender':bpy.app.version_string,'original_source_unchanged':True,
 'socket':{'parent':'hand_r','space':'Blender pose-bone HEAD matrix, centimeters; column-vector matrices',
 'bone_local_matrix':[list(r) for r in socket], 'hold_rifle_component_matrix':[list(r) for r in rifle_world],
 'scale':[1,1,1]},'clips':{}}
samples={}
for action in (hold,walk):
    set_action(arm,action);sc.frame_start,sc.frame_end=map(round,action.frame_range);sc.render.fps=60
    frames=[sc.frame_start,(sc.frame_start+sc.frame_end)//2,sc.frame_end]
    samples[action.name]={}
    for frame in frames:
        sc.frame_set(frame)
        samples[action.name][frame]={'joints':{b.name:b.head.copy() for b in arm.pose.bones},'vertices':mesh_points(mesh)}
    export_fbx(OUT/(action.name+'.fbx'),arm,animated=True)
    a=samples[action.name][frames[0]]['joints']; b=samples[action.name][frames[-1]]['joints']
    report['clips'][action.name]={'frame_range':[sc.frame_start,sc.frame_end],'fps':60,
      'pelvis_end_delta_cm':list(b['pelvis']-a['pelvis']),
      'max_loop_joint_difference_cm':max((b[n]-a[n]).length for n in a)}
# Save separate editable pilot file with the unmodified original six actions.
set_action(arm,hold);sc.frame_start=1;sc.frame_end=61;sc.frame_set(1);fit()
# Use a constraint to retain the exact head-relative attachment in the saved file.
anchor=bpy.data.objects.new('PREVIEW_ONLY_R01_HandAnchor',None);sc.collection.objects.link(anchor)
con=anchor.constraints.new('COPY_TRANSFORMS');con.target=arm;con.subtarget='hand_r'
body.parent=anchor;body.matrix_parent_inverse=Matrix.Identity(4);body.matrix_basis=socket
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_Pilot.blend'))
# Changed-only actual FBX roundtrip onto the existing actual character export.
source_scene=sc;temp=bpy.data.scenes.new('PILOT_ROUNDTRIP');bpy.context.window.scene=temp
temp.unit_settings.system='METRIC';temp.unit_settings.scale_length=.01;temp.render.fps=60
bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/SK_VS_UrbanEntry01.fbx'),use_image_search=False)
rig=next(o for o in temp.objects if o.type=='ARMATURE');skin=next(o for o in temp.objects if o.type=='MESH')
for name,reference in samples.items():
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(OUT/(name+'.fbx')),anim_offset=0,use_image_search=False)
    added=set(bpy.data.objects)-before;assert not any(o.type=='MESH' for o in added)
    ar=next(o for o in added if o.type=='ARMATURE')
    assert set(ar.data.bones.keys())==set(rig.data.bones.keys())
    assert all((b.parent.name if b.parent else None)==(rig.data.bones[b.name].parent.name if rig.data.bones[b.name].parent else None) for b in ar.data.bones)
    rest_error=max(abs(x-y) for b in ar.data.bones for r,s in zip(b.matrix_local,rig.data.bones[b.name].matrix_local) for x,y in zip(r,s));assert rest_error<.001
    action=ar.animation_data.action;set_action(rig,action)
    assert list(map(round,action.frame_range))==report['clips'][name]['frame_range']
    assert temp.render.fps==60
    err=verr=0
    for f,ref in reference.items():
        temp.frame_set(f)
        err=max(err,max((rig.matrix_world@rig.pose.bones[n].head-p).length for n,p in ref['joints'].items()))
        verr=max(verr,max((v-w).length for v,w in zip(mesh_points(skin),ref['vertices'])))
    assert err<.01 and verr<.01,(name,err,verr)
    report['clips'][name].update({'animation_only':True,'rest_matrix_max_error':rest_error,'sampled_joint_error_cm':err,'sampled_vertex_error_cm':verr,'sampled_frames':list(reference)})
    for o in added:bpy.data.objects.remove(o,do_unlink=True)
bpy.context.window.scene=source_scene
for o in list(temp.objects):bpy.data.objects.remove(o,do_unlink=True)
bpy.data.scenes.remove(temp)
(DOC/'pilot-source-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('PILOT_EXPORTS_VERIFIED',flush=True)
# Concise source render only for new hold and changed gait; no old rerender.
sc.render.engine='BLENDER_WORKBENCH';sc.display.shading.light='STUDIO';sc.display.shading.color_type='TEXTURE'
sc.display.shading.show_shadows=True;sc.display.shading.show_cavity=True;sc.display.shading.show_specular_highlight=False
sc.render.resolution_x=640;sc.render.resolution_y=720;sc.render.resolution_percentage=100
camera=sc.camera;camera.data.type='ORTHO'
def render(name,pos,target,size):
    camera.location=pos;camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=size
    sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
set_action(arm,hold);sc.frame_set(1)
render('hold_front',(270,-430,180),(10,0,85),195)
render('hold_hands',(150,-230,170),(25,-16,141),78)
render('hold_support',(150,230,170),(25,-16,141),78)
set_action(arm,walk)
for f in (1,16,32,47,63):
    sc.frame_set(f);render('walk_%02d'%f,(270,-430,180),(10,0,85),195)
print('PILOT_PREVIEWS_COMPLETE',flush=True)
