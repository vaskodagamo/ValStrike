import bpy,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parents[3]; DOC=REPO/'docs/assets/rifle02'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(REPO/'Artifacts/Meshy/R02/2026-11-04/originals/model.glb'))
o=next(x for x in bpy.context.scene.objects if x.type=='MESH')
coords=[v.co.copy() for v in o.data.vertices]; lo=Vector([min(v[i] for v in coords) for i in range(3)]); hi=Vector([max(v[i] for v in coords) for i in range(3)])
s=75/(hi.x-lo.x)
for v in o.data.vertices: v.co=(v.co-(lo+hi)/2)*s
sc=bpy.context.scene; sc.render.engine='CYCLES';sc.cycles.samples=12;sc.cycles.use_denoising=True
sc.world=bpy.data.worlds.new('World');sc.world.use_nodes=True;sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.23,.26,.30,1)
sc.render.resolution_x=1400;sc.render.resolution_y=650;sc.render.resolution_percentage=100
bpy.ops.object.camera_add(location=(0,-170,0));cam=bpy.context.object;cam.rotation_euler=(Vector((0,0,0))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=84;sc.camera=cam
for loc,power,size in [((0,-60,70),180000,90),((0,40,20),100000,80)]:
 bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=power;l.data.shape='DISK';l.data.size=size;l.rotation_euler=(-l.location).to_track_quat('-Z','Y').to_euler()
sc.view_settings.view_transform='AgX';sc.render.image_settings.file_format='PNG';sc.render.filepath=str(DOC/'Previews/draft_side.png');bpy.ops.render.render(write_still=True)
print('RAW_BOUNDS',list(lo),list(hi),'CM_SCALE',s,flush=True)
bpy.ops.wm.save_as_mainfile(filepath=str(REPO/'Artifacts/Meshy/R02/draft.blend'))
