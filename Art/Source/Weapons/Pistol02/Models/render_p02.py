import bpy,math,json,sys
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
 if '--export-only' in sys.argv and name!='P02_fbx_same_camera.png':return
 sc.render.filepath=str(OUT/name);bpy.ops.render.render(write_still=True)
render('P02_three_quarter.png')
# Same scene/camera/lighting comparison against untouched Meshy output.
corrected = [o for o in sc.objects if o.type == 'MESH']
for o in corrected:o.hide_render=True
before=set(sc.objects)
bpy.ops.import_scene.gltf(filepath=str(ROOT.parent/'Originals/model.glb'))
raw=next(o for o in set(sc.objects)-before if o.type=='MESH')
bpy.ops.object.select_all(action='DESELECT');raw.select_set(True);bpy.context.view_layer.objects.active=raw
scale=21/raw.dimensions.x;raw.scale=(scale,)*3;raw.rotation_mode='XYZ';raw.rotation_euler.z=math.pi;raw.location=(5,0,.2)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
render('P02_original_same_camera.png')
bpy.data.objects.remove(raw,do_unlink=True)
# Final export round-trip, retaining the exact baseline material/light setup.
# Rebind source PBR explicitly: this checks exported geometry/normals, not FBX
# auto-material conversion or an Unreal material import.
source_materials=list(corrected[0].data.materials)
export_objects=[]
for file in sorted((ROOT/'Exports').glob('*.fbx')):
 before=set(sc.objects)
 bpy.ops.import_scene.fbx(filepath=str(file),use_image_search=False)
 imported=list(set(sc.objects)-before);export_objects.extend(imported)
 for o in imported:
  if o.type=='MESH':
   indices=[p.material_index for p in o.data.polygons]
   o.data.materials.clear()
   for mat in source_materials:o.data.materials.append(mat)
   for poly,idx in zip(o.data.polygons,indices):poly.material_index=idx
render('P02_fbx_same_camera.png')
for o in export_objects:bpy.data.objects.remove(o,do_unlink=True)
for o in corrected:o.hide_render=False
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
