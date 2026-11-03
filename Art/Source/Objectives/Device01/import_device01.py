"""ART-04 native authoring and strict fresh-process verification, UE 5.8.4.
Use the shared with_ue_slot wrapper. Existing batches require explicit -VSDeviceRefresh.
-VSDeviceInspectOnly does not modify native packages.
"""
import hashlib
import json
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
SOURCE=Path(__file__).resolve().parent
MODELS=SOURCE/'Models'
REPORTS=ROOT/'docs/assets/device01'
BASE='/Game/Art/Objectives/Device01'
MESH='SM_VS_Device01'
APPEARANCE=json.loads((SOURCE/'appearance.json').read_text())
STATES=('Idle','Armed','Defused')
LIB=unreal.MaterialEditingLibrary

def require(ok,message):
    if not ok:raise RuntimeError(message)

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def xyz(v):return [round(getattr(v,a),6) for a in ('x','y','z')]
def rgba(v):return [v.r,v.g,v.b,v.a]
def close(a,b,tolerance=1e-5):return len(a)==len(b) and all(abs(x-y)<tolerance for x,y in zip(a,b))
def linear(hexcode):
    rgb=[int(hexcode[i:i+2],16)/255 for i in (0,2,4)]
    return [v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb]
def color(hexcode):return unreal.LinearColor(*linear(hexcode),1)
def load(path):
    asset=unreal.load_asset(path)
    require(asset is not None,'Missing '+path)
    return asset

def new_asset(name,folder,cls,factory,refresh):
    path=f'{BASE}/{folder}/{name}'
    if refresh and unreal.EditorAssetLibrary.does_asset_exist(path):return load(path)
    obj=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,f'{BASE}/{folder}',cls,factory)
    require(obj,'Create failed: '+path)
    return obj

def save(asset):require(unreal.EditorAssetLibrary.save_loaded_asset(asset,False),'Save failed '+asset.get_name())
def connect(a,out,b,inp):require(LIB.connect_material_expressions(a,out,b,inp),'Material connection failed '+inp)
def output(node,prop):require(LIB.connect_material_property(node,'',prop),'Material output connection')

def vector_node(mat,name,value,x,y):
    node=LIB.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,x,y)
    node.set_editor_property('parameter_name',name)
    node.set_editor_property('default_value',color(value))
    return node

def scalar_node(mat,name,value,x,y):
    node=LIB.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,x,y)
    node.set_editor_property('parameter_name',name)
    node.set_editor_property('default_value',value)
    return node

def make_surface(role,refresh):
    mat=new_asset('M_VS_D01_'+role,'Materials',unreal.Material,unreal.MaterialFactoryNew(),refresh)
    LIB.delete_all_material_expressions(mat)
    values=APPEARANCE[role]
    base=vector_node(mat,'BaseColor',values['srgb_hex'],-350,0)
    rough=scalar_node(mat,'Roughness',values['roughness'],-350,150)
    metal=scalar_node(mat,'Metallic',values['metallic'],-350,300)
    output(base,unreal.MaterialProperty.MP_BASE_COLOR)
    output(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    output(metal,unreal.MaterialProperty.MP_METALLIC)
    require(not LIB.recompile_material(mat),'Surface shader compile failed')
    save(mat)
    return mat

def import_mask(state,refresh):
    name='T_VS_D01_Status'+state
    task=unreal.AssetImportTask()
    for key,value in {'filename':str(MODELS/'Textures'/f'{name}.png'),'destination_path':f'{BASE}/Textures',
                      'destination_name':name,'automated':True,'replace_existing':refresh,'save':False}.items():
        task.set_editor_property(key,value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=load(f'{BASE}/Textures/{name}')
    require(isinstance(texture,unreal.Texture2D),'Expected texture2D')
    texture.set_editor_property('srgb',False)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    save(texture)
    return texture

def make_status(textures,refresh):
    mat=new_asset('M_VS_D01_Status','Materials',unreal.Material,unreal.MaterialFactoryNew(),refresh)
    LIB.delete_all_material_expressions(mat)
    status=APPEARANCE['Status']
    background=vector_node(mat,'Background',status['background_srgb_hex'],-650,-100)
    glyph=vector_node(mat,'GlyphColor',status['states']['Idle']['srgb_hex'],-650,100)
    mask=LIB.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,-650,300)
    mask.set_editor_property('parameter_name','GlyphMask')
    mask.set_editor_property('texture',textures['Idle'])
    mask.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    uv=LIB.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-900,300)
    uv.set_editor_property('coordinate_index',0)
    connect(uv,'',mask,'UVs')
    mix=LIB.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,-250,0)
    connect(background,'',mix,'A');connect(glyph,'',mix,'B');connect(mask,'R',mix,'Alpha')
    output(mix,unreal.MaterialProperty.MP_BASE_COLOR)
    marked=LIB.create_material_expression(mat,unreal.MaterialExpressionMultiply,-250,250)
    connect(glyph,'',marked,'A');connect(mask,'R',marked,'B')
    strength=scalar_node(mat,'GlyphEmission',status['glyph_emissive_strength'],-650,600)
    emission=LIB.create_material_expression(mat,unreal.MaterialExpressionMultiply,0,250)
    connect(marked,'',emission,'A');connect(strength,'',emission,'B')
    output(emission,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    for prop,key,y in [(unreal.MaterialProperty.MP_ROUGHNESS,'roughness',450),(unreal.MaterialProperty.MP_METALLIC,'metallic',550)]:
        output(scalar_node(mat,key.title(),status[key],-250,y),prop)
    require(not LIB.recompile_material(mat),'Status shader compile failed')
    save(mat)
    instances={}
    for state in STATES:
        instance=new_asset('MI_VS_D01_Status'+state,'Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew(),refresh)
        LIB.set_material_instance_parent(instance,mat)
        # UE5.8.4 setters mutate successfully but return false; verify actual getters below.
        LIB.set_material_instance_vector_parameter_value(instance,'GlyphColor',color(status['states'][state]['srgb_hex']))
        LIB.set_material_instance_texture_parameter_value(instance,'GlyphMask',textures[state])
        require(close(rgba(LIB.get_material_instance_vector_parameter_value(instance,'GlyphColor')),linear(status['states'][state]['srgb_hex'])+[1]),'State color readback failed')
        require(LIB.get_material_instance_texture_parameter_value(instance,'GlyphMask')==textures[state],'State mask readback failed')
        LIB.update_material_instance(instance)
        save(instance);instances[state]=instance
    return instances

def import_mesh(materials,refresh):
    options=unreal.FbxImportUI()
    for key,value in {'import_mesh':True,'import_as_skeletal':False,'import_animations':False,
                      'import_materials':False,'import_textures':False,
                      'mesh_type_to_import':unreal.FBXImportType.FBXIT_STATIC_MESH,
                      'automated_import_should_detect_type':False}.items():options.set_editor_property(key,value)
    data=options.get_editor_property('static_mesh_import_data')
    for key,value in {'combine_meshes':True,'auto_generate_collision':False,'build_nanite':False,
                      'generate_lightmap_u_vs':False,'import_uniform_scale':1.0,'convert_scene':True,
                      'convert_scene_unit':True,'force_front_x_axis':False,'transform_vertex_to_absolute':True,
                      'bake_pivot_in_vertex':False,'normal_import_method':unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS}.items():data.set_editor_property(key,value)
    task=unreal.AssetImportTask()
    for key,value in {'filename':str(MODELS/'Exports'/f'{MESH}.fbx'),'destination_path':f'{BASE}/Meshes',
                      'destination_name':MESH,'automated':True,'replace_existing':refresh,'save':False,
                      'factory':unreal.FbxFactory(),'options':options}.items():task.set_editor_property(key,value)
    unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=load(f'{BASE}/Meshes/{MESH}')
    slots=mesh.get_editor_property('static_materials')
    require([str(s.get_editor_property('material_slot_name')) for s in slots]==['Shell','Trim','Status'],'FBX slot names/order changed')
    for index,mat in enumerate(materials):mesh.set_material(index,mat)
    sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    sub.remove_collisions(mesh)
    for index in range(mesh.get_num_sections(0)):sub.enable_section_collision(mesh,False,0,index)
    body=mesh.get_editor_property('body_setup');instance=body.get_editor_property('default_instance')
    instance.set_editor_property('collision_profile_name','NoCollision')
    instance.set_editor_property('collision_enabled',unreal.CollisionEnabled.NO_COLLISION)
    body.set_editor_property('default_instance',instance)
    mesh.set_editor_property('light_map_coordinate_index',1)
    mesh.set_editor_property('light_map_resolution',64)
    socket=mesh.find_socket('Carry')
    if socket is None:
        socket=unreal.StaticMeshSocket(outer=mesh)
        socket.set_editor_property('socket_name','Carry')
        mesh.add_socket(socket)
    socket.set_editor_property('relative_location',unreal.Vector(0,0,8))
    socket.set_editor_property('relative_rotation',unreal.Rotator())
    socket.set_editor_property('relative_scale',unreal.Vector(1,1,1))
    socket.set_editor_property('tag','ART-04 presentation attachment; not placement/collision authority')
    save(mesh)


def inputs(mat,node):
    return dict(zip(LIB.get_material_expression_input_names(node),LIB.get_inputs_for_material_expression(mat,node)))

def inspect_status_graph(parent):
    expressions=LIB.get_material_expressions(parent)
    parameters={str(e.get_editor_property('parameter_name')):e for e in expressions
                if isinstance(e,(unreal.MaterialExpressionVectorParameter,unreal.MaterialExpressionScalarParameter,unreal.MaterialExpressionTextureSampleParameter2D))}
    require(set(parameters)=={'Background','GlyphColor','GlyphMask','GlyphEmission','Roughness','Metallic'},'Status parameter set differs')
    base=LIB.get_material_property_input_node(parent,unreal.MaterialProperty.MP_BASE_COLOR)
    require(isinstance(base,unreal.MaterialExpressionLinearInterpolate),'Expected status lerp')
    base_inputs=inputs(parent,base)
    require(base_inputs.get('A')==parameters['Background'] and base_inputs.get('B')==parameters['GlyphColor'] and base_inputs.get('Alpha')==parameters['GlyphMask'],'Status mask/color wiring differs')
    mask=parameters['GlyphMask']
    require(mask.get_editor_property('sampler_type')==unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,'Wrong actual glyph sampler')
    require(mask.get_editor_property('texture')==load(f'{BASE}/Textures/T_VS_D01_StatusIdle'),'Wrong parent mask')
    uv=inputs(parent,mask).get('UVs')
    require(isinstance(uv,unreal.MaterialExpressionTextureCoordinate) and uv.get_editor_property('coordinate_index')==0,'Status must sample UV0')
    emission=LIB.get_material_property_input_node(parent,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    emission_inputs=inputs(parent,emission)
    require(emission_inputs.get('B')==parameters['GlyphEmission'],'Emission strength wiring differs')
    marked=emission_inputs.get('A')
    require(isinstance(marked,unreal.MaterialExpressionMultiply),'Glyph emission mask missing')
    marked_inputs=inputs(parent,marked)
    require(marked_inputs.get('A')==parameters['GlyphColor'] and marked_inputs.get('B')==mask,'Only glyph may emit')
    for prop,name in [(unreal.MaterialProperty.MP_ROUGHNESS,'Roughness'),(unreal.MaterialProperty.MP_METALLIC,'Metallic')]:
        require(LIB.get_material_property_input_node(parent,prop)==parameters[name],'Status surface parameter disconnected')
    return {'parameters':sorted(parameters),'uv_channel':0,'sampler_type':str(mask.get_editor_property('sampler_type')),
            'default_mask':mask.get_editor_property('texture').get_path_name(),'base_color_mask_links_verified':True,'glyph_only_emission_links_verified':True}

def inspect_materials():
    report={}
    for role in ('Shell','Trim'):
        mat=load(f'{BASE}/Materials/M_VS_D01_{role}')
        values=APPEARANCE[role]
        for prop,name in [(unreal.MaterialProperty.MP_BASE_COLOR,'BaseColor'),(unreal.MaterialProperty.MP_ROUGHNESS,'Roughness'),(unreal.MaterialProperty.MP_METALLIC,'Metallic')]:
            node=LIB.get_material_property_input_node(mat,prop)
            require(node is not None and str(node.get_editor_property('parameter_name'))==name,'Surface parameter disconnected: '+name)
        rgb=rgba(LIB.get_material_default_vector_parameter_value(mat,'BaseColor'))
        require(close(rgb,linear(values['srgb_hex'])+[1]),'Surface color differs '+role)
        scalars={p:LIB.get_material_default_scalar_parameter_value(mat,p.title()) for p in ('roughness','metallic')}
        require(all(abs(scalars[p]-values[p])<1e-6 for p in scalars),'Surface values differ')
        require(LIB.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR) is None,'Shell/Trim cannot emit')
        report[role]={'asset':mat.get_path_name(),'linear_base_color':rgb,**scalars}
    parent=load(f'{BASE}/Materials/M_VS_D01_Status')
    status=APPEARANCE['Status']
    report['StatusGraph']=inspect_status_graph(parent)
    require(isinstance(LIB.get_material_property_input_node(parent,unreal.MaterialProperty.MP_BASE_COLOR),unreal.MaterialExpressionLinearInterpolate),'Status must mix mask colors')
    require(isinstance(LIB.get_material_property_input_node(parent,unreal.MaterialProperty.MP_EMISSIVE_COLOR),unreal.MaterialExpressionMultiply),'Status glyph emission missing')
    report['Status']={}
    for state in STATES:
        instance=load(f'{BASE}/Materials/MI_VS_D01_Status{state}')
        require(instance.get_editor_property('parent')==parent,'Wrong status parent')
        rgb=rgba(LIB.get_material_instance_vector_parameter_value(instance,'GlyphColor'))
        background=rgba(LIB.get_material_instance_vector_parameter_value(instance,'Background'))
        emission=LIB.get_material_instance_scalar_parameter_value(instance,'GlyphEmission')
        roughness=LIB.get_material_instance_scalar_parameter_value(instance,'Roughness')
        metallic=LIB.get_material_instance_scalar_parameter_value(instance,'Metallic')
        texture=LIB.get_material_instance_texture_parameter_value(instance,'GlyphMask')
        require(close(rgb,linear(status['states'][state]['srgb_hex'])+[1]),'State color differs')
        require(close(background,linear(status['background_srgb_hex'])+[1]),'Screen background differs')
        require(abs(emission-status['glyph_emissive_strength'])<1e-6,'Emission differs')
        require(abs(roughness-status['roughness'])<1e-6 and abs(metallic-status['metallic'])<1e-6,'Status surface differs')
        require(texture==load(f'{BASE}/Textures/T_VS_D01_Status{state}'),'Wrong state glyph path')
        require(not texture.get_editor_property('srgb'),'Glyph mask must use linear data')
        require(texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_MASKS,'Wrong texture compression')
        require(texture.get_editor_property('address_x')==unreal.TextureAddress.TA_CLAMP and texture.get_editor_property('address_y')==unreal.TextureAddress.TA_CLAMP,'Mask wrapping')
        report['Status'][state]={'asset':instance.get_path_name(),'parent':parent.get_path_name(),
            'linear_glyph_color':rgb,'linear_background':background,'glyph_emission':emission,
            'roughness':roughness,'metallic':metallic,'glyph':status['states'][state]['glyph'],
            'mask':texture.get_path_name(),'srgb':False,'compression':'TC_MASKS','address_mode':'clamp',
            'source_mask_sha256':sha(MODELS/'Textures'/f'T_VS_D01_Status{state}.png')}
    return report

def inspect_mesh():
    source=json.loads((MODELS/'mesh_validation.json').read_text())
    require(source['appearance']==APPEARANCE and source['appearance_sha256']==sha(SOURCE/'appearance.json'),'Source appearance stale')
    for state in STATES:
        require(source['textures'][state]['sha256']==sha(MODELS/'Textures'/f'T_VS_D01_Status{state}.png'),'Source mask changed '+state)
    require(len(source['assets'])==1 and source['assets'][0]['name']==MESH,'Source must contain exactly the contracted mesh')
    src=source['assets'][0]
    require(close(src['bounds_min_cm'],[-15,-9,0],.002) and close(src['bounds_max_cm'],[15,9,10],.002),'Source contract bounds differ')
    require(src['material_slots']==['Shell','Trim','Status'],'Source material order differs')
    mesh=load(f'{BASE}/Meshes/{MESH}')
    bounds=mesh.get_bounding_box();low,high=xyz(bounds.min),xyz(bounds.max)
    require(close(low,[-15,-9,0],.002) and close(high,[15,9,10],.002),'Fixed native envelope/pivot differs')
    require(mesh.get_num_triangles(0)==src['triangles'] and mesh.get_num_triangles(0)<4000,'Triangle mismatch/budget')
    require(mesh.get_num_lods()==1,'Exactly one LOD expected')
    slots=mesh.get_editor_property('static_materials')
    names=[str(s.get_editor_property('material_slot_name')) for s in slots]
    require(names==['Shell','Trim','Status'],'Fixed slot order differs')
    paths=[mesh.get_material(i).get_path_name() for i in range(3)]
    expected=[f'{BASE}/Materials/{name}.{name}' for name in ('M_VS_D01_Shell','M_VS_D01_Trim','MI_VS_D01_StatusIdle')]
    require(paths==expected,'Wrong default materials')
    sub=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    uv=sub.get_num_uv_channels(mesh,0);require(uv==2,'Expected two UV channels')
    require(not sub.get_nanite_settings(mesh).get_editor_property('enabled'),'Unexpected Nanite')
    require(sub.get_simple_collision_count(mesh)==sub.get_convex_collision_count(mesh)==0,'Unexpected collision hull')
    sections=[sub.is_section_collision_enabled(mesh,0,j) for j in range(mesh.get_num_sections(0))]
    require(not any(sections),'Section collision enabled')
    body=mesh.get_editor_property('body_setup').get_editor_property('default_instance')
    require(str(body.get_editor_property('collision_profile_name'))=='NoCollision' and body.get_editor_property('collision_enabled')==unreal.CollisionEnabled.NO_COLLISION,'Mesh collision defaults differ')
    probe=unreal.StaticMeshComponent();require(probe.set_static_mesh(mesh),'Probe failed')
    require([str(n) for n in probe.get_all_socket_names()]==['Carry'],'Exactly one Carry socket expected')
    socket=mesh.find_socket('Carry');location=xyz(socket.get_editor_property('relative_location'))
    scale=xyz(socket.get_editor_property('relative_scale'));rot=socket.get_editor_property('relative_rotation')
    angles=[rot.roll,rot.pitch,rot.yaw]
    require(close(location,[0,0,8]) and close(scale,[1,1,1]) and close(angles,[0,0,0]),'Carry transform differs')
    return {'asset':mesh.get_path_name(),'bounds_min_cm':low,'bounds_max_cm':high,
        'dimensions_cm':[b-a for a,b in zip(low,high)],'triangles':mesh.get_num_triangles(0),
        'native_vertices':mesh.get_num_vertices(0),'lods':1,'uv_channels':uv,'slot_names':names,'materials':paths,
        'nanite':False,'simple_collision_count':0,'convex_collision_count':0,'section_collision_enabled':sections,
        'collision_profile':'NoCollision','collision_enabled':str(body.get_editor_property('collision_enabled')),
        'sockets':{'Carry':{'location_cm':location,'roll_pitch_yaw_degrees':angles,'scale':scale}},
        'fbx_sha256':sha(MODELS/'Exports'/f'{MESH}.fbx')}

def main():
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.4-'),'Pinned engine required')
    require(Path(unreal.Paths.project_dir()).resolve()==ROOT.resolve(),'Wrong worktree')
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),'Use fresh editor, no PIE')
    command=unreal.SystemLibrary.get_command_line()
    inspect_only='-VSDeviceInspectOnly' in command;refresh='-VSDeviceRefresh' in command
    expected=[f'{BASE}/Meshes/{MESH}']+[f'{BASE}/Materials/M_VS_D01_{r}' for r in ('Shell','Trim','Status')]
    expected += [f'{BASE}/Materials/MI_VS_D01_Status{s}' for s in STATES]+[f'{BASE}/Textures/T_VS_D01_Status{s}' for s in STATES]
    if not inspect_only:
        exists=[unreal.EditorAssetLibrary.does_asset_exist(p) for p in expected]
        require(refresh or not any(exists),'Native batch exists/partial; inspect and explicitly opt into -VSDeviceRefresh')
        textures={state:import_mask(state,refresh) for state in STATES}
        shell=make_surface('Shell',refresh);trim=make_surface('Trim',refresh)
        status=make_status(textures,refresh)
        import_mesh([shell,trim,status['Idle']],refresh)
    report={'task':'ART-04','engine':unreal.SystemLibrary.get_engine_version(),'date_europe_berlin':'2026-11-03',
        'mode':'fresh_process_reopen' if inspect_only else 'author_and_save','appearance_sha256':sha(SOURCE/'appearance.json'),
        'source_validation_sha256':sha(MODELS/'mesh_validation.json'),'mesh':inspect_mesh(),'materials':inspect_materials(),
        'forward_axis':'+X','up_axis':'+Z','units':'cm','runtime_verified':False,'gameplay_verified':False}
    if inspect_only:
        old=json.loads((REPORTS/'unreal-import-report.json').read_text())
        for key in ('appearance_sha256','source_validation_sha256','mesh','materials'):require(report[key]==old[key],'Fresh-process evidence differs: '+key)
    REPORTS.mkdir(parents=True,exist_ok=True)
    target=REPORTS/('unreal-reopen-report.json' if inspect_only else 'unreal-import-report.json')
    target.write_text(json.dumps(report,indent=2)+'\n')
    unreal.log('VALSTRIKE_DEVICE01_NATIVE_SUCCESS: '+report['mode'])
    return report

if __name__=='__main__':
    try:main()
    except Exception as error:
        unreal.log_error('VALSTRIKE_DEVICE01_NATIVE_FAILED: '+str(error));raise
