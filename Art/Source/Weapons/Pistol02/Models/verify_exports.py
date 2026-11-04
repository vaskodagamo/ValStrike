"""One focused saved-source/FBX round-trip, no Unreal or test suite."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[4];DOC=REPO/'docs/assets/pistol02'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'P02_Kestrel.blend'))
sc=bpy.context.scene
parts=[o for o in sc.objects if o.type=='MESH'];ref={}
def stats(o):
 o.data.calc_loop_triangles();vs=[o.matrix_world@v.co for v in o.data.vertices]
 return {'bounds':[[min(v[i] for v in vs) for i in range(3)],[max(v[i] for v in vs) for i in range(3)]],'triangles':len(o.data.loop_triangles),'uv_layers':len(o.data.uv_layers),'material_slots':len(o.data.materials)}
for o in parts:ref[o.name]=stats(o)
report={'saved_blend_reopened':True,'packed_images':[{'name':i.name,'size':list(i.size),'packed':bool(i.packed_file)} for i in bpy.data.images if i.source=='FILE'],'parts':{},'anchors':{o.name:list(o.location) for o in sc.objects if o.type=='EMPTY'}}
body=next(o for o in parts if o.name.endswith('Body'))
vs=[v.co for v in body.data.vertices if -3.5<v.co.z<-2.5 and v.co.x<2]
report['grip_cross_section_z_minus3_cm']={'x_range':[min(v.x for v in vs),max(v.x for v in vs)],'y_range':[min(v.y for v in vs),max(v.y for v in vs)],'method':'Vertex band z=-3 +/-0.5 cm, x<2; silhouette envelope, not internal clearance'}
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
for name,expected in ref.items():
 before=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/(name+'.fbx')),use_image_search=False)
 added=set(bpy.data.objects)-before;o=next(o for o in added if o.type=='MESH');actual=stats(o);error=max(abs(x-y) for a,b in zip(expected['bounds'],actual['bounds']) for x,y in zip(a,b))
 assert error<.001,(name,error)
 assert actual['triangles']==expected['triangles']
 assert actual['uv_layers']==expected['uv_layers']
 report['parts'][name]={'source':expected,'fbx':actual,'max_bound_error_cm':error,'passed':True}
 for o in added:bpy.data.objects.remove(o,do_unlink=True)
fit=json.loads((DOC/'fit-measurements.json').read_text());mat=Matrix(fit['hand_r_bone_relative_matrix']);fit['hand_r_relative_location_cm']=list(mat.translation);fit['hand_r_relative_rotation_xyz_degrees']=[math.degrees(v) for v in mat.to_euler('XYZ')]
points=fit['bones_in_weapon_grip_frame_cm'];fit['support_hand_wrist_distance_to_grip_cm']=Vector(points['hand_l']).length
fit['right_index_distal_joint_local_cm']=points['index_03_r'];fit['visual_fit_review']='Palm is accommodated at scale 1. Frozen fingers overlap the lower trigger guard; support hand is forward. Pistol-specific finger/left-hand posing is required; no fit acceptance claimed.'
(DOC/'fit-measurements.json').write_text(json.dumps(fit,indent=2)+'\n')
(DOC/'export-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('ROUNDTRIP_PASSED',json.dumps(report),flush=True)
