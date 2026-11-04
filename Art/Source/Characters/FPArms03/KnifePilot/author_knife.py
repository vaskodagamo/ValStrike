"""ART-26 derived knife source. Frozen inputs; local authoring only, no service/UE."""
import bpy, math, json, sys, hashlib
from pathlib import Path
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[4]/'docs/assets/fparms03/knife-pilot'
MODE=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'study'
def rot(axis,angle):return Matrix.Rotation(math.radians(angle),4,axis)
def trans(v):return Matrix.Translation(Vector(v))
def smooth(t):t=max(0,min(1,t));return t*t*(3-2*t)
def interpolate(keys,frame):
    for (a,av),(b,bv) in zip(keys,keys[1:]):
        if frame<=b:
            t=smooth((frame-a)/(b-a));return [x+(y-x)*t for x,y in zip(av,bv)]
    return keys[-1][1]
for source in json.loads((DOC/'inputs.json').read_text())['sources']:
    assert hashlib.sha256((ROOT/'Reference/Frozen'/source['name']).read_bytes()).hexdigest()==source['sha256']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Reference/Frozen/ART24_FPArms03.blend'))
scene=bpy.context.scene;scene.render.fps=60;scene.frame_start=1
arm=bpy.data.objects['SKEL_FPArms03'];meshes=[bpy.data.objects['SK_FPArms03_'+s] for s in ['R','L']]
arm.animation_data.action=None
for a in list(bpy.data.actions):bpy.data.actions.remove(a)
for b in arm.pose.bones:b.rotation_mode='QUATERNION';b.matrix_basis=Matrix.Identity(4)
K=trans((8,-10,-2))@rot('Z',18)@rot('Y',-20)
CROSS=Matrix(((0,1,0,0),(.8,0,-.6,0),(-.6,0,-.8,0),(0,0,0,1)))
FOREARM=22.612010205024273
RIGHT_CURL={'index':[35,35,0],'middle':[35,35,0],'ring':[30,35,0],'pinky':[15,15,0],'thumb':[-10,-15,-5]}
def place(side,wrist,basis,direction):
    wrist=Vector(wrist);direction=Vector(direction).normalized();elbow=wrist-direction*FOREARM
    root=arm.data.bones['forearm_'+side]
    dorsal=basis.to_3x3()@Vector((0,0,1));lateral=dorsal.cross(direction).normalized();normal=direction.cross(lateral).normalized()
    turn=Matrix((direction,lateral,normal)).transposed().to_4x4()
    arm.pose.bones[root.name].matrix=trans(elbow)@turn@trans(-root.head_local)@root.matrix_local
    bpy.context.view_layer.update()
    hand=arm.data.bones['hand_'+side]
    arm.pose.bones[hand.name].matrix=trans(wrist)@basis@trans(-hand.head_local)@hand.matrix_local
    bpy.context.view_layer.update()
def pose(kind,frame):
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    for side in ['r','l']:
        for digit in ['index','middle','ring','pinky','thumb']:
            angles=RIGHT_CURL[digit] if side=='r' else ([0,8,5] if digit=='thumb' else [15,22,12])
            for i,angle in enumerate(angles):arm.pose.bones[f'{digit}_{i+1:02}_{side}'].rotation_quaternion=rot('X',-angle).to_quaternion()
        arm.pose.bones['thumb_01_'+side].rotation_quaternion=(rot('Z',-8 if side=='r' else 3)@rot('X',10 if side=='r' else 0)).to_quaternion()
    # Translation (cm) + component-space Euler rotation deltas (degrees).
    keys={
      'Equip':[(1,[-8,4,-27,0,35,-15]),(13,[-3,1,-9,0,12,-5]),(25,[0,0,0,0,0,0])],
      'Slash':[(1,[0,0,0,0,0,0]),(5,[-3,-8,5,-8,-5,35]),(10,[5,10,2,12,5,-35]),(17,[0,14,-4,15,18,-50]),(34,[0,0,0,0,0,0])],
      'Stab':[(1,[0,0,0,0,0,0]),(10,[-6,-3,1,0,12,0]),(19,[17,1,3,0,0,-8]),(26,[13,1,2,0,0,-8]),(55,[0,0,0,0,0,0])]}
    values=interpolate(keys[kind],frame) if kind in keys else [0,0,.28*math.sin((frame-1)/60*2*math.pi),0,0,0]
    if kind=='Locomotion':values=[0,.6*math.sin((frame-1)/48*2*math.pi),.65*math.sin((frame-1)/48*4*math.pi),0,0,1.2*math.sin((frame-1)/48*2*math.pi)]
    delta=trans(values[:3])@trans(K.translation)@rot('Z',values[5])@rot('Y',values[4])@rot('X',values[3])@trans(-K.translation)
    right=delta@K;wrist=right@Vector((-1.5,-9.55,3))
    elbow_forward=9+4*max(0,values[0]/17) if kind=='Stab' else 9
    place('r',wrist,right.to_quaternion().to_matrix().to_4x4()@CROSS,right.to_3x3()@Vector((elbow_forward,20,-3)))
    retract=0
    if kind in ['Slash','Stab']:retract=interpolate([(1,[0]),(5,[1]),(19,[1]),(34 if kind=='Slash' else 55,[0])],frame)[0]
    left_wrist=Vector((15,17+5*retract,-7-4*retract))
    if kind=='Equip':left_wrist.z+=values[2]*.65
    if kind=='Locomotion':left_wrist.z-=values[2]*.7
    place('l',left_wrist,rot('Z',-15)@rot('Y',10),(18,-4,5))
    return right
pose('Idle',1)
attachment=(arm.matrix_world@arm.pose.bones['weapon_socket_r'].matrix).inverted()@K
def attach(weapon):weapon.matrix_world=arm.matrix_world@arm.pose.bones['weapon_socket_r'].matrix@attachment
clips={'Equip':{'end':25,'duration':.40},'Idle':{'end':61,'duration':1.0,'loop':True},'Locomotion':{'end':49,'duration':.8,'loop':True},'Slash':{'end':34,'duration':.55,'impact_frame':10,'impact_seconds':.15},'Stab':{'end':55,'duration':.90,'impact_frame':19,'impact_seconds':.30}}
if MODE=='build':
    for kind,clip in clips.items():
        arm.animation_data.action=None
        a=bpy.data.actions.new('A_FPArms03_Knife'+kind);a.use_fake_user=True;arm.animation_data.action=a
        scene.frame_end=clip['end']
        for frame in range(1,clip['end']+1):
            scene.frame_set(frame);pose(kind,frame)
            for b in arm.pose.bones:
                for prop in ['location','rotation_quaternion','scale']:b.keyframe_insert(data_path=prop,frame=frame,group=b.name)
        if 'impact_frame' in clip:a.pose_markers.new('Impact').frame=clip['impact_frame']
        clip.update(action=a.name,start=1,fps=60,root_motion=False,frame_to_seconds='(frame - 1) / 60')
    arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update();scene.frame_start=1;scene.frame_end=61
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'KnifePilot.blend'))
    metadata={'task':'ART-26','fps':60,'root_motion':False,'units':'cm; +X forward / +Z up','clips':clips,'attachment':{'bone':'weapon_socket_r','weapon_local_from_socket':[list(row) for row in attachment],'formula':'K01 component matrix = arm component matrix @ posed weapon_socket_r matrix @ weapon_local_from_socket; column vectors','weapon_scale':1,'K01_commit':'d774eafb710af68bf5fa23666e5d0ddacb651d3e'},'camera':{'position_cm':[-20,0,5],'target_cm':[80,0,5],'horizontal_fov_degrees':90},'impact_authority':'Visual markers only; server gameplay owns impact validation/damage.'}
    (DOC/'animation-contract.json').write_text(json.dumps(metadata,indent=2)+'\n')
    print('ART26_BUILD_COMPLETE');sys.exit(0)
# Actual frozen K01 source staging, never saved/exported with the hands.
with bpy.data.libraries.load(str(ROOT/'Reference/Frozen/K01.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Knife01']
weapon=dst.objects[0];scene.collection.objects.link(weapon);weapon.hide_render=False;weapon.hide_viewport=False;weapon.hide_set(False);attach(weapon)
scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True;scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Knife studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.15,.20,1)
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((10,-50,70),110000,65),((-20,40,30),85000,60),((60,0,35),65000,50)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(8,-10,-2))
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
for name,loc,target,scale in [('front',(35,-55,20),(8,-10,-2),36),('reverse',(0,40,12),(8,-10,-2),36),('above',(8,-10,70),(8,-10,-2),40),('90fov',(-20,0,5),(80,0,5),None)]:
    cam.location=loc;aim(cam,target);cam.data.type='ORTHO' if scale else 'PERSP';cam.data.ortho_scale=scale or 50;cam.data.lens_unit='FOV';cam.data.sensor_fit='HORIZONTAL';cam.data.angle=math.radians(90);cam.data.clip_start=.1;cam.data.clip_end=1000
    scene.render.filepath=str(DOC/'Previews'/f'study_{name}.png');bpy.ops.render.render(write_still=True)
print('ART26_STUDY_COMPLETE')
