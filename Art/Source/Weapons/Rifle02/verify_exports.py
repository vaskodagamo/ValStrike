"""One focused saved-source and FBX scale/parts/anchor round-trip."""
import bpy,bmesh,json,hashlib,math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[3]/'docs/assets/rifle02'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'R02_Compact.blend'))
source=json.loads((DOC/'source-measurements.json').read_text());report={'blender':bpy.app.version_string,'source_reopened':True,'parts':{},'packed_images':[]}
for image in bpy.data.images:
 if image.users:report['packed_images'].append({'name':image.name,'size':list(image.size),'packed':bool(image.packed_file),'colorspace':image.colorspace_settings.name})
for name,ref in source['parts'].items():
 obj=bpy.data.objects[name];bm=bmesh.new();bm.from_mesh(obj.data)
 topo={'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'boundary_details':[{'points':[list(v.co) for v in e.verts],'length_cm':e.calc_length(),'linked_faces':len(e.link_faces)} for e in bm.edges if e.is_boundary]};bm.free()
 obj.data.calc_loop_triangles();assert len(obj.data.loop_triangles)==ref['triangles'];assert tuple(obj.scale)==(1,1,1);assert all(abs(x)<1e-6 for x in obj.rotation_euler)
 report['parts'][name]={'source_topology':topo}
for name,ref in source['anchors'].items():
 obj=bpy.data.objects['SOCKET_'+name];assert (obj.location-Vector(ref['location_cm'])).length<1e-5
report['anchors_match_manifest']=True
sc=bpy.data.scenes.new('ROUNDTRIP');sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=.01;bpy.context.window.scene=sc
for name,ref in source['parts'].items():
 before=set(sc.objects);bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/(name+'.fbx')),use_image_search=False,use_anim=False);added=set(sc.objects)-before;obj=next(o for o in added if o.type=='MESH');obj.data.calc_loop_triangles();vs=[obj.matrix_world@v.co for v in obj.data.vertices];lo=[min(v[i] for v in vs) for i in range(3)];hi=[max(v[i] for v in vs) for i in range(3)];err=max(abs(a-b) for a,b in zip(lo+hi,ref['local_bounds']['min']+ref['local_bounds']['max']))
 print('CHECK',name,report['parts'][name]['source_topology'],err,len(obj.data.loop_triangles),ref['triangles'],flush=True);assert err<.002,(name,err);assert len(obj.data.loop_triangles)==ref['triangles'];assert len(obj.data.uv_layers)==1;assert obj.location.length<1e-5;assert all(abs(s-1)<1e-5 for s in obj.scale)
 report['parts'][name]['roundtrip']={'triangles':len(obj.data.loop_triangles),'max_bounds_error_cm':err,'uv_layers':[u.name for u in obj.data.uv_layers],'material_slots':len(obj.data.materials),'object_location_cm':list(obj.location),'object_scale':list(obj.scale),'rotation_xyz_deg':[math.degrees(x) for x in obj.rotation_euler]}
 for o in added:bpy.data.objects.remove(o,do_unlink=True)
report['files']={str(p.relative_to(ROOT)):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in [ROOT/'R02_Compact.blend',*sorted((ROOT/'Exports').glob('*.fbx')),*sorted((ROOT/'Textures').glob('*.png'))]}
(DOC/'export-verification.json').write_text(json.dumps(report,indent=2)+'\n');print('R02_ROUNDTRIP_PASS',json.dumps(report),flush=True)
