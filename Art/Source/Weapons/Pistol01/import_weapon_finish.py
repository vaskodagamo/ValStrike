"""ART-03 native pistol import and material-only rifle finish, Unreal 5.8.4.
Run fresh rendering Editor. -VSInspectOnly loads and checks saved data without writes.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
PALETTE_FILE = ROOT / 'Art/Source/Weapons/Shared/weapon_palette.json'
PALETTE = json.loads(PALETTE_FILE.read_text())
ROLES = PALETTE['variants'][PALETTE['default']]['roles']
RIFLE_HASHES = {'SM_VS_Rifle01_Body':'559ebec2a5ad0882098c3487e2c1947a300628b2004bf34838e7c68cda40205b',
               'SM_VS_Rifle01_Magazine':'07b9328195b8a7138b75e0b39259139feb365672b86d900094ea3caf4773c3fc'}

def module():
    spec = importlib.util.spec_from_file_location('rifle_import', ROOT/'Art/Source/Weapons/Rifle01/import_rifle.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

r = module()
require = r.require

def linear(hex_color):
    values = [int(hex_color[i:i+2],16)/255 for i in (0,2,4)]
    return [v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in values]

def verify_material(material, role):
    settings = ROLES[role]
    lib = unreal.MaterialEditingLibrary
    color = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_ROUGHNESS)
    metal = lib.get_material_property_input_node(material, unreal.MaterialProperty.MP_METALLIC)
    require(isinstance(color, unreal.MaterialExpressionConstant3Vector), 'Expected constant BaseColor')
    require(isinstance(rough, unreal.MaterialExpressionConstant) and isinstance(metal, unreal.MaterialExpressionConstant), 'Expected scalar constants')
    actual = color.get_editor_property('constant')
    rgb = [actual.r,actual.g,actual.b]
    require(r.close(rgb, linear(settings['srgb_hex']),1e-6), f'BaseColor mismatch: {material.get_name()}')
    roughness,metallic = rough.get_editor_property('r'),metal.get_editor_property('r')
    require(abs(roughness-settings['roughness']) < 1e-6 and abs(metallic-settings['metallic']) < 1e-6, 'Surface mismatch')
    require(lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR) is None,'Unexpected emissive')
    return {'path':material.get_path_name(),'role':role,'srgb_hex':settings['srgb_hex'],
            'linear_base_color':rgb,'roughness':roughness,'metallic':metallic,'emissive_connected':False}

def revise_material(material, role):
    settings = ROLES[role]
    lib = unreal.MaterialEditingLibrary
    color = lib.get_material_property_input_node(material,unreal.MaterialProperty.MP_BASE_COLOR)
    color.set_editor_property('constant',unreal.LinearColor(*linear(settings['srgb_hex']),1))
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,settings['roughness']),
                       (unreal.MaterialProperty.MP_METALLIC,settings['metallic'])]:
        lib.get_material_property_input_node(material,prop).set_editor_property('r',value)
    require(not lib.recompile_material(material), 'Material compilation failed')
    require(unreal.EditorAssetLibrary.save_loaded_asset(material,False),'Save failed')

def save_evidence(kind, report, inspect_only):
    folder = ROOT / f'docs/assets/{kind}'
    folder.mkdir(parents=True,exist_ok=True)
    basename = 'unreal-finish' if kind == 'rifle01' else 'unreal-import'
    original = folder / f'{basename}-report.json'
    if inspect_only:
        prior = json.loads(original.read_text())
        require(report['assets'] == prior['assets'],'Fresh-process mesh evidence differs')
        require(report['materials'] == prior['materials'],'Fresh-process materials differ')
        require(report['palette_sha256'] == prior['palette_sha256'],'Palette differs')
    target = folder / (f'{basename}-reopen-report.json' if inspect_only else f'{basename}-report.json')
    target.write_text(json.dumps(report,indent=2)+'\n')

def base_report(inspect_only):
    return {'task':'ART-03','engine':unreal.SystemLibrary.get_engine_version(),
            'date_europe_berlin':'2026-11-03','mode':'fresh_process_reopen' if inspect_only else 'author_and_save',
            'palette_sha256':hashlib.sha256(PALETTE_FILE.read_bytes()).hexdigest(),
            'variant':PALETTE['default'],'units':'cm','forward':'+X','up':'+Z',
            'gameplay_verified':False,'animation_verified':False}

def rifle(inspect_only):
    global r
    r=module()
    sources=json.loads((r.SOURCE/'mesh_validation.json').read_text())
    require(sources['palette']=={name:dict(ROLES[role]) for name,role in PALETTE['rifle_material_roles'].items()},'Stale rifle source palette')
    old=json.loads((r.REPORTS/'unreal-import-report.json').read_text())
    report=base_report(inspect_only)
    report['assets']={}
    report['materials']={}
    for name,role in PALETTE['rifle_material_roles'].items():
        mat=unreal.load_asset(f'{r.BASE}/Materials/{name}')
        require(mat is not None, f'Missing rifle material {name}')
        if not inspect_only: revise_material(mat,role)
        report['materials'][name]=verify_material(mat,role)
    for source in sources['assets']:
        name=source['name']
        data=r.inspect(name,source)
        old_data=dict(old['assets'][name])
        compare=dict(data)
        old_data.pop('source_sha256');compare.pop('source_sha256')
        require(compare == old_data, f'Rifle immutable native contract differs: {name}')
        native=ROOT/f'Content/Art/Weapons/Rifle01/Meshes/{name}.uasset'
        sha=hashlib.sha256(native.read_bytes()).hexdigest()
        require(sha == RIFLE_HASHES[name],f'Native rifle mesh modified: {name}')
        data['unchanged_native_mesh_sha256']=sha
        data['source_export_materials_revised_without_native_mesh_reimport']=True
        report['assets'][name]=data
    report['original_native_import_provenance']='unreal-import-report.json (ART-02; historic FBX hashes)'
    save_evidence('rifle01',report,inspect_only)

def import_pistol_mesh(name, materials, replace_existing=False):
    options = unreal.FbxImportUI()
    for key, value in {
        "import_mesh": True, "import_as_skeletal": False, "import_animations": False,
        "import_materials": False, "import_textures": False,
        "mesh_type_to_import": unreal.FBXImportType.FBXIT_STATIC_MESH,
        "automated_import_should_detect_type": False,
    }.items():
        options.set_editor_property(key, value)
    data = options.get_editor_property("static_mesh_import_data")
    for key, value in {
        "combine_meshes": True, "auto_generate_collision": False,
        "build_nanite": False, "generate_lightmap_u_vs": False,
        "import_uniform_scale": 1.0, "convert_scene": True,
        "convert_scene_unit": True, "force_front_x_axis": False,
        "transform_vertex_to_absolute": True, "bake_pivot_in_vertex": False,
        "normal_import_method": unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
    }.items():
        data.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    for key, value in {
        "filename": str(r.SOURCE / "Exports" / f"{name}.fbx"),
        "destination_path": f"{r.BASE}/Meshes", "destination_name": name,
        "automated": True, "replace_existing": replace_existing, "save": False,
        "factory": unreal.FbxFactory(), "options": options,
    }.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(f"{r.BASE}/Meshes/{name}")
    require(isinstance(mesh, unreal.StaticMesh), f"Mesh import failed: {name}")
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        require(slot_name in materials, f"Unknown material: {slot_name}")
        mesh.set_material(index, materials[slot_name])
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    for section in range(mesh.get_num_sections(0)):
        subsystem.enable_section_collision(mesh, False, 0, section)
    body_setup = mesh.get_editor_property("body_setup")
    instance = body_setup.get_editor_property("default_instance")
    instance.set_editor_property("collision_profile_name", "NoCollision")
    instance.set_editor_property("collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    body_setup.set_editor_property("default_instance", instance)
    mesh.set_editor_property("light_map_coordinate_index", 1)
    mesh.set_editor_property("light_map_resolution", 64)
    if name == r.BODY:
        for socket_name, location in r.SOCKETS.items():
            if mesh.find_socket(socket_name):
                continue
            socket = unreal.StaticMeshSocket(outer=mesh)
            socket.set_editor_property("socket_name", socket_name)
            socket.set_editor_property("relative_location", unreal.Vector(*location))
            socket.set_editor_property("relative_rotation", unreal.Rotator(roll=0, pitch=0, yaw=0))
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
            socket.set_editor_property("tag", "ART-03 visual attachment")
            mesh.add_socket(socket)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), f"Mesh save failed: {name}")


def pistol(inspect_only):
    global r
    r=module()
    r.BASE='/Game/Art/Weapons/Pistol01'
    r.SOURCE=ROOT/'Art/Source/Weapons/Pistol01/Models'
    r.BODY='SM_VS_Pistol01_Body'
    sources=json.loads((r.SOURCE/'mesh_validation.json').read_text())
    require(sources['palette']=={'M_VS_P01_'+role:dict(values) for role,values in ROLES.items()},'Stale pistol source palette')
    require(sources['shared_palette_sha256']==hashlib.sha256(PALETTE_FILE.read_bytes()).hexdigest(),'Stale pistol palette hash')
    r.SOCKETS={n:tuple(v['location_cm']) for n,v in sources['sockets'].items()}
    names=[v['name'] for v in sources['assets']]
    require(set(names)=={'SM_VS_Pistol01_Body','SM_VS_Pistol01_Slide','SM_VS_Pistol01_Magazine'},'Pistol mesh set differs')
    mats={}
    refresh='-VSRefreshPistolMeshes' in unreal.SystemLibrary.get_command_line()
    if not inspect_only:
        expected=[f'{r.BASE}/Meshes/{n}' for n in names]+[f'{r.BASE}/Materials/M_VS_P01_{role}' for role in ROLES]
        exists=[unreal.EditorAssetLibrary.does_asset_exist(p) for p in expected]
        require(all(exists) if refresh else not any(exists),'Pistol batch exists/partial; refresh requires complete prior batch')
        unreal.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
        for role,settings in ROLES.items():
            name='M_VS_P01_'+role
            if refresh:
                mats[name]=unreal.load_asset(f'{r.BASE}/Materials/{name}')
                revise_material(mats[name],role)
            else:
                mats[name]=r.make_material(name,settings['srgb_hex'],settings['roughness'],settings['metallic'])
        for name in names: import_pistol_mesh(name,mats,refresh)
    report=base_report(inspect_only)
    report['materials']={f'M_VS_P01_{role}':verify_material(unreal.load_asset(f'{r.BASE}/Materials/M_VS_P01_{role}'),role) for role in ROLES}
    report['assets']={s['name']:r.inspect(s['name'],s) for s in sources['assets']}
    require(all(v['lods']==1 for v in report['assets'].values()),'Pistol expects exactly one LOD')
    save_evidence('pistol01',report,inspect_only)

def main():
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.4-'),'Pinned engine required')
    require(Path(unreal.Paths.project_dir()).resolve()==ROOT.resolve(),'Wrong project/worktree')
    require(not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor(),'PIE must be stopped')
    inspect_only='-VSInspectOnly' in unreal.SystemLibrary.get_command_line()
    rifle(inspect_only)
    pistol(inspect_only)
    unreal.log('VALSTRIKE_ART03_NATIVE_SUCCESS: '+('fresh-process reopen' if inspect_only else 'author/import/save'))

if __name__=='__main__':
    try: main()
    except Exception as error:
        unreal.log_error('VALSTRIKE_ART03_NATIVE_FAILED: '+str(error))
        raise
