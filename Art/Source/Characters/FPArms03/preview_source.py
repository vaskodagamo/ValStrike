"""Render actual saved rig deformation. Blender source only, never gameplay."""
import bpy, math, json
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[3]/'docs/assets/fparms03'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'FPArms03.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms03']
scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=1000;scene.render.resolution_y=750;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Source studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.14,.17,.21,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.7
def aim(o,t):o.rotation_euler=(Vector(t)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((20,-50,70),110000,65),((0,40,30),85000,60),((60,0,35),65000,50)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(24,0,0))
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
def action(kind,frame):
    a=bpy.data.actions['A_FPArms03_'+kind];arm.animation_data.action=a;arm.animation_data.action_slot=a.slots[0];scene.frame_set(frame);bpy.context.view_layer.update()
def render(name,loc,target,scale=52,persp=False):
    cam.location=loc;aim(cam,target);cam.data.type='PERSP' if persp else 'ORTHO';cam.data.ortho_scale=scale;cam.data.lens_unit='FOV';cam.data.sensor_fit='HORIZONTAL';cam.data.angle=math.radians(90);cam.data.clip_start=.1;cam.data.clip_end=1000
    scene.render.filepath=str(DOC/'Previews'/f'{name}.png');bpy.ops.render.render(write_still=True)
action('Open',1);render('01_pair_open_dorsal',(22,0,90),(22,0,0),64)
render('02_pair_open_palm',(22,0,-90),(22,0,0),64)
action('Grip',15);render('03_pair_grip',(52,-47,45),(25,0,0),55)
bpy.data.objects['SK_FPArms03_L'].hide_render=True
render('04_right_grip_close',(50,-45,17),(32,-13,-2),26)
render('05_right_grip_palm',(37,-10,-45),(32,-13,-2),26)
bpy.data.objects['SK_FPArms03_L'].hide_render=False
action('Wrist',15);render('06_wrist_twist',(50,-45,35),(24,0,0),55)
action('Open',1);render('07_first_person_open_90fov',(-9,0,14),(40,0,4),persp=True)
print('ART22_DEFORMATION_RENDERS_COMPLETE')
