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
if MODE.endswith('export'):
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
if MODE=='c1-source':cases=[('Idle',1,v) for v in ['reverse','above','90fov']]
if MODE=='c1-export':cases=[('Idle',1,v) for v in views]+[('Slash',f,'90fov') for f in [5,10,17,34]]+[('Stab',f,'90fov') for f in [10,19,26,55]]
if '--sheet' in sys.argv:
    # Inspect every exported attack/hold frame, not just the strip samples.
    # Camera convention is fixed by the contract: (-20,0,5), +X, 90 horizontal.
    import numpy as np
    right=next(o for o in meshes if o.name.startswith('SK_FPArms03_R'))
    ids=np.array([v.index for v in right.data.vertices if 'forearm' not in right.vertex_groups[max(v.groups,key=lambda g:g.weight).group].name])
    framing={'camera':contract['camera'],'method':'Every frame, all right hand/finger dominant-weight vertices plus all K01 vertices; normalized UV viewport. Forearm excluded; equip entry intentionally excluded.','clips':{}}
    def world_points(o,evaluated=False):
        m=o.evaluated_get(bpy.context.evaluated_depsgraph_get()).data if evaluated else o.data
        data=np.empty(len(m.vertices)*3,dtype=np.float32);m.vertices.foreach_get('co',data)
        data=data.reshape((-1,3));mat=np.array(o.matrix_world)
        return data@mat[:3,:3].T+mat[:3,3]
    arm.animation_data_create()
    for kind in ['Idle','Locomotion','Slash','Stab']:
        arm.animation_data.action=actions[kind];arm.animation_data.action_slot=actions[kind].slots[0];frames=[]
        for frame in range(1,contract['clips'][kind]['end']+1):
            scene.frame_set(frame);bpy.context.view_layer.update();weapon.matrix_world=arm.matrix_world@arm.pose.bones['weapon_socket_r'].matrix@attachment
            points=np.concatenate([world_points(right,True)[ids],world_points(weapon)])
            depth=points[:,0]+20;u=.5-points[:,1]/(2*depth);v=.5+(points[:,2]-5)/(2*(9/16)*depth)
            frames.append({'frame':frame,'uv_min':[float(u.min()),float(v.min())],'uv_max':[float(u.max()),float(v.max())],'front':bool(np.all(depth>0))})
        framing['clips'][kind]={'all_inside':all(f['front'] and min(f['uv_min'])>=0 and max(f['uv_max'])<=1 for f in frames),'frames':frames}
    (DOC/'c1-framing.json').write_text(json.dumps(framing,indent=2)+'\n')
if '--sheet' in sys.argv:
    cases=[(kind,f,'90fov') for kind,frames in {'Equip':[1,5,9,13,19,25],'Idle':[1,13,25,37,49,61],'Locomotion':[1,10,20,30,40,49],'Slash':[1,5,10,17,25,34],'Stab':[1,10,19,26,40,55]}.items() for f in frames]
    scene.render.resolution_x=480;scene.render.resolution_y=270;scene.cycles.samples=8
    MODE='c1-strip'
for kind,frame,view in cases:
    arm.animation_data_create();arm.animation_data.action=actions[kind];arm.animation_data.action_slot=actions[kind].slots[0];scene.frame_set(frame);bpy.context.view_layer.update()
    weapon.matrix_world=arm.matrix_world@arm.pose.bones['weapon_socket_r'].matrix@attachment
    loc,target,scale=views[view];cam.location=loc;aim(cam,target);cam.data.type='ORTHO' if scale else 'PERSP';cam.data.ortho_scale=scale or 50;cam.data.lens_unit='FOV';cam.data.sensor_fit='HORIZONTAL';cam.data.angle=math.radians(90);cam.data.clip_start=.1;cam.data.clip_end=1000
    scene.render.filepath=str(DOC/'Previews'/f'{MODE}_{kind.lower()}_{frame:02}_{view}.png');bpy.ops.render.render(write_still=True)
print('ART26_'+MODE.upper()+'_RENDERS_COMPLETE')
