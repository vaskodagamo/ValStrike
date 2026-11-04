"""ART-27 preserved-rig K01 source authoring. Blender 5.2.2 LTS; no services/Unreal."""
import bpy,sys,json,math
from pathlib import Path
from mathutils import Matrix,Vector,Quaternion
ROOT=Path(__file__).resolve().parent;CHAR=ROOT.parent;REPO=CHAR.parents[3]
DOC=REPO/'docs/assets/urban-entry01/knife-pilot';PRE=DOC/'Previews'
sys.dont_write_bytecode=True
sys.path.insert(0,str(CHAR))
from prepare_source import set_action,reset,curves,export_fbx
bpy.ops.wm.open_mainfile(filepath=str(CHAR/'UrbanEntry01_Pilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01']
for o in list(sc.objects):
    if o.name.startswith('PREVIEW_ONLY_'):bpy.data.objects.remove(o,do_unlink=True)
set_action(arm,bpy.data.actions['A_UE01_Pilot_RifleHold']);sc.frame_set(1)
ref_hand=arm.pose.bones['hand_r'].matrix.copy()
fingers={b.name:b.matrix_basis.copy() for b in arm.pose.bones if b.name.endswith('_r') and any(s in b.name for s in ('thumb','index','middle','ring','pinky'))}
# Close the trigger finger for a knife grip, using the existing ring finger curl.
for part in ('01','02','03'):fingers['index_'+part+'_r']=fingers['ring_'+part+'_r'].copy()
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
    b=arm.pose.bones[name];p=b.head.copy()
    b.matrix=Matrix.Translation(p)@Matrix.Rotation(math.radians(degrees),4,axis)@Matrix.Translation(-p)@b.matrix
    bpy.context.view_layer.update()
# Reference knife head-local transform, refined by pose/contact inspection.
socket=Matrix(json.loads((REPO/'docs/assets/knife01/readiness.json').read_text())['suggested_hold']['hand_r_head_local_matrix'])
socket.translation.x+=.5
# Desired knife axes are explicit in character component space. Forward +X,
# source character left +Y. Wrist rotation follows the constant hand socket.
def rotation_for(blade,edge):
    x=Vector(blade).normalized();z=Vector(edge);z=(z-x*z.dot(x)).normalized();y=z.cross(x).normalized()
    return Matrix(((x.x,y.x,z.x),(x.y,y.y,z.y),(x.z,y.z,z.z)))@socket.to_3x3().inverted()
right_rot=rotation_for((.6,.12,.79),(.79,0,-.6))
# Authored targets are bone-head positions, not bone tails or scaled limbs.
def pose(r=(27,-21,123),l=(25,22,127),blade=(.6,.12,.79),edge=(.79,0,-.6),twist=0):
    reset(arm)
    rotate('spine_02','Z',twist*.45);rotate('spine_03','Z',twist*.55)
    solve_wrist('r',r,rotation_for(blade,edge),(2,-35,110))
    for n,m in fingers.items():arm.pose.bones[n].matrix_basis=m
    bpy.context.view_layer.update()
    mirror=Matrix.Diagonal((1,-1,1,1))
    def mirrored(n):
        b=arm.pose.bones[n];lb=arm.data.bones[n[:-1]+'l']
        bm=Matrix.Translation((27,-21,123))@right_rot.to_4x4() if n=='hand_r' else b.matrix
        return (mirror@bm@b.bone.matrix_local.inverted()@mirror@lb.matrix_local).to_quaternion().to_matrix()
    # Loose guard hand, kept on its own side outside the entire attack sweep.
    leftrot=Matrix.Rotation(math.radians(-12),3,'Z')@mirrored('hand_r')
    solve_wrist('l',l,leftrot,(0,36,110))
    for rn in fingers:
        ln=rn[:-1]+'l';b=arm.pose.bones[ln]
        if 'left_fingers' in globals():b.matrix_basis=left_fingers[ln]
        else:b.matrix=Matrix.Translation(b.head)@mirrored(rn).to_4x4()
        bpy.context.view_layer.update()
    return {b.name:b.matrix_basis.copy() for b in arm.pose.bones}
idle=pose()
left_fingers={rn[:-1]+'l':arm.pose.bones[rn[:-1]+'l'].matrix_basis.copy() for rn in fingers}
# Only the requested frozen K01 mesh is appended; no geometry/material edits.
with bpy.data.libraries.load(str(REPO/'Art/Source/Weapons/Knife01/Knife01.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Knife01']
knife=dst.objects[0];sc.collection.objects.link(knife);knife.name='PREVIEW_ONLY_K01'
anchor=bpy.data.objects.new('PREVIEW_ONLY_K01_hand_r',None);sc.collection.objects.link(anchor)
con=anchor.constraints.new('COPY_TRANSFORMS');con.target=arm;con.subtarget='hand_r'
knife.parent=anchor;knife.matrix_parent_inverse=Matrix.Identity(4);knife.matrix_basis=socket
# Create sampled quaternion motion with smoothstep easing between authored poses.
# Dense 60 Hz keys make the source/export motion agree without Bezier overshoot.
clips={}
def action(name,keys,end,impact=None,loop=False):
    a=bpy.data.actions['A_UE01_Pilot_RifleHold'].copy();a.name='A_UE01_Knife_'+name;a.use_fake_user=True
    frames=sorted(keys)
    sampled={}
    for frame in range(1,end+1):
        lo=max(f for f in frames if f<=frame);hi=min(f for f in frames if f>=frame)
        t=0 if hi==lo else (frame-lo)/(hi-lo);t=t*t*(3-2*t)
        sampled[frame]={}
        for n in keys[lo]:
            p,q,s=keys[lo][n].decompose();p2,q2,s2=keys[hi][n].decompose();q.make_compatible(q2)
            sampled[frame][n]=(p.lerp(p2,t),q.slerp(q2,t),s.lerp(s2,t))
    for f in curves(a):
        n=f.data_path.split('"')[1];kind=0 if f.data_path.endswith('.location') else 1 if f.data_path.endswith('.rotation_quaternion') else 2
        f.keyframe_points.clear()
        for fr in range(1,end+1):
            k=f.keyframe_points.insert(fr,sampled[fr][n][kind][f.array_index]);k.interpolation='LINEAR'
        f.update()
    if impact is not None:a.pose_markers.new('Impact').frame=impact
    a.pose_markers.new('Ready').frame=end
    clips[a.name]={'frame_range':[1,end],'fps':60,'duration_seconds':(end-1)/60,'impact_frame':impact,'impact_seconds':None if impact is None else (impact-1)/60,'loop':loop,'root_motion':False}
    return a
hold=action('Hold',{1:idle,61:idle},61,loop=True)
equip=action('Equip',{1:pose((10,-29,101),(20,24,119),(.75,-.2,.63),(.6,0,-.8)),11:pose((22,-28,117),(23,23,124),(.4,-.2,.9),(.9,0,-.4)),21:pose((28,-21,125)),25:idle},25)
slash=action('Slash',{1:idle,5:pose((20,-33,133),(20,29,125),(.45,-.4,.8),(.6,0,-.7),-9),10:pose((38,-6,131),(20,30,124),(.7,.55,.45),(.5,0,-.8),10),16:pose((29,8,123),(18,31,128),(.5,.8,.33),(.5,0,-.8),16),25:pose((25,-13,119),(23,25,126),(.7,.15,.7),(.7,0,-.7),5),34:idle},34,10)
stab=action('Stab',{1:idle,9:pose((16,-24,125),(20,26,126),(.8,.05,.6),(.6,0,-.8),-5),19:pose((38,-13,126),(18,29,129),(.78,0,.625),(.625,0,-.78),7),24:pose((37,-13,126),(18,29,129),(.78,0,.625),(.625,0,-.78),7),36:pose((22,-24,123),(22,26,126),(.8,.1,.6),(.6,0,-.8),-2),55:idle},55,19)
# Retain original lower-body F-curves exactly, freeze only the upper-body stance.
upper={'spine_01'}|{b.name for b in arm.data.bones['spine_01'].children_recursive}
set_action(arm,hold);sc.frame_set(1);spine=arm.pose.bones['spine_01'].matrix.copy();pelvis=arm.pose.bones['pelvis'].head.copy()
for original,label in [('Walking','Walk'),('Running','Run')]:
    a=bpy.data.actions['A_UE01_'+original].copy();a.name='A_UE01_Knife_'+label;a.use_fake_user=True
    set_action(arm,a);sc.frame_set(1)
    arm.pose.bones['spine_01'].matrix=Matrix.Translation(arm.pose.bones['pelvis'].head-pelvis)@spine;bpy.context.view_layer.update()
    p=dict(idle);p['spine_01']=arm.pose.bones['spine_01'].matrix_basis.copy()
    for f in curves(a):
        n=f.data_path.split('"')[1]
        if n not in upper:continue
        v=p[n].decompose()[0 if f.data_path.endswith('.location') else 1 if f.data_path.endswith('.rotation_quaternion') else 2][f.array_index]
        for k in f.keyframe_points:k.co.y=v;k.handle_left.y=v;k.handle_right.y=v
        f.update()
    end=round(a.frame_range[1]);clips[a.name]={'frame_range':[1,end],'fps':60,'duration_seconds':(end-1)/60,'impact_frame':None,'impact_seconds':None,'loop':True,'root_motion':False,'lower_body_source':'A_UE01_'+original}
for img in bpy.data.images:
    if img.source=='FILE' and not img.packed_file:img.filepath=bpy.path.abspath(img.filepath)
set_action(arm,hold);sc.frame_set(1);sc.frame_start=1;sc.frame_end=61;sc.render.fps=60
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_KnifePilot.blend'))
for img in bpy.data.images:
    if img.source=='FILE' and not img.packed_file:img.filepath=bpy.path.relpath(img.filepath,start=str(ROOT))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_KnifePilot.blend'))
(DOC/'animation-contract.json').write_text(json.dumps({'task':'ART-27','clips':clips,'socket':{'parent':'hand_r','space':'Blender bone HEAD-local centimeters, column vectors','matrix':[list(r) for r in socket],'scale':[1,1,1],'hold_hand_component_matrix':[list(r) for r in arm.pose.bones['hand_r'].matrix],'hold_knife_component_matrix':[list(r) for r in knife.matrix_world]}},indent=2)+'\n')
if '--preview-only' not in sys.argv:
    for name,meta in clips.items():
        if '--attacks-only' in sys.argv and not name.endswith(('Slash','Stab')):continue
        set_action(arm,bpy.data.actions[name]);sc.frame_start,sc.frame_end=meta['frame_range'];sc.frame_set(1)
        export_fbx(ROOT/'Exports'/(name+'.fbx'),arm,animated=True)
if '--attacks-only' in sys.argv:
    print('ART27_ATTACKS_UPDATED',flush=True);sys.exit(0)
sc.render.engine='CYCLES';sc.cycles.samples=12;sc.cycles.use_denoising=True;sc.render.threads_mode='FIXED';sc.render.threads=2
sc.render.resolution_x=900;sc.render.resolution_y=800;sc.render.resolution_percentage=100
cam=sc.camera;cam.data.type='ORTHO'
def shot(name,pos,target,size):
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=size;sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
set_action(arm,hold);sc.frame_set(1)
shot('hold_contact',(90,-100,152),(34,-19,130),43)
shot('hold_contact_reverse',(30,100,145),(34,-19,130),43)
shot('hold_body',(270,-430,180),(12,0,85),195)
print('ART27_AUTHORED',flush=True)
