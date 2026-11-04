"""Focused K01 grip/off-hand source inspection, supplementing the real views."""
import bpy,json,math
from pathlib import Path
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[4]/'docs/assets/fparms03/knife-pilot'
contract=json.loads((DOC/'animation-contract.json').read_text());attachment=Matrix(contract['attachment']['weapon_local_from_socket'])
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'KnifePilot.blend'))
scene=bpy.context.scene;arm=bpy.data.objects['SKEL_FPArms03'];right=bpy.data.objects['SK_FPArms03_R'];left=bpy.data.objects['SK_FPArms03_L']
with bpy.data.libraries.load(str(ROOT/'Reference/Frozen/K01.blend'),link=False) as (src,dst):dst.objects=['SM_VS_Knife01']
weapon=dst.objects[0]
tree=BVHTree.FromPolygons([weapon.matrix_world@v.co for v in weapon.data.vertices],[tuple(p.vertices) for p in weapon.data.polygons],all_triangles=False)
def inside(point):
    votes=0
    for direction in [Vector((1,.123,.234)).normalized(),Vector((.231,1,.173)).normalized(),Vector((.137,.297,1)).normalized()]:
        origin=point.copy();hits=0
        for attempt in range(32):
            location,n,face,distance=tree.ray_cast(origin,direction,100)
            if location is None:break
            hits+=1;origin=location+direction*.0001
        votes+=hits%2
    return votes>=2
report={'method':'Sparse vertices, nearest K01 surface and majority of three ray-parity directions; 0.05 cm threshold. Supplement to images, not exhaustive collision or swept-volume acceptance.','grip':{},'samples':{},'dense_deformation':{}}
reference=None
for kind,clip in contract['clips'].items():
    a=bpy.data.actions[clip['action']];arm.animation_data.action=a;arm.animation_data.action_slot=a.slots[0]
    frames=sorted(set([1,(clip['end']+1)//2,clip.get('impact_frame',1),clip['end']]))
    for frame in frames:
        scene.frame_set(frame);bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();inverse=(arm.matrix_world@arm.pose.bones['weapon_socket_r'].matrix@attachment).inverted()
        rm=right.evaluated_get(dg).data;lm=left.evaluated_get(dg).data
        points=[inverse@(right.matrix_world@rm.vertices[i].co) for i in range(0,len(rm.vertices),100)]
        if reference is None:reference=points
        deviation=max((a-b).length for a,b in zip(points,reference))
        off_dist=min(tree.find_nearest(inverse@(left.matrix_world@lm.vertices[i].co))[3] for i in range(0,len(lm.vertices),100))
        report['samples'][f'{kind}:{frame}']={'sampled_off_hand_min_surface_distance_cm':off_dist,'right_arm_shape_vs_attachment_max_deviation_cm':deviation}
        if kind=='Idle' and frame==1:
            groups={g.index:g.name for g in right.vertex_groups}
            for i in range(0,len(rm.vertices),20):
                v=right.data.vertices[i];group=groups[max(v.groups,key=lambda g:g.weight).group]
                if 'forearm' in group:continue
                point=inverse@(right.matrix_world@rm.vertices[i].co);hit,normal,face,distance=tree.find_nearest(point);signed=(point-hit).dot(normal)
                entry=report['grip'].setdefault(group,{'sampled':0,'inside_over_0_05cm':0,'max_depth_cm':0});entry['sampled']+=1
                if signed<-.05 and inside(point):
                    entry['inside_over_0_05cm']+=1
                    if -signed>entry['max_depth_cm']:entry['max_depth_cm']=-signed;entry['deepest_point_K01_cm']=list(point)
            for o,m in [(right,rm),(left,lm)]:
                ratios=[]
                for e in o.data.edges:
                    i,j=e.vertices;length=(o.data.vertices[i].co-o.data.vertices[j].co).length
                    if length>.015:ratios.append((m.vertices[i].co-m.vertices[j].co).length/length)
                ratios.sort();report['dense_deformation'][o.name]={'edge_ratio_p99':ratios[int(len(ratios)*.99)],'edge_ratio_max':max(ratios)}
(DOC/'contact-inspection.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART26_CONTACT_INSPECTION_COMPLETE')
