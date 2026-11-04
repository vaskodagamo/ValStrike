"""ART-25 local source preservation and real matched FBX views. No service/Unreal calls.
Run with Blender --background --factory-startup --python-exit-code 1 --python this.py.
Only the three assigned inputs and this task's source/docs paths are used.
"""
import bpy, bmesh, hashlib, json, math, shutil, tempfile, sys
from pathlib import Path
from mathutils import Matrix, Vector
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[4]
DOCS=ROOT/'docs/assets/meshy-site01/courtyard-pilot'
PREV=DOCS/'Previews'
INBOX=Path('/Users/webproduktion3/Documents/ValStrike')
IDS={'MSH-W01':('StraightWall',3.0),'MSH-W08':('PedestrianDoorway',3.0),'MSH-W09':('StraightParapet',2.0)}
ENTRIES={a['asset_id']:a for a in json.loads((ROOT/'docs/assets/meshy-site01-intake.json').read_text())['assets']}

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def bounds(o):
 p=[o.matrix_world@v.co for v in o.data.vertices]
 return [[min(v[a] for v in p) for a in range(3)],[max(v[a] for v in p) for a in range(3)]]
def stats(o):
 o.data.calc_loop_triangles();bm=bmesh.new();bm.from_mesh(o.data)
 b=bounds(o)
 s={'vertices':len(o.data.vertices),'triangles':len(o.data.loop_triangles),'loops':len(o.data.loops),'uv_channels':len(o.data.uv_layers),'material_slots':len(o.data.materials),'custom_normals':o.data.has_custom_normals,'boundary_edges':sum(e.is_boundary for e in bm.edges),'bounds_m':b,'dimensions_cm':[(b[1][a]-b[0][a])*100 for a in range(3)]}
 bm.free();return s

def scene_setup():
 s=bpy.context.scene;s.unit_settings.system='METRIC';s.unit_settings.scale_length=1;s.unit_settings.length_unit='CENTIMETERS'
 s.world=bpy.data.worlds.new('Neutral inspection world');s.world.use_nodes=True;s.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.25,.25,.25,1);s.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.5
 s.render.engine='CYCLES';s.cycles.samples=16;s.cycles.use_denoising=True;s.render.threads_mode='FIXED';s.render.threads=4
 s.render.resolution_x=900;s.render.resolution_y=800;s.render.resolution_percentage=100
 s.view_settings.view_transform='AgX';s.view_settings.look='AgX - Medium High Contrast';s.view_settings.exposure=0;s.view_settings.gamma=1
 bpy.context.preferences.filepaths.save_version=0
 return s

def shot(obj,path,detail=False,back=False):
 if "--resume-unchanged" in sys.argv and path.is_file():return
 b=bounds(obj);c=Vector([(b[0][a]+b[1][a])/2 for a in range(3)]);size=max(b[1][a]-b[0][a] for a in range(3))
 # Fixed world light; camera changes only for the intentionally separate back view.
 focus=c if not detail else Vector((b[1][0]*.82,b[0][1]+.02,b[1][2]*.82))
 pos=focus+Vector((size*.9,size*(1.8 if back else -1.8),size*.65))
 bpy.ops.object.camera_add(location=pos);cam=bpy.context.object;cam.rotation_euler=(focus-pos).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=size*(.65 if detail else 1.45);bpy.context.scene.camera=cam
 lights=[]
 for loc,power,area in [(c+Vector((size,-size,size*2)),1500*(size/3)**2,size*1.6),(c+Vector((-size,size,size)),500*(size/3)**2,size*2)]:
  bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.size=area;l.rotation_euler=(c-loc).to_track_quat('-Z','Y').to_euler();lights.append(l)
 bpy.context.scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
 for o in [cam,*lights]:bpy.data.objects.remove(o,do_unlink=True)

def wire_supplied_maps(mat,paths):
 # Preserve imported shader factors and links. Replace embedded JPG/path aliases
 # with the canonical accompanying lossless PNG for the same channel.
 for node in mat.node_tree.nodes:
  if node.type!='TEX_IMAGE':continue
  destinations=[l.to_socket.name for l in node.outputs['Color'].links]
  if 'Base Color' in destinations:role='basecolor'
  elif 'Roughness' in destinations:role='roughness'
  elif 'Metallic' in destinations:role='metallic'
  elif any(l.to_node.type=='NORMAL_MAP' for l in node.outputs['Color'].links):role='normal'
  else:raise RuntimeError('Unrecognized imported texture connection')
  node.image=bpy.data.images.load(str(paths[role]),check_existing=True)
  node.image.colorspace_settings.name='sRGB' if role=='basecolor' else 'Non-Color'
  node.label=role

def uv_values(o):return [tuple(v.uv) for v in o.data.uv_layers[0].data]

for aid,(kind,factor) in IDS.items():
 bpy.ops.wm.read_factory_settings(use_empty=True);scene_setup()
 out=HERE/aid;out.mkdir(parents=True,exist_ok=True);tex=out/'Textures';tex.mkdir(exist_ok=True);PREV.mkdir(parents=True,exist_ok=True)
 entry=ENTRIES[aid];paths={};records=[]
 for f in entry['files']:
  p=INBOX/f['file'];assert digest(p)==f['sha256']
  if p.suffix=='.png':
   role=next((r for r in ['normal','roughness','metallic'] if r in p.name),'basecolor');dest=tex/p.name;shutil.copy2(p,dest);paths[role]=dest
   records.append({'role':role,'path':str(dest.relative_to(ROOT)),'sha256':digest(dest),'bytes':dest.stat().st_size,'srgb':role=='basecolor'})
 f=next(f for f in entry['files'] if f['file'].endswith('.fbx'))
 with tempfile.TemporaryDirectory(prefix='art25-import-') as td:
  tmp=Path(td)/'original.fbx';shutil.copy2(INBOX/f['file'],tmp)
  bpy.ops.import_scene.fbx(filepath=str(tmp),use_anim=False)
  objs=[o for o in bpy.context.scene.objects if o.type=='MESH'];assert len(objs)==1
  raw=objs[0];raw.name=aid+'_ORIGINAL_MASTER';original_stats=stats(raw)
  assert raw.data.has_custom_normals
  assert len(raw.data.materials)==1
  wire_supplied_maps(raw.data.materials[0],paths)
  raw.data.materials[0].name='M_'+aid.replace('-','_')+'_Preserved'
  # Retain byte-identical recoverable FBX alongside the editable imported master.
  shutil.copy2(INBOX/f['file'],out/'Original.fbx')
  bpy.ops.object.select_all(action='DESELECT')
  b=bounds(raw);transform=Matrix.Translation(Vector([-v*factor for v in b[0]])) @ Matrix.Scale(factor,4) @ raw.matrix_world
  master_matrix=raw.matrix_world.copy()
  candidate=raw.copy();candidate.data=raw.data.copy();bpy.context.collection.objects.link(candidate);candidate.name='SM_VS_'+aid.replace('-','_')+'_'+kind
  normals=[transform.to_3x3().inverted().transposed()@n.vector for n in raw.data.corner_normals]
  for n in normals:n.normalize()
  candidate.data.transform(transform);candidate.matrix_world=Matrix.Identity(4)
  candidate.data.normals_split_custom_set(normals);candidate.data.update();bpy.context.view_layer.update()
  assert uv_values(candidate)==uv_values(raw)
  assert len(candidate.data.vertices)==len(raw.data.vertices)
  # Display original with the same rigid/uniform presentation transform, without editing its mesh data.
  raw.matrix_world=transform
  candidate.hide_render=True
  shot(raw,PREV/(aid+'_original.png'));shot(raw,PREV/(aid+'_original_detail.png'),detail=True)
  raw.hide_render=True;candidate.hide_render=False
  shot(candidate,PREV/(aid+'_source.png'));shot(candidate,PREV/(aid+'_source_detail.png'),detail=True);shot(candidate,PREV/(aid+'_source_back.png'),back=True)
  raw.matrix_world=master_matrix;raw.hide_set(True)
  for o in bpy.context.scene.objects:o.select_set(False)
  candidate.select_set(True);bpy.context.view_layer.objects.active=candidate
  export=out/(candidate.name+'.fbx')
  bpy.ops.export_scene.fbx(filepath=str(export),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',bake_space_transform=False,bake_anim=False,path_mode='RELATIVE',mesh_smooth_type='OFF',use_tspace=True)
  for image in list(bpy.data.images):
   if image.users==0:bpy.data.images.remove(image)
   elif image.source=='FILE':image.filepath='//Textures/'+Path(bpy.path.abspath(image.filepath)).name
  blend=out/(kind+'.blend');bpy.ops.wm.save_as_mainfile(filepath=str(blend))
  # Reopen actual saved source, then actual exported FBX. No old pipeline repeat.
  candidate_name=candidate.name
  bpy.ops.wm.open_mainfile(filepath=str(blend));candidate=bpy.data.objects[candidate_name]
  for image in bpy.data.images:
   if image.source=='FILE':assert Path(bpy.path.abspath(image.filepath)).is_file()
  candidate.hide_render=True
  old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(export),use_anim=False)
  rt=next(o for o in bpy.data.objects if o not in old and o.type=='MESH')
  # FBX recreates shader data; restore the selected original channel mapping for fair comparison.
  wire_supplied_maps(rt.data.materials[0],paths)
  rt_stats=stats(rt);source_stats=stats(candidate)
  assert rt_stats['triangles']==source_stats['triangles']
  delta=max(abs(a-b) for row1,row2 in zip(source_stats['bounds_m'],rt_stats['bounds_m']) for a,b in zip(row1,row2))
  assert delta<.00001
  shot(rt,PREV/(aid+'_reimport.png'));shot(rt,PREV/(aid+'_reimport_detail.png'),detail=True)
  report={'asset_id':aid,'blender_version':bpy.app.version_string,'original':original_stats,'source':source_stats,'fbx_reimport':rt_stats,'max_bounds_error_cm':delta*100,'uniform_scale':factor,'edits':['uniform scale','minimum-XYZ corner pivot','canonical external PNG path rebinding'],'geometry_edits':'none beyond affine transform; no snapping, decimation, smoothing or rebake','source_uv_values_exactly_preserved':True,'source_custom_normals':'original corner normals transformed by inverse transpose; no recompute','source_reopened':True,'textures_resolve':True,'textures':records,'input_files':entry['files'],'blend':str(blend.relative_to(ROOT)),'fbx':str(export.relative_to(ROOT)),'original_fbx':str((out/'Original.fbx').relative_to(ROOT)),'normal_convention':'Imported tangent normal node interprets +Y/OpenGL; vendor convention unlabelled. Native verify green flip.','collision':'No source collision meshes. Native visual only; preserve existing blockers and routes.'}
  for key in ['blend','fbx','original_fbx']:report[key+'_sha256']=digest(ROOT/report[key])
  (DOCS/(aid+'-manifest.json')).write_text(json.dumps(report,indent=2)+'\n')
  print('ART25_ASSET_DONE',aid,flush=True)
print('ART25_SOURCE_DONE',flush=True)
