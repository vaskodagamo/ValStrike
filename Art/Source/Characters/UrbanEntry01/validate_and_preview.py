"""Focused ART-13 saved-source/export round-trip and supplied-clip previews.
Animation frames are sampled at 15 FPS for compact evidence; source stays 60 FPS.
No scene changes are saved. No Unreal runs or new test framework.
"""
import bpy, bmesh, json, math, sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[3]
DOC=REPO/'docs/assets/urban-entry01'
CLIPS=['Walking','Running','Crouch_Walk_Left_with_Gun','Vault_with_Rifle','Walk_Backward_While_Shooting','Walk_Backward_with_Gun']

def set_action(arm,action):
    arm.animation_data_create();arm.animation_data.action=action
    arm.animation_data.action_slot=action.slots[0]
    bpy.context.view_layer.update()

def joint_points(arm):return {p.name:arm.matrix_world@p.head for p in arm.pose.bones}

def vertices(obj):
    e=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());m=e.to_mesh()
    v=[e.matrix_world@m.vertices[i].co for i in range(0,len(m.vertices),23)]
    e.to_mesh_clear();return v

def set_label(text):
    obj=bpy.data.objects.get('Preview_Label')
    if obj is None:
        curve=bpy.data.curves.new('Preview_Label','FONT');curve.size=3;curve.space_line=1.3
        obj=bpy.data.objects.new('Preview_Label',curve);bpy.context.scene.collection.objects.link(obj)
        mat=bpy.data.materials.new('Label');mat.diffuse_color=(.04,.04,.04,1);obj.data.materials.append(mat)
    obj.data.body=text
    obj.parent=bpy.context.scene.camera
    obj.location=(-91,89,-280)
    return obj

def main():
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/'UrbanEntry01.blend'))
    sc=bpy.context.scene;source_scene=sc
    arm=bpy.data.objects['SKEL_UE01'];mesh=bpy.data.objects['SK_VS_UrbanEntry01']
    actions={c:bpy.data.actions['A_UE01_'+c] for c in CLIPS}
    report={'source_reopened':True,'texture_files':[], 'clips':{},'roundtrip':{}}
    for img in bpy.data.images:
        if img.source=='FILE':
            path=Path(bpy.path.abspath(img.filepath))
            assert path.exists(),path
            img.reload();assert img.size[:]==(4096,4096),img.size[:]
            report['texture_files'].append({'file':img.filepath,'size':list(img.size),'space':img.colorspace_settings.name})
    bm=bmesh.new();bm.from_mesh(mesh.data)
    report['topology']={'raw_boundary_edges':sum(e.is_boundary for e in bm.edges),'degenerate_faces':sum(f.calc_area()<1e-10 for f in bm.faces)}
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001)
    report['topology']['diagnostic_weld_boundary_edges']=sum(e.is_boundary for e in bm.edges)
    report['topology']['diagnostic_weld_nonmanifold_edges']=sum(not e.is_manifold for e in bm.edges)
    report['topology']['diagnostic_only']='Coordinate weld on a disposable copy; delivered topology unchanged'
    bm.free()
    base_names=list(arm.data.bones.keys())
    assert len(base_names)==66 and len(mesh.data.materials)==1
    samples={}
    for clip in CLIPS:
        action=actions[clip];set_action(arm,action)
        start,end=map(round,action.frame_range)
        frames=list(range(start,end+1))
        records=[];samples[clip]={}
        for f in frames:
            sc.frame_set(f);points=joint_points(arm)
            records.append({name:list(points[name]) for name in ['pelvis','foot_l','ball_l','foot_r','ball_r','hand_l','hand_r']})
            if f in {start,end,(start+end)//2}:
                samples[clip][f]={'bones':points,'vertices':vertices(mesh),
                    'rotations':{p.name:(arm.matrix_world@p.matrix).to_quaternion() for p in arm.pose.bones}}
        first,last=samples[clip][start],samples[clip][end]
        origin_delta=last['bones']['pelvis']-first['bones']['pelvis']
        names=[n for n in base_names if n!='pelvis']
        loop=max(((last['bones'][n]-last['bones']['pelvis'])-(first['bones'][n]-first['bones']['pelvis'])).length for n in names)
        angular=max(math.degrees(first['rotations'][n].rotation_difference(last['rotations'][n]).angle) for n in names)
        mins=[min(r['pelvis'][i] for r in records) for i in range(3)]
        maxs=[max(r['pelvis'][i] for r in records) for i in range(3)]
        clipdata={'frame_range':[start,end],'fps':60,'duration_seconds':(end-start)/60,
            'pelvis_start_cm':list(first['bones']['pelvis']),'pelvis_end_delta_cm':list(origin_delta),
            'pelvis_range_cm':[[mins[i],maxs[i]] for i in range(3)],
            'first_last_max_joint_difference_pelvis_relative_cm':loop,
            'first_last_max_joint_angle_degrees':angular,
            'toe_height_range_cm':{n:[min(r[n][2] for r in records),max(r[n][2] for r in records)] for n in ['ball_l','ball_r']},
            'ground_root_bone':False,'trajectory_samples_60fps':records}
        report['clips'][clip]=clipdata
        print('MOTION',clip,json.dumps({k:v for k,v in clipdata.items() if k!='trajectory_samples_60fps'}),flush=True)
    # The actual character export is imported once. All six animation-only files
    # are imported against it; inspect names/rest/hierarchy before assigning actions.
    temp=bpy.data.scenes.new('ROUNDTRIP_ONLY');temp.unit_settings.system='METRIC';temp.unit_settings.scale_length=.01;temp.render.fps=60
    bpy.context.window.scene=temp
    bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports/SK_VS_UrbanEntry01.fbx'),use_image_search=False)
    rig=next(o for o in temp.objects if o.type=='ARMATURE');body=next(o for o in temp.objects if o.type=='MESH')
    body.data.calc_loop_triangles()
    assert set(rig.data.bones.keys())==set(base_names)
    assert len(body.data.loop_triangles)==30970
    bind_error=max((rig.matrix_world@rig.data.bones[n].head_local-arm.matrix_world@arm.data.bones[n].head_local).length for n in base_names)
    assert bind_error<.01
    report['roundtrip']['character']={'triangles':len(body.data.loop_triangles),'vertices':len(body.data.vertices),'bone_count':len(rig.data.bones),'max_bind_origin_error_cm':bind_error,'uv_layers':[u.name for u in body.data.uv_layers]}
    for clip in CLIPS:
        before=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(ROOT/'Exports'/('A_UE01_'+clip+'.fbx')),use_anim=True,use_image_search=False,anim_offset=0.0)
        added=set(bpy.data.objects)-before
        assert len([o for o in added if o.type=='MESH'])==0
        ar=next(o for o in added if o.type=='ARMATURE')
        assert set(ar.data.bones.keys())==set(base_names)
        rest_error=max(abs(x-y) for n in base_names for a,b in zip(rig.data.bones[n].matrix_local,ar.data.bones[n].matrix_local) for x,y in zip(a,b))
        assert rest_error<.001
        assert all((rig.data.bones[n].parent.name if rig.data.bones[n].parent else None)==(ar.data.bones[n].parent.name if ar.data.bones[n].parent else None) for n in base_names)
        a=ar.animation_data.action
        expected=report['clips'][clip]['frame_range'];assert list(map(round,a.frame_range))==expected
        assert temp.render.fps/temp.render.fps_base==60
        set_action(rig,a)
        joint_error=mesh_error=0
        for frame,reference in samples[clip].items():
            temp.frame_set(frame)
            actual=joint_points(rig)
            joint_error=max(joint_error,max((actual[n]-reference['bones'][n]).length for n in base_names))
            mesh_error=max(mesh_error,max((v-w).length for v,w in zip(vertices(body),reference['vertices'])))
        assert joint_error<.01 and mesh_error<.01,(clip,joint_error,mesh_error)
        report['roundtrip'][clip]={'animation_only':True,'skeleton_rest_max_difference':rest_error,'frame_range':expected,'fps':60,'max_sampled_joint_error_cm':joint_error,'max_sampled_mesh_error_cm':mesh_error}
        print('ROUNDTRIP',clip,joint_error,mesh_error,flush=True)
        for o in added:bpy.data.objects.remove(o,do_unlink=True)
    bpy.context.window.scene=source_scene
    for o in list(temp.objects):bpy.data.objects.remove(o,do_unlink=True)
    bpy.data.scenes.remove(temp)
    # Keep measured trajectory samples in a separate compact file for reproducibility.
    trajectories={c:report['clips'][c].pop('trajectory_samples_60fps') for c in CLIPS}
    (DOC/'motion_samples.json').write_text(json.dumps(trajectories,separators=(',',':'))+'\n')
    (DOC/'animation_validation.json').write_text(json.dumps(report,indent=2)+'\n')
    if '--no-renders' in sys.argv:
        print('ART13_EXPORTS_VERIFIED',flush=True);return
    # Texture-based studio preview keeps the full unmodified mesh, fast enough
    # for all supplied clips. No normal texture is synthesized.
    sc.render.engine='BLENDER_WORKBENCH'
    sc.display.shading.light='STUDIO';sc.display.shading.studiolight_rotate_z=0
    sc.display.shading.color_type='TEXTURE';sc.display.shading.show_shadows=True
    sc.display.shading.show_cavity=True;sc.display.shading.cavity_type='BOTH'
    sc.display.shading.background_type='VIEWPORT';sc.display.shading.background_color=(.28,.30,.33)
    sc.display.shading.show_specular_highlight=True
    sc.render.resolution_x=480;sc.render.resolution_y=560;sc.render.resolution_percentage=100
    sc.camera=bpy.data.objects['Preview_Front']
    # Locked camera for foot travel / root movement visibility; vault uses a wider
    # frame based on pelvis travel so no portion of its path is deliberately hidden.
    bpy.ops.mesh.primitive_plane_add(size=900,location=(0,0,-.3))
    floor=bpy.context.object;floor.name='PREVIEW_ONLY_Floor'
    floor_mat=bpy.data.materials.new('Floor');floor_mat.diffuse_color=(.36,.38,.40,1);floor.data.materials.append(floor_mat)
    line_mat=bpy.data.materials.new('Grid');line_mat.diffuse_color=(.18,.20,.22,1)
    for axis in range(2):
        for step in range(-300,351,50):
            curve=bpy.data.curves.new('50cm_grid','CURVE');curve.dimensions='3D';curve.bevel_depth=.16;curve.bevel_resolution=0
            line=curve.splines.new('POLY');line.points.add(1)
            line.points[0].co=(step,-350,0,1) if axis==0 else (-350,step,0,1)
            line.points[1].co=(step,350,0,1) if axis==0 else (350,step,0,1)
            o=bpy.data.objects.new('PREVIEW_ONLY_Grid',curve);sc.collection.objects.link(o);curve.materials.append(line_mat)
    preview_manifest={}
    for clip in CLIPS:
        set_action(arm,actions[clip])
        start,end=report['clips'][clip]['frame_range']
        frames=list(range(start,end,4))
        duration=(end-start)/60
        path=DOC/'Previews'/'Frames'/clip;path.mkdir(parents=True,exist_ok=True)
        sc.camera=bpy.data.objects['Preview_Front']
        sc.camera.data.ortho_scale=195
        ranges=report['clips'][clip]['pelvis_range_cm']
        center=Vector(((ranges[0][0]+ranges[0][1])/2,(ranges[1][0]+ranges[1][1])/2,85))
        travel=math.hypot(ranges[0][1]-ranges[0][0],ranges[1][1]-ranges[1][0])
        sc.camera.data.ortho_scale=max(195,195+.65*travel)
        if clip=='Vault_with_Rifle':
            center.z=100;sc.camera.data.ortho_scale=360
        sc.camera.location=center+Vector((270,-430,95))
        sc.camera.rotation_euler=(center-sc.camera.location).to_track_quat('-Z','Y').to_euler()
        label=set_label('URBAN ENTRY  /  '+clip.replace('_',' ')+'\nBLENDER SOURCE  |  15 FPS PREVIEW  |  NO ENGINE')
        # Text overlays are separate from all export data and saved source.
        label.location.x=-sc.camera.data.ortho_scale*.40;label.location.y=sc.camera.data.ortho_scale*.45
        label.data.size=2.3 if clip!='Vault_with_Rifle' else 3.5
        for i,f in enumerate(frames):
            sc.frame_set(f);sc.render.filepath=str(path/('%03d.png'%i));bpy.ops.render.render(write_still=True)
        preview_manifest[clip]={'frames':frames,'source_fps':60,'duration_seconds':duration,'nominal_preview_fps':15,'repeat_for_review':True,'in_place_loop_candidate':clip in {'Walking','Running'}}
        print('PREVIEWED',clip,len(frames),flush=True)
    (DOC/'preview_manifest.json').write_text(json.dumps(preview_manifest,indent=2)+'\n')
    print('ART13_EXPORTS_AND_PREVIEWS_COMPLETE',flush=True)

if __name__=='__main__':main()
