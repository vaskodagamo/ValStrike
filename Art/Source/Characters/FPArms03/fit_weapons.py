"""Frozen R02/P02 fit studies; no edits to references, no native acceptance."""
import bpy, math, json,hashlib,sys
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[3];DOC=REPO/'docs/assets/fparms03'
PISTOL_ONLY='--pistol-only' in sys.argv
KNIFE_ONLY='--knife-only' in sys.argv
KINDS=['Knife'] if KNIFE_ONLY else ['Pistol'] if PISTOL_ONLY else ['Rifle','Pistol','Knife']
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'FPArms03.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms03'];meshes=[bpy.data.objects['SK_FPArms03_'+s] for s in ['R','L']]
for kind in KINDS:
    name='A_FPArms03_'+kind+'Hold'
    if name in bpy.data.actions:bpy.data.actions.remove(bpy.data.actions[name])
scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=1280;scene.render.resolution_y=720;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Fit studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.12,.15,.20,1)
def aim(o,t):o.rotation_euler=(Vector(t)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((10,-50,70),110000,65),((-20,40,30),85000,60),((60,0,35),65000,50)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(5,0,0))
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
def render(name,loc,target,ortho=None):
    cam.location=loc;aim(cam,target);cam.data.type='ORTHO' if ortho else 'PERSP';cam.data.ortho_scale=ortho or 50;cam.data.lens_unit='FOV';cam.data.sensor_fit='HORIZONTAL';cam.data.angle=math.radians(90);cam.data.clip_start=.1;cam.data.clip_end=1000
    scene.render.filepath=str(DOC/'Previews'/f'{name}.png');bpy.ops.render.render(write_still=True)
previous=json.loads((DOC/'weapon-fit.json').read_text()) if (PISTOL_ONLY or KNIFE_ONLY) else {}
references=previous.get('references',{});recipes=previous.get('recipes',{})
def load_weapon(kind):
    key={'Rifle':'R02','Pistol':'P02','Knife':'K01'}[kind]
    frozen=json.loads((DOC/'frozen-weapon-sources.json').read_text())[key]
    path=ROOT/'Reference/Frozen'/(key+'.blend');prefix={'Rifle':'SM_VS_Rifle02','Pistol':'SM_VS_Pistol02','Knife':'SM_VS_Knife01'}[kind]
    assert hashlib.sha256(path.read_bytes()).hexdigest()==frozen['sha256']
    with bpy.data.libraries.load(str(path),link=False) as (src,dst):dst.objects=[n for n in src.objects if n.startswith(prefix)]
    objects=[]
    for o in dst.objects:
        if o:
            scene.collection.objects.link(o);o.hide_render=False;o.hide_viewport=False;o.hide_set(False)
            objects.append(o)
    references[kind]={'file':frozen['path'],'sha256':frozen['sha256'],'frozen_commit':frozen['commit'],'objects':[o.name for o in objects]}
    return objects
def reset():
    arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4);b.rotation_mode='XYZ'
    bpy.context.view_layer.update()
def set_hold(kind):
    reset()
    for side in ['r','l']:
        for digit in ['index','middle','ring','pinky']:
            angles=[60,74,42]
            if digit=='index' and side=='r' and kind!='Knife':angles=[12,22,12]
            if side=='l' and kind=='Knife':angles=[12,18,8]
            if side=='l' and kind=='Rifle':angles=[25,65,42]
            for i,a in enumerate(angles):arm.pose.bones[f'{digit}_{i+1:02}_{side}'].rotation_euler.x=-math.radians(a)
        for i,a in enumerate([18,32,20] if kind=='Knife' and side=='r' else [8,22,15]):arm.pose.bones[f'thumb_{i+1:02}_{side}'].rotation_euler.x=-math.radians(a)
        arm.pose.bones['thumb_01_'+side].rotation_euler.z=math.radians(-30 if kind=='Knife' and side=='r' else -8 if side=='r' else 8)
    # Component-space recipes are explicit, at unit scale. Wrist is the pivot.
    transforms={'r':((-11,-2.9,-4),90),'l':((23,3,0),180) if kind=='Rifle' else ((-13,4.7,-5),-90)}
    if kind=='Knife':transforms={'r':((-10,-3.4,-1),90),'l':((-8,17,-6),-15)}
    for side,(target,angle) in transforms.items():
        pivot=Vector((24,-13 if side=='r' else 13,0))
        turn=Matrix.Rotation(math.radians(-25 if side=='l' and kind=='Rifle' else 0),4,'Z')
        matrix=Matrix.Translation(Vector(target))@turn@Matrix.Rotation(math.radians(angle),4,'X')@Matrix.Translation(-pivot)
        arm.pose.bones['forearm_'+side].matrix=matrix@arm.data.bones['forearm_'+side].matrix_local
    bpy.context.view_layer.update()
    if kind=='Rifle':
        # Keep the support hand fixed while lowering the forearm end outside the FP frame.
        hand_matrix=arm.pose.bones['hand_l'].matrix.copy()
        wrist=Vector(transforms['l'][0]);direction=Vector((16,-7,14)).normalized()
        elbow=wrist-direction*22.612010205024273
        rotation=Vector((1,0,0)).rotation_difference(direction).to_matrix().to_4x4()
        base=Vector((24-22.612010205024273,13,0))
        matrix=Matrix.Translation(elbow)@rotation@Matrix.Translation(-base)
        arm.pose.bones['forearm_l'].matrix=matrix@arm.data.bones['forearm_l'].matrix_local
        bpy.context.view_layer.update();arm.pose.bones['hand_l'].matrix=hand_matrix;bpy.context.view_layer.update()
    recipes[kind]={'wrist_targets_cm':transforms,'scale':1,'forearm_component_matrices':{s:[list(row) for row in arm.pose.bones['forearm_'+s].matrix] for s in ['r','l']},'hand_component_matrices':{s:[list(row) for row in arm.pose.bones['hand_'+s].matrix] for s in ['r','l']},'note':'Initial source fit only; no accepted production contact or runtime socket.'}
    action=bpy.data.actions.new('A_FPArms03_'+kind+'Hold');action.use_fake_user=True;arm.animation_data.action=action
    for f in [1,30]:
        for b in arm.pose.bones:
            for prop in ['location','rotation_euler','scale']:b.keyframe_insert(data_path=prop,frame=f,group=b.name)
    scene.frame_set(1)
    return action
def export_action(path):
    original=arm.animation_data.action;temporary=original.copy();temporary.name='EXPORT_ONLY_NEUTRAL'
    for layer in temporary.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    curve.keyframe_points.insert(0,1.0 if curve.data_path.endswith('.scale') else 0.0,options={'FAST'});curve.update()
    arm.animation_data.action=temporary;arm.animation_data.action_slot=temporary.slots[0];scene.frame_set(0)
    bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS')
    arm.animation_data.action=original;arm.animation_data.action_slot=original.slots[0];bpy.data.actions.remove(temporary);scene.frame_set(1)

for kind in KINDS:
    weapons=load_weapon(kind);set_hold(kind)
    render('08_'+kind.lower()+'_hold_90fov',(-25,14,12),(80,14,12))
    render('09_'+kind.lower()+'_contact',(22,-45,16),(-2,0,-2),38)
    render('10_'+kind.lower()+'_reverse_contact',(-5,45,-10),(-3,0,-3),35)
    bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
    export_action(ROOT/'Exports'/f'A_FPArms03_{kind}Hold.fbx')
    for o in weapons:bpy.data.objects.remove(o,do_unlink=True)
reset();scene.frame_set(1)
for o in list(bpy.data.objects):
    if o.type in ['CAMERA','LIGHT']:bpy.data.objects.remove(o,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'FPArms03.blend'))
(DOC/'weapon-fit.json').write_text(json.dumps({'references':references,'recipes':recipes,'renderer':'Blender Cycles; 90 degree horizontal FOV; not gameplay','status':'Frozen corrected R02/P02/K01 loaded at source unit scale. Final visual/contact findings recorded in readiness.json; no runtime acceptance.'},indent=2)+'\n')
print('ART22_FROZEN_WEAPON_FIT_SAVED')
