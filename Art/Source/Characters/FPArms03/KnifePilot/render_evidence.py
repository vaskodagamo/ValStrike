"""Actual saved source/FBX views, with the same original PBR and K01 staging."""
import bpy,json,math,sys
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[4]/'docs/assets/fparms03/knife-pilot'
MODE=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'source'
contract=json.loads((DOC/'animation-contract.json').read_text());attachment=Matrix(contract['attachment']['weapon_local_from_socket'])
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'KnifePilot.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms03'];meshes=[bpy.data.objects['SK_FPArms03_'+s] for s in ['R','L']];material=meshes[0].data.materials[0]
actions={k:bpy.data.actions[v['action']] for k,v in contract['clips'].items()}
if MODE=='export':
    for o in [arm]+meshes:o.hide_render=True
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/SK_FPArms03_KnifePilot.fbx'),use_anim=False)
    imported=[o for o in bpy.data.objects if o not in before];arm=next(o for o in imported if o.type=='ARMATURE');meshes=[o for o in imported if o.type=='MESH']
    for o in meshes:o.data.materials.clear();o.data.materials.append(material)
    for kind,clip in contract['clips'].items():
        before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/(clip['action']+'.fbx')),anim_offset=0)
        imported_arm=next(o for o in bpy.data.objects if o not in before and o.type=='ARMATURE');actions[kind]=imported_arm.animation_data.action;actions[kind].use_fake_user=True
        bpy.data.objects.remove(imported_arm,do_unlink=True)
with bpy.data.libraries.load(str(ROOT/'Reference/Frozen/K01.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Knife01']
weapon=dst.objects[0];scene.collection.objects.link(weapon);weapon.hide_render=False;weapon.hide_viewport=False;weapon.hide_set(False)
scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True;scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Knife studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.15,.20,1)
def aim(o,target):o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((10,-50,70),110000,65),((-20,40,30),85000,60),((60,0,35),65000,50)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(8,-10,-2))
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
views={'front':((35,-55,20),(8,-10,-2),36),'reverse':((0,40,12),(8,-10,-2),36),'above':((8,-10,70),(8,-10,-2),40),'90fov':((-20,0,5),(80,0,5),None)}
cases=[('Idle',1,v) for v in views]+[('Equip',13,'90fov'),('Equip',25,'90fov'),('Locomotion',13,'90fov'),('Slash',5,'90fov'),('Slash',10,'90fov'),('Slash',34,'90fov'),('Stab',10,'90fov'),('Stab',19,'90fov'),('Stab',55,'90fov')]
for kind,frame,view in cases:
    arm.animation_data_create();arm.animation_data.action=actions[kind];arm.animation_data.action_slot=actions[kind].slots[0];scene.frame_set(frame);bpy.context.view_layer.update()
    weapon.matrix_world=arm.matrix_world@arm.pose.bones['weapon_socket_r'].matrix@attachment
    loc,target,scale=views[view];cam.location=loc;aim(cam,target);cam.data.type='ORTHO' if scale else 'PERSP';cam.data.ortho_scale=scale or 50;cam.data.lens_unit='FOV';cam.data.sensor_fit='HORIZONTAL';cam.data.angle=math.radians(90);cam.data.clip_start=.1;cam.data.clip_end=1000
    scene.render.filepath=str(DOC/'Previews'/f'{MODE}_{kind.lower()}_{frame:02}_{view}.png');bpy.ops.render.render(write_still=True)
print('ART26_'+MODE.upper()+'_RENDERS_COMPLETE')
