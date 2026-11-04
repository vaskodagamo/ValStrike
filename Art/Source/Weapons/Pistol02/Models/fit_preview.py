"""Read-only frozen character pose preview: no character source or pose edits."""
import bpy,json,math
from mathutils import Vector,Matrix
from pathlib import Path
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[4];DOC=REPO/'docs/assets/pistol02';OUT=DOC/'Previews'
bpy.ops.wm.open_mainfile(filepath=str(REPO/'Artifacts/Meshy/P02/character-reference/UrbanEntry01.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];char=bpy.data.objects['SK_VS_UrbanEntry01']
a=bpy.data.actions['A_UE01_Walk_Backward_with_Gun'];arm.animation_data_create();arm.animation_data.action=a;arm.animation_data.action_slot=a.slots[0];sc.frame_set(1);bpy.context.view_layer.update()
with bpy.data.libraries.load(str(ROOT/'P02_Kestrel.blend'),link=False) as (src,dst):dst.objects=[n for n in src.objects if n.startswith('SM_VS_Pistol02_')]
transform=Matrix.Translation((52,-34.3,112))@Matrix.Rotation(math.radians(-15.4),4,'Z')
parts=[]
for o in dst.objects:
 sc.collection.objects.link(o);o.matrix_world=transform;parts.append(o)
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True
sc.render.resolution_x=900;sc.render.resolution_y=1000;sc.render.resolution_percentage=100
cam=bpy.data.objects['Preview_Front'];sc.camera=cam;cam.data.clip_end=3000
cameras={}
def shot(name,position,target,scale=None,fov=None):
 cam.location=position;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
 if fov:cam.data.type='PERSP';cam.data.lens_unit='FOV';cam.data.angle=math.radians(fov);cam.data.sensor_fit='HORIZONTAL'
 else:cam.data.type='ORTHO';cam.data.ortho_scale=scale
 cameras[name]={'location_cm':list(cam.location),'target_cm':target,'type':cam.data.type,'ortho_scale_cm':scale,'horizontal_fov_degrees':fov,'resolution':[sc.render.resolution_x,sc.render.resolution_y]}
 sc.render.filepath=str(OUT/name);bpy.ops.render.render(write_still=True)
shot('P02_full_character.png',(115,-360,155),(35,-15,77),190)
sc.render.resolution_x=1100;sc.render.resolution_y=780
shot('P02_grip_contact.png',(59,-100,128),(53,-34,111),30)
shot('P02_support_hand_gap.png',(68,-105,140),(64,-29,116),50)
# Weapon-only FPS composition: exact 90-degree horizontal FOV, no fabricated arms.
char.hide_render=True
for o in parts:o.matrix_world=Matrix.Translation((35,9,-8))
sc.render.resolution_x=1280;sc.render.resolution_y=720
shot('P02_first_person_90deg.png',(0,0,0),(100,0,0),fov=90)
# Sample actual source geometry around the palm-grip region.
body=parts[0]
weapon_inv=transform.inverted();bones={n:list(weapon_inv@(arm.matrix_world@arm.pose.bones[n].head)) for n in ['hand_r','hand_l','index_01_r','index_03_r','middle_01_r','middle_03_r','thumb_03_r']}
report={'character_unchanged':True,'character_neutral_height_cm':170,'pose_action':a.name,'pose_frame':1,'weapon_world_matrix':[list(row) for row in transform], 'weapon_world_location_cm':[52,-34.3,112],'weapon_world_rotation_xyz_degrees':[0,0,-15.4], 'hand_r_bone_relative_matrix':[list(row) for row in ((arm.matrix_world@arm.pose.bones['hand_r'].matrix).inverted()@transform)],'bones_in_weapon_grip_frame_cm':bones,'cameras':cameras,'limitations':['Frozen rifle pose reused without bone or mesh edits; not a pistol animation acceptance.','Support hand remains forward of pistol grip; a pistol-specific two-hand pose is required.','First-person image is weapon-only at 90-degree horizontal FOV, not a game screenshot.']}
(DOC/'fit-measurements.json').write_text(json.dumps(report,indent=2)+'\n')
print('FIT_PREVIEW_DONE',flush=True)
