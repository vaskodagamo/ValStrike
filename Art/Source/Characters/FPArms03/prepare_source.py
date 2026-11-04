"""ART-22 local finishing of the recorded Meshy draft; no service calls.
Blender 5.2.2, one process / two threads. Rebuild overwrites generated FPArms03 files.
"""
import bpy, bmesh, math, json, hashlib, shutil
from pathlib import Path
from mathutils import Vector, Matrix
ROOT=Path(__file__).resolve().parent; REPO=ROOT.parents[3]; DOC=REPO/'docs/assets/fparms03'
for n in ['Exports','Textures']: (ROOT/n).mkdir(exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(ROOT/'Original/model.glb'))
o=next(o for o in bpy.context.scene.objects if o.type=='MESH');o.name='SK_FPArms03_R'
bpy.context.view_layer.objects.active=o;o.select_set(True)
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
bpy.context.scene.unit_settings.system="METRIC";bpy.context.scene.unit_settings.scale_length=.01
before={'vertices':len(o.data.vertices),'triangles':len(o.data.polygons)}
# Producer preservation gate: no weld, reduction, smoothing, UV or material edits.
original_normals=[n.vector.copy() for n in o.data.corner_normals]
original_custom_normals=o.data.has_custom_normals
raw=[v.co.copy() for v in o.data.vertices]
HAND=17.83610451842081; FOREARM=22.612010205024273; TOP=.9504119753837585;BOTTOM=-.9521030187606812
SCALE=(HAND+FOREARM)/(TOP-BOTTOM);WRIST=BOTTOM+FOREARM/SCALE
def convert(v,side='r'):
    x,y,z=v; xcm=24+(z-WRIST)*SCALE
    return Vector((xcm,-13-(x-.057)*SCALE,-y*SCALE)) if side=='r' else Vector((xcm,13+(x-.057)*SCALE,-y*SCALE))
for v,p in zip(o.data.vertices,raw): v.co=convert(p)
# Transform supplied corner normals by the inverse transpose of the scale/basis.
transformed_normals=[]
for loop,n in zip(o.data.loops,original_normals):
    p=raw[loop.vertex_index];sz=SCALE
    transformed_normals.append(Vector((n.z/sz,-n.x/SCALE,-n.y/SCALE)).normalized())
o.data.normals_split_custom_set(transformed_normals)
# Preserve Meshy's imported PBR node graph and packed texture inputs exactly.
for img in bpy.data.images:
    if img.source=='FILE' and not img.packed_file:img.pack()
for role in ['base_color','roughness','metallic','normal']:
    shutil.copyfile(ROOT/'Original'/('texture_0_'+role+'.png'),ROOT/'Textures'/('T_FPArms03_'+role+'.png'))
# Explicit reversed winding and correspondingly reversed UV/normal corners for left.
leftmesh=bpy.data.meshes.new('MeshyPreservedLeft')
leftmesh.from_pydata([(v.co.x,-v.co.y,v.co.z) for v in o.data.vertices],[],[list(p.vertices)[::-1] for p in o.data.polygons]);leftmesh.update()
for mat in o.data.materials:leftmesh.materials.append(mat)
for old in o.data.uv_layers:
    uv=leftmesh.uv_layers.new(name=old.name)
    for before_face,after_face in zip(o.data.polygons,leftmesh.polygons):
        for old_idx,new_idx in zip(list(before_face.loop_indices)[::-1],after_face.loop_indices):uv.data[new_idx].uv=old.data[old_idx].uv
left_normals=[]
for p in o.data.polygons:
    for idx in list(p.loop_indices)[::-1]:
        n=transformed_normals[idx];left_normals.append((n.x,-n.y,n.z))
for before_face,after_face in zip(o.data.polygons,leftmesh.polygons):after_face.use_smooth=before_face.use_smooth;after_face.material_index=before_face.material_index
leftmesh.normals_split_custom_set(left_normals)
left=bpy.data.objects.new('SK_FPArms03_L',leftmesh);bpy.context.collection.objects.link(left)
meshes=[o,left]
# Measured dorsal landmarks, then thickness centers sampled from the generated surface.
landmarks={
 'index':[(-.068,.535),(-.067,.650),(-.065,.765),(-.065,.885)],
 'middle':[(.048,.55),(.048,.715),(.048,.825),(.048,.945)],
 'ring':[(.148,.53),(.148,.660),(.150,.775),(.150,.885)],
 'pinky':[(.250,.48),(.262,.60),(.270,.705),(.275,.795)],
 'thumb':[(-.115,.24),(-.18,.365),(-.235,.49),(-.277,.60)]}
def thickness_center(x,z):
    ys=sorted(p.y for p in raw if abs(p.x-x)<.026 and abs(p.z-z)<.025)
    return (ys[int(len(ys)*.08)]+ys[int(len(ys)*.92)])/2 if ys else 0
paths={k:[Vector((x,thickness_center(x,z),z)) for x,z in pts] for k,pts in landmarks.items()}
armdata=bpy.data.armatures.new('SKEL_FPArms03');arm=bpy.data.objects.new('SKEL_FPArms03',armdata);bpy.context.collection.objects.link(arm)
bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm;bpy.ops.object.mode_set(mode='EDIT')
def bone(name,head,tail,parent=None,deform=True):
    b=armdata.edit_bones.new(name);b.head=head;b.tail=tail;b.use_deform=deform
    if parent:b.parent=armdata.edit_bones[parent]
    b.align_roll(Vector((0,0,1)));return b
bone('root',(0,0,0),(0,0,3),deform=False)
for side in ['r','l']:
    sign=-1 if side=='r' else 1; y=13*sign
    bone('forearm_'+side,(24-FOREARM,y,0),(13,y,0),'root')
    bone('forearm_twist_'+side,(13,y,0),(24,y,0),'forearm_'+side)
    bone('hand_'+side,(24,y,0),(33,y,0),'forearm_twist_'+side)
    for digit,pts in paths.items():
        for i in range(3):bone(f'{digit}_{i+1:02}_{side}',convert(pts[i],side),convert(pts[i+1],side),'hand_'+side if i==0 else f'{digit}_{i:02}_{side}')
    bone('weapon_socket_'+side,(27,y,0),(30,y,0),'hand_'+side,False)
bpy.ops.object.mode_set(mode='OBJECT');arm.show_in_front=True
def smooth(a,b,x):
    t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
def nearest_on_path(p,pts):
    best=None;length=0
    for i,(a,b) in enumerate(zip(pts,pts[1:])):
        delta=b-a;l=delta.length;t=max(0,min(1,(p-a).dot(delta)/delta.length_squared));q=a+delta*t;dist=(p-q).length
        if best is None or dist<best[0]:best=(dist,length+t*l,i)
        length+=l
    return best
weight_stats={}
for mesh,side in zip(meshes,['r','l']):
    groups={b.name:mesh.vertex_groups.new(name=b.name) for b in armdata.bones if b.use_deform and b.name.endswith('_'+side)}
    for i,p in enumerate(raw):
        thumb_boundary=-.12-max(0,p.z-.30)*.25
        thumb=p.x<thumb_boundary+.02 and .22<p.z<.67
        candidates=['thumb'] if thumb else ['index','middle','ring','pinky']
        flat=Vector((p.x,0,p.z))
        digit=min(candidates,key=lambda d:nearest_on_path(flat,[Vector((q.x,0,q.z)) for q in paths[d]])[0]);pts=paths[digit]
        finger=(thumb or p.z>pts[0].z-.028)
        if finger:
            dist,s,seg=nearest_on_path(p,pts);lengths=[(b-a).length for a,b in zip(pts,pts[1:])]
            # Signed extension into the palm permits an actual MCP blend.
            if seg==0:s=max(-.08,(p-pts[0]).dot((pts[1]-pts[0]).normalized()))
            w={};width=.035
            if s<width:
                t=smooth(-width,width,s);w['hand_'+side]=1-t;w[f'{digit}_01_{side}']=t
            else:
                j1=lengths[0];j2=j1+lengths[1]
                t1=smooth(j1-width,j1+width,s);t2=smooth(j2-width,j2+width,s)
                w={f'{digit}_01_{side}':1-t1,f'{digit}_02_{side}':t1*(1-t2),f'{digit}_03_{side}':t2}
        elif p.z>WRIST-.08:
            t=smooth(WRIST-.08,WRIST+.08,p.z);w={'forearm_twist_'+side:1-t,'hand_'+side:t}
        else:
            t=smooth(-.62,-.30,p.z);w={'forearm_'+side:1-t,'forearm_twist_'+side:t}
        if thumb:
            influence=1-smooth(thumb_boundary-.065,thumb_boundary+.02,p.x)
            w={name:value*influence for name,value in w.items()};w['hand_'+side]=w.get('hand_'+side,0)+1-influence
        total=sum(w.values())
        for name,value in w.items():
            if value>.00001:groups[name].add([i],value/total,'REPLACE')
    # Smooth weights over mesh adjacency at web/joint boundaries, preserving separated digits.
    weights=[{g.group:g.weight for g in v.groups} for v in mesh.data.vertices]
    neighbors=[set() for v in mesh.data.vertices]
    for e in mesh.data.edges:
        a,b=e.vertices;neighbors[a].add(b);neighbors[b].add(a)
    for iteration in range(3):
        updated=[]
        for i,w in enumerate(weights):
            nw={k:v*.65 for k,v in w.items()};ns=neighbors[i]
            for j in ns:
                for k,v in weights[j].items():nw[k]=nw.get(k,0)+v*.35/len(ns)
            keep=sorted(nw.items(),key=lambda kv:kv[1],reverse=True)[:4];total=sum(v for k,v in keep);updated.append({k:v/total for k,v in keep if v>.0001})
        weights=updated
    # Coincident UV-split vertices keep identical skin weights without welding geometry.
    coincident={}
    for i,p in enumerate(raw):coincident.setdefault(tuple(round(v,6) for v in p),[]).append(i)
    for ids in coincident.values():
        if len(ids)<2:continue
        averaged={}
        for i in ids:
            for k,v in weights[i].items():averaged[k]=averaged.get(k,0)+v/len(ids)
        keep=sorted(averaged.items(),key=lambda kv:kv[1],reverse=True)[:4];total=sum(v for k,v in keep)
        for i in ids:weights[i]={k:v/total for k,v in keep}
    for g in mesh.vertex_groups:g.remove(range(len(weights)))
    for i,w in enumerate(weights):
        total=sum(w.values())
        for k,v in w.items():mesh.vertex_groups[k].add([i],v/total,'REPLACE')
    mod=mesh.modifiers.new('Finger wrist forearm deformation','ARMATURE');mod.object=arm;mod.use_deform_preserve_volume=False
    mesh.parent=arm
    weight_stats[mesh.name]={'vertices':len(mesh.data.vertices),'triangles':len(mesh.data.polygons),'unweighted':sum(not v.groups for v in mesh.data.vertices),'max_influences':max(len(v.groups) for v in mesh.data.vertices)}
# Four bounded demonstration actions. Holds use these finger controls, not a humanoid auto-rig.
def reset():
    if arm.animation_data:arm.animation_data.action=None
    for b in arm.pose.bones:b.rotation_mode='XYZ';b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()
def pose(kind,amount=1):
    for side in ['r','l']:
        if kind in ['Grip']:
            for digit in ['index','middle','ring','pinky']:
                angles=[62,78,48]
                if kind in ['RifleHold','PistolHold'] and digit=='index' and side=='r':angles=[18,28,12]
                for i,a in enumerate(angles):arm.pose.bones[f'{digit}_{i+1:02}_{side}'].rotation_euler.x=-math.radians(a)*amount
            for i,a in enumerate([8,22,15]):arm.pose.bones[f'thumb_{i+1:02}_{side}'].rotation_euler.x=-math.radians(a)*amount
            arm.pose.bones['thumb_01_'+side].rotation_euler.z=math.radians(-8 if side=='r' else 8)*amount
        if kind=='Wrist':
            arm.pose.bones['hand_'+side].rotation_euler.x=math.radians(28)*amount
            arm.pose.bones['forearm_twist_'+side].rotation_euler.y=math.radians(22 if side=='r' else -22)*amount
    bpy.context.view_layer.update()
scene=bpy.context.scene;scene.render.fps=30;scene.frame_start=1;scene.frame_end=30
actions={}
for kind in ['Open','Grip','Wrist']:
    reset();action=bpy.data.actions.new('A_FPArms03_'+kind);action.use_fake_user=True;arm.animation_data_create();arm.animation_data.action=action
    for frame,amount in [(1,0),(15,1),(30,0)]:
        scene.frame_set(frame);pose(kind,amount)
        for b in arm.pose.bones:
            for prop in ['location','rotation_euler','scale']:b.keyframe_insert(data_path=prop,frame=frame,group=b.name)
    actions[kind]=action
reset();scene.frame_set(1)
def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in objects:ob.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
select([arm]+meshes)
bpy.ops.export_scene.fbx(filepath=str(ROOT/'Exports/SK_FPArms03.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',path_mode='RELATIVE')
for kind,action in actions.items():
    arm.animation_data.action=action;arm.animation_data.action_slot=action.slots[0];scene.frame_set(1);select([arm])
    bpy.ops.export_scene.fbx(filepath=str(ROOT/'Exports'/f'A_FPArms03_{kind}.fbx'),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS')
reset();select([arm]+meshes)
report={'blender':bpy.app.version_string,'generated_before':before,'preservation':{'geometry_reduction':False,'geometry_smoothing':False,'uvs_preserved':True,'supplied_custom_normals':original_custom_normals,'custom_normals_basis_transformed':True,'original_imported_pbr_unchanged':True},'cleaned':weight_stats,'basis':'centimeters; +X fingers/forward, Z up; right side -Y; left +Y','measurements_cm':{'forearm':FOREARM,'wrist_to_middle_tip':HAND},'character_reference_commit':'45deb700a96b2aad3084808b1c7809d4589fdbd7','landmarks_raw':{k:[list(p) for p in v] for k,v in paths.items()},'bones':[{'name':b.name,'parent':b.parent.name if b.parent else None,'head_cm':list(b.head_local),'tail_cm':list(b.tail_local),'deform':b.use_deform} for b in armdata.bones]}
(DOC/'source-inspection.json').write_text(json.dumps(report,indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'FPArms03.blend'))
print('ART22_SOURCE_SAVED')
