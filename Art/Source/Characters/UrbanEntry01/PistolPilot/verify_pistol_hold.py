"""ART-23 focused saved-pose/export readback and matched source comparison.
Existing character/weapon geometry and material evidence is reused, not rebuilt.
"""
import bpy,sys,json,hashlib,struct,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent;CHAR=ROOT.parent;REPO=CHAR.parents[3];DOC=REPO/'docs/assets/urban-entry01/pistol-pilot';PRE=DOC/'Previews'
sys.path.insert(0,str(CHAR))
from prepare_source import curves,set_action,mesh_points
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01_PistolPilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01'];action=bpy.data.actions['A_UE01_Pilot_PistolHold']
sc.frame_set(1);bpy.context.view_layer.update()
def hash_json(data):return hashlib.sha256(json.dumps(data,separators=(',',':')).encode()).hexdigest()
def action_hash(a):return hash_json([(f.data_path,f.array_index,[(list(k.co),list(k.handle_left),list(k.handle_right),k.interpolation) for k in f.keyframe_points]) for f in curves(a)])
def mesh_hashes(obj):
    m=obj.data;m.calc_loop_triangles()
    return {'vertices':len(m.vertices),'triangles':len(m.loop_triangles),'custom_normals':m.has_custom_normals,
      'positions':hash_json([list(v.co) for v in m.vertices]),'topology':hash_json([list(p.vertices) for p in m.polygons]),
      'normals':hash_json([list(n.vector) for n in m.corner_normals]),
      'uvs':hash_json([(uv.name,[list(t.uv) for t in uv.data]) for uv in m.uv_layers]),
      'weights':hash_json([[(g.group,g.weight) for g in v.groups] for v in m.vertices]),
      'material_indices':hash_json([p.material_index for p in m.polygons])}
def rig_hash(obj):return hash_json([(b.name,b.parent.name if b.parent else None,[list(r) for r in b.matrix_local]) for b in obj.data.bones])
def material_hash(mat):
    nodes=[]
    for n in mat.node_tree.nodes:
        inputs=[]
        for i in n.inputs:
            if hasattr(i,'default_value'):
                v=i.default_value
                try:v=list(v)
                except TypeError:pass
                if isinstance(v,(str,int,float,bool,list)):inputs.append((i.name,v))
        im=None
        if n.type=='TEX_IMAGE' and n.image:
            image=n.image
            data=bytes(image.packed_file.data) if image.packed_file else Path(bpy.path.abspath(image.filepath)).read_bytes()
            im=(hashlib.sha256(data).hexdigest(),image.colorspace_settings.name)
        nodes.append((n.name,n.type,inputs,im))
    return hash_json((nodes,[(l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name) for l in mat.node_tree.links]))
report={'task_id':'ART-23','blender':bpy.app.version_string,'source_reopened':True,'frame_range':[1,61],'fps':60,'root_motion':False,'root':'pelvis','preservation':{},'weapons':{},'views':{}}
# Append only the intact baseline character and armature for a matched same-pose
# comparison. Bones and mesh data must match, not just their counts.
with bpy.data.libraries.load(str(CHAR/'UrbanEntry01_Pilot.blend'),link=False) as (src,dst):dst.objects=['SKEL_UE01','SK_VS_UrbanEntry01'];dst.actions=list(src.actions)
base_arm,base_mesh=dst.objects
for o in (base_arm,base_mesh):sc.collection.objects.link(o)
assert mesh_hashes(base_mesh)==mesh_hashes(mesh)
assert rig_hash(base_arm)==rig_hash(arm)
assert material_hash(base_mesh.data.materials[0])==material_hash(mesh.data.materials[0])
base_actions={a.name.rsplit('.',1)[0] if a.name[-4:-3]=='.' else a.name:a for a in dst.actions}
original_actions={}
for a in dst.actions:
    name=a.name[:-4] if a.name[-4:-3]=='.' else a.name
    original_actions[name]=action_hash(a)
    assert action_hash(bpy.data.actions[name])==original_actions[name],name
report['preservation']={'character_mesh':mesh_hashes(mesh),'rig_hash':rig_hash(arm),'material_hash':material_hash(mesh.data.materials[0]),'original_action_hashes':original_actions,'all_equal_to_frozen_pilot_source':True}
set_action(base_arm,action);base_mesh.hide_render=True
# Normalized source points are sampled only for this constant pose.
refs={}
for f in (1,31,61):
    sc.frame_set(f);refs[f]={'joints':{b.name:arm.matrix_world@b.head for b in arm.pose.bones},'verts':mesh_points(mesh)}
sc.frame_set(1)
assert max((refs[61]['joints'][n]-p).length for n,p in refs[1]['joints'].items())<.001
# Import actual existing character FBX once, then only the newly authored animation.
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(CHAR/'Exports/SK_VS_UrbanEntry01.fbx'),use_image_search=False)
added=set(bpy.data.objects)-before;rig=next(o for o in added if o.type=='ARMATURE');skin=next(o for o in added if o.type=='MESH')
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/A_UE01_Pilot_PistolHold.fbx'),use_image_search=False,anim_offset=0)
animated=set(bpy.data.objects)-before;assert not any(o.type=='MESH' for o in animated)
import_arm=next(o for o in animated if o.type=='ARMATURE');import_action=import_arm.animation_data.action
names=set(arm.data.bones.keys());assert len(names)==66 and set(rig.data.bones.keys())==names and set(import_arm.data.bones.keys())==names
assert all((b.parent.name if b.parent else None)==(rig.data.bones[b.name].parent.name if rig.data.bones[b.name].parent else None) for b in import_arm.data.bones)
rest_error=max(abs(x-y) for n in names for a,b in zip(import_arm.data.bones[n].matrix_local,rig.data.bones[n].matrix_local) for x,y in zip(a,b));assert rest_error<.001
assert list(map(round,import_action.frame_range))==[1,61] and sc.render.fps==60
set_action(rig,import_action);joint_error=vertex_error=0
for f,ref in refs.items():
    sc.frame_set(f)
    joint_error=max(joint_error,max((rig.matrix_world@rig.pose.bones[n].head-p).length for n,p in ref['joints'].items()))
    vertex_error=max(vertex_error,max((a-b).length for a,b in zip(mesh_points(skin),ref['verts'])))
assert joint_error<.01 and vertex_error<.01
report['roundtrip']={'animation_only':True,'bone_count':66,'rest_matrix_max_error':rest_error,'sampled_frames':[1,31,61],'max_joint_error_cm':joint_error,'max_vertex_error_cm':vertex_error,'loop_max_joint_delta_cm':max((refs[61]['joints'][n]-p).length for n,p in refs[1]['joints'].items()),'material_note':'Original character material rebound to the imported character for matched shading, not automatic native FBX shader acceptance.'}
skin.data.materials.clear();skin.data.materials.append(mesh.data.materials[0]);skin.hide_render=True
for o in animated:bpy.data.objects.remove(o,do_unlink=True)
sc.frame_set(1)
# Existing weapon objects are read-only appended references. Finger and weapon
# scales must stay 1; distinguish source head-local matrices from native sockets.
assert max(abs(s-1) for b in arm.pose.bones for s in b.scale)<.0001
for ident in ('P01','P02'):
    col=bpy.data.collections['PREVIEW_ONLY_'+ident];col.hide_viewport=False;bpy.context.view_layer.update()
    body=bpy.data.objects['SM_VS_Pistol'+ident[1:]+'_Body'];world=body.matrix_world.copy();h=arm.matrix_world@arm.pose.bones['hand_r'].matrix
    report['weapons'][ident]={'hand_r_component_matrix':[list(r) for r in h],'weapon_component_matrix':[list(r) for r in world],
      'hand_r_local_matrix':[list(r) for r in h.inverted()@world],'scale':list(world.to_scale()),
      'bone_references_weapon_cm':{n:list(world.inverted()@(arm.matrix_world@arm.pose.bones[n].head)) for n in ['hand_r','hand_l','index_01_r','index_03_r','thumb_03_r','index_03_l','thumb_03_l']}}
    assert max(abs(s-1) for s in world.to_scale())<.0001
# Preview scenes keep identical source studio lights, exposure, colors and normals.
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True;sc.render.threads_mode='FIXED';sc.render.threads=2
sc.render.resolution_percentage=100;cam=sc.camera;cam.data.clip_end=3000
sc.view_settings.view_transform='AgX'
def shot(name,pos,target,size=None,distance=False):
    sc.render.resolution_x,sc.render.resolution_y=(1920,1080) if distance else (900,800)
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    if distance:cam.data.type='PERSP';cam.data.sensor_fit='HORIZONTAL';cam.data.sensor_width=36;cam.data.lens=18
    else:cam.data.type='ORTHO';cam.data.ortho_scale=size
    report['views'][name]={'position_cm':list(cam.location),'target_cm':list(target),'orthographic_scale_cm':size,'horizontal_fov_degrees':90 if distance else None,'resolution':[sc.render.resolution_x,sc.render.resolution_y],'source_only':True}
    sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
for ident in ('P01','P02'):
    for key in ('P01','P02'):bpy.data.collections['PREVIEW_ONLY_'+key].hide_render=key!=ident
    mesh.hide_render=False;skin.hide_render=True;base_mesh.hide_render=True
    shot(ident+'_candidate_body',(270,-430,180),(12,0,85),195)
    shot(ident+'_candidate_right',(90,-100,148),(37,0,133),42)
    shot(ident+'_candidate_left',(90,100,148),(37,0,133),42)
    target=Vector((12,0,85));shot(ident+'_distance_5m',target+Vector((.53,-.848,0)).normalized()*500,target,distance=True)
    mesh.hide_render=True;base_mesh.hide_render=False
    shot(ident+'_original_same_pose',(90,-100,148),(37,0,133),42)
    base_mesh.hide_render=True;skin.hide_render=False
    shot(ident+'_reimport_right',(90,-100,148),(37,0,133),42)
    shot(ident+'_reimport_left',(90,100,148),(37,0,133),42)
(DOC/'source-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART23_SAVED_SOURCE_AND_ANIMATION_CHECKED',flush=True)
