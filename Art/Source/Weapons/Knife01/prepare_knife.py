"""Normalize the preserved Meshy original without changing its visual surfaces.
Blender 5.2.2 --background --factory-startup --threads 2 --python-exit-code 1 --python this_file
No reduction, smoothing, retopology, UV changes, normal regeneration or material edits.
"""
import bpy,bmesh,json,math,hashlib
from pathlib import Path
from mathutils import Vector,Matrix
R=Path(__file__).resolve().parent;DOC=R.parents[3]/'docs/assets/knife01';PRE=DOC/'Previews'
bpy.ops.wm.read_factory_settings(use_empty=True);s=bpy.context.scene
s.unit_settings.system='METRIC';s.unit_settings.scale_length=.01;s.unit_settings.length_unit='CENTIMETERS'
bpy.ops.import_scene.gltf(filepath=str(R/'Originals/K01_Meshy.glb'))
o=next(o for o in s.objects if o.type=='MESH');o.name='SM_VS_Knife01'
bpy.context.view_layer.objects.active=o;o.select_set(True)
def uv_hash(mesh):return hashlib.sha256(repr([[tuple(x.uv) for x in layer.data] for layer in mesh.uv_layers]).encode()).hexdigest()
raw={'vertices':len(o.data.vertices),'triangles':sum(len(p.vertices)-2 for p in o.data.polygons),'uv_sha256':uv_hash(o.data),'has_custom_normals':o.data.has_custom_normals}
points=[o.matrix_world@v.co for v in o.data.vertices];xmin=min(v.x for v in points);xmax=max(v.x for v in points);span=xmax-xmin;scale=27.8/span;grip_x=xmin+.774*span
normalization=Matrix.Translation((grip_x*scale,0,.00788*span*scale))@Matrix.Scale(scale,4)@Matrix.Rotation(math.pi,4,'Z')
o.matrix_world=normalization@o.matrix_world;bpy.context.view_layer.update()
world_positions=[o.matrix_world@v.co for v in o.data.vertices]
norm_matrix=o.matrix_world.to_3x3().inverted().transposed();world_normals=[(norm_matrix@n.vector).normalized() for n in o.data.corner_normals]
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True);bpy.context.view_layer.update()
# Carry the original transformed loop normals explicitly through Blender's apply step.
o.data.normals_split_custom_set([tuple(n) for n in world_normals]);o.data.update()
position_error=max((v.co-w).length for v,w in zip(o.data.vertices,world_positions))
normal_dot=min(n.vector.normalized().dot(w) for n,w in zip(o.data.corner_normals,world_normals))
print('PRESERVATION_DIAGNOSTIC',position_error,normal_dot,flush=True)
assert position_error<.0001 and normal_dot>.99999
assert raw['uv_sha256']==uv_hash(o.data) and raw['vertices']==len(o.data.vertices)
# Extract byte-identical packed GLB maps for a portable native material handoff.
embedded=R/'Textures/Embedded';embedded.mkdir(exist_ok=True)
texture_records=[]
for im in bpy.data.images:
 if not im.packed_file:continue
 data=bytes(im.packed_file.data);ext='.png' if data.startswith(bytes([137,80,78,71])) else '.jpg'
 path=embedded/(im.name+ext);path.write_bytes(data);im.filepath='//Textures/Embedded/'+path.name
 texture_records.append({'image':im.name,'path':str(path.relative_to(R.parents[3])),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'size':list(im.size),'space':im.colorspace_settings.name,'byte_identical_to_glb_packed_map':True})
lo=Vector([min(v.co[i] for v in o.data.vertices) for i in range(3)]);hi=Vector([max(v.co[i] for v in o.data.vertices) for i in range(3)]);tip=max(o.data.vertices,key=lambda v:v.co.x).co.copy()
for name,loc in [('Grip',(0,0,0)),('Tip',tip)]:
 a=bpy.data.objects.new(name,None);s.collection.objects.link(a);a.empty_display_type='ARROWS';a.empty_display_size=1.3;a.location=loc;a.parent=o;a['purpose']='visual presentation frame only; no attack volume';a['forward']='+X';a['up']='+Z'
o['units']='centimeters';o['provenance']='Unreduced original Meshy 7.1 task 01a1a6d1-dcbd-74dd-8074-6fd6634c4843';o['collision_intent']='NoCollision equipped; gameplay absent'
bpy.ops.object.select_all(action='DESELECT')
for a in [o,bpy.data.objects['Grip'],bpy.data.objects['Tip']]:a.select_set(True)
bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.fbx(filepath=str(R/'Exports/SM_VS_Knife01.fbx'),use_selection=True,object_types={'MESH','EMPTY'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_space_transform=True,bake_space_transform=False,mesh_smooth_type='FACE',use_mesh_modifiers=True,use_tspace=True,bake_anim=False,add_leaf_bones=False,path_mode='RELATIVE')
bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(R/'Knife01.blend'))
def stats(bm):return {'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'degenerate_faces':sum(f.calc_area()<1e-10 for f in bm.faces)}
bm=bmesh.new();bm.from_mesh(o.data);top=stats(bm)
# Diagnostic weld on an ephemeral copy only; never written to the source mesh.
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001);geometric_top=stats(bm);bm.free()
report={'blender':bpy.app.version_string,'raw':raw,'source_vertices':len(o.data.vertices),'source_triangles':sum(len(p.vertices)-2 for p in o.data.polygons),'material_slots':len(o.data.materials),'material_names':[m.name for m in o.data.materials],'uv_layers':[u.name for u in o.data.uv_layers],'bounds_cm':[list(lo),list(hi)],'dimensions_cm':list(hi-lo),'object_location':list(o.location),'object_rotation':list(o.rotation_euler),'object_scale':list(o.scale),'Grip':{'location_cm':[0,0,0],'rotation_degrees':[0,0,0],'scale':[1,1,1]},'Tip':{'location_cm':list(tip),'rotation_degrees':[0,0,0],'scale':[1,1,1]},'topology':top,'geometric_topology_on_diagnostic_welded_copy_only':geometric_top,'preservation':{'normalized_vertex_max_error_cm':position_error,'custom_corner_normals_min_dot':normal_dot,'uv_hash_unchanged':raw['uv_sha256']==uv_hash(o.data),'material_graph_unchanged':True,'mesh_reduction':False,'smoothing':False,'retopology':False,'material_darkening':False},'textures':texture_records,'cleanup':['Preserve original Meshy geometry, UVs, custom normals and PBR material graph','Normalize cm/+X, grip-local origin; apply transform with measured position/normal preservation','Add Grip/Tip visual frames and export FBX','Extract packed GLB map bytes without re-encoding','Hold rejected reconstruction under Candidates/Rejected_Retopology; not the final asset'],'fbx':{'axis_forward':'-Y','axis_up':'Z','apply_scale_options':'FBX_SCALE_UNITS','bake_space_transform':False}}
(DOC/'source-inspection.json').write_text(json.dumps(report,indent=2)+'\n');print('K01_ORIGINAL_PRESERVED',json.dumps(report),flush=True)
# Source studio renders; not saved into deliverable blend.
s.render.engine='CYCLES';s.cycles.samples=24;s.cycles.use_denoising=True;s.render.resolution_x=1200;s.render.resolution_y=700;s.render.resolution_percentage=100
s.world=bpy.data.worlds.new('Neutral studio');s.world.use_nodes=True;s.world.node_tree.nodes['Background'].inputs[0].default_value=(.17,.2,.25,1);s.world.node_tree.nodes['Background'].inputs[1].default_value=.45
s.view_settings.view_transform='AgX'
def area(name,pos,power,size):
 data=bpy.data.lights.new(name,'AREA');a=bpy.data.objects.new(name,data);s.collection.objects.link(a);a.location=pos;a.rotation_euler=(Vector((7,0,0))-a.location).to_track_quat('-Z','Y').to_euler();data.energy=power;data.shape='DISK';data.size=size
area('Key',(0,-18,30),25000,25);area('Fill',(15,20,15),18000,20);area('Rim',(-15,2,8),12000,15);area('LowerFill',(8,-12,-18),10000,20)
cam=bpy.data.objects.new('StudioCamera',bpy.data.cameras.new('StudioCamera'));s.collection.objects.link(cam);s.camera=cam;cam.data.type='ORTHO'
def render(name,pos,target,ortho):
 cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=ortho;s.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
render('neutral_three_quarter',(23,-40,21),(7,0,0),33)
render('neutral_reverse',(12,40,15),(7,0,0),33)
print('K01_STUDIO_RENDERED',flush=True)
