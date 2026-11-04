"""ART-23 focused contact diagnostics, not a collision or anatomical proof.
Nearest-normal sign on an open mesh is only a local penetration proxy. No pass
assertion is derived from it; the actual matched rendered views govern review.
"""
import bpy,json
from pathlib import Path
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parent.parents[3];DOC=REPO/'docs/assets/urban-entry01/pistol-pilot'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01_PistolPilot.blend'))
for c in bpy.data.collections:
    if c.name.startswith('PREVIEW_ONLY_'):c.hide_viewport=False
bpy.context.view_layer.update()
m=bpy.data.objects['SK_VS_UrbanEntry01'];e=m.evaluated_get(bpy.context.evaluated_depsgraph_get());em=e.to_mesh();em.calc_loop_triangles()
verts=[e.matrix_world@v.co for v in em.vertices];sides={}
for side in ('l','r'):
    ids={g.index for g in m.vertex_groups if g.name.endswith('_'+side) and any(n in g.name for n in ('hand','thumb','index','middle','ring','pinky'))}
    selected={v.index for v in m.data.vertices if sum(g.weight for g in v.groups if g.group in ids)>.8}
    tris=[list(t.vertices) for t in em.loop_triangles if all(v in selected for v in t.vertices)]
    sides[side]=(selected,BVHTree.FromPolygons(verts,tris,all_triangles=True))
report={'limitations':'Selected skin vertices, not continuous collision. Nearest-normal sign is not a reliable inside test for open/multipart gun meshes. Visual review still required.','hand_to_hand':{},'weapons':{}}
for side in ('l','r'):
    other='r' if side=='l' else 'l';negative=[];distances=[]
    for i in sides[side][0]:
        pos,n,idx,dist=sides[other][1].find_nearest(verts[i]);distances.append(dist)
        if n.dot(verts[i]-pos)<0 and dist>.05:negative.append(dist)
    report['hand_to_hand'][side]={'selected_vertices':len(distances),'nearest_surface_distance_cm':min(distances),'negative_normal_samples_deeper_than_0_05cm':len(negative),'max_negative_normal_depth_cm':max(negative,default=0)}
for ident in ('P01','P02'):
    obj=bpy.data.objects['SM_VS_Pistol'+ident[1:]+'_Slide'];obj.data.calc_loop_triangles()
    vv=[obj.matrix_world@v.co for v in obj.data.vertices];tree=BVHTree.FromPolygons(vv,[t.vertices for t in obj.data.loop_triangles],all_triangles=True)
    chosen=sides['r'][0]|sides['l'][0]
    body=bpy.data.objects['SM_VS_Pistol'+ident[1:]+'_Body'];local=[body.matrix_world.inverted()@verts[i] for i in chosen]
    report['weapons'][ident]={'minimum_selected_hand_vertex_to_slide_surface_cm':min(tree.find_nearest(verts[i])[3] for i in chosen),
        'maximum_hand_vertex_x_in_weapon_cm':max(v.x for v in local),'muzzle_anchor_x_cm':16.7 if ident=='P01' else 15.5,
        'hand_vertices_behind_muzzle_anchor_cm':(16.7 if ident=='P01' else 15.5)-max(v.x for v in local)}
e.to_mesh_clear()
(DOC/'contact-diagnostics.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
