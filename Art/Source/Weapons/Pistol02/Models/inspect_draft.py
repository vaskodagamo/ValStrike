import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[5]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(R/'Art/Source/Weapons/Pistol02/Originals/model.glb'))
r=[]
for o in bpy.context.scene.objects:
 if o.type=='MESH':
  o.data.calc_loop_triangles();r.append(dict(name=o.name,dimensions=list(o.dimensions),matrix=[list(row) for row in o.matrix_world],bounds=[list(v) for v in o.bound_box],triangles=len(o.data.loop_triangles)))
print(json.dumps(r));(R/'docs/assets/pistol02/draft-inspection.json').write_text(json.dumps(r,indent=2))
