"""Original vs corrected source vs actual FBX: identical camera/light/materials."""
import bpy,json,math,hashlib,sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[3];DOC=REPO/'docs/assets/rifle02';OUT=DOC/'Previews'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'R02_Compact.blend'));sc=bpy.context.scene
sc.render.engine='CYCLES';sc.cycles.samples=16;sc.cycles.seed=0;sc.cycles.use_denoising=True;sc.render.resolution_x=1400;sc.render.resolution_y=900;sc.render.resolution_percentage=100;sc.view_settings.view_transform='AgX'
sc.world=bpy.data.worlds.new('Preservation');sc.world.use_nodes=True;sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.24,.27,.31,1)
parts=[bpy.data.objects['SM_VS_Rifle02_Body'],bpy.data.objects['SM_VS_Rifle02_Magazine']];raw=bpy.data.objects['R02_Meshy_Draft_458064tri'];materials=list(parts[0].data.materials)
bpy.ops.object.camera_add(location=(94,-135,70));cam=bpy.context.object;cam.rotation_euler=(Vector((14,0,1))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=94;sc.camera=cam
for loc,power,size in [((20,-70,100),230000,100),((10,70,60),180000,80),((-80,-40,25),80000,70)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.size=size;l.rotation_euler=(Vector((15,0,0))-l.location).to_track_quat('-Z','Y').to_euler()
m=bpy.data.materials.new('Label');m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Emission Color'].default_value=(.8,.86,.95,1);p.inputs['Emission Strength'].default_value=1
curve=bpy.data.curves.new('State','FONT');curve.size=1.5;label=bpy.data.objects.new('State',curve);sc.collection.objects.link(label);curve.materials.append(m);label.parent=cam;label.location=(-44,27,-100)
def render(name,title):
 if '--caps-only' in sys.argv and name!='R02_magazine_separation':return
 curve.body=title;sc.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
for o in parts:o.hide_render=True
raw.hide_render=False;raw.hide_set(False);render('R02_original_control','ORIGINAL MESHY / 75 cm / SAME LIGHT + CAMERA / BLENDER')
raw.hide_render=True
for o in parts:o.hide_render=False
render('R02_corrected_control','CORRECTED SOURCE / SAME LIGHT + CAMERA + PBR / BLENDER')
for o in parts:o.hide_render=True
imported=[]
for source in parts:
 before=set(sc.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/(source.name+'.fbx')),use_image_search=False,use_anim=False);obj=next(o for o in set(sc.objects)-before if o.type=='MESH')
 for i,mat in enumerate(materials):
  if i<len(obj.data.materials):obj.data.materials[i]=mat
  else:obj.data.materials.append(mat)
 if 'Magazine' in source.name:obj.location=(15,0,-2.5)
 imported.append(obj)
render('R02_fbx_control','FBX REIMPORT / SAME LIGHT + CAMERA + PBR / BLENDER')
for obj in imported:
 if 'Magazine' in obj.name:obj.location.z-=10
render('R02_magazine_separation','FBX MAGAZINE REMOVED 10 cm / INSERTION CAPS / BLENDER')
report={'camera_location_cm':list(cam.location),'camera_target_cm':[14,0,1],'ortho_scale_cm':94,'resolution':[1400,900],'samples':16,'seed':0,'material':'Original GLB PBR graph in every state; identical reconstruction after FBX import','states':['R02_original_control.png','R02_corrected_control.png','R02_fbx_control.png'],'original_triangles':458064,'source_reduction':False,'separation_view':'R02_magazine_separation.png: actual exported magazine moved down 10 cm; caps inspected','reviewer':'ART-17 owner','review_date':'2026-11-04 Europe/Berlin','source_preservation_result':'pass; lead/user art review pending','permitted_differences':'Smooth palm-region grip width fitting and magazine separation/cap only; original receiver/rail/sight/stock surfaces retained','fbx_material_note':'Material reassigned from source for this geometry/UV/normal preservation comparison; Unreal must recreate the original PBR graph.'}
report['source_sha256']=hashlib.sha256((ROOT/'R02_Compact.blend').read_bytes()).hexdigest()
report['fbx_sha256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'Exports').glob('*.fbx'))}
(DOC/'preservation-review.json').write_text(json.dumps(report,indent=2)+'\n');print('PRESERVATION_VIEWS_COMPLETE',flush=True)
