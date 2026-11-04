"""Real Blender source compositions. Frozen character is read only; no pose edits."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[3];DOC=REPO/'docs/assets/rifle02';OUT=DOC/'Previews'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'R02_Compact.blend'))
sc=bpy.context.scene;sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True
sc.render.resolution_x=1400;sc.render.resolution_y=900;sc.render.resolution_percentage=100
sc.world=bpy.data.worlds.new('R02_Studio');sc.world.use_nodes=True;sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.24,.27,.31,1)
sc.view_settings.view_transform='AgX';sc.render.image_settings.file_format='PNG'
body=bpy.data.objects['SM_VS_Rifle02_Body'];mag=bpy.data.objects['SM_VS_Rifle02_Magazine'];parts=[body,mag];base={o.name:o.matrix_world.copy() for o in parts}
bpy.ops.object.camera_add();cam=bpy.context.object;cam.name='R02_ReviewCamera';sc.camera=cam
lights=[]
for loc,power,size in [((20,-70,100),230000,100),((10,70,60),180000,80),((-80,-40,25),80000,70)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(Vector((15,0,0))-l.location).to_track_quat('-Z','Y').to_euler();lights.append(l)
labelmat=bpy.data.materials.new('Review label');labelmat.use_nodes=True;bs=labelmat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.8,.86,.95,1);bs.inputs['Emission Color'].default_value=(.8,.86,.95,1);bs.inputs['Emission Strength'].default_value=1
labels=[];rulers=[];cameras={}
def label(txt,x,y,size=1.5):
 c=bpy.data.curves.new('Blender composition label','FONT');c.body=txt;c.size=size
 o=bpy.data.objects.new('LABEL',c);sc.collection.objects.link(o);o.data.materials.append(labelmat);o.parent=cam;o.location=(x,y,-100);labels.append(o);return o
def clearlabels():
 for o in labels:bpy.data.objects.remove(o,do_unlink=True)
 labels.clear()
def camera(loc,target,ortho=100):
 cam.location=loc;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=ortho

def render(name):
 cameras[name]={'location_cm':list(cam.location),'rotation_xyz_deg':[math.degrees(x) for x in cam.rotation_euler],'type':cam.data.type,'ortho_scale_cm':cam.data.ortho_scale,'horizontal_fov_deg':math.degrees(cam.data.angle_x),'resolution':[sc.render.resolution_x,sc.render.resolution_y]}
 sc.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
def line(name,points,r=.07):
 c=bpy.data.curves.new(name,'CURVE');c.dimensions='3D';c.bevel_depth=r;c.bevel_resolution=0;s=c.splines.new('POLY');s.points.add(len(points)-1)
 for p,v in zip(s.points,points):p.co=(*v,1)
 o=bpy.data.objects.new(name,c);sc.collection.objects.link(o);o.data.materials.append(labelmat);rulers.append(o);return o
def ruler(xmin,xmax,z,y=-7):
 line('cm_ruler',[(xmin,y,z),(xmax,y,z)])
 for x in [xmin]+list(range(math.ceil(xmin/10)*10,math.floor(xmax/10)*10+1,10))+[xmax]:line('cm_tick',[(x,y,z-.65),(x,y,z+.65)])
# Actual side, width/part separation evident in three-quarter.
camera((14,-160,2),(14,0,2),90);ruler(-23,52,-15)
label('R02  /  CLEANED MESHY SOURCE  /  BLENDER',-42,25,1.5);label('75.00 cm   |   6.60 cm wide   |   25.45 cm high   |   ruler ticks 10 cm',-42,-24,1.2);render('R02_side')
for o in rulers:o.hide_render=True
clearlabels();camera((94,-135,70),(14,0,1),94);label('R02  /  THREE-QUARTER  /  BLENDER',-44,27,1.5);label('Separate magazine  |  packed PBR textures  |  centimeter source',-44,-26,1.2);render('R02_threequarter')
# R01 and R02 in one orthographic scene, identical scale and camera.
clearlabels()
with bpy.data.libraries.load(str(REPO/'Art/Source/Weapons/Rifle01/Models/R01_Kestrel.blend'),link=False) as (data,to):to.objects=[n for n in data.objects if n in ['SM_VS_Rifle01_Body','SM_VS_Rifle01_Magazine']]
old=to.objects
for o in old:
 sc.collection.objects.link(o);o.hide_render=False;o.hide_set(False);o.location=(14,0,25) if 'Magazine' in o.name else (0,0,23)
for o in parts:o.location.z-=23
camera((14,-200,0),(14,0,0),110);sc.render.resolution_y=1400
label('SAME CAMERA / SAME CENTIMETERS / BLENDER',-52,49,1.6);label('R01  86.00 x 9.04 x 32.20 cm',-52,43,1.5);label('R02  75.00 x 6.60 x 25.45 cm',-52,-5,1.5)
ruler(-30,56,3);ruler(-23,52,-38);render('R01_R02_same_scale')
for o in old:o.hide_render=True
for o in rulers:o.hide_render=True
for o in parts:o.matrix_world=base[o.name]
# Append only frozen character objects/actions from our staged copy.
clearlabels();sc.render.resolution_y=1000
charpath=REPO/'Artifacts/Meshy/R02/character-reference/UrbanEntry01.blend'
with bpy.data.libraries.load(str(charpath),link=False) as (data,to):
 to.objects=['SKEL_UE01','SK_VS_UrbanEntry01'];to.actions=['A_UE01_Walk_Backward_While_Shooting']
for o in to.objects:sc.collection.objects.link(o);o.hide_render=False;o.hide_set(False)
arm=next(o for o in to.objects if o.type=='ARMATURE');char=next(o for o in to.objects if o.type=='MESH');act=to.actions[0]
arm.animation_data_create();arm.animation_data.action=act;arm.animation_data.action_slot=act.slots[0];sc.frame_set(1)
# Resolve staged texture paths without touching the frozen source directory.
for mat in char.data.materials:
 for n in mat.node_tree.nodes:
  if n.type=='TEX_IMAGE' and n.image:
   p=REPO/'Artifacts/Meshy/R02/character-reference/Textures'/Path(n.image.filepath).name
   if p.exists():n.image.filepath=str(p);n.image.reload()
points={p.name:arm.matrix_world@p.head for p in arm.pose.bones}
# Rigid alignment: seat the slim grip in the existing right palm. No bone edits.
right=(points['middle_01_r']+points['middle_03_r_end'])/2
local_grip=Vector((-3.5,0,-5.4));offset=right-local_grip
M=Matrix.Translation(offset)
for o in parts:o.matrix_world=M@base[o.name]
for l in lights:l.location+=Vector((0,0,110))
bpy.context.view_layer.update()
# Diagnostic distances to the actual body mesh, not a pose acceptance claim.
verts=[body.matrix_world@v.co for v in body.data.vertices];polys=[list(p.vertices) for p in body.data.polygons];tree=BVHTree.FromPolygons(verts,polys)
landmarks={n:{'world_cm':list(p),'nearest_body_surface_cm':tree.find_nearest(p)[3]} for n,p in points.items() if n in ['hand_r','hand_l','middle_01_r','middle_03_r_end','index_03_r_end','middle_01_l','middle_03_l_end','thumb_03_r_end']}
local_shoulder=Vector((-23,0,6));stockworld=M@local_shoulder
fit={'character_reference':'ART-13 frozen 170 cm source, unchanged','action':act.name,'frame':1,'fps':60,'character_scale':[1,1,1],'weapon_world_location_cm':list(offset),'weapon_world_rotation_xyz_deg':[0,0,0],'weapon_scale':[1,1,1],'hand_r_bone_relative_matrix_rows':[list(row) for row in ((arm.matrix_world@arm.pose.bones['hand_r'].matrix).inverted()@M)],'grip_palm_target_world_cm':list(right),'grip_contact_local_cm':list(local_grip),'stock_contact_world_cm':list(stockworld),'right_upperarm_head_world_cm':list(points['upperarm_r']),'stock_to_upperarm_head_cm':(stockworld-points['upperarm_r']).length,'landmarks':landmarks,'limitation':'Unmodified supplied moving clip: numerical contact placement only, fingers/shoulder are not reposed. Review renders for visible gaps/intersections. No animation or native fit pass.'}
(DOC/'character-fit.json').write_text(json.dumps(fit,indent=2)+'\n')
camera((250,-300,155),(12,-8,80),210);sc.render.resolution_x=1200;sc.render.resolution_y=1200
label('R02 / FROZEN 170 cm CHARACTER / BLENDER',-98,94,3.1);label('Supplied shooting clip, frame 1. Pose unchanged.',-98,-91,2.5);label('Contact study: shoulder/finger fit remains for animation review.',-98,-97,2.2)
line('170_cm_reference',[(-42,-5,0),(-42,-5,170)],.2);line('0_cm',[(-47,-5,0),(-37,-5,0)],.2);line('170_cm',[(-47,-5,170),(-37,-5,170)],.2);render('R02_full_character')
for o in rulers:o.hide_render=True
clearlabels();sc.render.resolution_x=1400;sc.render.resolution_y=950
camera((75,-110,162),(25,-18,141),65)
label('R02 / ACTUAL HAND CONTACT / BLENDER',-30,19,1.3);label('Frozen pose; no hand or character scaling. Gaps are visible evidence.',-30,-20,1.0);render('R02_grip_contact')
# First-person composition of the real weapon only, no invented arms.
char.hide_render=True;arm.hide_render=True
for o in parts:o.matrix_world=base[o.name]
for l in lights:l.location-=Vector((0,0,110))
clearlabels();sc.render.resolution_x=1400;sc.render.resolution_y=788
cam.data.type='PERSP';cam.data.lens=18;cam.data.sensor_width=36;cam.data.sensor_fit='HORIZONTAL';cam.location=(-30,12,19);cam.rotation_euler=Vector((1,0,0)).to_track_quat('-Z','Y').to_euler()
label('R02 / 90 DEG HORIZONTAL FOV / BLENDER COMPOSITION',-93,47,2.2);label('Source weapon only. Native camera, arms, recoil and equip still pending.',-93,41,1.8);render('R02_firstperson_90fov')
(DOC/'preview-cameras.json').write_text(json.dumps(cameras,indent=2)+'\n')
print('R02_PREVIEWS_COMPLETE',flush=True)
