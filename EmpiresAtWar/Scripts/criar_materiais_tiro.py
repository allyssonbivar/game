"""Materiais do tiro em /Game/Efeitos: fumaca de polvora (opacidade por bolha), clarao do disparo e ferida de bala."""
import unreal

DEST = "/Game/Efeitos"
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def novo(nome):
    if lib.does_asset_exist(DEST + "/" + nome):
        lib.delete_asset(DEST + "/" + nome)
    return tools.create_asset(nome, DEST, unreal.Material, unreal.MaterialFactoryNew())


def const3(m, rgb, x, y):
    e = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, x, y)
    e.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return e


# Fumaca: translucida, cinza-claro; opacidade = dado da instancia x borda suave (1 - fresnel).
m = novo("M_Fumaca")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("used_with_instanced_static_meshes", True)   # sem isso o jogo usa o material padrao (quadriculado)
m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
cor = const3(m, (0.62, 0.61, 0.58), -400, 0)
dado = mel.create_material_expression(m, unreal.MaterialExpressionPerInstanceCustomData, -700, 200)
dado.set_editor_property("data_index", 0)
fres = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -700, 350)
fres.set_editor_property("exponent", 1.6)
inv = mel.create_material_expression(m, unreal.MaterialExpressionOneMinus, -500, 350)
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 250)
mel.connect_material_expressions(fres, "", inv, "")
vi = mel.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -500, 200)   # na 4.27 o dado da instancia so existe no vertex shader
mel.connect_material_expressions(dado, "", vi, "")
mel.connect_material_expressions(vi, "", mul, "A")
mel.connect_material_expressions(inv, "", mul, "B")
mel.connect_material_property(cor, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(m)

# Clarao: aditivo, sem luz, laranja muito forte (dura 0,06 s).
c = novo("M_Clarao")
c.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
c.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mel.connect_material_property(const3(c, (40.0, 18.0, 4.0), -300, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(c)

# Ferida de bala: vermelho quase preto, umido.
f = novo("M_Ferida")
mel.connect_material_property(const3(f, (0.045, 0.002, 0.002), -300, 0), "", unreal.MaterialProperty.MP_BASE_COLOR)
r = mel.create_material_expression(f, unreal.MaterialExpressionConstant, -300, 150)
r.set_editor_property("r", 0.3)
mel.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(f)

lib.save_directory(DEST)
for p in lib.list_assets(DEST, recursive=False):
    unreal.log("EFEITO: " + p)
