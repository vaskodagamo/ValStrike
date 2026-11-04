"""ART-13: preserve Meshy character/rig, consolidate six clips, normalize transforms.
Run with Blender 5.2 --factory-startup --background --threads 2 --python-exit-code 1.
No original files are modified. No Unreal operations. No rerig/decimation.
"""
import bpy, json, math, hashlib, shutil, tempfile, sys
from pathlib import Path
from mathutils import Matrix, Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
DOC=REPO/'docs/assets/urban-entry01'
INBOX=Path('/Users/webproduktion3/Documents/ValStrike/meshy_ai_assets')
MANIFEST=json.loads((REPO/'docs/assets/meshy-character-intake.json').read_text())
BLEND=ROOT/'UrbanEntry01.blend'
CLIPS=['Walking','Running','Crouch_Walk_Left_with_Gun','Vault_with_Rifle','Walk_Backward_While_Shooting','Walk_Backward_with_Gun']
TURN=Matrix.Rotation(math.pi/2,4,'Z')

def curves(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                yield from bag.fcurves

def set_action(arm,action):
    arm.animation_data_create()
    arm.animation_data.action=action
    arm.animation_data.action_slot=action.slots[0]
    bpy.context.view_layer.update()

def reset(arm):
    if arm.animation_data: arm.animation_data.action=None
    for p in arm.pose.bones: p.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()

def points(arm):
    return {p.name:arm.matrix_world@p.head for p in arm.pose.bones}

def mesh_points(obj):
    dg=bpy.context.evaluated_depsgraph_get()
    evaluated=obj.evaluated_get(dg)
    mesh=evaluated.to_mesh()
    result=[evaluated.matrix_world@mesh.vertices[i].co for i in range(0,len(mesh.vertices),23)]
    evaluated.to_mesh_clear()
    return result

def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects: o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]

def material(mesh):
    mat=bpy.data.materials.new('M_VS_UrbanEntry01')
    mat.use_nodes=True
    nodes=mat.node_tree.nodes
    nodes.clear()
    out=nodes.new('ShaderNodeOutputMaterial');out.location=(600,0)
    shader=nodes.new('ShaderNodeBsdfPrincipled');shader.location=(300,0)
    shader.inputs['IOR'].default_value=1.45
    mat.node_tree.links.new(shader.outputs['BSDF'],out.inputs['Surface'])
    for n,(role,suffix,space,socket) in enumerate([
        ('BaseColor','','sRGB','Base Color'),('Metallic','_metallic','Non-Color','Metallic'),
        ('Roughness','_roughness','Non-Color','Roughness')]):
        src=next(r for r in MANIFEST['extracted_files'] if Path(r['file']).name=='Meshy_AI_texture_0'+suffix+'.png')
        dst=ROOT/'Textures'/('T_UE01_'+role+'.png')
        shutil.copyfile(INBOX/src['file'],dst)
        img=bpy.data.images.load(str(dst),check_existing=False)
        img.colorspace_settings.name=space
        node=nodes.new('ShaderNodeTexImage');node.image=img;node.label=role;node.location=(-200,250-n*270)
        mat.node_tree.links.new(node.outputs['Color'],shader.inputs[socket])
        if role=='BaseColor': nodes.active=node
    mesh.data.materials.clear();mesh.data.materials.append(mat)
    return mat

def export_fbx(path,arm,mesh=None,animated=False):
    scene=bpy.context.scene
    original_action=arm.animation_data.action if arm.animation_data else None
    original_frame=scene.frame_current
    temporary_action=None
    if animated:
        # Animation-only FBX has no skin BindPose. Blender writes Model defaults
        # from the current pose. A temporary neutral frame outside the exported
        # [1,end] range makes those defaults match the character bind skeleton.
        temporary_action=original_action.copy()
        temporary_action.name='EXPORT_ONLY_NEUTRAL_FRAME'
        for f in curves(temporary_action):
            value=1.0 if f.data_path.endswith('.scale') or (f.data_path.endswith('.rotation_quaternion') and f.array_index==0) else 0.0
            f.keyframe_points.insert(0,value,options={'FAST'})
            f.update()
        reset(arm);set_action(arm,temporary_action);scene.frame_set(0)
    select([arm]+([mesh] if mesh else []))
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,
        object_types={'ARMATURE','MESH'} if mesh else {'ARMATURE'},
        global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
        axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='OFF',
        add_leaf_bones=False,use_armature_deform_only=False,bake_anim=animated,
        bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,
        bake_anim_force_startend_keying=True,bake_anim_step=1,bake_anim_simplify_factor=0,
        primary_bone_axis='Y',secondary_bone_axis='X',path_mode='RELATIVE',embed_textures=False)
    if temporary_action:
        set_action(arm,original_action)
        bpy.data.actions.remove(temporary_action)
        scene.frame_set(original_frame)

def create_studio():
    sc=bpy.context.scene
    c=bpy.data.collections.new('SOURCE_PREVIEW_ONLY');sc.collection.children.link(c)
    def move(o):
        for old in list(o.users_collection):old.objects.unlink(o)
        c.objects.link(o)
    sc.world=bpy.data.worlds.new('Neutral Studio')
    sc.world.use_nodes=True
    sc.world.node_tree.nodes['Background'].inputs[0].default_value=(.18,.20,.23,1)
    sc.world.node_tree.nodes['Background'].inputs[1].default_value=.65
    for name,pos,energy,size in [('Key',(210,-180,260),1500000,160),('Fill',(100,200,170),850000,130),('Rim',(-120,30,230),1300000,110)]:
        bpy.ops.object.light_add(type='AREA',location=pos)
        o=bpy.context.object;o.name=name;o.data.energy=energy;o.data.shape='DISK';o.data.size=size
        o.rotation_euler=(Vector((0,0,90))-o.location).to_track_quat('-Z','Y').to_euler();move(o)
    for name,pos in [('Preview_Front',(270,-430,180)),('Preview_Rear',(-270,430,180)),('Preview_Side',(0,-480,150))]:
        bpy.ops.object.camera_add(location=pos)
        o=bpy.context.object;o.name=name;o.data.type='ORTHO';o.data.ortho_scale=195;o.data.clip_end=3000
        o.rotation_euler=(Vector((0,0,85))-o.location).to_track_quat('-Z','Y').to_euler();move(o)
    sc.camera=bpy.data.objects['Preview_Front']
    sc.render.engine='CYCLES';sc.cycles.device='CPU';sc.cycles.samples=24;sc.cycles.use_denoising=True
    sc.render.threads_mode='FIXED';sc.render.threads=2
    sc.render.resolution_x=600;sc.render.resolution_y=720;sc.render.resolution_percentage=100
    sc.render.image_settings.file_format='PNG';sc.render.film_transparent=False
    sc.view_settings.view_transform='AgX'
    sc.render.fps=60
    return c

def main():
    for d in [ROOT/'Textures',ROOT/'Exports',DOC/'Previews']:d.mkdir(exist_ok=True,parents=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
    scene.unit_settings.length_unit='CENTIMETERS';scene.render.fps=60
    report={'date_europe_berlin':'2026-11-04','blender':bpy.app.version_string,'inputs_verified':[],
      'changes':['Consolidate identical clip rigs onto one mesh and skeleton','Normalize uniform 100x object scale and rotate common frame to geometric +X forward','Relink supplied texture bytes to portable paths; explicit sRGB/data color spaces; omit unused normal-map node'],
      'preserved':['Original geometry/UVs/custom normals','66 bone names/hierarchy and bind relationships','Original normalized skin weights (max 4)','Original animation trajectories and timing'], 'clips':{}}
    with tempfile.TemporaryDirectory(prefix='_source_',dir=ROOT) as temp:
        for row in MANIFEST['extracted_files']:
            src=INBOX/row['file'];data=src.read_bytes()
            assert len(data)==row['bytes'] and hashlib.sha256(data).hexdigest()==row['sha256']
            shutil.copyfile(src,Path(temp)/src.name)
            report['inputs_verified'].append({'file':src.name,'sha256':row['sha256']})
        bpy.ops.import_scene.fbx(filepath=str(Path(temp)/'Meshy_AI_Character_output.fbx'),use_image_search=False)
        arm=next(o for o in scene.objects if o.type=='ARMATURE')
        mesh=next(o for o in scene.objects if o.type=='MESH')
        arm.name='SKEL_UE01';mesh.name='SK_VS_UrbanEntry01'
        base_rest={b.name:b.matrix_local.copy() for b in arm.data.bones}
        base_parent={b.name:b.parent.name if b.parent else None for b in arm.data.bones}
        old_material=mesh.data.materials[0]
        reset(arm)
        base_bones=points(arm);base_vertices=mesh_points(mesh)
        old_actions=list(bpy.data.actions)
        actions={};original_samples={}
        for clip in CLIPS:
            pre=set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(Path(temp)/('Meshy_AI_Animation_'+clip+'_withSkin.fbx')),use_image_search=False)
            added=set(bpy.data.objects)-pre
            rig=next(o for o in added if o.type=='ARMATURE')
            assert set(base_rest)==set(rig.data.bones.keys())
            assert max(abs(x-y) for b in rig.data.bones for rowa,rowb in zip(base_rest[b.name],b.matrix_local) for x,y in zip(rowa,rowb))<1e-7
            assert all(base_parent[b.name]==(b.parent.name if b.parent else None) for b in rig.data.bones)
            action=rig.animation_data.action
            action.name='A_UE01_'+clip;action.use_fake_user=True
            # Existing action data is reused, never retargeted to a different rig.
            set_action(arm,action)
            first,last=map(round,action.frame_range)
            fps=scene.render.fps/scene.render.fps_base
            print('CLIP',clip,'fps',fps,'frames',first,last,flush=True)
            original_samples[clip]={}
            for frame in range(first,last+1):
                scene.frame_set(frame)
                original_samples[clip][frame]={'bones':points(arm)}
                if frame in {first,last,(first+last)//2}:
                    original_samples[clip][frame]['vertices']=mesh_points(mesh)
            channel_paths=sorted(set(f.data_path for f in curves(action)))
            object_channels=[p for p in channel_paths if not p.startswith('pose.bones[')]
            assert not object_channels,object_channels
            report['clips'][clip]={'input_frame_range':[first,last],'fps':fps,
                'duration_seconds':(last-first)/fps,'bone_rest_compatible':True,'source_object_channels':object_channels}
            actions[clip]=action
            for o in added:bpy.data.objects.remove(o,do_unlink=True)
        reset(arm)
        for a in old_actions:
            if a.users==0:bpy.data.actions.remove(a)
        # Uniform basis change: apply one shared world rotation/scale to bind data.
        # Local translation channels scale by the same 100; quaternion/scale keys stay untouched.
        old_world=arm.matrix_world.copy()
        assert max(abs(v-100) for v in arm.scale)<1e-3
        arm.matrix_world=TURN@arm.matrix_world
        bpy.context.view_layer.update()
        # Unparent while preserving world space before applying the bind transforms.
        mesh_world=mesh.matrix_world.copy()
        mesh.parent=None;mesh.matrix_world=mesh_world
        select([arm,mesh])
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        mesh.parent=arm;mesh.matrix_parent_inverse=Matrix.Identity(4)
        for action in actions.values():
            for f in curves(action):
                if f.data_path.endswith('.location'):
                    for key in f.keyframe_points:
                        key.co.y*=100;key.handle_left.y*=100;key.handle_right.y*=100
                    f.update()
        reset(arm)
        assert max((points(arm)[n]-TURN@v).length for n,v in base_bones.items())<.001
        assert max((a-TURN@b).length for a,b in zip(mesh_points(mesh),base_vertices))<.001
        # Compare every source animation frame, not merely the bind pose.
        for clip,action in actions.items():
            set_action(arm,action)
            max_bone=max_mesh=0
            for frame,sample in original_samples[clip].items():
                scene.frame_set(frame);current=points(arm)
                max_bone=max(max_bone,max((current[n]-TURN@p).length for n,p in sample['bones'].items()))
                if 'vertices' in sample:max_mesh=max(max_mesh,max((a-TURN@b).length for a,b in zip(mesh_points(mesh),sample['vertices'])))
            print('PRESERVATION',clip,max_bone,max_mesh,flush=True)
            assert max_bone<.01 and max_mesh<.01
            report['clips'][clip].update({'max_normalization_joint_error_cm':max_bone,'max_sampled_mesh_error_cm':max_mesh})
        reset(arm)
        create_studio()
        # Before/after share the same geometry, camera, lighting and neutral pose.
        if '--no-renders' not in sys.argv:
            scene.render.filepath=str(DOC/'Previews/01_material_before.png')
            bpy.ops.render.render(write_still=True)
        material(mesh)
        if '--no-renders' not in sys.argv:
            scene.render.filepath=str(DOC/'Previews/02_material_after.png')
            bpy.ops.render.render(write_still=True)
            scene.camera=bpy.data.objects['Preview_Rear'];scene.render.filepath=str(DOC/'Previews/03_rear.png')
            bpy.ops.render.render(write_still=True);scene.camera=bpy.data.objects['Preview_Front']
        reset(arm)
        export_fbx(ROOT/'Exports/SK_VS_UrbanEntry01.fbx',arm,mesh)
        for clip,action in actions.items():
            set_action(arm,action)
            scene.frame_start,scene.frame_end=map(round,action.frame_range)
            scene.render.fps=round(report['clips'][clip]['fps'])
            export_fbx(ROOT/'Exports'/('A_UE01_'+clip+'.fbx'),arm,animated=True)
        reset(arm);scene.frame_start=1;scene.frame_end=77;scene.frame_set(1)
        arm['source']='MSH-C01 Meshy user supplied; no ground-root bone; pelvis is root'
        arm['coordinates']='centimeters; +X forward; Z up; object transforms identity'
        arm['clip_order']=json.dumps(CLIPS)
        mesh.data.calc_loop_triangles()
        report['mesh']={'vertices':len(mesh.data.vertices),'triangles':len(mesh.data.loop_triangles),'material_slots':len(mesh.data.materials),'uv_layers':[u.name for u in mesh.data.uv_layers],'custom_normals':mesh.data.has_custom_normals}
        report['bones']=[{'name':b.name,'parent':b.parent.name if b.parent else None,'head_cm':list(b.head_local)} for b in arm.data.bones]
        report['armature_matrix']=[list(r) for r in arm.matrix_world]
        report['mesh_matrix']=[list(r) for r in mesh.matrix_world]
        # Remove orphan duplicate imports/materials/images so the editable source stays compact.
        bpy.ops.outliner.orphans_purge(do_recursive=True)
        for image in bpy.data.images:
            if image.source == 'FILE' and image.filepath:
                image.filepath=bpy.path.relpath(image.filepath,start=str(ROOT))
        bpy.context.preferences.filepaths.save_version=0
        select([mesh])
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    report['exports']=[{'file':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted((ROOT/'Exports').glob('*.fbx'))]
    (DOC/'source_preparation.json').write_text(json.dumps(report,indent=2)+'\n')
    print('ART13_SOURCE_PREPARED',flush=True)

if __name__=='__main__':main()
