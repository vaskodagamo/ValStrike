"""ART-23 local stationary pistol-pose authoring, preserved UrbanEntry rig.
No base mesh/material/normal/rest edits, services or Unreal. Blender 5.2.2 LTS.
Use -- --preview-only during pose fitting; final export follows focused review.
"""
import bpy,sys,json,math,hashlib
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parent;CHAR=ROOT.parent;REPO=CHAR.parents[3]
DOC=REPO/'docs/assets/urban-entry01/pistol-pilot';PRE=DOC/'Previews'
sys.path.insert(0,str(CHAR))
from prepare_source import set_action,reset,curves,export_fbx
PRE.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(CHAR/'UrbanEntry01_Pilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01']
# Remove only the previous preview rifle from this new derived scene.
for obj in list(sc.objects):
    if obj.name.startswith('PREVIEW_ONLY_'):bpy.data.objects.remove(obj,do_unlink=True)
set_action(arm,bpy.data.actions['A_UE01_Pilot_RifleHold']);sc.frame_set(1)
right_hand=arm.pose.bones['hand_r'].matrix.copy()
right_fingers={b.name:b.matrix.copy() for b in arm.pose.bones if b.name.endswith('_r') and any(s in b.name for s in ('thumb','index','middle','ring','pinky'))}
right_pose={b.name:b.matrix_basis.copy() for b in arm.pose.bones if b.name in right_fingers}
reset(arm)
# All lower-body and torso bones retain the original neutral pose. Pose arms only.
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
# Straighten the dominant hand's yaw for a forward pistol, preserving its roll.
yaw=math.atan2(right_hand[1][1],right_hand[0][1])
right_rotation=Matrix.Rotation(-yaw,3,'Z')@right_hand.to_3x3()
solve_wrist('r',(25,-7.3,132),right_rotation,(5,-28,114))
for n,m in right_pose.items():arm.pose.bones[n].matrix_basis=m
bpy.context.view_layer.update()
# Mirror the existing right-hand deformation through the sagittal plane to obtain
# an anatomical left-hand curl. Only rotations are used; left bones keep length.
mirror=Matrix.Diagonal((1,-1,1,1))
def mirror_rotation(name):
    r=arm.pose.bones[name];l=arm.data.bones[name[:-1]+'l']
    return (mirror@r.matrix@r.bone.matrix_local.inverted()@mirror@l.matrix_local).to_quaternion().to_matrix()
left_rotation=mirror_rotation('hand_r')
solve_wrist('l',(29,3.3,130.5),left_rotation,(5,28,114))
for rn in right_fingers:
    ln=rn[:-1]+'l';bone=arm.pose.bones[ln];target=bone.head.copy()
    bone.matrix=Matrix.Translation(target)@mirror_rotation(rn).to_4x4();bpy.context.view_layer.update()
# Shape the support index around the dominant fingers, not as a second trigger
# finger; spread the support thumb along the grip side. All changes are rotations.
def rotate_bone_world(name,axis,degrees):
    b=arm.pose.bones[name];p=b.head.copy()
    b.matrix=Matrix.Translation(p)@Matrix.Rotation(math.radians(degrees),4,axis)@Matrix.Translation(-p)@b.matrix
    bpy.context.view_layer.update()
# Open the supplied tight rifle fist to clear the larger P01 grip; same scale.
for digit in ('middle','ring','pinky'):
    rotate_bone_world(digit+'_02_r','Z',-60)
rotate_bone_world('thumb_01_r','Z',50)
rotate_bone_world('thumb_01_r','Y',-60)
rotate_bone_world('index_01_l','Z',-45)
rotate_bone_world('index_02_l','X',35)
rotate_bone_world('index_03_l','Z',30)
rotate_bone_world('thumb_01_l','Z',50)
rotate_bone_world('thumb_02_l','Y',-20)
# Dominant index stays outside the guard, angled along the side of the frame.
rotate_bone_world('index_01_r','Z',-25)
rotate_bone_world('index_01_r','Y',-20)
pose={b.name:b.matrix_basis.copy() for b in arm.pose.bones}
action=bpy.data.actions['A_UE01_Pilot_RifleHold'].copy();action.name='A_UE01_Pilot_PistolHold';action.use_fake_user=True
for f in curves(action):
    name=f.data_path.split('"')[1];loc,quat,scale=pose[name].decompose()
    values=loc if f.data_path.endswith('.location') else quat if f.data_path.endswith('.rotation_quaternion') else scale
    f.keyframe_points.clear()
    for frame in (1,61):f.keyframe_points.insert(frame,values[f.array_index])
    f.update()
set_action(arm,action);sc.frame_set(1);sc.frame_start=1;sc.frame_end=61;sc.render.fps=60
weapons={}
for ident,file,names in [
 ('P01',REPO/'Art/Source/Weapons/Pistol01/Models/P01_Shrike.blend',['SM_VS_Pistol01_Body','SM_VS_Pistol01_Slide','SM_VS_Pistol01_Magazine']),
 ('P02',REPO/'Art/Source/Weapons/Pistol02/Models/P02_Kestrel.blend',['SM_VS_Pistol02_Body','SM_VS_Pistol02_Slide','SM_VS_Pistol02_Magazine'])]:
    with bpy.data.libraries.load(str(file),link=False) as (src,dst):dst.objects=names
    collection=bpy.data.collections.new('PREVIEW_ONLY_'+ident);sc.collection.children.link(collection)
    world=Matrix.Translation((38,-3,137) if ident=='P01' else (38,-3,134))
    socket=(arm.matrix_world@arm.pose.bones['hand_r'].matrix).inverted()@world
    anchor=bpy.data.objects.new('PREVIEW_ONLY_'+ident+'_hand_r',None);collection.objects.link(anchor)
    con=anchor.constraints.new('COPY_TRANSFORMS');con.target=arm;con.subtarget='hand_r'
    parts=[]
    for obj in dst.objects:
        collection.objects.link(obj);obj.hide_set(False);obj.hide_render=False
        offset=Matrix.Translation((0,0,3)) if ident=='P01' and obj.name.endswith('_Slide') else Matrix.Identity(4)
        obj.parent=anchor;obj.matrix_parent_inverse=Matrix.Identity(4);obj.matrix_basis=socket@offset
        parts.append(obj)
    weapons[ident]={'collection':collection,'objects':parts,'matrix':world,'socket':socket}
# Source image paths must remain portable after saving one directory deeper.
for img in bpy.data.images:
    if img.source=='FILE' and not img.packed_file:
        img.filepath=bpy.path.abspath(img.filepath)
sc.render.engine='BLENDER_WORKBENCH';sc.display.shading.light='STUDIO';sc.display.shading.color_type='TEXTURE'
sc.display.shading.show_shadows=True;sc.display.shading.show_cavity=True;sc.display.shading.show_specular_highlight=False
sc.render.resolution_x=900;sc.render.resolution_y=800;sc.render.resolution_percentage=100
sc.display.shading.background_type='WORLD';sc.world.color=(.2,.2,.2)
cam=sc.camera;cam.data.type='ORTHO'
def render(name,pos,target,size):
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=size
    sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
for ident,weapon in weapons.items():
    for k,w in weapons.items():w['collection'].hide_render=k!=ident;w['collection'].hide_viewport=k!=ident
    render(ident+'_body',(270,-430,180),(12,0,85),195)
    render(ident+'_right',(90,-100,148),(37,0,133),42)
    render(ident+'_left',(90,100,148),(37,0,133),42)
    render(ident+'_top',(42,0,230),(37,0,133),42)
for k,w in weapons.items():w['collection'].hide_render=k!='P01';w['collection'].hide_viewport=k!='P01'
report={'weapon_matrices':{k:{'component':[list(r) for r in w['matrix']],'hand_r_local':[list(r) for r in w['socket']]} for k,w in weapons.items()},'joints':{b.name:list(b.head) for b in arm.pose.bones if 'hand' in b.name or any(n in b.name for n in ('index','middle','thumb'))}}
(DOC/'pose-authoring.json').write_text(json.dumps(report,indent=2)+'\n')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_PistolPilot.blend'))
for img in bpy.data.images:
    if img.source=='FILE' and not img.packed_file:img.filepath=bpy.path.relpath(img.filepath,start=str(ROOT))
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'UrbanEntry01_PistolPilot.blend'))
if '--preview-only' not in sys.argv:
    (ROOT/'Exports').mkdir(exist_ok=True)
    export_fbx(ROOT/'Exports/A_UE01_Pilot_PistolHold.fbx',arm,animated=True)
print('ART23_POSE_PREVIEWED',flush=True)
