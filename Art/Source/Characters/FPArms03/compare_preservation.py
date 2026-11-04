"""Producer gate: original / held candidate / final FBX, identical view and PBR.
Read-only source inspection. Does not overwrite either source blend.
"""
import bpy,math,json,hashlib,array
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[3]/'docs/assets/fparms03'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'Original/model.glb'))
original=next(o for o in bpy.context.scene.objects if o.type=='MESH');original.name='Comparison_Original'
bpy.context.view_layer.objects.active=original;original.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
raw=[v.co.copy() for v in original.data.vertices];rawnorm=[n.vector.copy() for n in original.data.corner_normals]
def rawhash(collection,property,width,code):
    values=array.array(code,[0])*(len(collection)*width);collection.foreach_get(property,values);return hashlib.sha256(values.tobytes()).hexdigest()
def signature(mesh):return {'vertices':len(mesh.vertices),'polygons':len(mesh.polygons),'loop_vertex_sha256':rawhash(mesh.loops,'vertex_index',1,'i'),'uv_sha256':rawhash(mesh.uv_layers[0].data,'uv',2,'f'),'custom_normals':mesh.has_custom_normals}
original_signature=signature(original.data)
SCALE=(17.83610451842081+22.612010205024273)/(.9504119753837585+.9521030187606812)
WRIST=-.9521030187606812+22.612010205024273/SCALE
for v,p in zip(original.data.vertices,raw):v.co=(24+(p.z-WRIST)*SCALE,-13-(p.x-.057)*SCALE,-p.y*SCALE)
normals=[Vector((n.z,-n.x,-n.y)).normalized() for n in rawnorm]
original.data.normals_split_custom_set(normals)
baseline_material=original.data.materials[0]
with bpy.data.libraries.load(str(ROOT/'FPArms03.blend'),link=False) as (src,dst):dst.meshes=['Mesh_0'] if 'Mesh_0' in src.meshes else [n for n in src.meshes if n!='MeshyPreservedLeft'][:1]
source_mesh=dst.meshes[0];source_signature=signature(source_mesh)
assert original_signature==source_signature,(original_signature,source_signature)
theoretical_normal_error=max((n.vector-normals[i]).length for i,n in enumerate(source_mesh.corner_normals))
normal_error=max((n.vector-original.data.corner_normals[i].vector).length for i,n in enumerate(source_mesh.corner_normals))
position_error=max((v.co-original.data.vertices[i].co).length for i,v in enumerate(source_mesh.vertices))
print('PRESERVATION_ERRORS',normal_error,position_error,flush=True)
assert normal_error<.001 and position_error<.0001
candidate=bpy.data.objects.new('Comparison_Source',source_mesh);bpy.context.scene.collection.objects.link(candidate)
candidate.data.materials.clear();candidate.data.materials.append(baseline_material)
before=set(bpy.data.objects);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/SK_FPArms03.fbx'),use_anim=False)
imported=[o for o in bpy.data.objects if o not in before]
final=next(o for o in imported if o.type=='MESH' and '_R' in o.name)
for o in imported:
    if o!=final:o.hide_render=True
final.name='Comparison_FinalExport';final.data.materials.clear();final.data.materials.append(baseline_material)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=1000;scene.render.resolution_y=500;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Common PBR Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.14,.17,.21,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.7
def aim(o,t):o.rotation_euler=(Vector(t)-o.location).to_track_quat('-Z','Y').to_euler()
for loc,power,size in [((20,-50,70),110000,65),((0,40,30),85000,60),((60,0,35),65000,50)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.size=size;aim(o,(24,-13,0))
bpy.ops.object.camera_add(location=(22,-13,80));camera=bpy.context.object;camera.rotation_euler=(0,0,0);camera.data.type='ORTHO';camera.data.ortho_scale=47;camera.data.clip_end=1000;scene.camera=camera
for view,location,target,scale in [('whole',(22,-13,80),(22,-13,0),47),('wrist_glove',(31,-13,80),(31,-13,0),22)]:
    camera.location=location;aim(camera,target);camera.data.ortho_scale=scale
    for name,obj in [('original',original),('source',candidate),('export',final)]:
        for o in [original,candidate,final]:o.hide_render=o!=obj
        scene.render.filepath=str(DOC/'Previews'/f'comparison_{view}_{name}.png');bpy.ops.render.render(write_still=True)
report={'original':original_signature,'preserved_source':source_signature,'max_position_error_cm':position_error,'max_transformed_custom_normal_error':normal_error,'max_normal_storage_vs_theoretical_error':theoretical_normal_error,'transform':'Uniform scale and basis change only; no reshaping, reduction or smoothing.','comparison_material':'Same original imported Meshy PBR node graph rebound on all three; native FBX material recreation remains pending.','final_export_vertices':len(final.data.vertices),'final_export_triangles':len(final.data.polygons)}
(DOC/'preservation-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART22_PRESERVATION_COMPARISON_COMPLETE')
