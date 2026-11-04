import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[5]; D=R/'docs/assets/pistol02'
bpy.ops.wm.open_mainfile(filepath=str(R/'Artifacts/Meshy/P02/character-reference/UrbanEntry01.blend'))
a=bpy.data.objects['SKEL_UE01']; m=bpy.data.objects['SK_VS_UrbanEntry01']
r={'objects':[(o.name,o.type,list(o.dimensions)) for o in bpy.context.scene.objects], 'bones':{b.name:{'head':list(a.matrix_world@b.head_local),'tail':list(a.matrix_world@b.tail_local)} for b in a.data.bones},'actions':[x.name for x in bpy.data.actions]}
x=bpy.data.actions['A_UE01_Walk_Backward_with_Gun'];a.animation_data_create();a.animation_data.action=x;a.animation_data.action_slot=x.slots[0];bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
r['hold_frame1']={b.name:{'head':list(a.matrix_world@b.head),'tail':list(a.matrix_world@b.tail),'matrix':[list(row) for row in (a.matrix_world@b.matrix)]} for b in a.pose.bones}
keep=['hand_r','hand_l','head','index_01_r','index_03_r','middle_01_r','middle_03_r','thumb_01_r','thumb_03_r']
r['bones']={k:v for k,v in r['bones'].items() if k in keep};r['hold_frame1']={k:v for k,v in r['hold_frame1'].items() if k in keep}
(D/'reference-inspection.json').write_text(json.dumps(r,indent=2))
print('REFERENCE_INSPECTED')
