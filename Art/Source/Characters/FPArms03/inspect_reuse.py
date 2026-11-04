"""Read-only inspection of frozen ART-19 source for ART-22 reuse decision."""
import bpy,json,math
from pathlib import Path
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[3]/'docs/assets/fparms03'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Reference/Frozen/FPArms02.blend'))
arm=bpy.data.objects['SKEL_FPArms02'];arm.animation_data.action=None
for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
right=bpy.data.objects['SK_FPArms02_R'];bpy.data.objects['SK_FPArms02_L'].hide_render=True
report={'source_commit':'afa582abb823897248b666c10fef77464d167fdc','bones':[b.name for b in arm.data.bones],'right_vertices':len(right.data.vertices),'right_triangles':len(right.data.polygons),'uv_layers':len(right.data.uv_layers),'custom_normals':right.data.has_custom_normals,'observations':['Fitted glove surface and 39-bone names/hierarchy are reusable technical/reference inputs.','Below the narrow exposed wrist band, the full forearm surface is folded fabric with jacket cuff, tab and closed sleeve end; there is no anatomical forearm underneath.','Removing the sleeve leaves no elbow-to-wrist bare anatomy. Recoloring, smoothing or using a cylinder would not meet the requested anatomical change.'],'decision':'Reuse compatible rig structure/naming and glove design; clean complete geometry reuse is insufficient. Use the single reserved draft for a coherent right gloved hand with bare forearm. Preserve ART-19 unmodified.','reservation':'ART-22-FPArms03-draft-1','paid_submission_at_inspection':False}
(DOC/'reuse-decision.json').write_text(json.dumps(report,indent=2)+'\n')
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True;scene.render.resolution_x=1000;scene.render.resolution_y=600;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Reuse review');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.14,.17,.21,1)
def aim(o,t):o.rotation_euler=(Vector(t)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((20,-50,70),110000,65),((0,40,30),85000,60)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(24,-13,0))
bpy.ops.object.camera_add(location=(22,-13,80));cam=bpy.context.object;cam.rotation_euler=(0,0,0);cam.data.type='ORTHO';cam.data.ortho_scale=47;scene.camera=cam
scene.render.filepath=str(DOC/'Previews/00_reuse_source.png');bpy.ops.render.render(write_still=True)
cam.location=(24,-35,35);aim(cam,(24,-13,0));cam.data.ortho_scale=16
scene.render.filepath=str(DOC/'Previews/00_reuse_wrist_seam.png');bpy.ops.render.render(write_still=True)
print('ART22_REUSE_INSPECTED_NO_SOURCE_WRITES')
