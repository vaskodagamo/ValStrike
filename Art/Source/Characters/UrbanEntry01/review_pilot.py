"""Reopen the changed ART-15 source, inspect preservation and render new fit evidence.
No original clips rerendered; no Unreal operations. Run after prepare_pilot.py.
"""
import bpy,sys,json,hashlib,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;sys.path.insert(0,str(ROOT))
from prepare_source import curves,set_action
REPO=ROOT.parents[3];DOC=REPO/'docs/assets/urban-entry01';PRE=DOC/'Previews/Pilot'
def digest(action):
    values=[(f.data_path,f.array_index,[(tuple(k.co),tuple(k.handle_left),tuple(k.handle_right),k.interpolation) for k in f.keyframe_points]) for f in curves(action)]
    return hashlib.sha256(json.dumps(values).encode()).hexdigest()
def rig_data(arm):
    return [(b.name,b.parent.name if b.parent else None,[list(r) for r in b.matrix_local]) for b in arm.data.bones]
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01.blend'))
original={a.name:digest(a) for a in bpy.data.actions};original_rig=rig_data(bpy.data.objects['SKEL_UE01'])
original_mesh=bpy.data.objects['SK_VS_UrbanEntry01']
verts=[list(v.co) for v in original_mesh.data.vertices]
weights=[[(g.group,g.weight) for g in v.groups] for v in original_mesh.data.vertices]
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01_Pilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01']
assert original_rig==rig_data(arm)
assert verts==[list(v.co) for v in mesh.data.vertices]
assert weights==[[(g.group,g.weight) for g in v.groups] for v in mesh.data.vertices]
assert all(digest(bpy.data.actions[name])==value for name,value in original.items())
report=json.loads((DOC/'pilot-source-validation.json').read_text())
report['saved_pilot_reopened']=True;report['unchanged_original_action_fingerprints']=original
report['saved_pilot_bind_mesh_weights_identical']=True
report['texture_reload']=[]
for img in bpy.data.images:
    if img.source=='FILE':
        path=Path(bpy.path.abspath(img.filepath));assert path.is_file();img.reload();assert img.size[:]==(4096,4096)
        report['texture_reload'].append({'path':str(path.relative_to(REPO)),'size':list(img.size),'space':img.colorspace_settings.name})
set_action(arm,bpy.data.actions['A_UE01_Pilot_RifleHold']);sc.frame_set(1)
body=bpy.data.objects['PREVIEW_ONLY_SM_VS_Rifle01_Body'];bpy.context.view_layer.update()
weapon=body.matrix_world.copy();expected=arm.matrix_world@arm.pose.bones['hand_r'].matrix
local=expected.inverted()@weapon
assert max(abs(x-y) for a,b in zip(report['socket']['bone_local_matrix'],local) for x,y in zip(a,b))<.001
report['socket']['translation_cm']=list(local.translation)
report['socket']['quaternion_wxyz']=list(local.to_quaternion())
report['socket']['hold_hand_r_component_matrix']=[list(r) for r in expected]
report['socket']['support_hand_reference_in_rifle_cm']={n:list(weapon.inverted()@(arm.matrix_world@arm.pose.bones[n].head)) for n in ['hand_l','index_01_l','middle_01_l','ring_01_l','thumb_01_l']}
report['socket']['right_hand_reference_in_rifle_cm']={n:list(weapon.inverted()@(arm.matrix_world@arm.pose.bones[n].head)) for n in ['hand_r','index_01_r','middle_01_r','ring_01_r','thumb_01_r']}
# Changed gait only: verify original pelvis/leg F-curves retained exactly, and
# hand-to-rifle relation on representative alternating strides (not full resample).
walk=bpy.data.actions['A_UE01_Pilot_Walk_RifleHold'];source=bpy.data.actions['A_UE01_Walking']
upper={'spine_01'}|{b.name for b in arm.data.bones['spine_01'].children_recursive}
def lower_data(action):
    return [(f.data_path,f.array_index,[tuple(k.co) for k in f.keyframe_points]) for f in curves(action) if f.data_path.split('"')[1] not in upper]
assert lower_data(walk)==lower_data(source)
set_action(arm,walk);handrefs=[]
for f in (1,16,32,47,63):
    sc.frame_set(f);bpy.context.view_layer.update()
    handrefs.append(body.matrix_world.inverted()@(arm.matrix_world@arm.pose.bones['hand_l'].head))
report['walk_lower_body_curves_identical']=True
report['max_sampled_support_wrist_drift_relative_rifle_cm']=max((p-handrefs[0]).length for p in handrefs)
assert report['max_sampled_support_wrist_drift_relative_rifle_cm']<.001
# One PBR fit sheet and calibrated camera-distance views, only new pose.
set_action(arm,bpy.data.actions['A_UE01_Pilot_RifleHold']);sc.frame_set(1)
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True
sc.render.resolution_x=800;sc.render.resolution_y=900;sc.render.resolution_percentage=100
camera=sc.camera;camera.data.type='ORTHO';camera.data.ortho_scale=195
camera.location=(270,-430,180);camera.rotation_euler=(Vector((10,0,85))-camera.location).to_track_quat('-Z','Y').to_euler()
sc.render.filepath=str(PRE/'hold_pbr.png');bpy.ops.render.render(write_still=True)
# 90-degree horizontal FOV, 1920x1080, view centered at torso; distance in cm.
sc.render.resolution_x=1920;sc.render.resolution_y=1080
camera.data.type='PERSP';camera.data.sensor_fit='HORIZONTAL';camera.data.sensor_width=36;camera.data.lens=18
report['camera_distance_views']={'resolution':[1920,1080],'horizontal_fov_degrees':90,'distance_cm':[500,1000],'renderer':'Blender Cycles; source studio only, no runtime claim','target_cm':[10,0,85]}
for distance in (500,1000):
    target=Vector((10,0,85));camera.location=target+Vector((.53,-.848,0)).normalized()*distance
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    sc.render.filepath=str(PRE/('distance_%dm.png'%(distance//100)));bpy.ops.render.render(write_still=True)
# Reuse ART-13 file hashes rather than reimporting unchanged animations.
rows=[]
for row in json.loads((DOC/'source_preparation.json').read_text())['exports']:
    path=ROOT/'Exports'/row['file'];sha=hashlib.sha256(path.read_bytes()).hexdigest()
    assert sha==row['sha256']
    rows.append({'path':str(path.relative_to(REPO)),'sha256':sha,'bytes':path.stat().st_size})
intake=json.loads((REPO/'docs/assets/meshy-character-intake.json').read_text())
for role,suffix in [('BaseColor',''),('Roughness','_roughness'),('Metallic','_metallic')]:
    path=ROOT/'Textures'/('T_UE01_'+role+'.png');sha=hashlib.sha256(path.read_bytes()).hexdigest()
    original=next(r for r in intake['extracted_files'] if Path(r['file']).name=='Meshy_AI_texture_0'+suffix+'.png')
    assert sha==original['sha256']
    rows.append({'path':str(path.relative_to(REPO)),'sha256':sha,'bytes':path.stat().st_size})
report['unchanged_original_exports_and_textures']=rows
(DOC/'pilot-source-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('PILOT_REOPEN_PRESERVATION_FIT_CHECKED',flush=True)
