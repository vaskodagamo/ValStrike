"""ART-17 local artist cleanup of the retained Meshy draft; no service calls.
Rebuild overwrites the authored source. Preserve subsequent manual edits first.
"""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parents[3]; DOC=REPO/'docs/assets/rifle02'
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene;sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=.01
bpy.ops.import_scene.gltf(filepath=str(REPO/'Artifacts/Meshy/R02/2026-11-04/originals/model.glb'))
o=next(x for x in sc.objects if x.type=='MESH');o.name='R02_Meshy_Draft_458064tri'
# Retain the complete generated geometry for editable provenance, normalized only.
vs=[v.co.copy() for v in o.data.vertices];lo=Vector([min(v[i] for v in vs) for i in range(3)]);hi=Vector([max(v[i] for v in vs) for i in range(3)])
s=75/(hi.x-lo.x);center=(hi+lo)/2
for v in o.data.vertices:
 p=(v.co-center)*s;v.co=Vector((-p.x+14.5,-p.y,p.z+1.5))
orig=bpy.data.collections.new('00_ORIGINAL_MESHY_DRAFT');sc.collection.children.link(orig)
for c in list(o.users_collection):c.objects.unlink(o)
orig.objects.link(o);o.hide_render=True;o.hide_set(True)
body=o.copy();body.data=o.data.copy();body.name='SM_VS_Rifle02_Body';sc.collection.objects.link(body);body.hide_render=False;body.hide_set(False)
# Artist-shaped width refinement: receiver 6.6 cm, hand grip ~3.5 cm,
# smoothly blended into the receiver. No whole-weapon nonuniform scale.
for v in body.data.vertices:
 p=v.co;grip=max(0,min(1,(-p.z+.2)/2.4))*max(0,min(1,(5-p.x)/3))*max(0,min(1,(p.x+12)/3))
 p.y*=.91*(1-grip)+.62*grip
 # Reduce grip fore/aft depth locally, retaining the curved backstrap.
 if p.z<-2 and -11<p.x<1:
  amount=min(1,(-p.z-2)/1.5);cx=-1.8+.32*p.z;p.x=cx+(p.x-cx)*(1-.2*amount)
# Split the visible magazine across its insertion seam; retain UV data.
bm=bmesh.new();bm.from_mesh(body.data)
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001)
bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.00001)
bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=(0,0,-.55),plane_no=(0,0,1),clear_inner=False,clear_outer=False)
bm.to_mesh(body.data);bm.free()
mag=body.copy();mag.data=body.data.copy();mag.name='SM_VS_Rifle02_Magazine';sc.collection.objects.link(mag)
for obj,keepmag in [(body,False),(mag,True)]:
 bm=bmesh.new();bm.from_mesh(obj.data)
 remove=[]
 for f in bm.faces:
  c=f.calc_center_median();is_mag=8<c.x<23 and c.z<-.55001
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
 dec=obj.modifiers.new('Local decimation preserving UV seams','DECIMATE');dec.ratio=.09;dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name);obj.select_set(False)
 bm=bmesh.new();bm.from_mesh(obj.data)
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.001)
 bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.0001)
 edges=[e for e in bm.edges if e.is_boundary]
 if edges:bmesh.ops.holes_fill(bm,edges=edges,sides=0)
 bmesh.ops.triangulate(bm,faces=list(bm.faces))
 bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.00001)
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
# Explicit PBR shader from unchanged downloaded maps.
mat=bpy.data.materials.new('M_R02_Meshy_Gunmetal');mat.use_nodes=True;nt=mat.node_tree;bs=nt.nodes.get('Principled BSDF')
for suffix,socket,space in [('base_color','Base Color','sRGB'),('metallic','Metallic','Non-Color'),('roughness','Roughness','Non-Color'),('normal','Normal','Non-Color')]:
 img=bpy.data.images.load(str(ROOT/'Textures'/('texture_0_'+suffix+'.png')),check_existing=True);img.colorspace_settings.name=space
 tex=nt.nodes.new('ShaderNodeTexImage');tex.image=img;tex.label=suffix
 if suffix=='normal':
  normal=nt.nodes.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.25;nt.links.new(tex.outputs['Color'],normal.inputs['Color']);nt.links.new(normal.outputs['Normal'],bs.inputs[socket])
 elif suffix=='base_color':
  tint=nt.nodes.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1;tint.inputs[2].default_value=(.55,.65,.8,1);nt.links.new(tex.outputs['Color'],tint.inputs[1]);nt.links.new(tint.outputs[0],bs.inputs[socket])
 else:
  tune=nt.nodes.new('ShaderNodeMath');tune.operation='MULTIPLY' if suffix=='metallic' else 'MAXIMUM';tune.inputs[1].default_value=.4 if suffix=='metallic' else .55;nt.links.new(tex.outputs['Color'],tune.inputs[0]);nt.links.new(tune.outputs[0],bs.inputs[socket])
interior=bpy.data.materials.new('M_R02_SeamInterior');interior.diffuse_color=(.035,.043,.055,1);interior.use_nodes=True;interior.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=interior.diffuse_color;interior.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value=.8
for obj in [body,mag,o]:obj.data.materials.clear();obj.data.materials.append(mat);obj.data.materials.append(interior)
mag_anchor=Vector((15,0,-.55))
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
report={'length_units':'centimeters','forward':'+X','up':'+Z','source_scale':s,'draft_triangles':458064,'anchors':anchors,'parts':{}}
for obj in [body,mag]:
 obj.data.calc_loop_triangles();bm=bmesh.new();bm.from_mesh(obj.data)
 report['parts'][obj.name]={'triangles':len(obj.data.loop_triangles),'vertices':len(obj.data.vertices),'materials':len(obj.data.materials),'uv_layers':[u.name for u in obj.data.uv_layers],'local_bounds':bounds([v.co for v in obj.data.vertices]),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'degenerate_faces':sum(f.calc_area()<1e-10 for f in bm.faces)};bm.free()
bpy.context.view_layer.update()
assembled=[obj.matrix_world@v.co for obj in [body,mag] for v in obj.data.vertices];b=bounds(assembled);report['assembled_bounds']=b;report['assembled_dimensions_cm']=[b['max'][i]-b['min'][i] for i in range(3)]
report['grip_slice_z_minus5_cm']=bounds([v.co for v in body.data.vertices if -.0>v.co.x>-11 and -5.5<v.co.z<-4.5])
report['fbx']={'axis_forward':'-Y','axis_up':'Z','apply_unit_scale':True,'apply_scale_options':'FBX_SCALE_UNITS','visual_collision':False}
(DOC/'source-measurements.json').write_text(json.dumps(report,indent=2)+'\n')
for img in bpy.data.images:
 if img.users:img.pack()
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'R02_Compact.blend'))
print('R02_SOURCE',json.dumps(report),flush=True)
