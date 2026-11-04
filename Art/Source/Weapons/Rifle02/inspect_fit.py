import bpy,json
bpy.ops.wm.open_mainfile(filepath='Artifacts/Meshy/R02/draft.blend')
o=bpy.data.objects['Mesh_0']
regs={'grip':[v.co for v in o.data.vertices if 12<v.co.x<24 and -11<v.co.z<-3], 'mag':[v.co for v in o.data.vertices if -6<v.co.x<4 and v.co.z<-2]}
print('REGIONS', {k:([min(v[i] for v in vs) for i in range(3)],[max(v[i] for v in vs) for i in range(3)]) for k,vs in regs.items()})
bpy.ops.wm.open_mainfile(filepath='Artifacts/Meshy/R02/character-reference/UrbanEntry01.blend')
a=bpy.data.objects['SKEL_UE01'];a.animation_data_create();d={}
for n in ['A_UE01_Walk_Backward_While_Shooting','A_UE01_Crouch_Walk_Left_with_Gun']:
 a.animation_data.action=bpy.data.actions[n];a.animation_data.action_slot=a.animation_data.action.slots[0];bpy.context.scene.frame_set(1);d[n]={p.name:list(a.matrix_world@p.head) for p in a.pose.bones if p.name in ['head','pelvis','hand_l','hand_r','middle_01_r','middle_03_r_end','index_01_r','upperarm_r']}
print('POSES',json.dumps(d))
