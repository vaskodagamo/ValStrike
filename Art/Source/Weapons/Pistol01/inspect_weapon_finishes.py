"""Native ART-03 actual-mesh comparisons. Transient scene/material variants only.
Ten matching-camera captures; no maps/packages saved; not a gameplay test.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import time
import traceback
import uuid
import unreal

ROOT = Path(__file__).resolve().parents[4]
spec=importlib.util.spec_from_file_location('rifle_stage',ROOT/'Art/Source/Weapons/Rifle01/inspect_rifle.py')
i=importlib.util.module_from_spec(spec)
spec.loader.exec_module(i)
STATE=i.STATE
require=i.require
RESOLUTION=(1600,1000)
PALETTE=json.loads((ROOT/'Art/Source/Weapons/Shared/weapon_palette.json').read_text())
CAPTURES=[(weapon,'Studio',variant) for weapon in ('Rifle01','Pistol01') for variant in ('crimson','ice','ember')]
CAPTURES += [(weapon,kind,'crimson') for weapon in ('Rifle01','Pistol01') for kind in ('FirstPerson','FirstPerson_Dim')]

def transient_material(role,variant):
    values=PALETTE['variants'][variant]['roles'][role]
    mat=unreal.Material(name=f'ART03_{variant}_{role}_{STATE["run_id"]}')
    lib=unreal.MaterialEditingLibrary
    color=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector)
    srgb=[int(values['srgb_hex'][j:j+2],16)/255 for j in (0,2,4)]
    rgb=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in srgb]
    color.set_editor_property('constant',unreal.LinearColor(*rgb,1))
    require(lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR),'Variant color')
    for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,values['roughness']),(unreal.MaterialProperty.MP_METALLIC,values['metallic'])]:
        node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant)
        node.set_editor_property('r',value)
        require(lib.connect_material_property(node,'',prop),'Variant surface')
    require(not lib.recompile_material(mat),'Variant compile')
    return mat

def setup_pistol():
    actors={}
    for part in ('Body','Slide','Magazine'):
        name='SM_VS_Pistol01_'+part
        mesh=unreal.load_asset('/Game/Art/Weapons/Pistol01/Meshes/'+name)
        require(isinstance(mesh,unreal.StaticMesh),'Missing pistol '+part)
        actor=i.spawn(unreal.StaticMeshActor,'Pistol inspection '+part,(0,0,0))
        comp=actor.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_static_mesh(mesh)
        comp.set_collision_profile_name('NoCollision')
        actors[part]=actor
    rule=unreal.AttachmentRule.SNAP_TO_TARGET
    for part in ('Slide','Magazine'):
        require(actors[part].attach_to_component(actors['Body'].static_mesh_component,part,rule,rule,rule,False),'Pistol attach '+part)
    return actors

def apply_materials(weapon,variant):
    for actor in STATE['assemblies'][weapon].values():
        comp=actor.static_mesh_component
        mesh=comp.static_mesh
        for j,slot in enumerate(mesh.get_editor_property('static_materials')):
            name=str(slot.get_editor_property('material_slot_name'))
            role=PALETTE['rifle_material_roles'][name] if weapon=='Rifle01' else name.removeprefix('M_VS_P01_')
            # Default captures use the saved native materials, not lookalike overrides.
            mat=mesh.get_material(j) if variant=='crimson' else STATE['variants'][variant][role]
            comp.set_material(j,mat)

def verify_assembly(weapon):
    actors=STATE['assemblies'][weapon]
    body=actors['Body'].static_mesh_component
    evidence={}
    for part,actor in actors.items():
        comp=actor.static_mesh_component
        require(i.distance(actor.get_actor_scale3d(),unreal.Vector(1,1,1))<1e-4,'Scaled mesh')
        require(comp.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,'Collision enabled')
        item={'world':i.transform_data(comp.get_world_transform()),'collision':str(comp.get_collision_enabled())}
        if part!='Body':
            require(comp.get_attach_parent()==body and str(comp.get_attach_socket_name())==part,'Wrong socket attachment')
            error=i.distance(comp.get_world_location(),body.get_socket_location(part))
            require(error<.01,'Attachment mismatch')
            relative=i.transform_data(comp.get_relative_transform())
            require(max(abs(v) for v in relative['translation_cm'])<1e-4,'Nonzero attachment')
            require(max(abs(v) for v in relative['rotation_quaternion_xyzw'][:3])<1e-4,'Rotated attachment')
            item.update(relative=relative,attachment_error_cm=error)
        evidence[part]=item
    forward=body.get_socket_rotation('Muzzle').get_forward_vector()
    require(i.distance(forward,actors['Body'].get_actor_forward_vector())<1e-4,'Wrong muzzle forward')
    return evidence

def pose():
    weapon,kind,variant=CAPTURES[STATE['capture_index']]
    for name,assembly in STATE['assemblies'].items():
        assembly['Body'].set_actor_location(unreal.Vector(0,5000,0),False,True)
    body=STATE['assemblies'][weapon]['Body']
    dim=kind.endswith('_Dim')
    sun_intensity,sky_intensity=(.75,.3) if dim else (3.0,1.0)
    STATE['sun'].set_intensity(sun_intensity)
    STATE['sky'].set_intensity(sky_intensity)
    apply_materials(weapon,variant)
    if kind=='Studio':
        if weapon=='Rifle01':
            body_location=unreal.Vector(0,0,16.9)
            camera_location=unreal.Vector(90,-180,85)
            target=unreal.Vector(10,0,17.9)
            fov=32.0
        else:
            body_location=unreal.Vector(0,0,10.95)
            camera_location=unreal.Vector(38,-65,35)
            target=unreal.Vector(4.5,0,9.5)
            fov=29.0
        rotation=unreal.MathLibrary.find_look_at_rotation(camera_location,target)
        body.set_actor_location_and_rotation(body_location,unreal.Rotator(),False,True)
    else:
        camera_location=unreal.Vector(0,0,160)
        rotation=unreal.Rotator()
        offset=(32,16,-18) if weapon=='Rifle01' else (34,12,-14)
        body.set_actor_location_and_rotation(unreal.Vector(offset[0],offset[1],160+offset[2]),unreal.Rotator(),False,True)
        fov=85.0
    STATE['camera'].set_actor_location_and_rotation(camera_location,rotation,False,True)
    STATE['camera_component'].set_field_of_view(fov)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera_location,rotation)
    suffix=kind if variant=='crimson' else kind+'_'+variant
    output=f'docs/assets/{weapon.lower()}/Previews/VS_{weapon}_Unreal_{suffix}.png'
    capture={'weapon':weapon,'kind':kind,'variant':variant,'output':output,
             'resolution':list(RESOLUTION),'camera_location_cm':i.xyz(camera_location),
             'camera_rotation_degrees':{a:getattr(rotation,a) for a in ('pitch','yaw','roll')},
             'horizontal_fov_degrees':fov,'sun_intensity':sun_intensity,'sky_intensity':sky_intensity,
             'auto_exposure':False,'exposure_bias':0,'assembly':verify_assembly(weapon),
             'material_source':'saved native materials' if variant=='crimson' else 'transient variant constants from shared palette',
             'composition':'actual native meshes in unsaved editor scene; no arms, HUD, or gameplay'}
    if kind!='Studio': capture['grip_offset_from_camera_cm']=list(offset)
    STATE['current_capture']=capture
    STATE['pending_png']=ROOT/f'Saved/ART03/{STATE["run_id"]}-{STATE["capture_index"]}.png'
    STATE['pose_started']=time.monotonic()

def finish(error=None):
    if STATE['finished']:return
    STATE['finished']=True
    if STATE['handle'] is not None:unreal.unregister_slate_post_tick_callback(STATE['handle'])
    if error: unreal.log_error('VALSTRIKE_ART03_INSPECTION_FAILED: '+error)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(_dt):
    try:
        require(time.monotonic()-STATE['started']<240,'Capture timeout')
        if STATE['task'] is None:
            if time.monotonic()-STATE['pose_started']<3:return
            STATE['task']=unreal.AutomationLibrary.take_high_res_screenshot(*RESOLUTION,str(STATE['pending_png']),camera=STATE['camera'],delay=2,force_game_view=True)
            require(STATE['task'] and STATE['task'].is_valid_task(),'No screenshot task')
            return
        if not STATE['task'].is_task_done():return
        pending=STATE['pending_png'];data=pending.read_bytes()
        require(len(data)>1000 and data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==RESOLUTION,'Invalid fresh screenshot')
        capture=STATE['current_capture'];capture['sha256']=hashlib.sha256(data).hexdigest()
        STATE['report']['captures'].append(capture)
        STATE['completed_files'].append((pending,ROOT/capture['output']))
        STATE['capture_index']+=1;STATE['task']=None
        if STATE['capture_index']<len(CAPTURES):pose();return
        for temporary,final in STATE['completed_files']:
            final.parent.mkdir(parents=True,exist_ok=True);temporary.replace(final)
        STATE['report']['elapsed_seconds']=round(time.monotonic()-STATE['started'],3)
        target=ROOT/'docs/assets/pistol01/unreal-inspection-report.json'
        target.write_text(json.dumps(STATE['report'],indent=2)+'\n')
        unreal.log('VALSTRIKE_ART03_INSPECTION_SUCCESS: 10 actual native captures')
        finish()
    except Exception:finish(traceback.format_exc())

def main():
    require(unreal.SystemLibrary.get_engine_version().startswith('5.8.4-'),'Pinned engine required')
    require(Path(unreal.Paths.project_dir()).resolve()==ROOT.resolve(),'Wrong worktree')
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'Use fresh editor')
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE.update(started=time.monotonic(),run_id=uuid.uuid4().hex,completed_files=[],capture_index=0,task=None,finished=False)
    STATE['report']={'engine':unreal.SystemLibrary.get_engine_version(),'run_id':STATE['run_id'],
                     'date_europe_berlin':'2026-11-03','scene':'transient editor stage; no maps/packages saved',
                     'captures':[],'runtime_verified':False,'gameplay_verified':False,
                     'palette':PALETTE,'palette_sha256':hashlib.sha256((ROOT/'Art/Source/Weapons/Shared/weapon_palette.json').read_bytes()).hexdigest()}
    (ROOT/'Saved/ART03').mkdir(parents=True,exist_ok=True)
    i.load_rifle();i.create_stage()
    STATE['assemblies']={'Rifle01':{'Body':STATE['body'],'Magazine':STATE['magazine']},'Pistol01':setup_pistol()}
    actors=STATE['actors'].get_all_level_actors()
    STATE['sun']=next(a.get_component_by_class(unreal.DirectionalLightComponent) for a in actors if isinstance(a,unreal.DirectionalLight))
    STATE['sky']=next(a.get_component_by_class(unreal.SkyLightComponent) for a in actors if isinstance(a,unreal.SkyLight))
    STATE['variants']={variant:{role:transient_material(role,variant) for role in PALETTE['variants'][variant]['roles']} for variant in ('ice','ember')}
    pose();STATE['handle']=unreal.register_slate_post_tick_callback(tick)

if __name__=='__main__':
    try:main()
    except Exception:finish(traceback.format_exc())
