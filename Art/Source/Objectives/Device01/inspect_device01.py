"""Fresh-process native reopen plus eight actual-device inspection captures.
Run with -VSDeviceInspectOnly, through Scripts/with_ue_slot.py. No packages saved.
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

ROOT=Path(__file__).resolve().parents[4]
SOURCE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('device_import',SOURCE/'import_device01.py')
d=importlib.util.module_from_spec(spec);spec.loader.exec_module(d)
RESOLUTION=(1600,1000)
CAPTURES=[(kind,state) for kind in ('Studio','Dim') for state in d.STATES]+[('Carry','Idle'),('PlayerEye','Armed')]
STATE={'handle':None,'task':None,'index':0,'finished':False}
require=d.require

def spawn(cls,label,position,rotation=None):
    actor=STATE['actors'].spawn_actor_from_class(cls,unreal.Vector(*position),rotation or unreal.Rotator())
    require(actor,'Spawn failed '+label);actor.set_actor_label(label);return actor

def transform_record(t):
    q=t.get_editor_property('rotation')
    return {'translation_cm':d.xyz(t.get_editor_property('translation')),
            'rotation_quaternion_xyzw':[round(getattr(q,a),6) for a in ('x','y','z','w')],
            'scale':d.xyz(t.get_editor_property('scale3d'))}

def stage():
    STATE['world']=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    STATE['actors']=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor=spawn(unreal.StaticMeshActor,'D01 actual native mesh',(0,0,0))
    component=actor.static_mesh_component
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_static_mesh(d.load(f'{d.BASE}/Meshes/{d.MESH}'))
    component.set_collision_profile_name('NoCollision')
    STATE['device']=actor
    ground=unreal.Material(name='D01_TransientGround_'+STATE['run_id'])
    base=d.LIB.create_material_expression(ground,unreal.MaterialExpressionConstant3Vector)
    base.set_editor_property('constant',unreal.LinearColor(.32,.34,.35,1))
    rough=d.LIB.create_material_expression(ground,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.9)
    d.output(base,unreal.MaterialProperty.MP_BASE_COLOR);d.output(rough,unreal.MaterialProperty.MP_ROUGHNESS)
    require(not d.LIB.recompile_material(ground),'Ground compile failed')
    STATE['ground_material']=ground
    floor=spawn(unreal.StaticMeshActor,'Unsaved studio floor',(0,0,-1))
    floor.static_mesh_component.set_static_mesh(d.load('/Engine/BasicShapes/Cube.Cube'))
    floor.static_mesh_component.set_material(0,ground)
    floor.static_mesh_component.set_collision_profile_name('NoCollision')
    floor.set_actor_scale3d(unreal.Vector(40,40,.02))
    key=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(-200,350,450),unreal.Vector(0,0,10))
    sun=spawn(unreal.DirectionalLight,'Unsaved key',(-200,350,450),key)
    STATE['sun']=sun.get_component_by_class(unreal.DirectionalLightComponent)
    STATE['sun'].set_mobility(unreal.ComponentMobility.MOVABLE)
    STATE['sun'].set_editor_property('atmosphere_sun_light',True)
    STATE['sun'].set_editor_property('light_source_angle',3.0)
    spawn(unreal.SkyAtmosphere,'Unsaved atmosphere',(0,0,0))
    sky=spawn(unreal.SkyLight,'Unsaved sky',(0,0,400))
    STATE['sky']=sky.get_component_by_class(unreal.SkyLightComponent)
    STATE['sky'].set_mobility(unreal.ComponentMobility.MOVABLE)
    STATE['sky'].set_editor_property('real_time_capture',True)
    STATE['sky'].set_editor_property('lower_hemisphere_is_black',False)
    for command in ('r.EyeAdaptationQuality 0','r.DefaultFeature.AutoExposure 0','r.DefaultFeature.MotionBlur 0','r.DefaultFeature.Bloom 0'):
        unreal.SystemLibrary.execute_console_command(STATE['world'],command)
    camera=spawn(unreal.CameraActor,'Unsaved inspection camera',(45,70,65))
    cam=camera.get_component_by_class(unreal.CameraComponent)
    cam.set_aspect_ratio(RESOLUTION[0]/RESOLUTION[1]);cam.set_constraint_aspect_ratio(True)
    post=cam.get_editor_property('post_process_settings')
    for key,value in {'override_auto_exposure_min_brightness':True,'auto_exposure_min_brightness':1.0,
                       'override_auto_exposure_max_brightness':True,'auto_exposure_max_brightness':1.0,
                       'override_auto_exposure_bias':True,'auto_exposure_bias':0.0}.items():post.set_editor_property(key,value)
    cam.set_editor_property('post_process_settings',post);cam.set_editor_property('post_process_blend_weight',1.0)
    STATE['camera']=camera;STATE['camera_component']=cam
    unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)

def pose():
    kind,status=CAPTURES[STATE['index']]
    device=STATE['device'];component=device.static_mesh_component
    device.set_actor_location_and_rotation(unreal.Vector(0,0,.05),unreal.Rotator(),False,True)
    component.set_material(2,d.load(f'{d.BASE}/Materials/MI_VS_D01_Status{status}'))
    sun,sky=(.75,.3) if kind=='Dim' else (3.0,1.0)
    STATE['sun'].set_intensity(sun);STATE['sky'].set_intensity(sky)
    if kind in ('Studio','Dim'):
        camera_location=unreal.Vector(45,70,65);target=unreal.Vector(0,0,4.5)
        camera_rotation=unreal.MathLibrary.find_look_at_rotation(camera_location,target);fov=31.0
    elif kind=='PlayerEye':
        camera_location=unreal.Vector(0,100,160);target=unreal.Vector(0,0,5)
        camera_rotation=unreal.MathLibrary.find_look_at_rotation(camera_location,target);fov=75.0
    else:
        camera_location=unreal.Vector(0,0,160);camera_rotation=unreal.Rotator();fov=85.0
        # Carry at world (42,8,148), with body exactly 8cm below that anchor.
        # Yaw90 makes local+Y face the camera, preserving glyph orientation.
        device.set_actor_location_and_rotation(unreal.Vector(42,8,140),unreal.Rotator(yaw=90),False,True)
    STATE['camera'].set_actor_location_and_rotation(camera_location,camera_rotation,False,True)
    STATE['camera_component'].set_field_of_view(fov)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(camera_location,camera_rotation)
    require(component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,'Visual collision enabled')
    require(d.close(d.xyz(device.get_actor_scale3d()),[1,1,1]),'Presentation must use real scale1')
    socket=component.get_socket_transform('Carry',unreal.RelativeTransformSpace.RTS_WORLD)
    if kind=='Carry':require(d.close(d.xyz(socket.get_editor_property('translation')),[42,8,148],.001),'Carry position mismatch')
    output=f'docs/assets/device01/Previews/D01_Unreal_{kind}_{status}.png'
    STATE['capture']={'kind':kind,'status':status,'output':output,'resolution':list(RESOLUTION),
        'camera_location_cm':d.xyz(camera_location),'camera_rotation_degrees':{a:getattr(camera_rotation,a) for a in ('pitch','yaw','roll')},
        'horizontal_fov_degrees':fov,'sun_intensity':sun,'sky_intensity':sky,'auto_exposure':False,'exposure_bias':0,
        'device_world_transform':transform_record(component.get_world_transform()),'carry_socket_world':transform_record(socket),
        'slot_names':['Shell','Trim','Status'],'status_material':component.get_material(2).get_path_name(),
        'collision_profile':str(component.get_collision_profile_name()),'collision_enabled':str(component.get_collision_enabled()),
        'composition':'actual native mesh, unsaved editor stage; no gameplay, player or hands'}
    STATE['pending']=ROOT/f'Saved/Device01/{STATE["run_id"]}-{STATE["index"]}.png'
    STATE['pose_started']=time.monotonic()

def finish(error=None):
    if STATE['finished']:return
    STATE['finished']=True
    if STATE['handle'] is not None:unreal.unregister_slate_post_tick_callback(STATE['handle'])
    if error:unreal.log_error('VALSTRIKE_DEVICE01_INSPECTION_FAILED: '+error)
    unreal.EditorPythonScripting.set_keep_python_script_alive(False)

def tick(_dt):
    try:
        require(time.monotonic()-STATE['started']<240,'Device capture timeout')
        if STATE['task'] is None:
            if time.monotonic()-STATE['pose_started']<3:return
            STATE['task']=unreal.AutomationLibrary.take_high_res_screenshot(*RESOLUTION,str(STATE['pending']),camera=STATE['camera'],delay=2,force_game_view=True)
            require(STATE['task'] and STATE['task'].is_valid_task(),'Invalid screenshot task');return
        if not STATE['task'].is_task_done():return
        data=STATE['pending'].read_bytes()
        require(len(data)>1000 and data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==RESOLUTION,'Invalid fresh PNG')
        STATE['capture']['sha256']=hashlib.sha256(data).hexdigest()
        STATE['report']['captures'].append(STATE['capture'])
        STATE['completed'].append((STATE['pending'],ROOT/STATE['capture']['output']))
        STATE['index']+=1;STATE['task']=None
        if STATE['index']<len(CAPTURES):pose();return
        STATE['report']['elapsed_seconds']=round(time.monotonic()-STATE['started'],3)
        for temporary,final in STATE['completed']:
            final.parent.mkdir(parents=True,exist_ok=True);temporary.replace(final)
        (d.REPORTS/'unreal-inspection-report.json').write_text(json.dumps(STATE['report'],indent=2)+'\n')
        unreal.log('VALSTRIKE_DEVICE01_INSPECTION_SUCCESS: 8 native captures');finish()
    except Exception:finish(traceback.format_exc())

def main():
    require('-VSDeviceInspectOnly' in unreal.SystemLibrary.get_command_line(),'Capture requires fresh-process inspect-only mode')
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'Use fresh editor without unsaved work')
    reopening=d.main()
    unreal.EditorPythonScripting.set_keep_python_script_alive(True)
    STATE.update(started=time.monotonic(),run_id=uuid.uuid4().hex,completed=[])
    STATE['report']={'task':'ART-04','engine':unreal.SystemLibrary.get_engine_version(),'run_id':STATE['run_id'],
        'date_europe_berlin':'2026-11-03','fresh_reopen_verified':True,'appearance_sha256':reopening['appearance_sha256'],
        'fbx_sha256':reopening['mesh']['fbx_sha256'],'captures':[],
        'scene':'transient editor composition; no assets/maps saved','runtime_verified':False,
        'glyph_orientation':'View from native+Y corresponds to source-Y; +X source U, -Y native V',
        'status_behavior':'Material selection is a presentation fixture, not replicated game state; no time/pulse/countdown'}
    (ROOT/'Saved/Device01').mkdir(parents=True,exist_ok=True)
    stage();pose();STATE['handle']=unreal.register_slate_post_tick_callback(tick)

if __name__=='__main__':
    try:main()
    except Exception:finish(traceback.format_exc())
