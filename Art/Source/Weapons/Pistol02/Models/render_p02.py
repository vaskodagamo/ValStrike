import bpy,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[4];DOC=REPO/'docs/assets/pistol02';OUT=DOC/'Previews'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'P02_Kestrel.blend'))
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=20;sc.cycles.use_denoising=True
sc.render.resolution_x=1100;sc.render.resolution_y=780;sc.render.resolution_percentage=100
sc.world=bpy.data.worlds.new('Studio');sc.world.use_nodes=True;sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.15,.19,.25,1);sc.world.node_tree.nodes['Background'].inputs[1].default_value=.45
sc.view_settings.view_transform='AgX'
def camera(pos,target,scale):
 d=bpy.data.cameras.new('ReviewCamera');o=bpy.data.objects.new('ReviewCamera',d);sc.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();d.type='ORTHO';d.ortho_scale=scale;sc.camera=o;return o
cam=camera((7,-65,26),(5,0,0),31)
for name,pos,power,size in [('Key',(0,-25,40),120,30),('Fill',(20,25,20),90,25),('Rim',(-20,10,15),80,20)]:
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new(name,d);sc.collection.objects.link(o);o.location=pos;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
# Area lights are expressed in Blender scene coordinates; centimeter studio lighting.
for l in [o for o in sc.objects if o.type=='LIGHT']:l.data.energy*=100
mat=bpy.data.materials.new('Ruler');mat.diffuse_color=(.28,.38,.48,1);mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.28,.38,.48,1);bs.inputs['Roughness'].default_value=.8
ruler=[]
def line(a,b):
 c=bpy.data.curves.new('cm_ruler','CURVE');c.dimensions='3D';c.bevel_depth=.025;s=c.splines.new('POLY');s.points.add(1);s.points[0].co=(*a,1);s.points[1].co=(*b,1);o=bpy.data.objects.new('cm_ruler',c);sc.collection.objects.link(o);c.materials.append(mat);ruler.append(o)
line((-5.5,0,-8.2),(15.5,0,-8.2))
for x in range(22):line((x-5.5,0,-8.2),(x-5.5,0,-7.8 if x%5 else -7.5))
def render(name):
 sc.render.filepath=str(OUT/name);bpy.ops.render.render(write_still=True)
render('P02_three_quarter.png')
cam.location=(5,-65,0);cam.rotation_euler=(Vector((5,0,0))-cam.location).to_track_quat('-Z','Y').to_euler();render('P02_side.png')
for o in ruler:o.hide_render=True
# Existing P01 unchanged, same scene units and a single camera for both weapons.
with bpy.data.libraries.load(str(REPO/'Art/Source/Weapons/Pistol01/Models/P01_Shrike.blend'),link=False) as (src,dst):dst.objects=[n for n in src.objects if n.startswith('SM_VS_Pistol01_')]
old=[]
for o in dst.objects:
 if o:sc.collection.objects.link(o);old.append(o);o.hide_render=False;o.hide_viewport=False;o.location=(0,0,14 if o.name.endswith('_Slide') else 11)
for o in sc.objects:
 if o.name.startswith('SM_VS_Pistol02_'):o.location.z-=11
cam.location=(5,-65,0);cam.data.ortho_scale=48;sc.render.resolution_x=1000;sc.render.resolution_y=1000;render('P01_P02_same_scale.png')
print('STUDIO_RENDERS_DONE',flush=True)
