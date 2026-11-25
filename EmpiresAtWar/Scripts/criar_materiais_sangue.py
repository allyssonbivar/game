"""Cria os materiais de sangue em /Game/Efeitos (gotas e manchas no chao)."""
import unreal

DEST = "/Game/Efeitos"
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def novo_material(nome):
    path = DEST + "/" + nome
    if lib.does_asset_exist(path):
        lib.delete_asset(path)
    return tools.create_asset(nome, DEST, unreal.Material, unreal.MaterialFactoryNew())


# Gotas: vermelho escuro, brilhante (liquido).
m = novo_material("M_Sangue")
m.set_editor_property("used_with_instanced_static_meshes", True)   # gotas sao instancias (bug #46)
cor = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 0)
cor.set_editor_property("constant", unreal.LinearColor(0.22, 0.005, 0.005, 1.0))
rug = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 150)
rug.set_editor_property("r", 0.18)
mel.connect_material_property(cor, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rug, "", unreal.MaterialProperty.MP_ROUGHNESS)
# Brilho proprio leve: gotas pequenas contra a luz pareciam pretas no teste (2026-11-25).
emi = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -300, 300)
emi.set_editor_property("constant", unreal.LinearColor(0.5, 0.01, 0.01, 1.0))
mel.connect_material_property(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m)

# Mancha no chao: decal translucido, borda suave (SphereMask nas coordenadas UV).
d = novo_material("M_Sangue_Mancha")
d.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
d.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
cor2 = mel.create_material_expression(d, unreal.MaterialExpressionConstant3Vector, -500, 0)
cor2.set_editor_property("constant", unreal.LinearColor(0.12, 0.0, 0.0, 1.0))
uv = mel.create_material_expression(d, unreal.MaterialExpressionTextureCoordinate, -700, 200)
centro = mel.create_material_expression(d, unreal.MaterialExpressionConstant2Vector, -700, 320)
centro.set_editor_property("r", 0.5)
centro.set_editor_property("g", 0.5)
mascara = mel.create_material_expression(d, unreal.MaterialExpressionSphereMask, -450, 220)
mascara.set_editor_property("attenuation_radius", 0.48)
mascara.set_editor_property("hardness_percent", 35.0)
mel.connect_material_expressions(uv, "", mascara, "A")
mel.connect_material_expressions(centro, "", mascara, "B")
rug2 = mel.create_material_expression(d, unreal.MaterialExpressionConstant, -500, 420)
rug2.set_editor_property("r", 0.25)
mel.connect_material_property(cor2, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(mascara, "", unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(rug2, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(d)

lib.save_directory(DEST)
for p in lib.list_assets(DEST, recursive=False):
    unreal.log("EFEITO: " + p)
