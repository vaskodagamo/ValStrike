"""ART-18 local cleanup of the preserved Meshy draft; no API calls.
Centimeter coordinates; Meshy geometry remains the visible source.
"""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parents[4]; DOC=REPO/'docs/assets/pistol02'; SRC=ROOT.parent
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene;sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=.01
bpy.ops.import_scene.gltf(filepath=str(SRC/'Originals/model.glb'))
o=next(o for o in sc.objects if o.type=='MESH');bpy.context.view_layer.objects.active=o;o.select_set(True)
low=[v.co.x for v in o.data.vertices if v.co.z<-.3];print('LOW_GRIP_X',sum(low)/len(low),flush=True)
# Orient the muzzle away from the low grip mass. Uniform normalization only.
flip=1 if sum(low)/len(low)<0 else -1
scale=21/o.dimensions.x
for v in o.data.vertices:v.co=Vector((v.co.x*flip*scale,v.co.y*flip*scale,v.co.z*scale))
# Grip datum at center of upper palm contact, below slide; shift geometry only.
# Observed raw grip extends rearward; datum leaves room for a full palm.
for v in o.data.vertices:v.co-=Vector((-5,0,-.2))
# Weld coincident geometry at UV-island boundaries; UVs remain per-loop.
bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
mod=o.modifiers.new('Local draft reduction preserving UVs','DECIMATE');mod.ratio=.055
bpy.ops.object.modifier_apply(modifier=mod.name)
smooth=o.modifiers.new('Local surface relaxation','SMOOTH');smooth.factor=.25;smooth.iterations=3
bpy.ops.object.modifier_apply(modifier=smooth.name)
# Reduce generated micro-normal and mirror-like response for the restrained family.
for mat in o.data.materials:
 mat.name='P02_Gunmetal_Charcoal_PBR'
 for node in mat.node_tree.nodes:
  if node.type=='NORMAL_MAP':node.inputs['Strength'].default_value=.18
 bs=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
 for socket,mult,minimum in [('Metallic',.5,0),('Roughness',1,.52)]:
  inp=bs.inputs[socket]
  if inp.is_linked:
   upstream=inp.links[0].from_socket
   mathnode=mat.node_tree.nodes.new('ShaderNodeMath');mathnode.operation='MULTIPLY' if socket=='Metallic' else 'MAXIMUM';mathnode.inputs[1].default_value=mult if socket=='Metallic' else minimum
   mat.node_tree.links.new(upstream,mathnode.inputs[0]);mat.node_tree.links.new(mathnode.outputs[0],inp)
# Separate moving shells through actual mesh cuts. Fill only cut loops.
def cutcopy(name,planes):
 n=o.copy();n.data=o.data.copy();sc.collection.objects.link(n);n.name=name
 bm=bmesh.new();bm.from_mesh(n.data)
 for point,normal,inside,outside in planes:
  result=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=point,plane_no=normal,clear_inner=inside,clear_outer=outside)
  edges=[e for e in result['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_boundary]
  if edges:
   filled=bmesh.ops.holes_fill(bm,edges=edges,sides=0)
   for f in filled.get('faces',[]):f.material_index=1
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(n.data);bm.free();n.data.update();return n
# The returned draft's horizontal slide/frame seam lies at z=4.0 cm.
cap=bpy.data.materials.new('P02_CutInterior');cap.diffuse_color=(.035,.045,.060,1);cap.use_nodes=True
cap.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=cap.diffuse_color
cap.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.7
o.data.materials.append(cap)
body=cutcopy('SM_VS_Pistol02_Body',[((0,0,3.9),(0,0,1),False,True),((0,0,-6.05),(0,0,1),True,False)])
slide=cutcopy('SM_VS_Pistol02_Slide',[((0,0,3.9),(0,0,1),True,False)])
mag=cutcopy('SM_VS_Pistol02_Magazine',[((0,0,-6.05),(0,0,1),False,True)])
bpy.data.objects.remove(o,do_unlink=True)
parts=[body,slide,mag]
# Magazine floor is Meshy-derived; add a simple concealed visual stem, no internals.
bpy.ops.mesh.primitive_cube_add(size=1,location=(-2.5,0,-3.5));stem=bpy.context.object;stem.name='Magazine_visual_stem';stem.dimensions=(3.2,2.1,5.0);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);stem.data.materials.append(cap)
be=stem.modifiers.new('Soft stem edges','BEVEL');be.width=.15;be.segments=2;bpy.ops.object.modifier_apply(modifier=be.name)
bpy.ops.object.select_all(action='DESELECT');mag.select_set(True);stem.select_set(True);bpy.context.view_layer.objects.active=mag;bpy.ops.object.join()
# Pack textures into editable source; keep original PBR images alongside original exports.
for im in bpy.data.images:
 if im.source=='FILE' and im.has_data:im.pack()
anchors={'Grip':(0,0,0),'Muzzle':(15.50,0,5.0),'Slide':(0,0,3.9),'Magazine':(-2.5,0,-1.0)}
for name,p in anchors.items():
 e=bpy.data.objects.new(name,None);sc.collection.objects.link(e);e.location=p;e.empty_display_size=.65
# All exported components use the shared grip datum so identity assembly is exact.
(ROOT/'Exports').mkdir(exist_ok=True)
report={'tool':bpy.app.version_string,'units':'cm','forward':'+X','up':'+Z','uniform_scale_from_meshy':scale,'axis_flip_xy':flip,'source_meshy_triangles':454988,'anchors':{k:{'location_cm':v,'rotation_xyz_degrees':[0,0,0],'scale':[1,1,1]} for k,v in anchors.items()},'parts':[]}
allverts=[]
for p in parts:
 bpy.context.view_layer.update();p.data.calc_loop_triangles();verts=[p.matrix_world@v.co for v in p.data.vertices];allverts+=verts
 report['parts'].append({'name':p.name,'triangles':len(p.data.loop_triangles),'vertices':len(p.data.vertices),'materials':[m.name for m in p.data.materials],'dimensions_cm':[max(v[i] for v in verts)-min(v[i] for v in verts) for i in range(3)],'transform_identity':all(abs(p.matrix_world[i][j]-(1 if i==j else 0))<1e-5 for i in range(4) for j in range(4))})
 bpy.ops.object.select_all(action='DESELECT');p.select_set(True);bpy.context.view_layer.objects.active=p
 bpy.ops.export_scene.fbx(filepath=str(ROOT/'Exports'/(p.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_anim=False,path_mode='COPY',embed_textures=True)
report['bounds_cm']=[[min(v[i] for v in allverts) for i in range(3)],[max(v[i] for v in allverts) for i in range(3)]]
report['dimensions_cm']=[report['bounds_cm'][1][i]-report['bounds_cm'][0][i] for i in range(3)]
(DOC/'source-measurements.json').write_text(json.dumps(report,indent=2)+'\n')
bpy.ops.object.select_all(action='DESELECT')
for p in parts:p.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'P02_Kestrel.blend'))
print('P02_CLEANUP_DONE',json.dumps(report),flush=True)
