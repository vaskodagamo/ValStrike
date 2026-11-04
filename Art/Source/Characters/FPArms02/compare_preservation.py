"""Producer gate: original / held candidate / final FBX, identical view and PBR.
Read-only source inspection. Does not overwrite either source blend.
"""
import bpy,math,json,hashlib,array
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent;DOC=ROOT.parents[3]/'docs/assets/fparms02'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'Original/model.glb'))
original=next(o for o in bpy.context.scene.objects if o.type=='MESH');original.name='Comparison_Original'
bpy.context.view_layer.objects.active=original;original.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
raw=[v.co.copy() for v in original.data.vertices];rawnorm=[n.vector.copy() for n in original.data.corner_normals]
def rawhash(collection,property,width,code):
    values=array.array(code,[0])*(len(collection)*width);collection.foreach_get(property,values);return hashlib.sha256(values.tobytes()).hexdigest()
def signature(mesh):return {'vertices':len(mesh.vertices),'polygons':len(mesh.polygons),'loop_vertex_sha256':rawhash(mesh.loops,'vertex_index',1,'i'),'uv_sha256':rawhash(mesh.uv_layers[0].data,'uv',2,'f'),'custom_normals':mesh.has_custom_normals}
original_signature=signature(original.data)
for v,p in zip(original.data.vertices,raw):
    sz=17.83610451842081/(.951028-.02) if p.z>=.02 else 22.612010205024273/(.02+.952124)
    v.co=(24+(p.z-.02)*sz,-13-(p.x-.06)*19.16,-p.y*19.16)
normals=[]
for loop,n in zip(original.data.loops,rawnorm):
    p=raw[loop.vertex_index];sz=17.83610451842081/(.951028-.02) if p.z>=.02 else 22.612010205024273/(.02+.952124)
    normals.append(Vector((n.z/sz,-n.x/19.16,-n.y/19.16)).normalized())
original.data.normals_split_custom_set(normals)
baseline_material=original.data.materials[0]
with bpy.data.libraries.load(str(ROOT/'FPArms02.blend'),link=False) as (src,dst):dst.meshes=['Mesh_0'] if 'Mesh_0' in src.meshes else [n for n in src.meshes if n!='MeshyPreservedLeft'][:1]
source_mesh=dst.meshes[0];source_signature=signature(source_mesh)
assert original_signature==source_signature,(original_signature,source_signature)
theoretical_normal_error=max((n.vector-normals[i]).length for i,n in enumerate(source_mesh.corner_normals))
normal_error=max((n.vector-original.data.corner_normals[i].vector).length for i,n in enumerate(source_mesh.corner_normals))
position_error=max((v.co-original.data.vertices[i].co).length for i,v in enumerate(source_mesh.vertices))
print('PRESERVATION_ERRORS',normal_error,position_error,flush=True)
assert normal_error<.001 and position_error<.0001
with bpy.data.libraries.load(str(ROOT/'Candidates/FPArms02_Reduced.blend'),link=False) as (src,dst):dst.objects=['SK_FPArms02_R']
candidate=dst.objects[0];bpy.context.scene.collection.objects.link(candidate);candidate.parent=None;candidate.modifiers.clear();candidate.name='Comparison_HeldReduced';candidate.data.materials.clear();candidate.data.materials.append(baseline_material)
before=set(bpy.data.objects);bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/SK_FPArms02.fbx'),use_anim=False)
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
for name,obj in [('11_original_same_setup',original),('12_reduced_candidate_same_setup',candidate),('13_final_export_same_setup',final)]:
    for o in [original,candidate,final]:o.hide_render=o!=obj
    scene.render.filepath=str(DOC/'Previews'/f'{name}.png');bpy.ops.render.render(write_still=True)
# Separate design comparison for the required nonuniform hand/forearm fitting.
# Uniform original has the same total length and endpoints, with proportions untouched.
uniform=(17.83610451842081+22.612010205024273)/(.951028+.952124)
for v,p in zip(original.data.vertices,raw):v.co=(24-22.612010205024273+(p.z+.952124)*uniform,-13-(p.x-.06)*uniform,-p.y*uniform)
original.data.normals_split_custom_set([Vector((n.z,-n.x,-n.y)).normalized() for n in rawnorm])
for o in [original,candidate,final]:o.hide_render=o!=original
scene.render.filepath=str(DOC/'Previews/14_original_uniform_proportions.png');bpy.ops.render.render(write_still=True)
report={'original':original_signature,'preserved_source':source_signature,'max_position_error_cm':position_error,'max_transformed_custom_normal_error':normal_error,'max_normal_storage_vs_theoretical_error':theoretical_normal_error,'comparison_camera':{'location_cm':[22,-13,80],'rotation_euler':[0,0,0],'orthographic_span_cm':47,'resolution':[1000,500]},'comparison_material':'Same original imported Meshy PBR node graph on all three meshes; no tint, roughness override or changed lighting.','candidate':{'triangles':len(candidate.data.polygons),'verdict':'HELD: reduction/smoothing/material-adjustment candidate is not the selected source; no production budget justifies destructive changes.'},'nonuniform_fit_comparison':{'uniform_original_hand_length_cm':(.951028-.02)*uniform,'selected_hand_length_cm':17.83610451842081,'uniform_original_forearm_length_cm':(.02+.952124)*uniform,'selected_forearm_length_cm':22.612010205024273,'reason':'Assigned fit to frozen UrbanEntry hand/forearm measurements; no finger scaling to fit a weapon.','evidence':['14_original_uniform_proportions.png','13_final_export_same_setup.png'],'visual_acceptance':'Art review pending for the changed proportions.'},'selected':'Preserved Meshy surface with paired local rig, original PBR, transformed supplied custom normals.','final_export_vertices':len(final.data.vertices),'final_export_triangles':len(final.data.polygons)}
(DOC/'preservation-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print('ART19_PRESERVATION_COMPARISON_COMPLETE')
