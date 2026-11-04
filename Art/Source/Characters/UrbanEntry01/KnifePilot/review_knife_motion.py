"""Focused ART-27 source preservation, actual FBX readback, and pose review.
Reuses ART-23 fingerprints and ART-15 export/readback approach. No engine work.
"""
import bpy,sys,json,hashlib,math
from pathlib import Path
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent;CHAR=ROOT.parent;REPO=CHAR.parents[3]
DOC=REPO/'docs/assets/urban-entry01/knife-pilot';PRE=DOC/'Previews'
sys.dont_write_bytecode=True
sys.path.insert(0,str(CHAR))
from prepare_source import set_action,curves,mesh_points
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01_KnifePilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01'];knife=bpy.data.objects['PREVIEW_ONLY_K01']
contract=json.loads((DOC/'animation-contract.json').read_text());socket=Matrix(contract['socket']['matrix'])
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

report={'task':'ART-27','blender':bpy.app.version_string,'source_reopened':True,'preservation':{},'clips':{},'views':{},'native_runtime':'pending'}
if '--attacks-only' in sys.argv:report=json.loads((DOC/'source-validation.json').read_text())
with bpy.data.libraries.load(str(CHAR/'UrbanEntry01_Pilot.blend'),link=False) as (src,dst):dst.objects=['SKEL_UE01','SK_VS_UrbanEntry01'];dst.actions=list(src.actions)
base_arm,base_mesh=dst.objects
for o in dst.objects:sc.collection.objects.link(o)
assert mesh_hashes(base_mesh)==mesh_hashes(mesh)
assert rig_hash(base_arm)==rig_hash(arm)
assert material_hash(base_mesh.data.materials[0])==material_hash(mesh.data.materials[0])
original_actions={}
for a in dst.actions:
    name=a.name[:-4] if a.name[-4:-3]=='.' else a.name
    original_actions[name]=action_hash(a);assert action_hash(bpy.data.actions[name])==original_actions[name]
base_mesh.hide_render=True
with bpy.data.libraries.load(str(REPO/'Art/Source/Weapons/Knife01/Knife01.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Knife01']
base_knife=dst.objects[0];sc.collection.objects.link(base_knife);base_knife.hide_render=True
assert mesh_hashes(base_knife)==mesh_hashes(knife)
assert material_hash(base_knife.data.materials[0])==material_hash(knife.data.materials[0])
report['preservation']={'character_mesh':mesh_hashes(mesh),'rig_hash':rig_hash(arm),'material_hash':material_hash(mesh.data.materials[0]),'original_action_hashes':original_actions,'knife_mesh':mesh_hashes(knife),'knife_material_hash':material_hash(knife.data.materials[0]),'all_equal_to_preserved_sources':True}
# Existing original character export, imported once. Animations are armature-only.
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(CHAR/'Exports/SK_VS_UrbanEntry01.fbx'),use_image_search=False)
added=set(bpy.data.objects)-before;rig=next(o for o in added if o.type=='ARMATURE');skin=next(o for o in added if o.type=='MESH')
skin.data.materials.clear();skin.data.materials.append(mesh.data.materials[0]);skin.hide_render=True
imported={}
upper={'spine_01'}|{b.name for b in arm.data.bones['spine_01'].children_recursive}
def lower_signature(a):
    return hash_json([(f.data_path,f.array_index,[(list(k.co),list(k.handle_left),list(k.handle_right),k.interpolation) for k in f.keyframe_points]) for f in curves(a) if f.data_path.split('"')[1] not in upper])
# Surface diagnostics on actual deformed geometry at every exported frame.
# BVH surface crossings are evidence; not a continuous-time collision guarantee.
knife_polys=[list(p.vertices) for p in knife.data.polygons]
blade_polys=[p for p in knife_polys if all(knife.data.vertices[i].co.x>7.3 for i in p)]
guard_polys=[p for p in knife_polys if 5.3<sum(knife.data.vertices[i].co.x for i in p)/len(p)<7.3]
hand_names={g.index for g in mesh.vertex_groups if g.name.endswith('_r') and any(w in g.name for w in ('hand','index','middle','ring','pinky','thumb'))}
hand_ids={v.index for v in mesh.data.vertices if sum(g.weight for g in v.groups if g.group in hand_names)>.5}
char_polys=[list(p.vertices) for p in mesh.data.polygons]
body_polys=[p for p in char_polys if not any(i in hand_ids for i in p)]
hand_polys=[p for p in char_polys if all(i in hand_ids for i in p)]
for name,meta in contract['clips'].items():
    if '--attacks-only' in sys.argv and not name.endswith(('Slash','Stab')):continue
    action=bpy.data.actions[name];set_action(arm,action);start,end=meta['frame_range'];sc.frame_start=start;sc.frame_end=end
    frames=sorted(set([start,(start+end)//2,end]+([meta['impact_frame']] if meta['impact_frame'] else [])))
    refs={};root=[];blade_hits=[];guard_hits=[];body_hits=[];max_scale_error=0
    for fr in range(start,end+1):
        sc.frame_set(fr);root.append(arm.pose.bones['pelvis'].head.copy())
        max_scale_error=max(max_scale_error,max(abs(x-1) for b in arm.pose.bones for x in b.scale))
        if fr in frames:refs[fr]={'joints':{b.name:arm.matrix_world@b.head for b in arm.pose.bones},'verts':mesh_points(mesh)}
        e=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());em=e.to_mesh();ps=[mesh.matrix_world@v.co for v in em.vertices]
        cb=BVHTree.FromPolygons(ps,char_polys);body=BVHTree.FromPolygons(ps,body_polys);hb=BVHTree.FromPolygons(ps,hand_polys)
        kp=[knife.matrix_world@v.co for v in knife.data.vertices]
        blade=BVHTree.FromPolygons(kp,blade_polys);guard=BVHTree.FromPolygons(kp,guard_polys);whole=BVHTree.FromPolygons(kp,knife_polys)
        n=len(cb.overlap(blade));g=len(hb.overlap(guard));b=len(body.overlap(whole))
        if n:blade_hits.append([fr,n])
        if g:guard_hits.append([fr,g])
        if b:body_hits.append([fr,b])
        e.to_mesh_clear()
    assert max_scale_error<.0001
    if 'lower_body_source' in meta:assert lower_signature(action)==lower_signature(bpy.data.actions[meta['lower_body_source']])
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/(name+'.fbx')),anim_offset=0,use_image_search=False)
    new=set(bpy.data.objects)-before;assert not any(o.type=='MESH' for o in new)
    ar=next(o for o in new if o.type=='ARMATURE');ia=ar.animation_data.action;imported[name]=ia
    assert set(ar.data.bones.keys())==set(rig.data.bones.keys())==set(arm.data.bones.keys())
    assert all((b.parent.name if b.parent else None)==(rig.data.bones[b.name].parent.name if rig.data.bones[b.name].parent else None) for b in ar.data.bones)
    rest=max(abs(x-y) for b in ar.data.bones for row,row2 in zip(b.matrix_local,rig.data.bones[b.name].matrix_local) for x,y in zip(row,row2));assert rest<.001
    assert list(map(round,ia.frame_range))==[start,end] and sc.render.fps==60
    set_action(rig,ia);jerr=verr=0
    for fr,ref in refs.items():
        sc.frame_set(fr);jerr=max(jerr,max((rig.matrix_world@rig.pose.bones[n].head-v).length for n,v in ref['joints'].items()));verr=max(verr,max((v-w).length for v,w in zip(mesh_points(skin),ref['verts'])))
    assert jerr<.01 and verr<.01,(name,jerr,verr)
    seam=max((refs[end]['joints'][n]-v).length for n,v in refs[start]['joints'].items())
    if meta['loop']:assert seam<.01
    report['clips'][name]={**meta,'animation_only':True,'bones':len(ar.data.bones),'rest_matrix_max_error':rest,'sampled_frames':frames,'max_joint_error_cm':jerr,'max_vertex_error_cm':verr,'end_to_start_joint_delta_cm':seam,'pelvis_end_delta_cm':list(root[-1]-root[0]),'max_pose_scale_error':max_scale_error,'lower_body_curves_unchanged':True if 'lower_body_source' in meta else None,'surface_crossings':{'checked_frames':[start,end],'blade_character':blade_hits,'guard_right_hand':guard_hits,'knife_body_excluding_right_hand':body_hits,'limitation':'Discrete 60 Hz triangle surface intersections; not continuous collision or hidden internal penetration proof.'}}
    for o in new:bpy.data.objects.remove(o,do_unlink=True)
    print('ART27_READBACK',name,flush=True)
(DOC/'source-validation.json').write_text(json.dumps(report,indent=2)+'\n')
# Same studio/camera/material for original, authored candidate and actual FBX.
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True;sc.render.threads_mode='FIXED';sc.render.threads=2
sc.render.resolution_percentage=100;sc.view_settings.view_transform='AgX';cam=sc.camera;cam.data.clip_end=3000
anchor=bpy.data.objects['PREVIEW_ONLY_K01_hand_r']
def select_pose(name,frame,kind='candidate'):
    a=bpy.data.actions['A_UE01_Knife_'+name];set_action(arm,a);set_action(base_arm,a);set_action(rig,imported[a.name]);sc.frame_set(frame)
    mesh.hide_render=kind!='candidate';base_mesh.hide_render=kind!='original';skin.hide_render=kind!='reimport'
    anchor.constraints[0].target=rig if kind=='reimport' else arm
    bpy.context.view_layer.update()
    base_knife.matrix_world=knife.matrix_world.copy();base_knife.hide_render=kind!='original';knife.hide_render=kind=='original'
def shot(name,pos,target,size=195,distance=False,res=None):
    sc.render.resolution_x,sc.render.resolution_y=res or ((1920,1080) if distance else (900,800))
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.type='PERSP' if distance else 'ORTHO'
    if distance:cam.data.sensor_fit='HORIZONTAL';cam.data.sensor_width=36;cam.data.lens=18
    else:cam.data.ortho_scale=size
    sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
    report['views'][name]={'camera_cm':list(pos),'target_cm':list(target),'ortho_size_cm':None if distance else size,'horizontal_fov_degrees':90 if distance else None,'source_only':True}
for kind in (() if '--attacks-only' in sys.argv else ('original','candidate','reimport')):
    select_pose('Hold',1,kind);shot('hold_'+kind+'_matched',(90,-100,152),(34,-19,130),43)
if '--attacks-only' not in sys.argv:
    select_pose('Hold',1,'reimport');shot('hold_reimport_reverse',(70,100,155),(34,-19,130),43)
# Actual reimport views at accepted impact and follow-through; same camera.
for clip,frames in [('Equip',[1,11,25]),('Slash',[5,10,16,25,34]),('Stab',[9,19,36,55]),('Walk',[1,16,32,47]),('Run',[1,10,20,30])]:
    if '--attacks-only' in sys.argv and clip not in ('Slash','Stab'):continue
    for fr in frames:
        select_pose(clip,fr,'reimport');shot(clip.lower()+'_%02d'%fr,(270,-430,180),(12,0,85),195,res=(600,700))
for clip,fr in [('Hold',1),('Slash',10),('Stab',19)]:
    if '--attacks-only' in sys.argv and clip not in ('Slash','Stab'):continue
    select_pose(clip,fr,'reimport');target=Vector((12,0,85));shot(clip.lower()+'_5m',target+Vector((.53,-.848,0)).normalized()*500,target,distance=True)
# A second angle makes the stab extension and slash side separation assessable.
for clip,fr in [('Slash',10),('Slash',16),('Stab',19)]:
    select_pose(clip,fr,'reimport');shot(clip.lower()+'_side_%02d'%fr,(100,330,170),(20,0,129),100)
report['material_note']='Original PBR rebound to imported character for matched shading; native material/axis acceptance remains pending.'
(DOC/'source-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART27_REVIEW_COMPLETE',flush=True)
