"""ART-20 local finishing of the single Meshy draft; no service/Unreal calls.
Blender 5.2.2 --background --threads 2 --python-exit-code 1 --python this_file
"""
import bpy,bmesh,json,math
from pathlib import Path
from mathutils import Vector,Matrix
R=Path(__file__).resolve().parent;DOC=R.parents[3]/'docs/assets/knife01';PRE=DOC/'Previews'
bpy.ops.wm.read_factory_settings(use_empty=True);s=bpy.context.scene
s.unit_settings.system='METRIC';s.unit_settings.scale_length=.01;s.unit_settings.length_unit='CENTIMETERS'
bpy.ops.import_scene.gltf(filepath=str(R/'Originals/K01_Meshy.glb'))
o=next(o for o in s.objects if o.type=='MESH');o.name='SM_VS_Knife01'
bpy.context.view_layer.objects.active=o;o.select_set(True)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
raw={'vertices':len(o.data.vertices),'triangles':len(o.data.polygons)}
# Meshy draft is already broad face X/Z, with tip toward -X. Turn 180 about Z.
xmin=min(v.co.x for v in o.data.vertices);xmax=max(v.co.x for v in o.data.vertices)
span=xmax-xmin;scale=27.8/span;grip_x=xmin+.774*span
for v in o.data.vertices:v.co=Vector(((grip_x-v.co.x)*scale,-v.co.y*scale,(v.co.z+.00788*span)*scale))
bm=bmesh.new();bm.from_mesh(o.data)
bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0001)
bmesh.ops.dissolve_degenerate(bm,edges=list(bm.edges),dist=.00001)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
post_weld=len(o.data.vertices)
# Keep the actual generated grip. Remove uneven generated trim and blade
# for local artist retopology matched to its measured silhouette.
bm=bmesh.new();bm.from_mesh(o.data)
bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=(5.15,0,0),plane_no=(1,0,0),clear_outer=True,clear_inner=False)
bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.00001,plane_co=(-4.55,0,0),plane_no=(-1,0,0),clear_outer=True,clear_inner=False)
bmesh.ops.holes_fill(bm,edges=[e for e in bm.edges if e.is_boundary],sides=0)
for _ in range(5):bmesh.ops.smooth_vert(bm,verts=list(bm.verts),factor=.35,use_axis_x=True,use_axis_y=True,use_axis_z=True)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
d=o.modifiers.new('Generated grip local reduction','DECIMATE');d.ratio=.32;d.use_collapse_triangulate=True
bpy.ops.object.modifier_apply(modifier=d.name)
for p in o.data.polygons:p.use_smooth=True
bm=bmesh.new();bm.from_mesh(o.data)
for e in bm.edges:e.smooth=e.is_manifold and e.calc_face_angle(0)<math.radians(65)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free();o.data.update()
mat=bpy.data.materials.new('M_K01_Meshy_CoolSteel_Charcoal');mat.use_nodes=True;n=mat.node_tree.nodes;n.clear();ln=mat.node_tree.links
out=n.new('ShaderNodeOutputMaterial');bs=n.new('ShaderNodeBsdfPrincipled');ln.new(bs.outputs['BSDF'],out.inputs['Surface']);out.location=(600,0);bs.location=(300,0)
for i,(role,socket,space) in enumerate([('base_color','Base Color','sRGB'),('metallic','Metallic','Non-Color'),('roughness','Roughness','Non-Color'),('normal',None,'Non-Color')]):
 im=bpy.data.images.load(str(R/'Textures'/('texture_0_'+role+'.png')),check_existing=False);im.colorspace_settings.name=space;im.filepath='//Textures/'+Path(im.filepath).name
 node=n.new('ShaderNodeTexImage');node.image=im;node.label=role;node.location=(-600,300-i*280)
 if role=='normal':
  normal=n.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.1;normal.location=(0,-400);ln.new(node.outputs['Color'],normal.inputs['Color']);ln.new(normal.outputs['Normal'],bs.inputs['Normal'])
 elif role=='roughness':
  mr=n.new('ShaderNodeMapRange');mr.inputs['From Min'].default_value=0;mr.inputs['From Max'].default_value=1;mr.inputs['To Min'].default_value=.5;mr.inputs['To Max'].default_value=.8;mr.location=(-150,-200);ln.new(node.outputs['Color'],mr.inputs['Value']);ln.new(mr.outputs['Result'],bs.inputs[socket])
 elif role=='base_color':
  mix=n.new('ShaderNodeMixRGB');mix.blend_type='MIX';mix.inputs[0].default_value=.92;mix.inputs[2].default_value=(.012,.016,.023,1);mix.location=(-100,200);ln.new(node.outputs['Color'],mix.inputs[1]);ln.new(mix.outputs[0],bs.inputs[socket])
 elif role=='metallic':
  mult=n.new('ShaderNodeMath');mult.operation='MULTIPLY';mult.inputs[1].default_value=.08;ln.new(node.outputs['Color'],mult.inputs[0]);ln.new(mult.outputs[0],bs.inputs[socket])
o.data.materials.clear();o.data.materials.append(mat)
# Clean satin blade response removes generated baked jagged highlight bands.
steel=bpy.data.materials.new('M_K01_SatinSteel');steel.use_nodes=True
sb=steel.node_tree.nodes.get('Principled BSDF')
def linear(c):return c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4
color=tuple(linear(c/255) for c in (166,177,190))+(1,)
sb.inputs['Base Color'].default_value=color;sb.inputs['Metallic'].default_value=.65;sb.inputs['Roughness'].default_value=.32;steel.diffuse_color=color
o.data.materials.append(steel)
gunmetal=bpy.data.materials.new('M_K01_GunmetalTrim');gunmetal.use_nodes=True
gm=gunmetal.node_tree.nodes.get('Principled BSDF');mc=tuple(linear(c/255) for c in (77,87,101))+(1,)
gm.inputs['Base Color'].default_value=mc;gm.inputs['Metallic'].default_value=.6;gm.inputs['Roughness'].default_value=.4;gunmetal.diffuse_color=mc
o.data.materials.append(gunmetal)
for p in o.data.polygons:p.material_index=0
accent=bpy.data.materials.new('M_K01_BurgundyInset');accent.use_nodes=True
ab=accent.node_tree.nodes.get('Principled BSDF');ac=tuple(linear(c/255) for c in (111,31,47))+(1,)
ab.inputs['Base Color'].default_value=ac;ab.inputs['Metallic'].default_value=.15;ab.inputs['Roughness'].default_value=.48;accent.diffuse_color=ac
o.data.materials.append(accent)
# Clean local trim retopology uses the measured generated guard/pommel envelope.
# It covers the capped ends of the retained, UV-textured Meshy grip.
def trim(name,x0,x1,profile,material,bevel_width):
 vs=[(x,y,z) for x in (x0,x1) for y,z in profile];n=len(profile)
 fs=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
 me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update();ob=bpy.data.objects.new(name,me);s.collection.objects.link(ob);me.materials.append(material)
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 uv=me.uv_layers.new(name='UVMap')
 for poly in me.polygons:
  for li in poly.loop_indices:
   co=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=((co.y+1.3)/2.6,(co.z+2.5)/5.2)
 bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
 b=ob.modifiers.new('Trim bevel','BEVEL');b.width=bevel_width;b.segments=3
 bpy.ops.object.modifier_apply(modifier=b.name)
 for poly in ob.data.polygons:poly.use_smooth=False
 return ob
profile=[(-.52,2.48),(.52,2.48),(.95,1.9),(1.12,1.2),(1.12,-1.25),(.62,-2.12),(-.62,-2.12),(-1.12,-1.25),(-1.12,1.2),(-.95,1.9)]
guard=trim('K01_Retopology_Guard',5.0,6.55,profile,gunmetal,.12)
heel=[(-.6,1.47),(.6,1.47),(1.14,.95),(1.14,-.75),(.62,-1.5),(-.62,-1.5),(-1.14,-.75),(-1.14,.95)]
pommel=trim('K01_Retopology_Pommel',-6.28,-5.22,heel,gunmetal,.16)
heel2=[(y*.96,z*.96) for y,z in heel]
ring=trim('K01_Retopology_BurgundyInset',-5.25,-4.91,heel2,accent,.04)
collar=trim('K01_Retopology_HeelCollar',-4.94,-4.45,heel2,gunmetal,.06)
# Join locally retopologized trim to the retained generated grip.
bpy.ops.object.select_all(action='DESELECT')
for part in (o,guard,pommel,ring,collar):part.select_set(True)
bpy.context.view_layer.objects.active=o;bpy.ops.object.join()
# Blade-only local retopology: measured Meshy silhouette simplified into clean
# cross-sections, with continuous planar cheeks and a readable lower bevel.
# Generated grip and its UVs remain from the actual Meshy job; trim follows its envelope.
sections=[(6.25,1.92,-1.12),(7.25,1.93,-1.12),(7.8,1.94,-1.48),
 (10,1.97,-1.48),(13,2.01,-1.48),(15.4,2.04,-1.44),
 (16.6,2.04,-1.27),(17.7,1.98,-.98),(18.7,1.83,-.52),
 (19.7,1.65,.10),(20.6,1.48,.73)]
verts=[];faces=[]
for x,top,bottom in sections:
 thick=.32*min(1.,max(.12,(21.52-x)/1.6))
 height=top-bottom;upper=min(.26,height*.22);lower=min(.64,height*.43)
 verts.extend([(x,-thick*.44,top),(x,thick*.44,top),
  (x,thick,top-upper),(x,thick,bottom+lower),(x,.018,bottom),
  (x,-.018,bottom),(x,-thick,bottom+lower),(x,-thick,top-upper)])
for j in range(len(sections)-1):
 for k in range(8):faces.append((8*j+k,8*j+(k+1)%8,8*(j+1)+(k+1)%8,8*(j+1)+k))
faces.append(tuple(reversed(range(8))))
last=(len(sections)-1)*8;tipid=len(verts);verts.append((21.52,0,1.32))
for k in range(8):faces.append((last+k,last+(k+1)%8,tipid))
data=bpy.data.meshes.new('K01_BladeRetopology');data.from_pydata(verts,[],faces);data.update()
blade=bpy.data.objects.new('K01_BladeRetopology',data);s.collection.objects.link(blade);data.materials.append(steel)
bpy.ops.object.select_all(action='DESELECT');blade.select_set(True);bpy.context.view_layer.objects.active=blade
bm=bmesh.new();bm.from_mesh(data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(data);bm.free()
# Planar charts for untextured blade sides; bevel/edge overlaps require packing before a future custom bake.
uv=data.uv_layers.new(name='UVMap')
for p in data.polygons:
 for li in p.loop_indices:
  co=data.vertices[data.loops[li].vertex_index].co
  uv.data[li].uv=((co.x-6.25)/15.27*.45+(.5 if p.normal.y>0 else 0), (co.z+1.5)/3.6*.9+.05)
bevel=blade.modifiers.new('Micro bevel highlights','BEVEL');bevel.width=.025;bevel.segments=2;bevel.affect='EDGES';bevel.limit_method='ANGLE';bevel.angle_limit=math.radians(25)
bpy.ops.object.modifier_apply(modifier=bevel.name)
# Retain flat face normals: broad blade planes must not inherit melted gradients.
for p in blade.data.polygons:p.use_smooth=False
blade.select_set(True);o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.join()
# Triangulate export and source identically, without changing flat blade normals.
tri=o.modifiers.new('Final triangles','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
# Discard imported custom loop normals after changing vertex positions.
o.data.normals_split_custom_set([(0.,0.,0.)]*len(o.data.loops))

# Origin is the center of the visual grip. Frames are presentation-only.
lo=Vector([min(v.co[i] for v in o.data.vertices) for i in range(3)]);hi=Vector([max(v.co[i] for v in o.data.vertices) for i in range(3)])
tip=max(o.data.vertices,key=lambda v:v.co.x).co.copy()
for name,loc in [('Grip',(0,0,0)),('Tip',tip)]:
 a=bpy.data.objects.new(name,None);s.collection.objects.link(a);a.empty_display_type='ARROWS';a.empty_display_size=1.3;a.location=loc;a.parent=o;a['purpose']='visual presentation frame only; no attack volume';a['forward']='+X';a['up']='+Z'
o['units']='centimeters';o['provenance']='Meshy 7.1 image-to-3D 01a1a6d1-dcbd-74dd-8074-6fd6634c4843; locally cleaned';o['collision_intent']='NoCollision equipped; gameplay absent'
# Remove unused importer datablocks so the blend contains only final portable materials.
for im in list(bpy.data.images):
 if im.users==0:bpy.data.images.remove(im)
bpy.ops.object.select_all(action='DESELECT')
for a in [o,bpy.data.objects['Grip'],bpy.data.objects['Tip']]:a.select_set(True)
bpy.context.view_layer.objects.active=o
bpy.ops.export_scene.fbx(filepath=str(R/'Exports/SM_VS_Knife01.fbx'),use_selection=True,object_types={'MESH','EMPTY'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_space_transform=True,bake_space_transform=False,mesh_smooth_type='FACE',use_mesh_modifiers=True,use_tspace=True,bake_anim=False,add_leaf_bones=False,path_mode='RELATIVE')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(R/'Knife01.blend'))
bm=bmesh.new();bm.from_mesh(o.data);top={'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'loose_vertices':sum(not v.link_faces for v in bm.verts),'degenerate_faces':sum(f.calc_area()<1e-10 for f in bm.faces)};bm.free()
report={'blender':bpy.app.version_string,'raw':raw,'post_weld_vertices':post_weld,'source_vertices':len(o.data.vertices),'source_triangles':sum(len(p.vertices)-2 for p in o.data.polygons),'material_slots':len(o.data.materials),'uv_layers':[u.name for u in o.data.uv_layers],'bounds_cm':[list(lo),list(hi)],'dimensions_cm':list(hi-lo),'object_location':list(o.location),'object_rotation':list(o.rotation_euler),'object_scale':list(o.scale),'Grip':{'location_cm':[0,0,0],'rotation_degrees':[0,0,0],'scale':[1,1,1]},'Tip':{'location_cm':list(tip),'rotation_degrees':[0,0,0],'scale':[1,1,1]},'topology':top,'cleanup':['Normalize actual Meshy source to cm/+X and a grip-local origin','Weld generated seams and remove degenerate edges','Preserve and relax actual Meshy grip/UVs; local 32 percent reduction','Locally retopologize the uneven blade to the measured Meshy silhouette, with planar cheeks, crisp bevels and UVs','Local guard/pommel retopology follows generated envelope; preserve textured charcoal grip and original burgundy accent direction; satin steel blade','User rejected earlier uneven surface correction; this is the subsequent local artist revision'],'fbx':{'axis_forward':'-Y','axis_up':'Z','apply_scale_options':'FBX_SCALE_UNITS','bake_space_transform':False}}
(DOC/'source-inspection.json').write_text(json.dumps(report,indent=2)+'\n')
print('K01_CLEANED',json.dumps(report),flush=True)
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
