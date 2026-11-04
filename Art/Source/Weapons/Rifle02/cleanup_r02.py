"""ART-17 local artist cleanup of the retained Meshy draft; no service calls.
Rebuild overwrites the authored source. Preserve subsequent manual edits first.
"""
import bpy,bmesh,json,math,struct
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parents[3]; DOC=REPO/'docs/assets/rifle02'
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene;sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=.01
bpy.ops.import_scene.gltf(filepath=str(REPO/'Artifacts/Meshy/R02/2026-11-04/originals/model.glb'))
o=next(x for x in sc.objects if x.type=='MESH');o.name='R02_Meshy_Draft_458064tri'
# Preserve the original imported material and split normals. Bake coordinate
# changes as object transforms so normals rotate with the vertices.
vs=[v.co.copy() for v in o.data.vertices];lo=Vector([min(v[i] for v in vs) for i in range(3)]);hi=Vector([max(v[i] for v in vs) for i in range(3)])
s=75/(hi.x-lo.x);center=(hi+lo)/2
bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
o.location=-center*s;o.scale=(s,s,s);o.rotation_mode='XYZ';o.rotation_euler.z=math.pi
# The translation follows the rotated center, then the grip-local origin.
o.location=Vector((center.x*s+14.5,center.y*s, -center.z*s+1.5))
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
orig=bpy.data.collections.new('00_ORIGINAL_MESHY_DRAFT');sc.collection.children.link(orig)
for c in list(o.users_collection):c.objects.unlink(o)
orig.objects.link(o);o.hide_render=True;o.hide_set(True)
shaped=o.copy();shaped.data=o.data.copy();shaped.name='R02_HighResolution_Fit_NormalReference';orig.objects.link(shaped)
# Continuous local grip fit, including the inverse-transpose normal correction.
# No hard coordinate cutoffs or notches; preserve the Meshy receiver surfaces.
import numpy as np
mesh=shaped.data;n=len(mesh.vertices);co=np.empty(n*3,dtype=np.float32);mesh.vertices.foreach_get('co',co);co=co.reshape((-1,3));x,y,z=co.T
normals=np.empty(len(mesh.loops)*3,dtype=np.float32);mesh.corner_normals.foreach_get('vector',normals);normals=normals.reshape((-1,3))
loop_indices=np.empty(len(mesh.loops),dtype=np.int32);mesh.loops.foreach_get('vertex_index',loop_indices)
def smooth(v,a,b):
 t=np.clip((v-a)/(b-a),0,1);value=t*t*(3-2*t);deriv=np.where((v>a)&(v<b),6*t*(1-t)/(b-a),0);return value,deriv
left,dl=smooth(x,-12,-9);right,dr=smooth(x,0,3);depth,dd=smooth(-z,1,4)
w=left*(1-right)*depth;wx=(dl*(1-right)-left*dr)*depth;wz=-left*(1-right)*dd;cx=-1.8+.32*z
J=np.zeros((n,3,3),dtype=np.float32);J[:,0,0]=1;J[:,1,0]=-.38*y*wx;J[:,1,1]=1-.38*w;J[:,1,2]=-.38*y*wz;J[:,2,2]=1
fixed=np.einsum('nij,nj->ni',np.transpose(np.linalg.inv(J),(0,2,1))[loop_indices],normals);fixed/=np.maximum(np.linalg.norm(fixed,axis=1)[:,None],1e-10)
co[:,1]=y*(1-.38*w);mesh.vertices.foreach_set('co',co.ravel());mesh.update();mesh.normals_split_custom_set(fixed)
body=shaped.copy();body.data=shaped.data.copy();body.name='SM_VS_Rifle02_Body';sc.collection.objects.link(body);body.hide_render=False;body.hide_set(False)
# Split the visible magazine across its insertion seam; retain UV data.
bm=bmesh.new();bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001)
bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=(0,0,-2.5),plane_no=(0,0,1),clear_inner=False,clear_outer=False)
# Choose the entire connected lower magazine component, rather than clipping
# by a rectangular X mask (which previously introduced tiny cut fins).
lower={f for f in bm.faces if f.calc_center_median().z<-2.50001};components=[]
while lower:
 seed=lower.pop();component={seed};stack=[seed]
 while stack:
  f=stack.pop()
  for e in f.edges:
   for other in e.link_faces:
    if other in lower:lower.remove(other);component.add(other);stack.append(other)
 components.append(component)
candidates=[c for c in components if len(c)>100]
selected=min(candidates,key=lambda c:(sum((f.calc_center_median() for f in c),Vector())/len(c)-Vector((15,0,-5))).length)
for f in bm.faces:f.material_index=2 if f in selected else 0
print('MAGAZINE_COMPONENT',len(selected),'OTHER_COMPONENTS',[len(c) for c in candidates],flush=True)
bm.to_mesh(body.data);bm.free()
mag=body.copy();mag.data=body.data.copy();mag.name='SM_VS_Rifle02_Magazine';sc.collection.objects.link(mag)
for obj,keepmag in [(body,False),(mag,True)]:
 bm=bmesh.new();bm.from_mesh(obj.data)
 remove=[]
 for f in bm.faces:
  is_mag=f.material_index==2
  f.material_index=0
  if is_mag!=keepmag:remove.append(f)
 bmesh.ops.delete(bm,geom=remove,context='FACES')
 wire=[v for v in bm.verts if not v.link_faces]
 if wire:bmesh.ops.delete(bm,geom=wire,context='VERTS')
 # Close only the newly exposed insertion seam using a matte interior material.
 seam=[e for e in bm.edges if e.is_boundary]
 cap=bmesh.ops.holes_fill(bm,edges=seam,sides=0).get('faces',[]) if seam else []
 for f in cap:f.material_index=1
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 bpy.context.view_layer.objects.active=obj;obj.select_set(True)
 # No decimation: preserve the full generated surface until profiling requires LODs.
 obj.select_set(False)
 bm=bmesh.new();bm.from_mesh(obj.data)
 edges=[e for e in bm.edges if e.is_boundary]
 if edges:bmesh.ops.holes_fill(bm,edges=edges,sides=0)
 bmesh.ops.triangulate(bm,faces=list(bm.faces))
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 # Remove duplicated cap triangles and zero-thickness triangular fins revealed
 # by the FBX round-trip. Keep the surrounding textured surface intact.
 obj.data.validate(verbose=True,clean_customdata=False)
 bm=bmesh.new();bm.from_mesh(obj.data)
 for _ in range(3):
  fins=[f for f in bm.faces if sum(e.is_boundary for e in f.edges)>=2 and any(len(e.link_faces)>2 for e in f.edges)]
  if not fins:break
  bmesh.ops.delete(bm,geom=fins,context='FACES')
 loose=[v for v in bm.verts if not v.link_faces]
 if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
 for p in obj.data.polygons:p.use_smooth=True
# Preserve the material delivered in the GLB, including its full normal map.
mat=o.data.materials[0];mat.name='M_R02_Meshy_Gunmetal'
image_roles={}
for node in mat.node_tree.nodes:
 if node.type=='TEX_IMAGE':
  destinations=[link.to_node.type for link in node.outputs['Color'].links]
  role='base_color' if 'BSDF_PRINCIPLED' in destinations else ('orm' if 'SEPARATE_COLOR' in destinations else 'normal')
  img=node.image;img.name='T_R02_'+role;image_roles[role]=img
  # Preserve exact embedded JPEG bytes, not a recompressed or color-converted copy.
  raw_glb=(REPO/'Artifacts/Meshy/R02/2026-11-04/originals/model.glb').read_bytes();json_len=struct.unpack_from('<I',raw_glb,12)[0];gltf=json.loads(raw_glb[20:20+json_len]);image_index={'base_color':0,'orm':1,'normal':2}[role];view=gltf['bufferViews'][gltf['images'][image_index]['bufferView']];offset=28+json_len+view.get('byteOffset',0);data=raw_glb[offset:offset+view['byteLength']]
  path=ROOT/'Textures/GLB'/('T_R02_'+role+'.jpg');path.parent.mkdir(exist_ok=True);path.write_bytes(data);img.filepath=str(path);img.pack(data=data,data_len=len(data))
interior=bpy.data.materials.new('M_R02_SeamInterior');interior.diffuse_color=(.035,.043,.055,1);interior.use_nodes=True;interior.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=interior.diffuse_color;interior.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=.8
for obj in [body,mag]:
 obj.data.materials.clear();obj.data.materials.append(mat);obj.data.materials.append(interior)
 # Assign cap slots after mesh validation, which clamps absent material slots.
 for poly in obj.data.polygons:
  if all(abs(obj.data.vertices[i].co.z+2.5)<.0001 for i in poly.vertices):poly.material_index=1
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 if obj.data.has_custom_normals:bpy.ops.mesh.customdata_custom_splitnormals_clear()
 transfer=obj.modifiers.new('Preserve high-resolution authored normals','DATA_TRANSFER');transfer.object=shaped;transfer.use_loop_data=True;transfer.data_types_loops={'CUSTOM_NORMAL'};transfer.loop_mapping='POLYINTERP_NEAREST'
 bpy.ops.object.modifier_apply(modifier=transfer.name)
 # Internal insertion cap uses its own geometric normal, not a side-surface normal.
 ns=[n.vector.copy() for n in obj.data.corner_normals]
 for poly in obj.data.polygons:
  if poly.material_index==1:
   for i in poly.loop_indices:ns[i]=poly.normal.copy()
 obj.data.normals_split_custom_set(ns)
mag_anchor=Vector((15,0,-2.5))
for v in mag.data.vertices:v.co-=mag_anchor
mag.location=mag_anchor
anchors={'Grip':{'location_cm':[0,0,0],'rotation_xyz_deg':[0,0,0],'scale':[1,1,1]},'Muzzle':{'location_cm':[52,0,7.05],'rotation_xyz_deg':[0,0,0],'scale':[1,1,1]},'SupportHand':{'location_cm':[29,0,2.4],'rotation_xyz_deg':[0,0,0],'scale':[1,1,1]},'Magazine':{'location_cm':list(mag_anchor),'rotation_xyz_deg':[0,0,0],'scale':[1,1,1]}}
for name,values in anchors.items():
 e=bpy.data.objects.new('SOCKET_'+name,None);sc.collection.objects.link(e);e.location=values['location_cm'];e.empty_display_size=2
# Export each part at its own local pivot with identity transforms.
for obj in [body,mag]:
 bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 old=obj.location.copy();obj.location=(0,0,0)
 bpy.ops.export_scene.fbx(filepath=str(ROOT/'Exports'/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',bake_space_transform=False,use_mesh_modifiers=True,add_leaf_bones=False,bake_anim=False,path_mode='RELATIVE')
 obj.location=old
# Record geometry, actual component bounds and grip slices.
def bounds(vs):return {'min':[min(v[i] for v in vs) for i in range(3)],'max':[max(v[i] for v in vs) for i in range(3)]}
report={'length_units':'centimeters','forward':'+X','up':'+Z','source_scale':s,'draft_triangles':458064,'reduction_applied':False,'normal_policy':'Preserved imported custom normals through rigid normalization, inverse-transpose grip correction, and high-resolution normal transfer','material_policy':'Original GLB PBR graph, no tint/roughness/metallic/normal-strength override','anchors':anchors,'parts':{}}
for obj in [body,mag]:
 obj.data.calc_loop_triangles();bm=bmesh.new();bm.from_mesh(obj.data)
 report['parts'][obj.name]={'triangles':len(obj.data.loop_triangles),'vertices':len(obj.data.vertices),'materials':len(obj.data.materials),'uv_layers':[u.name for u in obj.data.uv_layers],'local_bounds':bounds([v.co for v in obj.data.vertices]),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'degenerate_faces':sum(f.calc_area()<1e-10 for f in bm.faces)};bm.free()
bpy.context.view_layer.update()
assembled=[obj.matrix_world@v.co for obj in [body,mag] for v in obj.data.vertices];b=bounds(assembled);report['assembled_bounds']=b;report['assembled_dimensions_cm']=[b['max'][i]-b['min'][i] for i in range(3)]
report['grip_slice_z_minus5_cm']=bounds([v.co for v in body.data.vertices if -.0>v.co.x>-11 and -5.5<v.co.z<-4.5])
report['fbx']={'axis_forward':'-Y','axis_up':'Z','apply_unit_scale':True,'apply_scale_options':'FBX_SCALE_UNITS','visual_collision':False}
(DOC/'source-measurements.json').write_text(json.dumps(report,indent=2)+'\n')
shaped_mesh=shaped.data;bpy.data.objects.remove(shaped,do_unlink=True);bpy.data.meshes.remove(shaped_mesh)
for img in bpy.data.images:
 if img.users and not img.packed_file:img.pack()
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'R02_Compact.blend'))
print('R02_SOURCE',json.dumps(report),flush=True)
