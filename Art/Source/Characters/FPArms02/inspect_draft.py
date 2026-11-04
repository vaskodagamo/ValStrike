import bpy, json, math
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
DOC=ROOT.parents[3]/'docs/assets/fparms02'
bpy.ops.wm.read_factory_settings(use_empty=True)
path=next((ROOT/'Original').rglob('*.glb'))
bpy.ops.import_scene.gltf(filepath=str(path))
report=[]
for o in bpy.context.scene.objects:
    if o.type=='MESH':
        report.append({'name':o.name,'vertices':len(o.data.vertices),'polygons':len(o.data.polygons),'dimensions':list(o.dimensions),'bounds':[[min((o.matrix_world@v.co)[i] for v in o.data.vertices),max((o.matrix_world@v.co)[i] for v in o.data.vertices)] for i in range(3)]})
        for p in o.data.polygons:p.use_smooth=True
(DOC/'draft-inspection.json').write_text(json.dumps(report,indent=2)+'\n')
scene=bpy.context.scene
scene.render.engine='CYCLES'; scene.cycles.samples=16;scene.cycles.use_denoising=True
scene.render.resolution_x=700;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.18,.18,.18,1)
def aim(o,t):o.rotation_euler=(Vector(t)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((2,-3,3),300,3),((-2,-1,1),200,2),((0,2,2),200,2)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.shape='DISK';o.data.size=size;aim(o,(0,0,0))
bpy.ops.object.camera_add(location=(0,-3,0));cam=bpy.context.object;cam.data.type='ORTHO';cam.data.ortho_scale=2.3;scene.camera=cam
for name,loc in [('front',(0,-3,0)),('back',(0,3,0)),('side',(3,0,0))]:
    cam.location=loc;aim(cam,(0,0,0));scene.render.filepath=str(DOC/'Previews'/('draft_'+name+'.png'));bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Original'/'MeshyDraft.blend'))
