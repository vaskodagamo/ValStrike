"""ART-28 focused saved source, actual animation FBX readback and visual evidence.
Preservation fingerprints and export readback reuse the ART-23/27 approach.
"""
import bpy,sys,json,hashlib,math
from pathlib import Path
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent;CHAR=ROOT.parent;REPO=CHAR.parents[3];DOC=REPO/'docs/assets/urban-entry01/rifle02-pilot';PRE=DOC/'Previews'
sys.dont_write_bytecode=True;sys.path.insert(0,str(CHAR))
from prepare_source import set_action,curves,mesh_points
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01_Rifle02Pilot.blend'))
sc=bpy.context.scene;arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01'];body=bpy.data.objects['PREVIEW_ONLY_SM_VS_Rifle02_Body'];mag=bpy.data.objects['PREVIEW_ONLY_SM_VS_Rifle02_Magazine']
contract=json.loads((DOC/'assembly-contract.json').read_text());socket=Matrix(contract['socket']['matrix'])
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


report={'task':'ART-28','blender':bpy.app.version_string,'source_reopened':True,'preservation':{},'clips':{},'contacts':{},'views':{},'native_runtime':'pending'}
with bpy.data.libraries.load(str(CHAR/'UrbanEntry01_Pilot.blend'),link=False) as (src,dst):dst.objects=['SKEL_UE01','SK_VS_UrbanEntry01'];dst.actions=list(src.actions)
base_arm,base_mesh=dst.objects
for o in dst.objects:sc.collection.objects.link(o)
assert mesh_hashes(base_mesh)==mesh_hashes(mesh);assert rig_hash(base_arm)==rig_hash(arm)
assert material_hash(base_mesh.data.materials[0])==material_hash(mesh.data.materials[0])
original_actions={}
for a in dst.actions:
    name=a.name[:-4] if a.name[-4:-3]=='.' else a.name;original_actions[name]=action_hash(a);assert action_hash(bpy.data.actions[name])==original_actions[name]
base_mesh.hide_render=True
with bpy.data.libraries.load(str(REPO/'Art/Source/Weapons/Rifle02/R02_Compact.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Rifle02_Body','SM_VS_Rifle02_Magazine']
base_parts=dst.objects
for a,b in zip(base_parts,(body,mag)):
    sc.collection.objects.link(a);a.hide_set(False);a.hide_render=True
    assert mesh_hashes(a)==mesh_hashes(b)
    assert [material_hash(m) for m in a.data.materials]==[material_hash(m) for m in b.data.materials]
report['preservation']={'character_mesh':mesh_hashes(mesh),'rig_hash':rig_hash(arm),'material_hash':material_hash(mesh.data.materials[0]),'prior_action_hashes':original_actions,'weapon_parts':{o.name:{'mesh':mesh_hashes(o),'materials':[material_hash(m) for m in o.data.materials]} for o in (body,mag)},'all_equal_to_frozen_sources':True}
before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(CHAR/'Exports/SK_VS_UrbanEntry01.fbx'),use_image_search=False)
new=set(bpy.data.objects)-before;rig=next(o for o in new if o.type=='ARMATURE');skin=next(o for o in new if o.type=='MESH');skin.hide_render=True
skin.data.materials.clear();skin.data.materials.append(mesh.data.materials[0]);imported={}
upper={'spine_01'}|{b.name for b in arm.data.bones['spine_01'].children_recursive}
def lower_hash(a):return hash_json([(f.data_path,f.array_index,[(list(k.co),list(k.handle_left),list(k.handle_right),k.interpolation) for k in f.keyframe_points]) for f in curves(a) if f.data_path.split('"')[1] not in upper])
weapon_tree=BVHTree.FromPolygons([v.co for v in body.data.vertices],[list(p.vertices) for p in body.data.polygons])
stock_ids=[v.index for v in body.data.vertices if v.co.x<-22.8]
head_groups={g.index for g in mesh.vertex_groups if g.name.startswith('head')}
head_ids=[v.index for v in mesh.data.vertices if sum(g.weight for g in v.groups if g.group in head_groups)>.8]
hand_ids={}
for side in ('r','l'):
    groups={g.index for g in mesh.vertex_groups if g.name.endswith('_'+side) and any(w in g.name for w in ('hand','index','middle','ring','pinky','thumb'))}
    hand_ids[side]=[v.index for v in mesh.data.vertices if sum(g.weight for g in v.groups if g.group in groups)>.8]
def contact():
    e=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());em=e.to_mesh();ps=[mesh.matrix_world@v.co for v in em.vertices];cb=BVHTree.FromPolygons(ps,[list(p.vertices) for p in em.polygons]);inv=body.matrix_world.inverted()
    ds=[]
    for i in stock_ids:
        p=body.matrix_world@body.data.vertices[i].co;q,n,idx,d=cb.find_nearest(p);ds.append((d,(p-q).dot(n)))
    result={'stock_rear_sample_count':len(ds),'stock_min_surface_distance_cm':min(d for d,s in ds),'stock_nearest_normal_min_signed_cm':min(s for d,s in ds),'stock_rear_median_distance_cm':sorted(d for d,s in ds)[len(ds)//2],'head_min_vertex_to_rifle_surface_cm':min(weapon_tree.find_nearest(inv@ps[i])[3] for i in head_ids),'hands':{}}
    for side,ids in hand_ids.items():
        vals=[]
        for i in ids:
            p=inv@ps[i];q,n,idx,d=weapon_tree.find_nearest(p);vals.append((d,(p-q).dot(n),list(p)))
        deepest=min(vals,key=lambda x:x[1]);result['hands'][side]={'selected_vertices':len(ids),'nearest_surface_distance_cm':min(v[0] for v in vals),'negative_normal_samples_deeper_than_0_1cm':sum(v[1]<-.1 for v in vals),'max_negative_normal_depth_cm':max(0,-deepest[1]),'deepest_sample_weapon_cm':deepest[2]}
    e.to_mesh_clear();return result
for name,meta in contract['clips'].items():
    a=bpy.data.actions[name];set_action(arm,a);start,end=meta['frame_range']
    frames=[1,31,61] if name.endswith('Hold') else [1,16,32,47,63] if name.endswith('Walk') else [1,10,20,30,39]
    refs={};contacts={}
    for fr in frames:
        sc.frame_set(fr);refs[fr]={'joints':{b.name:arm.matrix_world@b.head for b in arm.pose.bones},'verts':mesh_points(mesh)};contacts[fr]=contact()
        assert max(abs(x-1) for b in arm.pose.bones for x in b.scale)<.0001
    if 'lower_body_source' in meta:assert lower_hash(a)==lower_hash(bpy.data.actions[meta['lower_body_source']])
    before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/(name+'.fbx')),anim_offset=0,use_image_search=False)
    added=set(bpy.data.objects)-before;assert not any(o.type=='MESH' for o in added);ar=next(o for o in added if o.type=='ARMATURE');ia=ar.animation_data.action;imported[name]=ia
    assert set(ar.data.bones.keys())==set(rig.data.bones.keys())==set(arm.data.bones.keys())
    assert all((b.parent.name if b.parent else None)==(rig.data.bones[b.name].parent.name if rig.data.bones[b.name].parent else None) for b in ar.data.bones)
    rest=max(abs(x-y) for b in ar.data.bones for row,row2 in zip(b.matrix_local,rig.data.bones[b.name].matrix_local) for x,y in zip(row,row2));assert rest<.001
    assert list(map(round,ia.frame_range))==[start,end] and sc.render.fps==60
    set_action(rig,ia);je=ve=0
    for fr,ref in refs.items():
        sc.frame_set(fr);je=max(je,max((rig.matrix_world@rig.pose.bones[n].head-v).length for n,v in ref['joints'].items()));ve=max(ve,max((v-w).length for v,w in zip(mesh_points(skin),ref['verts'])))
    assert je<.01 and ve<.01
    seam=max((refs[end]['joints'][n]-v).length for n,v in refs[start]['joints'].items());assert seam<.01
    report['clips'][name]={**meta,'duration_seconds':(end-start)/60,'animation_only':True,'bone_count':len(ar.data.bones),'rest_matrix_max_error':rest,'sampled_frames':frames,'max_joint_error_cm':je,'max_vertex_error_cm':ve,'loop_joint_error_cm':seam,'lower_body_curves_unchanged':True if 'lower_body_source' in meta else None};report['contacts'][name]=contacts
    for o in added:bpy.data.objects.remove(o,do_unlink=True)
    print('ART28_READBACK',name,flush=True)
report['contact_limitations']='Selected vertices/nearest surface-normal signs, not a closed-volume penetration or continuous collision proof. Stock uses actual rear-pad vertices, not shoulder joint distance; visual evidence remains necessary.'
(DOC/'source-validation.json').write_text(json.dumps(report,indent=2)+'\n')
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.use_denoising=True;sc.render.threads_mode='FIXED';sc.render.threads=2;sc.render.resolution_percentage=100;sc.view_settings.view_transform='AgX'
cam=sc.camera;cam.data.clip_end=3000;anchor=bpy.data.objects['PREVIEW_ONLY_R02_hand_r']
def pose(clip,fr,kind='candidate'):
    name='A_UE01_R02_'+clip;set_action(arm,bpy.data.actions[name]);set_action(base_arm,bpy.data.actions[name]);set_action(rig,imported[name]);sc.frame_set(fr)
    mesh.hide_render=kind!='candidate';base_mesh.hide_render=kind!='original';skin.hide_render=kind!='reimport';anchor.constraints[0].target=rig if kind=='reimport' else arm;bpy.context.view_layer.update()
    for a,b in zip(base_parts,(body,mag)):
        a.parent=None;a.matrix_world=b.matrix_world.copy();a.hide_render=kind!='original';b.hide_render=kind=='original'
def shot(name,pos,target,size=100,distance=False,res=(1000,850)):
    sc.render.resolution_x,sc.render.resolution_y=(1920,1080) if distance else res;cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='PERSP' if distance else 'ORTHO'
    if distance:cam.data.sensor_fit='HORIZONTAL';cam.data.sensor_width=36;cam.data.lens=18
    else:cam.data.ortho_scale=size
    sc.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True);report['views'][name]={'position_cm':list(pos),'target_cm':list(target),'ortho_size_cm':None if distance else size,'horizontal_fov_degrees':90 if distance else None,'source_only':True}
for kind in ('original','candidate','reimport'):
    pose('Hold',1,kind);shot(kind+'_right',(70,-170,155),(20,-12,135));shot(kind+'_left',(70,170,155),(20,-12,135))
pose('Hold',1,'reimport')
shot('reimport_grip',(65,-100,138),(20,-14,128),38);shot('reimport_support',(75,110,145),(46,-11,133),42)
shot('reimport_stock_right',(80,-120,150),(-2,-14,134),45);shot('reimport_stock_left',(80,80,180),(-2,-14,134),55)
shot('reimport_grip_reverse',(10,50,90),(19,-14,125),42)
shot('reimport_top',(24,-12,260),(24,-12,132),100)
for clip,frames in [('Hold',[1]),('Walk',[1,16,32,47]),('Run',[1,10,20,30])]:
    for fr in frames:
        pose(clip,fr,'reimport');shot(clip.lower()+'_%02d'%fr,(270,-430,180),(12,0,85),195,res=(600,700))
    pose(clip,frames[1] if len(frames)>1 else 1,'reimport');target=Vector((12,0,85));shot(clip.lower()+'_5m',target+Vector((.53,-.848,0)).normalized()*500,target,distance=True)
# Known defective old fit, rendered only for a matched before/after source view.
pose('Hold',1);set_action(arm,bpy.data.actions['A_UE01_Pilot_RifleHold']);sc.frame_set(1)
old=Matrix(json.loads((REPO/'docs/assets/rifle02/character-fit.json').read_text())['hand_r_bone_relative_matrix_rows']);body.matrix_basis=old;mag.matrix_basis=old@Matrix.Translation((15,0,-2.5));bpy.context.view_layer.update()
shot('before_known_defect',(70,-170,155),(20,-12,135))
report['material_note']='Original PBR rebound to the actual character FBX for matched shading; no new weapon mesh export or native-material verification. Existing R02 export evidence reused unchanged.'
(DOC/'source-validation.json').write_text(json.dumps(report,indent=2)+'\n');print('ART28_REVIEW_COMPLETE',flush=True)
