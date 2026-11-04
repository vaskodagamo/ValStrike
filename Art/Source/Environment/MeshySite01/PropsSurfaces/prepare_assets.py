"""ART-12 source-only Meshy cleanup. Run with installed Blender --background --python.
Canonical inbox is read-only; imports use temporary FBX copies to contain extraction.
"""
import bpy, bmesh
import json, hashlib, shutil, tempfile, sys, math
from pathlib import Path
from mathutils import Vector
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[4]
DOC = REPO / 'docs/assets/meshy-site01/props-surfaces'
INBOX = Path('/Users/webproduktion3/Documents/ValStrike')
INVENTORY = REPO / 'docs/assets/meshy-site01-intake.json'
ASSIGNED = {'MSH-W02','MSH-W03','MSH-W04','MSH-W07','MSH-W10'}
LABELS = {'MSH-W02':'SafetySteps','MSH-W03':'ConcreteTile','MSH-W04':'LongCargoCrate','MSH-W07':'InfillPanel','MSH-W10':'CargoCrate'}

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def bounds(obj):
    points=[obj.matrix_world @ v.co for v in obj.data.vertices]
    lo=Vector([min(p[i] for p in points) for i in range(3)])
    hi=Vector([max(p[i] for p in points) for i in range(3)])
    return lo,hi

def metrics(obj):
    mesh=obj.data; mesh.calc_loop_triangles(); lo,hi=bounds(obj)
    bm=bmesh.new(); bm.from_mesh(mesh)
    edges={'boundary':sum(e.is_boundary for e in bm.edges),'nonmanifold':sum(not e.is_manifold for e in bm.edges),'wire':sum(e.is_wire for e in bm.edges)}
    degenerate=sum(f.calc_area()<1e-12 for f in bm.faces); bm.free()
    return {'name':obj.name,'vertices':len(mesh.vertices),'triangles':len(mesh.loop_triangles),'polygons':len(mesh.polygons),'materials':[m.name if m else None for m in mesh.materials],'uv_layers':[u.name for u in mesh.uv_layers],'bounds_min':list(lo),'bounds_max':list(hi),'dimensions':list(hi-lo),'location':list(obj.location),'rotation':list(obj.rotation_euler),'scale':list(obj.scale),'edges':edges,'degenerate_faces':degenerate}

def material(asset):
    folder=ROOT/asset['asset_id']/'Textures'; folder.mkdir(parents=True,exist_ok=True)
    mat=bpy.data.materials.new('M_'+asset['asset_id'].replace('-','_')); mat.use_nodes=True
    nodes=mat.node_tree.nodes; nodes.clear(); links=mat.node_tree.links
    out=nodes.new('ShaderNodeOutputMaterial'); out.location=(600,0)
    bs=nodes.new('ShaderNodeBsdfPrincipled'); bs.location=(300,0); links.new(bs.outputs['BSDF'],out.inputs['Surface'])
    textures=[]
    for f in asset['files']:
        if not f['file'].endswith('.png'): continue
        kind=next((x for x in ('normal','roughness','metallic') if f['file'].endswith('_'+x+'.png')),'basecolor')
        dest=folder/f'T_{asset["asset_id"].replace("-","_")}_{kind}.png'
        if not dest.exists(): shutil.copy2(INBOX/f['file'],dest)
        assert sha(dest)==f['sha256']
        img=bpy.data.images.load(str(dest),check_existing=True); img.colorspace_settings.name='sRGB' if kind=='basecolor' else 'Non-Color'
        node=nodes.new('ShaderNodeTexImage'); node.image=img; node.label=kind; node.location=(-600,-len(textures)*300)
        if kind=='normal':
            normal=nodes.new('ShaderNodeNormalMap'); normal.space='TANGENT'; normal.location=(0,-300); normal.inputs['Strength'].default_value=1
            links.new(node.outputs['Color'],normal.inputs['Color']); links.new(normal.outputs['Normal'],bs.inputs['Normal'])
        else: links.new(node.outputs['Color'],bs.inputs[{'basecolor':'Base Color','roughness':'Roughness','metallic':'Metallic'}[kind]])
        textures.append({'role':kind,'path':str(dest.relative_to(REPO)),'sha256':sha(dest),'size':list(img.size),'color_space':img.colorspace_settings.name})
    return mat,textures

def load_asset(asset):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for f in asset['files']:
        p=INBOX/f['file']; assert p.stat().st_size==f['bytes'] and sha(p)==f['sha256'],str(p)
    fbx=next(INBOX/f['file'] for f in asset['files'] if f['file'].endswith('.fbx'))
    with tempfile.TemporaryDirectory(prefix='art12-fbx-') as tmp:
        target=Path(tmp)/'input.fbx'; shutil.copy2(fbx,target)
        bpy.ops.import_scene.fbx(filepath=str(target),use_image_search=False)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    original=[metrics(o) for o in meshes]
    mat,textures=material(asset)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in meshes: obj.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0]
    if len(meshes)>1: bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    obj.data.materials.clear(); obj.data.materials.append(mat)
    for poly in obj.data.polygons: poly.material_index=0
    return obj,original,textures

def stage(obj, out, back=False):
    lo,hi=bounds(obj); center=(lo+hi)/2; span=max(hi-lo)
    scene=bpy.context.scene
    world=bpy.data.worlds.new('NeutralWorld'); scene.world=world; world.use_nodes=True
    world.node_tree.nodes['Background'].inputs[0].default_value=(0.38,0.38,0.38,1)
    world.node_tree.nodes['Background'].inputs[1].default_value=0.25
    bpy.ops.object.camera_add(location=center+Vector((-1.6,2.3,1.65) if back else (1.6,-2.3,1.65))*span)
    camera=bpy.context.object; camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler(); camera.data.type='ORTHO'; camera.data.ortho_scale=span*1.85; scene.camera=camera
    for name,offset,power,size in [('Key',(-2,-3,4),250,3),('Fill',(3,-1,2),120,2),('Rim',(1,3,3),180,2)]:
        bpy.ops.object.light_add(type='AREA',location=center+Vector(offset)*span)
        light=bpy.context.object; light.name=name; light.data.energy=power*span*span; light.data.shape='DISK'; light.data.size=size*span; light.rotation_euler=(center-light.location).to_track_quat('-Z','Y').to_euler()
    bpy.ops.mesh.primitive_plane_add(size=span*200,location=(center.x,center.y,lo.z-span*.006))
    floor=bpy.context.object; floor.name='PREVIEW_Ground'
    mat=bpy.data.materials.new('PREVIEW_Gray'); mat.diffuse_color=(.22,.22,.22,1); floor.data.materials.append(mat)
    scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=16; scene.cycles.use_denoising=True
    scene.render.threads_mode='FIXED'; scene.render.threads=6
    scene.render.resolution_x=800; scene.render.resolution_y=700; scene.render.resolution_percentage=100
    scene.view_settings.view_transform='AgX'; scene.render.image_settings.file_format='PNG'; scene.render.filepath=str(out)
    bpy.ops.render.render(write_still=True)
    for o in list(scene.objects):
        if o!=obj: bpy.data.objects.remove(o,do_unlink=True)

def normalize(obj, dims, pivot):
    lo,hi=bounds(obj)
    for v in obj.data.vertices:
        v.co=Vector([(v.co[i]-lo[i])/(hi[i]-lo[i])*dims[i] for i in range(3)])
        if pivot=='base_center': v.co-=Vector((dims[0]/2,dims[1]/2,0))
        if pivot=='top_corner': v.co.z-=dims[2]
    obj.data.update()

def clean(obj, aid):
    config={
        'MSH-W03':([2,2,.1],'top_corner',500),
        'MSH-W04':([2,1,1],'base_center',6000),
        'MSH-W10':([1,1,1],'base_center',6000),
        'MSH-W07':([2,.2,3],'base_corner',3774),
    }
    dims,pivot,target=config[aid]
    normalize(obj,dims,pivot)
    edits={}
    # Compress broad top-field deviations by 95%, retaining distinct vertices and
    # the uploaded seam/bevel geometry. Do not collapse surface folds to zero area.
    if aid=='MSH-W03':
        count=0
        for v in obj.data.vertices:
            x,y,z=v.co
            if z>-.018 and .018<x<1.982 and .018<y<1.982 and abs(x-1)>.018 and abs(y-1)>.018:
                v.co.z*=.05; count+=1
        edits['leveled_plateau_vertices']=count
        edits['plateau_residual_limit_m']=.0009
    else:
        count=0
        for v in obj.data.vertices:
            if v.co.z<.002:
                v.co.z*=.05; count+=1
        edits['base_contact_vertices']=count
        edits['base_contact_residual_limit_m']=.0001
    obj.data.update()
    master=obj.copy(); master.data=obj.data.copy(); bpy.context.collection.objects.link(master)
    master.name=obj.name.replace('SM_','SOURCE_')+'_DetailMaster'; master.hide_render=True; master.hide_set(True)
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    obj.data.calc_loop_triangles(); original_tri=len(obj.data.loop_triangles)
    print('ART12_REDUCE',aid,original_tri,target,flush=True)
    modifier=obj.modifiers.new('GameDensity','DECIMATE'); modifier.ratio=min(1,target/original_tri); modifier.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    if obj.data.has_custom_normals: bpy.ops.mesh.customdata_custom_splitnormals_clear()
    for poly in obj.data.polygons: poly.use_smooth=True
    # Retain high-detail shading across simplified topology rather than averaging
    # normals over the new, much larger triangles.
    transfer=obj.modifiers.new('SourceShading','DATA_TRANSFER'); transfer.object=master
    transfer.use_loop_data=True; transfer.data_types_loops={'CUSTOM_NORMAL'}
    transfer.loop_mapping='POLYINTERP_NEAREST'
    bpy.ops.object.modifier_apply(modifier=transfer.name)
    normalize(obj,dims,pivot)
    obj.data.update()
    return master,{'dimensions_m':dims,'pivot':pivot,'target_triangles':target,'edits':edits,'shading':'custom corner normals transferred from normalized detail master','budget_note':'Target preserves silhouettes with high-to-low texture rebake; panel retains its already modest source density.'}

def bake_game(obj, master, aid):
    """Reproject the supplied material onto simplified UVs; never paint over inputs."""
    dest=ROOT/aid/'Textures'; scene=bpy.context.scene
    scene.render.engine='CYCLES'; scene.cycles.device='CPU'; scene.cycles.samples=1
    scene.render.threads_mode='FIXED'; scene.render.threads=6
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    # Fresh UV charts prevent collapsed source seam coordinates stretching details.
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.015,area_weight=.5)
    bpy.ops.object.mode_set(mode='OBJECT')
    obj.data.uv_layers.active.name='UV0_Game'
    source_mat=master.data.materials[0]
    original_out=next(n for n in source_mat.node_tree.nodes if n.type=='OUTPUT_MATERIAL')
    original_bs=next(n for n in source_mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    emit=source_mat.node_tree.nodes.new('ShaderNodeEmission')
    game=bpy.data.materials.new('M_'+aid.replace('-','_')+'_Game'); game.use_nodes=True
    obj.data.materials.clear(); obj.data.materials.append(game)
    nodes=game.node_tree.nodes; bs=next(n for n in nodes if n.type=='BSDF_PRINCIPLED'); links=game.node_tree.links
    master.hide_set(False); master.hide_render=False
    bpy.ops.object.select_all(action='DESELECT'); master.select_set(True); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    images={}; rows=[]
    for kind in ('basecolor','roughness','metallic','normal'):
        print('ART12_BAKE',aid,kind,flush=True)
        image=bpy.data.images.new('T_'+aid.replace('-','_')+'_Game_'+kind,width=2048,height=2048,alpha=False)
        image.colorspace_settings.name='sRGB' if kind=='basecolor' else 'Non-Color'
        image.generated_color=(.5,.5,1,1) if kind=='normal' else (0,0,0,1)
        target=nodes.new('ShaderNodeTexImage'); target.image=image; target.label=kind; nodes.active=target
        for node in nodes: node.select=(node==target)
        if kind=='normal':
            source_mat.node_tree.links.new(original_bs.outputs['BSDF'],original_out.inputs['Surface'])
            bpy.ops.object.bake(type='NORMAL',use_selected_to_active=True,cage_extrusion=.012,max_ray_distance=.035,margin=16,normal_space='TANGENT',normal_r='POS_X',normal_g='POS_Y',normal_b='POS_Z')
        else:
            src=next(n for n in source_mat.node_tree.nodes if n.type=='TEX_IMAGE' and n.label==kind)
            source_mat.node_tree.links.new(src.outputs['Color'],emit.inputs['Color'])
            source_mat.node_tree.links.new(emit.outputs[0],original_out.inputs['Surface'])
            bpy.ops.object.bake(type='EMIT',use_selected_to_active=True,cage_extrusion=.012,max_ray_distance=.035,margin=16)
        output=dest/(image.name+'.png'); image.filepath_raw=str(output); image.file_format='PNG'; image.save()
        images[kind]=(image,target)
        rows.append({'role':kind,'path':str(output.relative_to(REPO)),'sha256':sha(output),'size':[2048,2048],'color_space':image.colorspace_settings.name})
    source_mat.node_tree.links.new(original_bs.outputs['BSDF'],original_out.inputs['Surface']); source_mat.node_tree.nodes.remove(emit)
    for kind,(image,node) in images.items():
        if kind=='normal':
            normal=nodes.new('ShaderNodeNormalMap'); normal.space='TANGENT'
            links.new(node.outputs['Color'],normal.inputs['Color']); links.new(normal.outputs['Normal'],bs.inputs['Normal'])
        else: links.new(node.outputs['Color'],bs.inputs[{'basecolor':'Base Color','roughness':'Roughness','metallic':'Metallic'}[kind]])
    master.hide_render=True; master.hide_set(True)
    return rows

def write_manifest(report):
    aid=report['asset_id']
    report['actual_identity']={'MSH-W02':'Five-step flight; detailed source only','MSH-W03':'Four-panel concrete paving tile','MSH-W04':'Long two-panel cargo crate','MSH-W07':'Thin metal infill panel, not a cabinet','MSH-W10':'Cube cargo crate'}[aid]
    report['delivery_status']='Deferred: controlled retopology and rebake required' if aid=='MSH-W02' else 'Source ready for art review; native import/runtime pending'
    report['collision_intent']='No source collision mesh; preserve existing map collision; panel requires wall backing; stairs not exported for gameplay.'
    report['blend_sha256']=sha(REPO/report['blend'])
    if aid in {'MSH-W03','MSH-W04','MSH-W10'}:
        report['bake_settings']={'resolution':[2048,2048],'channels':['basecolor','roughness','metallic','normal'],'color_scalar_pass':'EMIT','normal_pass':'TANGENT','normal_axes':['+X','+Y','+Z'],'margin_px':16,'cage_extrusion_m':.012,'max_ray_distance_m':.035}
    (DOC/aid/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')

def export(obj, master, asset, report, settings):
    aid=asset['asset_id']; dest=ROOT/aid; dest.mkdir(exist_ok=True)
    bpy.context.preferences.filepaths.save_version=0
    scene=bpy.context.scene; scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
    scene.unit_settings.length_unit='CENTIMETERS'
    # Master and game share the same original texture files, stored relative to the blend.
    for img in bpy.data.images:
        if img.source=='FILE' and img.filepath and Path(bpy.path.abspath(img.filepath)).is_file():
            imagepath=Path(bpy.path.abspath(img.filepath))
            if imagepath.is_relative_to(dest): img.filepath='//'+str(imagepath.relative_to(dest))
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True); bpy.context.view_layer.objects.active=obj
    master.hide_render=True; master.hide_set(True)
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/(LABELS[aid]+'.blend')),compress=True)
    fbx=dest/(obj.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH'},global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=True,add_leaf_bones=False,bake_anim=False,path_mode='RELATIVE',embed_textures=False)
    report.update({'cleanup':settings,'cleaned_game':metrics(obj),'detail_master':metrics(master),'game_fbx':str(fbx.relative_to(REPO)),'fbx_sha256':sha(fbx),'blend':str((dest/(LABELS[aid]+'.blend')).relative_to(REPO)),'normal_convention':'Supplied RGB retained, interpreted as tangent +Y/OpenGL in Blender. Vendor convention unlabelled; Unreal green-channel handling pending native inspection.','source_ready_not_native_verified':True})
    write_manifest(report)

def park_stairs(obj,asset,report):
    """Retain the uploaded design, but do not publish the rejected game reduction."""
    aid=asset['asset_id']; dest=ROOT/aid
    normalize(obj,[2,2,1],'base_corner')
    obj.name='SOURCE_VS_MSH_W02_SafetySteps_DetailOnly'
    scene=bpy.context.scene; scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=1
    scene.unit_settings.length_unit='CENTIMETERS'; bpy.context.preferences.filepaths.save_version=0
    for mat in obj.data.materials:
        for node in mat.node_tree.nodes:
            if node.type=='TEX_IMAGE':
                path=Path(bpy.path.abspath(node.image.filepath))
                node.image.filepath='//'+str(path.relative_to(dest))
    blend=dest/'SafetySteps_DetailOnly.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(blend),compress=True)
    report.update({'status':'Deferred: controlled retopology and rebake required','detail_master':metrics(obj),'blend':str(blend.relative_to(REPO)),'game_fbx':None,'game_textures':[],
        'issue':'Aggressive 2180-triangle and conservative 60000-triangle trial reductions/rebakes visibly tore or distorted tread shading. Preserve original high-detail design; no rejected game FBX is delivered.',
        'normal_convention':'Original unlabelled normal map interpreted as tangent +Y in Blender; no native convention claim.',
        'cleanup':{'dimensions_m':[2,2,1],'pivot':'base_corner','edits':'Scale/pivot normalization only; original detailed geometry/UV/material preserved.'}})
    write_manifest(report)
    stage(obj,DOC/'Previews'/f'{aid}_DetailOnly.png')
    print('ART12_DEFERRED',aid,flush=True)

def verify_outputs(ids):
    """One source reopen/FBX inspection pass, not a runtime test framework."""
    rows=[]
    for aid in sorted(ids):
        manifest=json.loads((DOC/aid/'manifest.json').read_text())
        blend=REPO/manifest['blend']; fbx=REPO/manifest['game_fbx'] if manifest['game_fbx'] else None
        bpy.ops.wm.open_mainfile(filepath=str(blend))
        expected=manifest.get('cleaned_game') or manifest['detail_master']
        game=bpy.data.objects[expected['name']]
        actual=metrics(game)
        assert actual['triangles']==expected['triangles']
        assert actual['degenerate_faces']==0
        assert actual['location']==[0,0,0] and actual['scale']==[1,1,1]
        assert len(game.data.uv_layers)==1 and len(game.data.materials)==1
        for vertex in game.data.vertices: assert all(math.isfinite(c) for c in vertex.co)
        for texture in manifest['textures']+manifest['game_textures']:
            assert sha(REPO/texture['path'])==texture['sha256']
        image_refs=[]
        for mesh in [o for o in bpy.context.scene.objects if o.type=='MESH']:
            for mat in mesh.data.materials:
                for node in mat.node_tree.nodes:
                    if node.type=='TEX_IMAGE':
                        image=node.image; path=Path(bpy.path.abspath(image.filepath))
                        assert path.is_file(), str(path)
                        image.reload()
                        assert len(image.pixels)>0 and math.isfinite(image.pixels[0]), image.name
                        image_refs.append({'image':image.name,'path':str(path.relative_to(REPO)),'size':list(image.size),'source':image.source})
        if fbx is None:
            rows.append({'asset_id':aid,'status':manifest['status'],'blend_sha256':sha(blend),'reopened_detail_source':actual,'material_image_references':image_refs,'fbx_reimport':None})
            print('ART12_DETAIL_SOURCE_CHECKED',aid,flush=True)
            continue
        if aid in {'MSH-W03','MSH-W10','MSH-W07'}:
            for other in list(bpy.context.scene.objects):
                if other!=game: bpy.data.objects.remove(other,do_unlink=True)
            copies=[game]
            positions=[(0,0,0),(2,0,0),(0,2,0),(2,2,0)] if aid=='MSH-W03' else ([(0,0,0),(0,0,1)] if aid=='MSH-W10' else [(0,0,0)])
            for position in positions[1:]:
                duplicate=game.copy(); duplicate.data=game.data.copy(); bpy.context.collection.objects.link(duplicate)
                duplicate.location=position; copies.append(duplicate)
            bpy.ops.object.select_all(action='DESELECT')
            for copy in copies: copy.select_set(True)
            bpy.context.view_layer.objects.active=game
            if len(copies)>1: bpy.ops.object.join()
            suffix={'MSH-W03':'TilePatch_2x2','MSH-W10':'CrateStack','MSH-W07':'PanelBack'}[aid]
            stage(game,DOC/'Previews'/f'{aid}_{suffix}.png',back=aid=='MSH-W07')
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(fbx),use_image_search=False)
        imported=[o for o in bpy.context.scene.objects if o.type=='MESH']
        assert len(imported)==1
        result=metrics(imported[0]); expected=manifest['cleaned_game']
        assert result['triangles']==expected['triangles']
        for bound in ['bounds_min','bounds_max']:
            assert max(abs(a-b) for a,b in zip(result[bound],expected[bound]))<.00001, (aid,bound)
        assert len(result['materials'])==1 and len(result['uv_layers'])==1
        rows.append({'asset_id':aid,'blend_sha256':sha(blend),'fbx_sha256':sha(fbx),'reopened_source':actual,'material_image_references':image_refs,'fbx_reimport':result,'bounds_tolerance_m':.00001})
        print('ART12_SOURCE_FBX_CHECKED',aid,flush=True)
    (DOC/'source-export-checks.json').write_text(json.dumps({'blender':bpy.app.version_string,'scope':'Source reopen and Blender FBX round-trip only; no Unreal/runtime verification','assets':rows},indent=2)+'\n')

args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
mode=args[0] if args else 'inspect'
ids=set(args[1:]) or ASSIGNED
assert ids<=ASSIGNED
assets=[a for a in json.loads(INVENTORY.read_text())['assets'] if a['asset_id'] in ids]
if mode=='verify':
    verify_outputs(ids)
    assets=[]
for asset in assets:
    obj,original,textures=load_asset(asset)
    aid=asset['asset_id']; obj.name='SM_VS_'+aid.replace('-','_')+'_'+LABELS[aid]
    report={'asset_id':aid,'source_files':asset['files'],'blender_version':bpy.app.version_string,'original_objects':original,'textures':textures}
    (DOC/aid).mkdir(parents=True,exist_ok=True)
    (DOC/aid/'inspection.json').write_text(json.dumps(report,indent=2)+'\n')
    print('ART12_IMPORT',aid,json.dumps(original),flush=True)
    stage(obj,DOC/'Previews'/f'{aid}_Before.png')
    if mode=='produce' and aid=='MSH-W02':
        park_stairs(obj,asset,report)
        continue
    if mode=='produce':
        master,settings=clean(obj,aid)
        report['game_textures']=bake_game(obj,master,aid) if aid!='MSH-W07' else textures
        export(obj,master,asset,report,settings)
        # Preview helper removes stage objects, so temporarily unlink the protected master.
        collection=master.users_collection[0]; collection.objects.unlink(master)
        stage(obj,DOC/'Previews'/f'{aid}_After.png')
        collection.objects.link(master)
        print('ART12_EXPORTED',aid,json.dumps(report['cleaned_game']),flush=True)
print('ART12_DONE',mode,flush=True)
