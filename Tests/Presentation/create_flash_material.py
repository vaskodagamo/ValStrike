"""Author the original unlit muzzle-flash material with Unreal, never raw bytes.

Run in a fresh commandlet under the shared engine slot. Existing assets are
verified and left untouched unless -VSRifleFlashRecompile is explicitly supplied.
"""
import json
from pathlib import Path
import unreal

PATH = "/Game/Presentation/Rifle01/M_VSRifleFlash"


def main():
    material = unreal.load_asset(PATH)
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_VSRifleFlash", "/Game/Presentation/Rifle01", unreal.Material, unreal.MaterialFactoryNew())
        assert material, "material creation failed"
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
        node.set_editor_property("constant", unreal.LinearColor(8.0, 2.5, 0.25, 1))
        assert unreal.MaterialEditingLibrary.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        assert not unreal.MaterialEditingLibrary.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    assert material.get_editor_property("shading_model") == unreal.MaterialShadingModel.MSM_UNLIT
    reopened = unreal.load_asset(PATH)
    assert reopened and reopened.get_path_name() == PATH + ".M_VSRifleFlash"
    emissive = unreal.MaterialEditingLibrary.get_material_property_input_node(reopened, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert isinstance(emissive, unreal.MaterialExpressionConstant3Vector), "unexpected emissive expression"
    color = emissive.get_editor_property("constant")
    actual = [color.r, color.g, color.b]
    assert all(abs(a - b) < 0.0001 for a, b in zip(actual, [8, 2.5, .25])), actual
    if "-VSRifleFlashRecompile" in unreal.SystemLibrary.get_command_line().split():
        assert not unreal.MaterialEditingLibrary.recompile_material(material)
        assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    front = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_FRONT_MATERIAL)
    diagnostics = {"substrate": unreal.SystemLibrary.get_console_variable_int_value("r.Substrate"),
                   "front_material": front.get_class().get_name() if front else None,
                   "material_domain": str(material.get_editor_property("material_domain")),
                   "used_with_static_mesh": material.get_editor_property("used_with_static_mesh")}
    output = Path(unreal.Paths.project_saved_dir()) / "Tests/Presentation/material.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"asset": PATH, "shading_model": "unlit", "original_emissive_linear": actual, "saved": True, **diagnostics}, indent=2))
    unreal.log("VS_RIFLE_FLASH_MATERIAL_READY")


main()
