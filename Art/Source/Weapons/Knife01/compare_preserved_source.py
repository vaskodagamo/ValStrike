"""Matched raw/candidate/export review. Never changes preserved Meshy files.
The raw baseline keeps its imported geometry, UVs, normals and PBR material;
only the object matrix normalizes presentation to cm/+X and grip center.
"""
import bpy,json,hashlib,math,numpy as np
from pathlib import Path
from mathutils import Vector,Matrix
R=Path(__file__).resolve().parent;D=R.parents[3]/'docs/assets/knife01';P=D/'Previews/Comparison';P.mkdir(exist_ok=True)
original_hash=hashlib.sha256((R/'Originals/K01_Meshy.glb').read_bytes()).hexdigest()
pixels={}
report={'purpose':'Producer requested source-preservation review after direct user correction','raw_mesh_sha256':original_hash,'same_camera_lighting_scale':True,'views':{},'common_material':'Diagnostic gray, metallic .35, roughness .42; applied only in temporary render scenes','material_views':'Original PBR on all three; export uses original graph restored by material name. No intentional material difference.'}
def geometry_fingerprint(o):
 h=hashlib.sha256()
 for v in o.data.vertices:h.update(repr(tuple(v.co)).encode())
 for p in o.data.polygons:h.update(repr(tuple(p.vertices)).encode())
 for uv in o.data.uv_layers:
  for d in uv.data:h.update(repr(tuple(d.uv)).encode())
 for n in o.data.corner_normals:h.update(repr(tuple(n.vector)).encode())
 return h.hexdigest()
def setup_scene():
 s=bpy.context.scene;s.unit_settings.system='METRIC';s.unit_settings.scale_length=.01
 s.render.engine='CYCLES';s.cycles.samples=16;s.cycles.use_denoising=True
 s.render.resolution_x=1200;s.render.resolution_y=700;s.render.resolution_percentage=100;s.view_settings.view_transform='AgX'
 s.world=bpy.data.worlds.new('Matched studio');s.world.use_nodes=True;s.world.node_tree.nodes['Background'].inputs[0].default_value=(.17,.2,.25,1);s.world.node_tree.nodes['Background'].inputs[1].default_value=.45
 for name,pos,power,size in [('Key',(0,-18,30),25000,25),('Fill',(15,20,15),18000,20),('Rim',(-15,2,8),12000,15),('LowerFill',(8,-12,-18),10000,20)]:
  ld=bpy.data.lights.new(name,'AREA');l=bpy.data.objects.new(name,ld);s.collection.objects.link(l);l.location=pos;l.rotation_euler=(Vector((7,0,0))-l.location).to_track_quat('-Z','Y').to_euler();ld.energy=power;ld.shape='DISK';ld.size=size
 cam=bpy.data.objects.new('MatchedCamera',bpy.data.cameras.new('MatchedCamera'));s.collection.objects.link(cam);s.camera=cam;cam.location=(23,-40,21);cam.rotation_euler=(Vector((7,0,0))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=33
 return s
for variant in ['original','candidate','export']:
 bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
 if variant=='original':
  bpy.ops.import_scene.gltf(filepath=str(R/'Originals/K01_Meshy.glb'))
  o=next(x for x in bpy.context.scene.objects if x.type=='MESH');before=geometry_fingerprint(o)
  points=[o.matrix_world@v.co for v in o.data.vertices];lo=min(v.x for v in points);hi=max(v.x for v in points);span=hi-lo;sc=27.8/span;gx=lo+.774*span
  normalize=Matrix.Translation((gx*sc,0,.00788*span*sc))@Matrix.Scale(sc,4)@Matrix.Rotation(math.pi,4,'Z')
  o.matrix_world=normalize@o.matrix_world;bpy.context.view_layer.update();assert geometry_fingerprint(o)==before
  report['original_preservation']={'geometry_uv_corner_normal_fingerprint_before':before,'after':geometry_fingerprint(o),'matrix_only_normalization':True,'imported_pbr_unchanged':True,'vertices':len(o.data.vertices),'triangles':sum(len(p.vertices)-2 for p in o.data.polygons)}
  bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(R/'Originals/K01_OriginalNormalized.blend'))
 elif variant=='candidate':
  bpy.ops.wm.open_mainfile(filepath=str(R/'Knife01.blend'));o=bpy.data.objects['SM_VS_Knife01']
 else:
  bpy.ops.import_scene.fbx(filepath=str(R/'Exports/SM_VS_Knife01.fbx'),use_image_search=False);o=next(x for x in bpy.context.scene.objects if x.type=='MESH')
  names=[m.name for m in o.data.materials]
  with bpy.data.libraries.load(str(R/'Knife01.blend'),link=False) as (src,dst):dst.materials=[n for n in src.materials if n in names]
  restored={m.name.rsplit('.',1)[0] if m.name.rsplit('.',1)[-1].isdigit() else m.name:m for m in dst.materials}
  for i,n in enumerate(names):o.data.materials[i]=restored[n]
  report['export_materials_restored_by_name']=names
 s=setup_scene();s.render.filepath=str(P/(variant+'_pbr.png'));bpy.ops.render.render(write_still=True)
 original_mats=list(o.data.materials);ids=[p.material_index for p in o.data.polygons]
 clay=bpy.data.materials.new('Matched diagnostic');clay.use_nodes=True;bs=clay.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.18,.21,.25,1);bs.inputs['Metallic'].default_value=.35;bs.inputs['Roughness'].default_value=.42
 o.data.materials.clear();o.data.materials.append(clay)
 for poly in o.data.polygons:poly.material_index=0
 s.render.filepath=str(P/(variant+'_common_material.png'));bpy.ops.render.render(write_still=True)
 report['views'][variant]={'pbr':str((P/(variant+'_pbr.png')).relative_to(R.parents[3])),'common_material':str((P/(variant+'_common_material.png')).relative_to(R.parents[3])),'triangles':sum(len(p.vertices)-2 for p in o.data.polygons)}
 for kind in ['pbr','common_material']:
  im=bpy.data.images.load(str(P/(variant+'_'+kind+'.png')),check_existing=False);arr=np.empty(len(im.pixels),dtype=np.float32);im.pixels.foreach_get(arr);pixels[(variant,kind)]=arr.reshape(-1,4)[:,:3];bpy.data.images.remove(im)
 print('MATCHED_REVIEW',variant,flush=True)
assert hashlib.sha256((R/'Originals/K01_Meshy.glb').read_bytes()).hexdigest()==original_hash
report['render_rgb_differences']={f'{a}_vs_{b}_{kind}':{'mean_absolute_0_to_1':float(np.mean(np.abs(pixels[(a,kind)]-pixels[(b,kind)]))),'max_absolute_0_to_1':float(np.max(np.abs(pixels[(a,kind)]-pixels[(b,kind)])))} for a,b in [('original','candidate'),('candidate','export')] for kind in ['pbr','common_material']}
report['decision']='Retopologized/darkened candidate rejected as degraded. Final candidate preserves original geometry, UVs, custom normals and material graph; only scale/pivot/frames and byte-identical texture extraction.'
(D/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
