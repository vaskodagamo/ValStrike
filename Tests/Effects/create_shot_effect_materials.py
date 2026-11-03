"""Author the original shot-effect materials with Unreal's editor API, never raw bytes.

Run in a fresh commandlet under the shared engine slot (see docs/workstreams/shot-effects.md):
  UnrealEditor-Cmd ValStrike.uproject -run=pythonscript -script=<this file> -unattended -nosound
Existing assets are verified and left untouched unless -VSShotEffectsRecompile is supplied,
which recompiles and resaves only these two owned materials. Add -AllowCommandletRendering
for a Metal shader compile on this Mac.

Both materials are plain numeric shader graphs: no textures, no imported content.
- M_VSShotTracer: unlit additive. Custom primitive data 0 = intensity, 1 = width scale.
  World position offset widens a hairline cylinder in proportion to view distance, so
  the tracer keeps a thin, roughly constant on-screen width near and far.
- M_VSImpactMark: unlit translucent disc on a plane. Custom primitive data 0 = fade,
  1 = heat. Dark core with a pale rim reads on light and dark surfaces and in dim
  light; heat tints it hot orange-white for the first moment (and for player hits).
"""
import json
from pathlib import Path

import unreal

FOLDER = "/Game/Effects"
TRACER = FOLDER + "/M_VSShotTracer"
MARK = FOLDER + "/M_VSImpactMark"
LIB = unreal.MaterialEditingLibrary

TRACER_COLOR = (4.0, 3.0, 1.4)       # Warm white; bloom adds a soft glow.
TRACER_RADIUS_PER_CM = 0.0025        # Radius per cm of view distance: ~2.5 px wide at 1000 px / 90 deg.
TRACER_MIN_RADIUS = 0.12             # cm
TRACER_MAX_RADIUS = 8.0              # cm
MARK_RIM = (0.62, 0.64, 0.68)
MARK_CORE = (0.015, 0.016, 0.02)
MARK_HOT = (5.0, 2.6, 0.9)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def node(material, cls, x, y, **properties):
    expression = LIB.create_material_expression(material, cls, x, y)
    require(expression, f"could not create {cls.__name__}")
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def link(source, target, pin, output=""):
    """An empty pin name selects the first input (single-input nodes such as Clamp/OneMinus)."""
    require(LIB.connect_material_expressions(source, output, target, pin),
            f"could not connect {source.get_name()} -> {target.get_name()}.{pin}")


def output(source, prop):
    require(LIB.connect_material_property(source, "", prop), f"could not connect {prop}")


def primitive_data(material, name, index, x, y):
    return node(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name,
                use_custom_primitive_data=True, primitive_data_index=index, default_value=1.0)


def constant3(material, rgb, x, y):
    return node(material, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(*rgb, 1.0))


def new_material(path, blend):
    folder, name = path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material,
                                                                       unreal.MaterialFactoryNew())
    require(material, f"material creation failed: {path}")
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("blend_mode", blend)
    material.set_editor_property("used_with_static_lighting", False)
    return material


def build_tracer():
    material = new_material(TRACER, unreal.BlendMode.BLEND_ADDITIVE)
    intensity = primitive_data(material, "Intensity", 0, -700, -200)
    width = primitive_data(material, "Width", 1, -700, 300)
    color = constant3(material, TRACER_COLOR, -700, -400)
    emissive = node(material, unreal.MaterialExpressionMultiply, -350, -300)
    link(color, emissive, "A")
    link(intensity, emissive, "B")
    output(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    camera = node(material, unreal.MaterialExpressionCameraPositionWS, -1300, 100)
    position = node(material, unreal.MaterialExpressionWorldPosition, -1300, 200,
                    world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    distance = node(material, unreal.MaterialExpressionDistance, -1050, 150)
    link(camera, distance, "A")
    link(position, distance, "B")
    scaled = node(material, unreal.MaterialExpressionMultiply, -850, 150, const_b=TRACER_RADIUS_PER_CM)
    link(distance, scaled, "A")
    radius = node(material, unreal.MaterialExpressionClamp, -650, 150, min_default=TRACER_MIN_RADIUS,
                  max_default=TRACER_MAX_RADIUS)
    link(scaled, radius, "")  # first input
    sized = node(material, unreal.MaterialExpressionMultiply, -450, 200)
    link(radius, sized, "A")
    link(width, sized, "B")
    normal = node(material, unreal.MaterialExpressionVertexNormalWS, -450, 50)
    offset = node(material, unreal.MaterialExpressionMultiply, -250, 100)
    link(normal, offset, "A")
    link(sized, offset, "B")
    output(offset, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return material


def build_mark():
    material = new_material(MARK, unreal.BlendMode.BLEND_TRANSLUCENT)
    fade = primitive_data(material, "Fade", 0, -400, 450)
    heat = primitive_data(material, "Heat", 1, -700, 250)
    uv = node(material, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
    center = node(material, unreal.MaterialExpressionConstant2Vector, -1500, 120, r=0.5, g=0.5)
    distance = node(material, unreal.MaterialExpressionDistance, -1300, 50)
    link(uv, distance, "A")
    link(center, distance, "B")
    radius = node(material, unreal.MaterialExpressionMultiply, -1120, 50, const_b=2.0)  # 0 centre .. 1 edge
    link(distance, radius, "A")

    core_edge = node(material, unreal.MaterialExpressionSmoothStep, -950, -100, const_min=0.34, const_max=0.46)
    link(radius, core_edge, "Value")
    core = node(material, unreal.MaterialExpressionOneMinus, -780, -100)
    link(core_edge, core, "")
    disc_edge = node(material, unreal.MaterialExpressionSmoothStep, -950, 150, const_min=0.78, const_max=0.98)
    link(radius, disc_edge, "Value")
    disc = node(material, unreal.MaterialExpressionOneMinus, -780, 150)
    link(disc_edge, disc, "")

    rim_color = constant3(material, MARK_RIM, -780, -350)
    core_color = constant3(material, MARK_CORE, -780, -250)
    base = node(material, unreal.MaterialExpressionLinearInterpolate, -550, -300)
    link(rim_color, base, "A")
    link(core_color, base, "B")
    link(core, base, "Alpha")
    hot = constant3(material, MARK_HOT, -550, -150)
    color = node(material, unreal.MaterialExpressionLinearInterpolate, -300, -250)
    link(base, color, "A")
    link(hot, color, "B")
    link(heat, color, "Alpha")
    output(color, unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    density = node(material, unreal.MaterialExpressionLinearInterpolate, -550, 50, const_a=0.6, const_b=0.95)
    link(core, density, "Alpha")
    hot_density = node(material, unreal.MaterialExpressionLinearInterpolate, -350, 100, const_b=1.0)
    link(density, hot_density, "A")
    link(heat, hot_density, "Alpha")
    shaped = node(material, unreal.MaterialExpressionMultiply, -200, 150)
    link(disc, shaped, "A")
    link(hot_density, shaped, "B")
    opacity = node(material, unreal.MaterialExpressionMultiply, -50, 250)
    link(shaped, opacity, "A")
    link(fade, opacity, "B")
    output(opacity, unreal.MaterialProperty.MP_OPACITY)
    return material


def compile_and_save(material):
    errors = LIB.recompile_material(material)
    require(not errors, f"{material.get_name()} failed to compile: {errors}")
    require(unreal.EditorAssetLibrary.save_loaded_asset(material, False), f"could not save {material.get_name()}")


def describe(path, blend):
    material = unreal.load_asset(path)
    require(material and material.get_path_name() == path + "." + path.rsplit("/", 1)[1], f"cannot reopen {path}")
    require(material.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_UNLIT, f"{path} not unlit")
    require(material.get_editor_property("blend_mode") == blend, f"{path} has the wrong blend mode")
    parameters = sorted(str(name) for name in LIB.get_scalar_parameter_names(material))
    used_textures = getattr(LIB, "get_material_used_textures", None) or LIB.get_used_textures
    textures = used_textures(material)
    require(not textures, f"{path} unexpectedly uses textures")
    return {"asset": path, "shading_model": "unlit", "blend_mode": str(blend), "scalar_parameters": parameters,
            "expressions": LIB.get_num_material_expressions(material), "textures": len(textures),
            "material_domain": str(material.get_editor_property("material_domain"))}


def mesh_bounds(path):
    mesh = unreal.load_asset(path)
    require(mesh, f"missing engine mesh {path}")
    bounds = mesh.get_bounds()
    return {"mesh": path, "origin": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
            "extent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]}


def main():
    created = []
    for path, builder in ((TRACER, build_tracer), (MARK, build_mark)):
        material = unreal.load_asset(path)
        if material is None:
            material = builder()
            compile_and_save(material)
            created.append(path)
        elif "-VSShotEffectsRecompile" in unreal.SystemLibrary.get_command_line().split():
            compile_and_save(material)
    report = {
        "created": created,
        "materials": [describe(TRACER, unreal.BlendMode.BLEND_ADDITIVE), describe(MARK, unreal.BlendMode.BLEND_TRANSLUCENT)],
        "meshes": [mesh_bounds("/Engine/BasicShapes/Cylinder"), mesh_bounds("/Engine/BasicShapes/Plane")],
        "substrate": unreal.SystemLibrary.get_console_variable_int_value("r.Substrate"),
        "saved": True,
    }
    out = Path(unreal.Paths.project_saved_dir()) / "Tests/Effects/materials.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2) + "\n")
    unreal.log("VS_SHOT_EFFECT_MATERIALS_READY " + json.dumps(report))


try:
    main()
except Exception as error:
    unreal.log_error(f"VS_SHOT_EFFECT_MATERIALS_FAILED: {error}")
    raise
