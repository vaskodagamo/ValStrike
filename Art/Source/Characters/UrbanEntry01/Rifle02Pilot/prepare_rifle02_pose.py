"""ART-28: shoulder-first R02 fit on the preserved UrbanEntry rig; Blender 5.2.2."""
import bpy,sys,json,math
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parent;CHAR=ROOT.parent;REPO=CHAR.parents[3];DOC=REPO/'docs/assets/urban-entry01/rifle02-pilot';PRE=DOC/'Previews'
sys.dont_write_bytecode=True;sys.path.insert(0,str(CHAR))
from prepare_source import set_action,reset,curves,export_fbx
bpy.ops.wm.open_mainfile(filepath=str(CHAR/'UrbanEntry01_Pilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01']
for o in list(sc.objects):
    if o.name.startswith('PREVIEW_ONLY_'):bpy.data.objects.remove(o,do_unlink=True)
set_action(arm,bpy.data.actions['A_UE01_Pilot_RifleHold']);sc.frame_set(1)
ref_hands={s:arm.pose.bones['hand_'+s].matrix.copy() for s in ('r','l')}
fingers={b.name:b.matrix_basis.copy() for b in arm.pose.bones if any(w in b.name for w in ('thumb','index','middle','ring','pinky'))}
reset(arm)
def solve_wrist(side,target,hand_rotation,pole):
    u=arm.pose.bones['upperarm_'+side];l=arm.pose.bones['lowerarm_'+side];h=arm.pose.bones['hand_'+side]
    a=u.head.copy();e=l.head.copy();w=h.head.copy();target=Vector(target)
    la=(e-a).length;lb=(w-e).length;direction=(target-a).normalized();distance=(target-a).length
    assert abs(la-lb)<distance<la+lb,(side,la,lb,distance)
    bend=Vector(pole)-a;bend=(bend-direction*bend.dot(direction)).normalized()
    along=(la*la-lb*lb+distance*distance)/(2*distance)
    elbow=a+direction*along+bend*math.sqrt(la*la-along*along)
    q=(e-a).rotation_difference(elbow-a).to_matrix().to_4x4()
    u.matrix=Matrix.Translation(a)@q@Matrix.Translation(-a)@u.matrix;bpy.context.view_layer.update()
    e=l.head.copy();w=h.head.copy();q=(w-e).rotation_difference(target-e).to_matrix().to_4x4()
    l.matrix=Matrix.Translation(e)@q@Matrix.Translation(-e)@l.matrix;bpy.context.view_layer.update()
    h.matrix=Matrix.Translation(target)@hand_rotation.to_4x4();bpy.context.view_layer.update()
    assert (h.head-target).length<.001

def rotate(name,axis,degrees):
    b=arm.pose.bones[name];p=b.head.copy();b.matrix=Matrix.Translation(p)@Matrix.Rotation(math.radians(degrees),4,axis)@Matrix.Translation(-p)@b.matrix;bpy.context.view_layer.update()
# Torso quarter-turn brings the support shoulder forward without changing reach.
rotate('spine_01','Z',-35);rotate('neck_01','Z',35)
world=Matrix.Translation((20,-15,129))
solve_wrist('r',(6.473,-17.63,126.24),ref_hands['r'].to_3x3(),(-8,-33,114))
solve_wrist('l',(41.35,-5,128.95),ref_hands['l'].to_3x3(),(18,20,113))
for n,m in fingers.items():arm.pose.bones[n].matrix_basis=m
bpy.context.view_layer.update()
# Fit rotations only: indexed trigger finger, open finger pads around the grip.
rotate('index_01_r','Z',-25)
rotate('thumb_01_r','Z',50)
rotate('thumb_01_r','Y',10)
rotate('thumb_02_r','Y',-3)
rotate('index_03_r','Z',-25)
# Retain the original distal finger curls; place the palm farther around the grip.
for digit in ('index','middle','ring','pinky'):rotate(digit+'_02_l','X',30)
for digit,angle in [('middle',-59),('ring',-33),('pinky',-35)]:rotate(digit+'_02_r','Z',angle)
rotate('index_03_l','X',25)
rotate('thumb_03_l','Z',10)
pose={b.name:b.matrix_basis.copy() for b in arm.pose.bones};socket=arm.pose.bones['hand_r'].matrix.inverted()@world
hold=bpy.data.actions['A_UE01_Pilot_RifleHold'].copy();hold.name='A_UE01_R02_Hold';hold.use_fake_user=True
for f in curves(hold):
    n=f.data_path.split('"')[1];v=pose[n].decompose()[0 if f.data_path.endswith('.location') else 1 if f.data_path.endswith('.rotation_quaternion') else 2][f.array_index]
    f.keyframe_points.clear()
    for fr in (1,61):f.keyframe_points.insert(fr,v)
    f.update()
set_action(arm,hold);sc.frame_set(1);spine=arm.pose.bones['spine_01'].matrix.copy();pelvis=arm.pose.bones['pelvis'].head.copy()
upper={'spine_01'}|{b.name for b in arm.data.bones['spine_01'].children_recursive};clips={'A_UE01_R02_Hold':{'frame_range':[1,61],'fps':60,'loop':True,'root_motion':False}}
for original,label in [('Walking','Walk'),('Running','Run')]:
    a=bpy.data.actions['A_UE01_'+original].copy();a.name='A_UE01_R02_'+label;a.use_fake_user=True;set_action(arm,a);sc.frame_set(1)
    arm.pose.bones['spine_01'].matrix=Matrix.Translation(arm.pose.bones['pelvis'].head-pelvis)@spine;bpy.context.view_layer.update()
    wp=dict(pose);wp['spine_01']=arm.pose.bones['spine_01'].matrix_basis.copy()
    for f in curves(a):
        n=f.data_path.split('"')[1]
        if n not in upper:continue
        v=wp[n].decompose()[0 if f.data_path.endswith('.location') else 1 if f.data_path.endswith('.rotation_quaternion') else 2][f.array_index]
        for k in f.keyframe_points:k.co.y=v;k.handle_left.y=v;k.handle_right.y=v
        f.update()
    clips[a.name]={'frame_range':list(map(round,a.frame_range)),'fps':60,'loop':True,'root_motion':False,'lower_body_source':'A_UE01_'+original}
with bpy.data.libraries.load(str(REPO/'Art/Source/Weapons/Rifle02/R02_Compact.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Rifle02_Body','SM_VS_Rifle02_Magazine']
body,mag=dst.objects
anchor=bpy.data.objects.new('PREVIEW_ONLY_R02_hand_r',None);sc.collection.objects.link(anchor);con=anchor.constraints.new('COPY_TRANSFORMS');con.target=arm;con.subtarget='hand_r'
for o,offset in [(body,Matrix.Identity(4)),(mag,Matrix.Translation((15,0,-2.5)))]:
    sc.collection.objects.link(o);o.name='PREVIEW_ONLY_'+o.name;o.hide_set(False);o.hide_render=False;o.parent=anchor;o.matrix_parent_inverse=Matrix.Identity(4);o.matrix_basis=socket@offset
for im in bpy.data.images:
    if im.source=='FILE' and not im.packed_file:im.filepath=bpy.path.abspath(im.filepath)
set_action(arm,hold);sc.frame_set(1);sc.frame_start=1;sc.frame_end=61;sc.render.fps=60
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_Rifle02Pilot.blend'))
for im in bpy.data.images:
    if im.source=='FILE' and not im.packed_file:im.filepath=bpy.path.relpath(im.filepath,start=str(ROOT))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_Rifle02Pilot.blend'))
# INT-ART-04 measured Y-reflected component, mesh and local bone bases.
F=Matrix.Diagonal((1,-1,1,1));native=F@socket@F;q=native.to_quaternion()
old=Matrix(json.loads((REPO/'docs/assets/rifle02/character-fit.json').read_text())['hand_r_bone_relative_matrix_rows'])
native_ref=json.loads((REPO/'docs/assets/weapon-replacement02/weapons-reopen-report.json').read_text())['grips']['R02_on_rifle_hold']
expected=F@old@F;eq=expected.to_quaternion();rq=native_ref['hand_r_offset_quat_xyzw']
assert max(abs(x-y) for x,y in zip(expected.translation,native_ref['hand_r_offset_location_cm']))<.001
assert abs(abs(sum(x*y for x,y in zip((eq.x,eq.y,eq.z,eq.w),rq)))-1)<.00001
(DOC/'assembly-contract.json').write_text(json.dumps({'task':'ART-28','clips':clips,'socket':{'parent':'hand_r','space':'Blender bone-head local centimeters; column-vector matrices','matrix':[list(r) for r in socket],'hold_hand_component':[list(r) for r in arm.pose.bones['hand_r'].matrix],'hold_rifle_component':[list(r) for r in body.matrix_world],'magazine_body_local':[list(r) for r in Matrix.Translation((15,0,-2.5))]},'expected_native':{'basis':'INT-ART-04 measured C=diag(1,-1,1,1); local matrix C*S*C','validated_against_existing_native_R02_offset':True,'local_matrix':[list(r) for r in native],'location_cm':list(native.translation),'quaternion_xyzw':[q.x,q.y,q.z,q.w],'hold_component_matrix':[list(r) for r in F@body.matrix_world@F],'new_pose_native_verification':'pending'},'dimensions_cm':[75,7.251219511,25.455163002],'scale':[1,1,1]},indent=2)+'\n')
if '--preview-only' not in sys.argv and '--hands-only' not in sys.argv and '--measure-only' not in sys.argv:
    for name,meta in clips.items():
        set_action(arm,bpy.data.actions[name]);sc.frame_start,sc.frame_end=meta['frame_range'];sc.frame_set(1);export_fbx(ROOT/'Exports'/(name+'.fbx'),arm,animated=True)
set_action(arm,hold);sc.frame_set(1)
if '--measure-only' in sys.argv or not any(x in sys.argv for x in ('--preview-only','--hands-only')):sys.exit(0)
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True;sc.render.threads_mode='FIXED';sc.render.threads=2
sc.render.resolution_x=1000;sc.render.resolution_y=850;sc.render.resolution_percentage=100;sc.view_settings.view_transform='AgX';cam=sc.camera;cam.data.type='ORTHO'
def shot(name,pos,target,size):
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=size;sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
if '--hands-only' not in sys.argv:shot('fit_right',(70,-170,155),(22,-12,135),100)
if '--hands-only' not in sys.argv:shot('fit_left',(70,170,155),(22,-12,135),100)
if '--hands-only' not in sys.argv:shot('fit_top',(24,-12,260),(24,-12,132),100)
shot('grip_contact',(65,-100,138),(23,-14,128),38)
shot('support_contact',(75,110,145),(46,-11,131),42)
if '--hands-only' not in sys.argv:shot('fit_body',(270,-430,180),(12,0,85),195)
print('ART28_POSE_SAVED',flush=True)
