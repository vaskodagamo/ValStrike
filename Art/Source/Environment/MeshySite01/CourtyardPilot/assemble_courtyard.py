"""ART-25 source assembly, sampled mating/normal inspection and actual source renders.
Context blocks/route guide represent retained Site01 geometry, not delivered art.
"""
import bpy,json,math,hashlib
from pathlib import Path
from mathutils import Vector
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[4];DOCS=ROOT/'docs/assets/meshy-site01/courtyard-pilot';PREV=DOCS/'Previews'
# Reuse the source scene helpers without executing the preparation loop.
h={'__file__':str(HERE/'prepare_sources.py')};exec((HERE/'prepare_sources.py').read_text().split('for aid,(kind,factor) in IDS.items():')[0],h)
report={'schema_version':1,'task_id':'ART-25','units':'cm','native_verified':False,'material_context':'Neutral proxy for unchanged Site01 walls/floor, actual selected Meshy source pieces.','export_surface_checks':{},'placements':[]}
# Targeted inspection of actual FBX corner surface data, not a new test harness.
for aid in ['MSH-W01','MSH-W08','MSH-W09']:
 m=json.loads((DOCS/(aid+'-manifest.json')).read_text());bpy.ops.wm.open_mainfile(filepath=str(ROOT/m['blend']))
 source=next(o for o in bpy.context.scene.objects if o.name.startswith('SM_VS_'))
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/m['fbx']),use_anim=False)
 rt=next(o for o in bpy.data.objects if o not in old and o.type=='MESH')
 # FBX retains loop ordering for these triangle-only inputs; establish that before comparing.
 topology_equal=[tuple(p.vertices) for p in source.data.polygons]==[tuple(p.vertices) for p in rt.data.polygons]
 assert topology_equal
 uv_error=max((a.uv-b.uv).length for a,b in zip(source.data.uv_layers[0].data,rt.data.uv_layers[0].data))
 normal_dot=min((source.matrix_world.to_3x3()@a.vector).normalized().dot((rt.matrix_world.to_3x3()@b.vector).normalized()) for a,b in zip(source.data.corner_normals,rt.data.corner_normals))
 pos_error=max((source.matrix_world@a.co-rt.matrix_world@b.co).length for a,b in zip(source.data.vertices,rt.data.vertices))
 assert uv_error<1e-6 and normal_dot>.99999 and pos_error<1e-5,(aid,uv_error,normal_dot,pos_error)
 report['export_surface_checks'][aid]={'polygon_indices_equal':topology_equal,'max_uv_error':uv_error,'min_corner_normal_dot':normal_dot,'max_position_error_cm':pos_error*100}
 if aid=='MSH-W08':
  center=source.dimensions.x/2; depth=source.dimensions.y/2
  samples=[]
  for z in [.25,1.0,1.52,2.0]:
   origin=Vector((center,depth,z));left=source.ray_cast(origin,Vector((-1,0,0)));right=source.ray_cast(origin,Vector((1,0,0)))
   samples.append({'height_cm':z*100,'width_cm':(right[1].x-left[1].x)*100 if left[0] and right[0] else None})
  up=source.ray_cast(Vector((center,depth,.01)),Vector((0,0,1)))
  report['doorway_aperture_samples']={'depth_cm':depth*100,'widths':samples,'center_lintel_height_cm':up[1].z*100 if up[0] else None,'native_use':'Backed blind recess only; no new traversable opening.'}

bpy.ops.wm.read_factory_settings(use_empty=True);s=h['scene_setup']();s.render.resolution_x=1400;s.render.resolution_y=900
sources={}
for aid in ['MSH-W01','MSH-W08','MSH-W09']:
 m=json.loads((DOCS/(aid+'-manifest.json')).read_text())
 with bpy.data.libraries.load(str(ROOT/m['blend']),link=False) as (data_in,data_out):data_out.objects=[n for n in data_in.objects if n.startswith('SM_VS_')]
 o=data_out.objects[0];bpy.context.collection.objects.link(o);o.hide_render=True;o.hide_set(True);sources[aid]=o
 # Make image references portable in the assembly file, without touching asset sources.
 for n in o.data.materials[0].node_tree.nodes:
  if n.type=='TEX_IMAGE':
   role=n.label
   rec=next(t for t in m['textures'] if t['role']==role)
   n.image.filepath=str(ROOT/rec['path'])
w=sources['MSH-W01'].dimensions.x;door=sources['MSH-W08'].dimensions.x;total=2*w+door+.02
recipe=[('WallWest','MSH-W01',(0,0,0)),('DoorRecess','MSH-W08',(w+.01,0,0)),('WallEast','MSH-W01',(w+door+.02,0,0))]
roof_start=(total-6.02)/2
for i in range(3):recipe.append(('RoofParapet'+str(i+1),'MSH-W09',(roof_start+i*2.01,.10,3.94)))
placed={}
for label,aid,loc in recipe:
 o=sources[aid].copy();o.data=sources[aid].data;bpy.context.collection.objects.link(o);o.name='ART25_'+label;o.hide_render=False;o.hide_set(False);o.location=loc;placed[label]=o
 bpy.context.view_layer.update();b=h['bounds'](o)
 # Native FBX Y-negation is already observed for this exporter by INT-ART-01.
 record={'label':'VS_ART25_'+label,'asset_id':aid,'source_local_pivot_m':list(loc),'native_expected_pivot_cm':[400+loc[0]*100,1510-loc[1]*100,loc[2]*100],'native_rotation_pitch_yaw_roll':[0,0,0],'scale':[1,1,1],'expected_native_world_bounds_cm':{'min':[400+b[0][0]*100,1510-b[1][1]*100,b[0][2]*100],'max':[400+b[1][0]*100,1510-b[0][1]*100,b[1][2]*100]},'collision':'NoCollision; retain all pre-existing Site01 blockers'}
 report['placements'].append(record)
report['facade_width_cm']=total*100
report['source_to_world']='Source assembly (U,V,Z) meters maps to native world cm (400+100U,1510-100V,100Z). Native pivot assumptions require measured import bounds; no negative actor scale.'
report['mating_samples']=[]
for lhs,rhs in [('WallWest','DoorRecess'),('DoorRecess','WallEast')]:
 a,b=placed[lhs],placed[rhs];x=(h['bounds'](a)[1][0]+h['bounds'](b)[0][0])/2
 for y in [.03,.08]:
  for z in [.25,1.,1.52,2.5,2.9]:
   orig=Vector((x,y,z));ra=a.ray_cast(a.matrix_world.inverted()@orig,Vector((-1,0,0)));rb=b.ray_cast(b.matrix_world.inverted()@orig,Vector((1,0,0)))
   report['mating_samples'].append({'joint':lhs+'/'+rhs,'depth_cm':y*100,'height_cm':z*100,'gap_cm':((b.matrix_world@rb[1]).x-(a.matrix_world@ra[1]).x)*100 if ra[0] and rb[0] else None,'interpretation':'Sampled side-profile gap; retained wall is backing. Not watertight structural mating.'})

def material(name,color):
 m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;bs=m.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(*color,1);bs.inputs['Roughness'].default_value=.85;return m
wall=material('CONTEXT_existing_service_wall',(.47,.46,.43));roof=material('CONTEXT_existing_roof_band',(.09,.12,.14));floor=material('CONTEXT_existing_floor',(.16,.185,.195));guide=material('GUIDE_route_not_game_art',(.1,.48,.55))
def box(name,center,size,mat):
 bpy.ops.mesh.primitive_cube_add(size=1,location=center);o=bpy.context.object;o.name=name;o.dimensions=size;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mat);return o
box('CONTEXT_NorthService_envelope',(2,1.35,1.9),(20,2.5,3.8),wall)
box('CONTEXT_ServiceRoofBand',(2,1.35,3.87),(20,2.5,.14),roof)
box('CONTEXT_Floor',(2,-1.4,-.05),(20,8,.1),floor)
# Dashed source-only line at the unchanged Service route centerline Y=1750.
for i in range(12):box('GUIDE_Service_route_centerline',(i*.6,-2.4,.006),(.3,.025,.01),guide)
for loc,power,area in [((2,-5,9),2200,7),((-4,2,6),1200,6)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.size=area;l.rotation_euler=(Vector((3,0,2))-l.location).to_track_quat('-Z','Y').to_euler()

def capture(name,pos,target,ortho=None,lens=30):
 bpy.ops.object.camera_add(location=pos);c=bpy.context.object;c.name=name;c.rotation_euler=(Vector(target)-c.location).to_track_quat('-Z','Y').to_euler();c.data.lens=lens
 if ortho:c.data.type='ORTHO';c.data.ortho_scale=ortho
 s.camera=c;s.render.filepath=str(PREV/(name+'.png'));bpy.ops.render.render(write_still=True);return c
capture('Assembly_eye_152cm',(total/2,-8,1.52),(total/2,0,2.3),lens=33)
capture('Assembly_service_approach',(-3,-2.4,1.52),(2.9,0,1.85),lens=28)
capture('Assembly_overhead',(total/2,-.6,14),(total/2,-.6,0),ortho=10)
capture('Assembly_mating',(w+.15,-1.8,1.65),(w,.06,1.6),ortho=1.25)
# Roof end-cap mating evidence: adjacent cap extremities are 1 cm apart; do not call seamless.
capture('Assembly_roof_joint',(roof_start+2.01,-1.5,5.5),(roof_start+2.005,.27,4.2),ortho=1.6)
for im in bpy.data.images:
 if im.source=='FILE':im.filepath=bpy.path.relpath(im.filepath,start=str(HERE))
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'CourtyardAssembly.blend'))
(DOCS/'assembly-measurements.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART25_ASSEMBLY_DONE',flush=True)
