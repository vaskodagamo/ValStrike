"""Source reopen/FBX roundtrip and read-only frozen-character fit evidence.
Does not save a character or include character geometry in weapon exports.
"""
import bpy,json,hashlib,math
from pathlib import Path
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree
R=Path(__file__).resolve().parent;REPO=R.parents[3];DOC=REPO/'docs/assets/knife01';PRE=DOC/'Previews'
bpy.ops.wm.open_mainfile(filepath=str(R/'Knife01.blend'));s=bpy.context.scene;o=bpy.data.objects['SM_VS_Knife01']
def bounds(ob):
 ps=[ob.matrix_world@v.co for v in ob.data.vertices];return [[min(v[i] for v in ps) for i in range(3)],[max(v[i] for v in ps) for i in range(3)]]
ref=bounds(o);tris=sum(len(p.vertices)-2 for p in o.data.polygons);frames={n:[list(r) for r in bpy.data.objects[n].matrix_world] for n in ['Grip','Tip']}
report={'source_reopened':True,'source_only_objects':[(x.name,x.type) for x in s.objects],'textures':[]}
assert {x.name for x in s.objects}=={'SM_VS_Knife01','Grip','Tip'}
for im in bpy.data.images:
 if im.users and im.source=='FILE':
  p=Path(bpy.path.abspath(im.filepath));assert p.is_file();im.reload();report['textures'].append({'path':str(p.relative_to(REPO)),'size':list(im.size),'color_space':im.colorspace_settings.name});assert min(im.size)>=2048
for x in list(bpy.data.objects):bpy.data.objects.remove(x,do_unlink=True)
bpy.ops.import_scene.fbx(filepath=str(R/'Exports/SM_VS_Knife01.fbx'),use_image_search=False)
imp=next(x for x in s.objects if x.type=='MESH');actual=bounds(imp);err=max(abs(a-b) for row,row2 in zip(ref,actual) for a,b in zip(row,row2));assert err<.001
assert sum(len(p.vertices)-2 for p in imp.data.polygons)==tris
frame_error={n:max(abs(a-b) for ra,rb in zip(frames[n],bpy.data.objects[n].matrix_world) for a,b in zip(ra,rb)) for n in frames};assert max(frame_error.values())<.001
report['fbx_roundtrip']={'bounds_cm':actual,'max_bounds_error_cm':err,'triangles':tris,'frames_max_matrix_error':frame_error,'material_slots':len(imp.data.materials),'uv_layers':[u.name for u in imp.data.uv_layers]}
# Actual frozen character, unchanged source, rig, pose, and scale.
P=REPO/'Art/Source/Characters/UrbanEntry01/UrbanEntry01_Pilot.blend';before=hashlib.sha256(P.read_bytes()).hexdigest();bpy.ops.wm.open_mainfile(filepath=str(P));s=bpy.context.scene;s.frame_set(1)
a=bpy.data.objects['SKEL_UE01'];skin=bpy.data.objects['SK_VS_UrbanEntry01'];bpy.context.view_layer.update()
for x in list(s.objects):
 if x.name.startswith('PREVIEW_ONLY_'):bpy.data.objects.remove(x,do_unlink=True)
with bpy.data.libraries.load(str(R/'Knife01.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Knife01','Grip','Tip']
for x in dst.objects:s.collection.objects.link(x)
o=bpy.data.objects['SM_VS_Knife01']
# Long axis passes across the existing curled hand, from little finger toward index.
index=a.matrix_world@a.pose.bones['index_01_r'].head;pinky=a.matrix_world@a.pose.bones['pinky_01_r'].head
x=(index-pinky).normalized();z=Vector((1,0,0));z=(z-x*z.dot(x)).normalized();y=z.cross(x).normalized();z=x.cross(y).normalized()
world=Matrix(((x.x,y.x,z.x,23.3),(x.y,y.y,z.y,-18.7),(x.z,y.z,z.z,132.8),(0,0,0,1)))
o.matrix_world=world;bpy.context.view_layer.update();hand=a.matrix_world@a.pose.bones['hand_r'].matrix;local=hand.inverted()@world
# Surface overlap is a diagnostic of this inherited rifle hand pose, not a gameplay collision test.
e=skin.evaluated_get(bpy.context.evaluated_depsgraph_get());em=e.to_mesh();names={g.index for g in skin.vertex_groups if g.name.endswith('_r') and any(w in g.name for w in ['hand','index','middle','ring','pinky','thumb'])};ids={v.index for v in skin.data.vertices if sum(g.weight for g in v.groups if g.group in names)>.5};polys=[list(p.vertices) for p in em.polygons if all(i in ids for i in p.vertices)]
hb=BVHTree.FromPolygons([skin.matrix_world@v.co for v in em.vertices],polys);kb=BVHTree.FromPolygons([o.matrix_world@v.co for v in o.data.vertices],[list(p.vertices) for p in o.data.polygons]);overlap=hb.overlap(kb);e.to_mesh_clear()
intersected={j for i,j in overlap};part_counts={'grip':0,'guard':0,'blade':0}
for j in intersected:
 px=sum(o.data.vertices[i].co.x for i in o.data.polygons[j].vertices)/len(o.data.polygons[j].vertices);part_counts['blade' if px>7.3 else 'guard' if px>5.3 else 'grip']+=1
report['fit']={'character_source':str(P.relative_to(REPO)),'character_sha256':before,'character_mesh_scale':list(skin.scale),'armature_scale':list(a.scale),'character_pose':'A_UE01_Pilot_RifleHold frame 1; unchanged','weapon_scale':list(o.scale),'knife_component_matrix':[list(r) for r in world],'hand_r_head_local_matrix':[list(r) for r in local],'handedness':'right-hand reference; no mirror applied','hold':'Blade exits index side of inherited curled rifle hand; reference only, no knife animation','hand_surface_intersection_pairs':len(overlap),'intersecting_knife_faces_by_region':part_counts,'knuckle_index_to_pinky_cm':(index-pinky).length,'native_socket_conversion':'Convert bone head matrix to UE import axes after inspecting actual imported skeleton; not raw UE Euler values.'}
# Use Workbench for texture-visible same-scale fit inspection, no invented hand mesh.
s.render.engine='BLENDER_WORKBENCH';s.display.shading.light='STUDIO';s.display.shading.studiolight_rotate_z=.5;s.display.shading.color_type='TEXTURE';s.display.shading.show_shadows=True;s.display.shading.show_cavity=True;s.display.shading.show_specular_highlight=False;s.display.shading.background_type='WORLD';s.world.color=(.08,.09,.11)
s.render.resolution_x=1100;s.render.resolution_y=900;s.render.resolution_percentage=100
cam=s.camera;cam.data.type='ORTHO'
def render(name,pos,target,size):
 cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=size;s.render.filepath=str(PRE/(name+'.png'));bpy.ops.render.render(write_still=True)
render('fit_right_hand',(90,-140,160),(23,-18,140),44)
render('fit_reverse',(105,35,200),(23,-18,138),43)
render('fit_character',(270,-430,180),(10,0,85),195)
s.render.engine='CYCLES';s.cycles.samples=16;s.cycles.use_denoising=True
render('fit_right_hand_pbr',(90,-140,160),(23,-18,140),44)
assert hashlib.sha256(P.read_bytes()).hexdigest()==before
report['character_file_unchanged']=True;report['unreal_runs']=0;report['render_method']='Blender Workbench texture studio plus Cycles actual-material close-up; actual frozen character same scale; reference-only knife hold'
(DOC/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print('K01_SOURCE_EXPORT_FIT_VERIFIED',json.dumps(report),flush=True)
