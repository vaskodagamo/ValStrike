import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[4];D=R/'docs/assets/knife01';P=R/'Art/Source/Characters/UrbanEntry01/UrbanEntry01_Pilot.blend'
bpy.ops.wm.open_mainfile(filepath=str(P));s=bpy.context.scene;s.frame_set(1);a=bpy.data.objects['SKEL_UE01'];m=bpy.data.objects['SK_VS_UrbanEntry01'];bpy.context.view_layer.update()
names=[b.name for b in a.pose.bones if b.name.endswith('_r') and any(w in b.name for w in ['hand','index','middle','ring','pinky','thumb'])]
bones={n:{'head':list(a.matrix_world@a.pose.bones[n].head),'tail':list(a.matrix_world@a.pose.bones[n].tail),'matrix':[list(r) for r in a.matrix_world@a.pose.bones[n].matrix]} for n in names}
e=m.evaluated_get(bpy.context.evaluated_depsgraph_get());em=e.to_mesh();idx={g.index for g in m.vertex_groups if g.name in names};v=[m.matrix_world@em.vertices[x.index].co for x in m.data.vertices if sum(g.weight for g in x.groups if g.group in idx)>.5];e.to_mesh_clear()
report={'character_source':str(P.relative_to(R)),'character_source_sha256':hashlib.sha256(P.read_bytes()).hexdigest(),'character_scale':list(m.scale),'armature_scale':list(a.scale),'bones_cm':bones,'hand_weighted_vertices_bounds_cm':[[min(p[i] for p in v) for i in range(3)],[max(p[i] for p in v) for i in range(3)]]}
(D/'hand-reference.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'bones':{n:dict(head=b['head'],tail=b['tail']) for n,b in bones.items()},'bounds':report['hand_weighted_vertices_bounds_cm']}))
