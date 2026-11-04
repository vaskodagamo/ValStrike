"""ART-24 bounded P02 pose study. No geometry/rest/material edits or service calls."""
import bpy, json, math, sys, hashlib
from pathlib import Path
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parent
DOC=ROOT.parents[3]/'docs/assets/fparms03/art24'
STAGE=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'candidate'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Reference/Frozen/ART22_FPArms03.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms03']
meshes=[bpy.data.objects['SK_FPArms03_'+s] for s in ['R','L']]
action=bpy.data.actions['A_FPArms03_PistolHold']
arm.animation_data.action=action;arm.animation_data.action_slot=action.slots[0]
scene.frame_set(1);bpy.context.view_layer.update()
cfg=json.loads((ROOT/'pistol-contact.json').read_text())
if STAGE not in ['before','export']:
    arm.animation_data.action=None
    for bone in arm.pose.bones:bone.rotation_mode='XYZ';bone.matrix_basis=Matrix.Identity(4)
    for side,word,roll in [('r','right',90),('l','left',-90)]:
        for digit in ['index','middle','ring','pinky','thumb']:
            angles=cfg[word+'_thumb'] if digit=='thumb' else cfg[word+'_curl']
            angles=cfg.get(word+'_'+digit,angles)
            for i,angle in enumerate(angles):arm.pose.bones[f'{digit}_{i+1:02}_{side}'].rotation_euler.x=-math.radians(angle)
        arm.pose.bones['thumb_01_'+side].rotation_euler.z=math.radians(cfg[word+'_thumb_spread'])
        if side=='r':arm.pose.bones['index_01_r'].rotation_euler.z=math.radians(cfg['right_index_spread'])
        pivot=Vector((24,-13 if side=='r' else 13,0))
        transform=Matrix.Translation(Vector(cfg[word+'_wrist']))@Matrix.Rotation(math.radians(roll),4,'X')@Matrix.Translation(-pivot)
        arm.pose.bones['forearm_'+side].matrix=transform@arm.data.bones['forearm_'+side].matrix_local
    bpy.context.view_layer.update()
    bpy.data.actions.remove(action);action=bpy.data.actions.new('A_FPArms03_PistolHold');action.use_fake_user=True
    arm.animation_data.action=action
    for frame in [1,30]:
        for bone in arm.pose.bones:
            for prop in ['location','rotation_euler','scale']:bone.keyframe_insert(data_path=prop,frame=frame,group=bone.name)
    scene.frame_set(1);bpy.context.view_layer.update()
if STAGE=='final':
    report={'settings':cfg,'scale':1,'bone_basis':{b.name:[list(row) for row in b.matrix_basis] for b in arm.pose.bones},'component_matrices':{b.name:[list(row) for row in b.matrix] for b in arm.pose.bones if b.name in ['forearm_r','forearm_l','hand_r','hand_l','weapon_socket_r']}}
    (DOC/'pose-recipe.json').write_text(json.dumps(report,indent=2)+'\n')
    arm.animation_data.action=None
    for bone in arm.pose.bones:bone.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'FPArms03.blend'))
    arm.animation_data.action=action;arm.animation_data.action_slot=action.slots[0];scene.frame_set(1)
if STAGE=='export':
    baseline_material=meshes[0].data.materials[0]
    for o in [arm]+meshes:o.hide_render=True
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/SK_FPArms03.fbx'),use_anim=False)
    imported=[o for o in bpy.data.objects if o not in before];export_arm=next(o for o in imported if o.type=='ARMATURE')
    for o in imported:
        if o.type=='MESH':o.data.materials.clear();o.data.materials.append(baseline_material)
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/A_FPArms03_PistolHold.fbx'),anim_offset=0)
    animation_arm=next(o for o in bpy.data.objects if o not in before and o.type=='ARMATURE')
    imported_action=animation_arm.animation_data.action
    export_arm.animation_data_create();export_arm.animation_data.action=imported_action;export_arm.animation_data.action_slot=imported_action.slots[0]
    animation_arm.hide_render=True;scene.frame_set(1);bpy.context.view_layer.update()
# Exact same lights/materials and existing front/reverse/FOV cameras as ART-22.
scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Fit studio');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.15,.20,1)
def aim(obj,target):obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((10,-50,70),110000,65),((-20,40,30),85000,60),((60,0,35),65000,50)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(5,0,0))
frozen=json.loads((DOC.parent/'frozen-weapon-sources.json').read_text())['P02'];path=ROOT/'Reference/Frozen/P02.blend'
assert hashlib.sha256(path.read_bytes()).hexdigest()==frozen['sha256']
with bpy.data.libraries.load(str(path),link=False) as (src,dst):dst.objects=[n for n in src.objects if n.startswith('SM_VS_Pistol02')]
for o in dst.objects:scene.collection.objects.link(o);o.hide_render=False;o.hide_viewport=False;o.hide_set(False)
# Narrow contact diagnostic supplements (never replaces) the opposite-side images.
if STAGE in ['diagnostic','candidate','final']:
    from mathutils.bvhtree import BVHTree
    dg=bpy.context.evaluated_depsgraph_get();vertices=[];faces=[]
    for o in dst.objects:
        if o.type!='MESH':continue
        m=o.evaluated_get(dg).to_mesh();offset=len(vertices);vertices.extend(o.matrix_world@v.co for v in m.vertices);faces.extend(tuple(offset+i for i in p.vertices) for p in m.polygons)
    tree=BVHTree.FromPolygons(vertices,faces,all_triangles=False)
    results={}
    for o in meshes:
        m=o.evaluated_get(dg).data;groups={g.index:g.name for g in o.vertex_groups};entry={}
        for i in range(0,len(m.vertices),20):
            v=o.data.vertices[i];group=groups[max(v.groups,key=lambda g:g.weight).group]
            if 'forearm' in group:continue
            point=o.matrix_world@m.vertices[i].co;hit,normal,face,dist=tree.find_nearest(point)
            signed=(point-hit).dot(normal)
            e=entry.setdefault(group,{'sampled':0,'inside_over_0_05cm':0,'max_depth_cm':0})
            e['sampled']+=1
            if signed<-.05:
                votes=0
                for direction in [Vector((1,.123,.234)).normalized(),Vector((.231,1,.173)).normalized(),Vector((.137,.297,1)).normalized()]:
                    origin=point.copy();hits=0
                    for attempt in range(32):
                        location,n,face,distance=tree.ray_cast(origin,direction,100)
                        if location is None:break
                        hits+=1;origin=location+direction*.0001
                    votes+=hits%2
                if votes>=2:
                    e['inside_over_0_05cm']+=1
                    if -signed>e['max_depth_cm']:e['max_depth_cm']=-signed;e['deepest_point_cm']=list(point);e['nearest_surface_cm']=list(hit)
        results[o.name]=entry
    (DOC/'contact-diagnostic.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results))
    if STAGE=='diagnostic':sys.exit(0)
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
for name,loc,target,scale in [('front',(22,-45,16),(-2,0,-2),38),('reverse',(-5,45,-10),(-3,0,-3),35),('above',(0,0,65),(-2,0,-1),34),('90fov',(-25,14,12),(80,14,12),None)]:
    cam.location=loc;aim(cam,target);cam.data.type='ORTHO' if scale else 'PERSP';cam.data.ortho_scale=scale or 50;cam.data.lens_unit='FOV';cam.data.sensor_fit='HORIZONTAL';cam.data.angle=math.radians(90);cam.data.clip_start=.1;cam.data.clip_end=1000
    scene.render.filepath=str(DOC/'Previews'/f'{STAGE}_{name}.png');bpy.ops.render.render(write_still=True)
print('ART24_'+STAGE.upper()+'_COMPLETE')
