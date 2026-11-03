"""ART-04 D01 Relay: original stylized objective visual, no gameplay.

Blender 5.2.2; centimeters, +X forward / +Z up, bottom-center origin.
-- --skip-render exports/validates only; -- --render-only uses saved source.
Glyph masks are original mathematical graphics, authored through Blender's
image API. There are no external fonts, textures, meshes or mechanisms.
"""
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
EXPORTS, TEXTURES, PREVIEWS = (ROOT / name for name in ('Exports', 'Textures', 'Previews'))
BLEND = ROOT / 'D01_Relay.blend'
APPEARANCE_PATH = ROOT.parent / 'appearance.json'
APPEARANCE = json.loads(APPEARANCE_PATH.read_text())
NAME = 'SM_VS_Device01'
SLOTS = ['Shell', 'Trim', 'Status']
MATERIALS, MASKS = {}, {}
SCREEN_Z = 8.81
TEXTURE_SIZE = 256


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def rgb(code):
    def linear(v):
        return v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4
    return tuple(linear(int(code[i:i+2],16)/255) for i in (0,2,4)) + (1,)


def material(name, color, roughness, metallic):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = rgb(color)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Base Color'].default_value = rgb(color)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metallic
    return mat


def segment_distance(x, y, start, end):
    dx, dy = end[0]-start[0], end[1]-start[1]
    t = max(0., min(1., ((x-start[0])*dx+(y-start[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(x-start[0]-t*dx, y-start[1]-t*dy)


def glyph_contains(state, x, y):
    """Vector-equivalent formula in physical-aspect coordinates; no font input."""
    if state == 'Idle':
        return abs(math.hypot(x,y)-.28) <= .035 or (abs(x) <= .15 and abs(y) <= .035)
    if state == 'Armed':
        triangle = [(-.33,-.27),(.33,-.27),(0,.32)]
        outline = any(segment_distance(x,y,triangle[i],triangle[(i+1)%3]) <= .028 for i in range(3))
        return outline or (abs(x) <= .032 and -.015 <= y <= .16) or math.hypot(x,y+.13) <= .036
    if state == 'Defused':
        box = [(-.34,-.34),(.34,-.34),(.34,.34),(-.34,.34)]
        outline = any(segment_distance(x,y,box[i],box[(i+1)%4]) <= .022 for i in range(4))
        check = (segment_distance(x,y,(-.23,-.01),(-.065,-.17)) <= .035 or
                 segment_distance(x,y,(-.065,-.17),(.23,.18)) <= .035)
        return outline or check
    raise ValueError(state)


def make_masks():
    # 3x3 supersampling provides small edge coverage values; background stays
    # exactly black, glyph interior white, all alpha opaque. At 8x5 cm, X's
    # 1.6 aspect correction makes the idle ring physically circular.
    for state in APPEARANCE['Status']['states']:
        image = bpy.data.images.new('T_VS_D01_Status'+state, width=TEXTURE_SIZE, height=TEXTURE_SIZE, alpha=True)
        image.colorspace_settings.name = 'Non-Color'
        image.use_fake_user = True
        image.alpha_mode = 'STRAIGHT'
        pixels = []
        for row in range(TEXTURE_SIZE):
            for col in range(TEXTURE_SIZE):
                covered = 0
                for sy in (.1666667,.5,.8333333):
                    for sx in (.1666667,.5,.8333333):
                        x = ((col+sx)/TEXTURE_SIZE-.5)*1.6
                        y = (row+sy)/TEXTURE_SIZE-.5
                        covered += glyph_contains(state,x,y)
                value = covered/9.
                pixels.extend((value,value,value,1.))
        image.pixels.foreach_set(pixels)
        image.filepath_raw = str(TEXTURES/(image.name+'.png'))
        image.file_format = 'PNG'
        image.save()
        image.pack()
        image.filepath = '//Textures/'+image.name+'.png'
        MASKS[state] = image


def make_materials():
    for name in ('Shell','Trim'):
        spec = APPEARANCE[name]
        MATERIALS[name] = material(name,spec['srgb_hex'],spec['roughness'],spec['metallic'])
    spec = APPEARANCE['Status']
    status = material('Status', spec['background_srgb_hex'],spec['roughness'],spec['metallic'])
    MATERIALS['Status'] = status
    nodes, links = status.node_tree.nodes, status.node_tree.links
    bsdf = nodes['Principled BSDF']
    texture = nodes.new('ShaderNodeTexImage')
    texture.name = 'StatusMask'
    texture.interpolation = 'Linear'
    texture.extension = 'EXTEND'
    mix = nodes.new('ShaderNodeMixRGB')
    mix.name = 'StatusBaseColor'
    mix.blend_type = 'MIX'
    mix.inputs[1].default_value = rgb(spec['background_srgb_hex'])
    links.new(texture.outputs['Color'],mix.inputs[0])
    links.new(mix.outputs['Color'],bsdf.inputs['Base Color'])
    emissive = nodes.new('ShaderNodeMixRGB')
    emissive.name = 'StatusGlyphEmission'
    emissive.blend_type = 'MULTIPLY'
    emissive.inputs[0].default_value = 1
    links.new(texture.outputs['Color'],emissive.inputs[1])
    links.new(emissive.outputs['Color'],bsdf.inputs['Emission Color'])
    bsdf.inputs['Emission Strength'].default_value = spec['glyph_emissive_strength']
    select_state('Idle')


def select_state(state):
    status = bpy.data.materials['Status']
    nodes = status.node_tree.nodes
    nodes['StatusMask'].image = bpy.data.images['T_VS_D01_Status'+state]
    color = rgb(APPEARANCE['Status']['states'][state]['srgb_hex'])
    nodes['StatusBaseColor'].inputs[2].default_value = color
    nodes['StatusGlyphEmission'].inputs[2].default_value = color
    status['visual_state'] = state


def recollect(obj, collection):
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    collection.objects.link(obj)


def finish(obj,label,mat,bevel):
    obj.name = label
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod = obj.modifiers.new('Stylized chamfer','BEVEL')
        mod.width = bevel
        mod.segments = 1
        bpy.ops.object.modifier_apply(modifier=mod.name)
    obj.data.materials.append(MATERIALS[mat])
    obj.select_set(False)
    return obj


def box(label, center, size, mat='Shell', bevel=.2):
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    obj=bpy.context.object
    obj.dimensions=size
    return finish(obj,label,mat,bevel)


def mesh_object(label, vertices, faces, mat, bevel=0):
    mesh=bpy.data.meshes.new(label+'_editable')
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    bm=bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm,faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    obj=bpy.data.objects.new(label,mesh)
    bpy.context.scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    return finish(obj,label,mat,bevel)


def prism(label, profile, width, y=0, mat='Shell',bevel=.2):
    n=len(profile)
    vertices=[(x,y+sign*width/2,z) for sign in (-1,1) for x,z in profile]
    faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh_object(label,vertices,faces,mat,bevel)


def screen_bezel():
    outer=[(-4.7,-3.2),(4.7,-3.2),(4.7,3.2),(-4.7,3.2)]
    inner=[(-4.08,-2.58),(4.08,-2.58),(4.08,2.58),(-4.08,2.58)]
    vertices=[(x,y,z) for loop,z in ((outer,8.4),(outer,8.95),(inner,8.4),(inner,8.95)) for x,y in loop]
    faces=[]
    for i in range(4):
        j=(i+1)%4
        faces += [(i,j,4+j,4+i),(8+i,12+i,12+j,8+j),(i,8+i,8+j,j),(4+i,4+j,12+j,12+i)]
    return mesh_object('status_bezel',vertices,faces,'Trim',.06)


def make_parts():
    parts=[box('main_case',(0,0,4.8),(27.6,16.2,6.8),'Shell',.65),
           box('lower_case',(0,0,1.85),(26.2,15.4,2.5),'Shell',.4),
           box('top_deck',(0,0,8.1),(21.4,11.2,.7),'Shell',.22),
           box('screen_recess',(0,0,8.53),(9.4,6.4,.5),'Shell',.1),
           screen_bezel()]
    for side in (-1,1):
        parts.append(box('ground_foot',(0,side*7.5,.8),(26,3,1.6),'Shell',.27))
        parts.append(prism('protective_top_rail',[(-12.3,8.0),(-11.1,10),(10.8,10),(12.8,8.65),(12.3,8.0)],
                           2.25,side*6.25,'Trim',.15))
        # Broad readable side inset and two low-detail shoulder facets.
        parts.append(prism('side_inset',[(-10.7,3),(-9.2,5.9),(9.4,5.9),(11.6,4.8),(10.8,3)],
                           .18,side*8.12,'Trim',.05))
        for x in (-8.3,8.3):
            parts.append(box('side_panel_bridge',(x,side*8.28,4.45),(1.45,.18,2.25),'Shell',.06))
    # Distinct chamfered endcaps establish exact +/-15cm X bounds.
    parts += [prism('forward_endcap',[(12.7,1.1),(14.85,2.1),(14.85,6.3),(13.8,7.8),(12.7,7.8)],14.4,mat='Shell',bevel=.22),
              prism('rear_endcap',[(-15,2.1),(-12.7,1.1),(-12.7,7.8),(-13.8,7.8),(-15,6.3)],14.4,mat='Shell',bevel=.22),
              prism('forward_direction_panel',[(14.75,3.1),(15,3.5),(15,5.7),(14.75,6.1)],6.5,mat='Trim',bevel=.04)]
    # The only open surface: two top-facing Status triangles after triangulation.
    parts.append(mesh_object('status_face',[(-4,-2.5,SCREEN_Z),(4,-2.5,SCREEN_Z),(4,2.5,SCREEN_Z),(-4,2.5,SCREEN_Z)],[(0,1,2,3)],'Status'))
    return parts


def uv_tiles(mesh):
    for layer in list(mesh.uv_layers):
        mesh.uv_layers.remove(layer)
    grid=math.ceil(math.sqrt(len(mesh.polygons)))
    for name in ('UV0','UV1_Lightmap'):
        layer=mesh.uv_layers.new(name=name)
        for face in mesh.polygons:
            if name=='UV0' and face.material_index==2:
                for index in face.loop_indices:
                    point=mesh.vertices[mesh.loops[index].vertex_index].co
                    layer.data[index].uv=((point.x+4)/8,(point.y+2.5)/5)
                continue
            axes=[i for i in range(3) if i!=max(range(3),key=lambda axis:abs(face.normal[axis]))]
            points=[[mesh.vertices[mesh.loops[index].vertex_index].co[a] for a in axes] for index in face.loop_indices]
            minima=[min(p[a] for p in points) for a in (0,1)]
            span=max(max(p[a] for p in points)-minima[a] for a in (0,1))
            require(span>1e-9,'UV projection degenerate')
            col,row=face.index%grid,face.index//grid
            for index,point in zip(face.loop_indices,points):
                layer.data[index].uv=tuple(((col if a==0 else row)+.08+.84*(point[a]-minima[a])/span)/grid for a in (0,1))
    mesh.uv_layers.active_index=0


def bounds(obj):
    points=[obj.matrix_world@v.co for v in obj.data.vertices]
    low=[min(v[a] for v in points) for a in range(3)]
    high=[max(v[a] for v in points) for a in range(3)]
    return low,high,[high[a]-low[a] for a in range(3)]


def canonical(parts):
    editable=bpy.data.collections.new(NAME+'_EDITABLE_PARTS')
    bpy.context.scene.collection.children.link(editable)
    bpy.ops.object.select_all(action='DESELECT')
    copies=[]
    for part in parts:
        recollect(part,editable)
        copy=part.copy()
        copy.data=part.data.copy()
        bpy.context.scene.collection.objects.link(copy)
        copy.select_set(True)
        copies.append(copy)
    bpy.context.view_layer.objects.active=copies[0]
    bpy.ops.object.join()
    obj=bpy.context.object
    obj.name=NAME
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    names=[mat.name for mat in obj.data.materials]
    assignments=[SLOTS.index(names[face.material_index]) for face in obj.data.polygons]
    obj.data.materials.clear()
    for name in SLOTS:
        obj.data.materials.append(MATERIALS[name])
    for face,index in zip(obj.data.polygons,assignments):
        face.material_index=index
    obj.data.update()
    uv_tiles(obj.data)
    mod=obj.modifiers.new('Deterministic triangles','TRIANGULATE')
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for part in parts:
        part.hide_render=True
        part.hide_set(True)
    return obj


def validate(obj):
    low,high,dimensions=bounds(obj)
    require(max(abs(a-b) for a,b in zip(low,(-15,-9,0)))<1e-5,'Wrong minimum bounds '+str(low))
    require(max(abs(a-b) for a,b in zip(high,(15,9,10)))<1e-5,'Wrong maximum bounds '+str(high))
    require(len(obj.data.polygons)<4000,'Triangle budget')
    require([mat.name for mat in obj.data.materials]==SLOTS,'Slot order')
    require(all(abs(v)<1e-7 for v in obj.location) and all(abs(v)<1e-7 for v in obj.rotation_euler)
            and all(abs(v-1)<1e-7 for v in obj.scale),'Nonidentity transform')
    bm=bmesh.new()
    bm.from_mesh(obj.data)
    require(all(face.calc_area()>1e-9 for face in bm.faces),'Degenerate triangle')
    boundary=[e for e in bm.edges if not e.is_manifold]
    require(len(boundary)==4 and all(e.is_boundary and all(f.material_index==2 for f in e.link_faces) for e in boundary),
            'Only status perimeter may be open')
    require(sum(f.material_index==2 for f in bm.faces)==2,'Expected two status triangles')
    bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.material_index==2],context='FACES')
    volume=bm.calc_volume(signed=True)
    require(volume>0,'Nonpositive closed-shell volume')
    bm.free()
    for layer in obj.data.uv_layers:
        for face in obj.data.polygons:
            uv=[layer.data[i].uv for i in face.loop_indices]
            require(all(0<=v<=1 for pair in uv for v in pair),'UV bounds')
            area=(uv[1].x-uv[0].x)*(uv[2].y-uv[0].y)-(uv[1].y-uv[0].y)*(uv[2].x-uv[0].x)
            require(abs(area)>1e-14,'Degenerate UV triangle')
    return {'name':NAME,'file':'Exports/'+NAME+'.fbx','bounds_min_cm':low,'bounds_max_cm':high,
            'dimensions_cm':dimensions,'vertices':len(obj.data.vertices),'triangles':len(obj.data.polygons),
            'material_slots':SLOTS,'uv_channels':[layer.name for layer in obj.data.uv_layers],
            'transforms_identity':True,'closed_manifold_shells_except_status':True,
            'intentional_open_status_triangles':2,'open_status_boundary_edges':4,
            'closed_shell_signed_volume_cm3':volume,'nondegenerate_geometry_uvs':True,
            'collision':'none; visual only, runtime owns placement/collision'}


def export(obj):
    bpy.ops.object.select_all(action='DESELECT')
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(EXPORTS/(NAME+'.fbx')),use_selection=True,object_types={'MESH'},
        global_scale=1,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',
        use_space_transform=True,bake_space_transform=False,mesh_smooth_type='FACE',use_mesh_modifiers=True,
        use_tspace=False,bake_anim=False,add_leaf_bones=False,path_mode='AUTO',embed_textures=False)


def roundtrip(record):
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(EXPORTS/(NAME+'.fbx')),use_anim=False)
    added=[o for o in bpy.data.objects if o not in before]
    meshes=[o for o in added if o.type=='MESH']
    require(len(meshes)==1,'Expected one FBX mesh')
    obj=meshes[0]
    low,high,dimensions=bounds(obj)
    require(max(abs(a-b) for a,b in zip(low,record['bounds_min_cm']))<.002,'FBX min bounds')
    require(max(abs(a-b) for a,b in zip(high,record['bounds_max_cm']))<.002,'FBX max bounds')
    require(len(obj.data.polygons)==record['triangles'],'FBX triangles')
    require(len(obj.data.uv_layers)==2,'FBX UV channels')
    require([m.name.split('.')[0] for m in obj.data.materials]==SLOTS,'FBX material order')
    for face in obj.data.polygons:
        if face.material_index==2:
            for index in face.loop_indices:
                point=obj.matrix_world@obj.data.vertices[obj.data.loops[index].vertex_index].co
                actual=obj.data.uv_layers[0].data[index].uv
                require(abs(actual.x-(point.x+4)/8)<1e-5 and abs(actual.y-(point.y+2.5)/5)<1e-5,'FBX status UV orientation')
    record['fbx_roundtrip']={'passed':True,'bounds_min_cm':low,'bounds_max_cm':high,'dimensions_cm':dimensions,
                              'triangles':len(obj.data.polygons),'material_slots':SLOTS,'uv_channels':2,
                              'status_uv_mapping_verified':True,'collision_meshes':0}
    for obj in added:
        bpy.data.objects.remove(obj,do_unlink=True)


def aim(obj,target):
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()


def presentation(obj):
    coll=bpy.data.collections.new('PREVIEW_ONLY_linked_mesh')
    bpy.context.scene.collection.children.link(coll)
    obj.hide_render=True
    obj.hide_set(True)
    copy=bpy.data.objects.new('DISPLAY_'+NAME,obj.data)
    coll.objects.link(copy)
    marker=bpy.data.objects.new('SOCKET_Carry',None)
    coll.objects.link(marker)
    marker.location=(0,0,8)
    marker.empty_display_type='ARROWS'
    marker.empty_display_size=2
    ground=material('PREVIEW_Ground','A6A9AD',.9,0)
    bpy.ops.mesh.primitive_plane_add(size=1000,location=(0,0,-.05))
    floor=bpy.context.object
    floor.name='PREVIEW_Ground'
    floor.data.materials.append(ground)
    recollect(floor,coll)
    scene=bpy.context.scene
    scene.world=bpy.data.worlds.new('PREVIEW_World')
    scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.56,.63,.7,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.4
    for name,location,energy,size in (('Key',(12,-36,52),90000,46),('Fill',(-32,30,30),45000,40)):
        bpy.ops.object.light_add(type='AREA',location=location)
        light=bpy.context.object
        light.name='PREVIEW_'+name
        light.data.energy=energy
        light.data.size=size
        light['neutral_energy']=energy
        aim(light,(0,0,4))
        recollect(light,coll)
    bpy.ops.object.camera_add(location=(38,-54,46))
    camera=bpy.context.object
    camera.name='PREVIEW_StudioCamera'
    camera.data.type='ORTHO'
    camera.data.ortho_scale=43
    camera.data.clip_start=.1
    camera.data.clip_end=10000
    aim(camera,(0,0,4))
    recollect(camera,coll)
    scene.camera=camera
    scene.render.engine='CYCLES'
    scene.cycles.samples=40
    scene.cycles.use_denoising=True
    scene.render.threads_mode='FIXED'
    scene.render.threads=4
    scene.render.resolution_x=1400
    scene.render.resolution_y=1000
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.view_settings.view_transform='AgX'
    scene.view_settings.look='AgX - Medium High Contrast'


def lighting(dim=False):
    for name in ('Key','Fill'):
        light=bpy.data.objects['PREVIEW_'+name]
        light.data.energy=light['neutral_energy']*(.18 if dim else 1)
    bpy.context.scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.07 if dim else .4


def render_previews():
    scene=bpy.context.scene
    captures=[]
    # Three matched neutral states plus the default Idle in dim light.
    for state,dim in [('Idle',False),('Armed',False),('Defused',False),('Idle',True)]:
        select_state(state)
        lighting(dim)
        kind='Dim' if dim else 'Neutral'
        target=PREVIEWS/('D01_Relay_'+state+'_'+kind+'.png')
        scene.render.filepath=str(target)
        bpy.ops.render.render(write_still=True)
        captures.append({'state':state,'lighting':kind,'path':'Previews/'+target.name,
                         'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
                         'kind':'actual Blender mesh render; not gameplay/native Unreal'})
    select_state('Idle')
    lighting(False)
    (ROOT/'preview_validation.json').write_text(json.dumps({'captures':captures,'camera_location_cm':[38,-54,46],
        'orthographic_scale_cm':43,'resolution':[1400,1000],
        'neutral_world_strength':.4,'dim_world_strength':.07,'dim_area_light_multiplier':.18},indent=2)+'\n')
    print('VALSTRIKE_D01_PREVIEWS_SUCCESS')


def check_saved_source():
    obj=bpy.data.objects[NAME]
    validate(obj)
    for name in ('Shell','Trim'):
        spec=APPEARANCE[name]
        node=bpy.data.materials[name].node_tree.nodes['Principled BSDF']
        require(max(abs(a-b) for a,b in zip(node.inputs['Base Color'].default_value,rgb(spec['srgb_hex'])))<1e-6,'Saved base color')
        require(abs(node.inputs['Roughness'].default_value-spec['roughness'])<1e-6,'Saved roughness')
        require(abs(node.inputs['Metallic'].default_value-spec['metallic'])<1e-6,'Saved metallic')
    status=bpy.data.materials['Status']
    require(status['visual_state']=='Idle','Saved default must be Idle')
    node=status.node_tree.nodes['Principled BSDF']
    spec=APPEARANCE['Status']
    require(abs(node.inputs['Roughness'].default_value-spec['roughness'])<1e-6,'Saved status roughness')
    require(abs(node.inputs['Metallic'].default_value-spec['metallic'])<1e-6,'Saved status metallic')
    require(abs(node.inputs['Emission Strength'].default_value-spec['glyph_emissive_strength'])<1e-6,'Saved glyph emissive')
    require(max(abs(a-b) for a,b in zip(status.node_tree.nodes['StatusBaseColor'].inputs[1].default_value,
            rgb(spec['background_srgb_hex'])))<1e-6,'Saved status background')
    for nodename in ('StatusBaseColor','StatusGlyphEmission'):
        require(max(abs(a-b) for a,b in zip(status.node_tree.nodes[nodename].inputs[2].default_value,
                rgb(spec['states']['Idle']['srgb_hex'])))<1e-6,'Saved Idle color')
    require(status.node_tree.nodes['StatusMask'].image.name=='T_VS_D01_StatusIdle','Saved Idle mask')
    require(node.inputs['Base Color'].is_linked and node.inputs['Emission Color'].is_linked,'Saved status shader links')
    for state in APPEARANCE['Status']['states']:
        image=bpy.data.images['T_VS_D01_Status'+state]
        require(bool(image.packed_file),'Status image not packed')
        require(image.filepath=='//Textures/'+image.name+'.png','Status texture relative path')
        require(image.colorspace_settings.name=='Non-Color','Mask must be linear data')
        require(tuple(image.size)==(TEXTURE_SIZE,TEXTURE_SIZE),'Mask dimensions')
    # Validate all state selections and exact shader constants, then restore Idle.
    for state,spec in APPEARANCE['Status']['states'].items():
        select_state(state)
        require(max(abs(a-b) for a,b in zip(status.node_tree.nodes['StatusBaseColor'].inputs[2].default_value,rgb(spec['srgb_hex'])))<1e-6,'State color')
        require(status.node_tree.nodes['StatusMask'].image.name=='T_VS_D01_Status'+state,'State mask')
    select_state('Idle')


def mask_evidence():
    """Read actual saved PNG pixels as data and distinguish asymmetric check shape."""
    evidence={}
    for state in APPEARANCE['Status']['states']:
        path=TEXTURES/('T_VS_D01_Status'+state+'.png')
        loaded=bpy.data.images.load(str(path),check_existing=False)
        loaded.colorspace_settings.name='Non-Color'
        pixels=list(loaded.pixels)
        values=pixels[0::4]
        require(min(values)==0 and max(values)==1,'Mask must contain exact black/white')
        require(all(abs(pixels[i]-pixels[i+1])<1e-7 and abs(pixels[i]-pixels[i+2])<1e-7
                    and abs(pixels[i+3]-1)<1e-7 for i in range(0,len(pixels),4)),'Expected equal RGB and opaque alpha')
        packed=list(bpy.data.images['T_VS_D01_Status'+state].pixels)
        require(max(abs(a-b) for a,b in zip(packed,pixels))<=1/255+1e-6,'Packed source/disk masks differ')
        reflected=sum(abs(values[row*TEXTURE_SIZE+col]-values[row*TEXTURE_SIZE+(TEXTURE_SIZE-1-col)])
                      for row in range(TEXTURE_SIZE) for col in range(TEXTURE_SIZE))/(TEXTURE_SIZE**2)
        evidence[state]={'file':'Textures/'+path.name,'size':[TEXTURE_SIZE,TEXTURE_SIZE],
                         'color_space':'Non-Color / linear data; native sRGB false',
                         'channels':'RGB equal mask; alpha opaque','sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                         'saved_png_value_min_max':[min(values),max(values)],
                         'saved_png_distinct_values':sorted(set(round(v,6) for v in values)),
                         'saved_png_coverage_mean':sum(values)/len(values),
                         'saved_png_horizontal_mirror_difference_mean':reflected,
                         'packed_source_matches_disk_within_8bit_step':True}
        if state=='Defused':
            def sample(x,y):
                col=int((x/1.6+.5)*TEXTURE_SIZE)
                row=int((y+.5)*TEXTURE_SIZE)
                return values[row*TEXTURE_SIZE+col]
            anchors={'left_lower_stroke':sample(-.20,-.04),'right_upper_stroke':sample(.19,.13),
                     'left_upper_clear':sample(-.19,.13),'right_lower_clear':sample(.20,-.04)}
            require(anchors['left_lower_stroke']>.9 and anchors['right_upper_stroke']>.9
                    and anchors['left_upper_clear']<.1 and anchors['right_lower_clear']<.1,'Checkmark orientation evidence')
            require(reflected>.02,'Defused check unexpectedly mirror-symmetric')
            evidence[state]['asymmetric_check_orientation_samples']=anchors
            evidence[state]['orientation']='Short stroke at source -X/lower V; long rising stroke toward source +X/+Y'
        bpy.data.images.remove(loaded)
    return evidence


def main():
    require(bpy.app.version[:3]==(5,2,2),'Pinned Blender 5.2.2 required')
    if '--verify-only' in sys.argv:
        bpy.ops.wm.open_mainfile(filepath=str(BLEND))
        check_saved_source()
        mask_evidence()
        report=json.loads((ROOT/'mesh_validation.json').read_text())
        require(report['appearance_sha256']==hashlib.sha256(APPEARANCE_PATH.read_bytes()).hexdigest(),'Appearance/report differs')
        print('VALSTRIKE_D01_VERIFY_SUCCESS: saved geometry, materials, packed masks and disk pixels')
        return
    if '--render-only' in sys.argv:
        bpy.ops.wm.open_mainfile(filepath=str(BLEND))
        check_saved_source()
        render_previews()
        return
    for path in (EXPORTS,TEXTURES,PREVIEWS):
        path.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.preferences.filepaths.save_version=0
    scene=bpy.context.scene
    scene.unit_settings.system='METRIC'
    scene.unit_settings.scale_length=.01
    scene.unit_settings.length_unit='CENTIMETERS'
    make_masks()
    make_materials()
    obj=canonical(make_parts())
    record=validate(obj)
    export(obj)
    roundtrip(record)
    presentation(obj)
    select_state('Idle')
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    check_saved_source()
    report={'name':'D01 Relay','task':'ART-04','date_europe_berlin':'2026-11-03','tool':'Blender '+bpy.app.version_string,
            'provenance':'Original project-authored mesh and mathematical glyph masks; no external assets/fonts/textures',
            'units':'centimeters','forward':'+X','up':'+Z','pivot':'bottom center (0,0,0)',
            'assets':[record],'sockets':{'Carry':{'location_cm':[0,0,8],'rotation_degrees':[0,0,0],'scale':[1,1,1]}},
            'appearance':APPEARANCE,'appearance_sha256':hashlib.sha256(APPEARANCE_PATH.read_bytes()).hexdigest(),
            'status_surface':{'plane_z_cm':SCREEN_Z,'x_min_max_cm':[-4,4],'y_min_max_cm':[-2.5,2.5],
                              'uv0_formula':'U=(X+4)/8; V=(Y+2.5)/5; source +Z normal',
                              'uv0_corners':[[[-4,-2.5,SCREEN_Z],[0,0]],[[4,-2.5,SCREEN_Z],[1,0]],
                                             [[4,2.5,SCREEN_Z],[1,1]],[[-4,2.5,SCREEN_Z],[0,1]]]},
            'textures':mask_evidence(),
            'fbx_export':{'axis_forward':'-Y','axis_up':'Z','global_scale':1,'scene_scale_length':.01,
                          'apply_scale_options':'FBX_SCALE_UNITS','apply_unit_scale':True,'embedded_textures':False,
                          'status_shader':'Native importer must rebuild status mask/color shader; FBX does not encode arbitrary Blender node graphs'},
            'saved_blend_reopen':{'passed':True,'geometry_material_values_masks_packing_relative_paths':True},
            'limits':['Static visual mesh only; no gameplay, collision, mechanisms, animation or VFX behavior',
                      'Three status glyphs use the same surface and slot, with runtime selection owned elsewhere',
                      'Two UV channels; non-status UVs are isolated face tiles, no light-bake/texture-density acceptance',
                      'Closed shell pieces intentionally overlap internally; only status surface is open (2 triangles)',
                      'Source previews are actual Blender meshes, not native Unreal or gameplay verification']}
    (ROOT/'mesh_validation.json').write_text(json.dumps(report,indent=2)+'\n')
    if '--skip-render' not in sys.argv:
        render_previews()
    print('VALSTRIKE_D01_SOURCE_SUCCESS')


if __name__=='__main__':
    main()
