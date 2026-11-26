"""Importa as texturas de efeitos (EmpiresAtWar/Texturas, geradas por gerar_texturas_efeitos.py) e cria os
materiais de sprite em /Game/Efeitos:
  M_Fumaca_Sprite  fumaca de polvora (translucida, com luz; dados da instancia: opacidade, quadro do atlas, brilho)
  M_Sangue_Sprite  filetes e gotas de sangue (mascarado, liquido brilhante; dados: quadro do atlas, desaparecer)
  M_Sangue_Nevoa   nevoa vermelha no impacto (translucida; mesmos dados da fumaca)
  M_Sangue_Mancha  mancha no chao (decal; parametro Variante = qual dos 4 respingos)
Rodar (editor FECHADO): UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
(com a variavel de ambiente EAW_SO_TEXTURAS=1 so reimporta as texturas)
"""
import os
import unreal

DEST = "/Game/Efeitos"
TEX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Texturas")
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
E = unreal  # atalho


def importar_textura(nome):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(os.path.join(TEX, nome + ".png"))
    task.destination_path = DEST
    task.destination_name = nome
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    t = lib.load_asset(DEST + "/" + nome)
    t.set_editor_property("srgb", False)   # R = transparencia: sem curva de gama
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    lib.save_loaded_asset(t)
    return t


def novo(nome):
    if lib.does_asset_exist(DEST + "/" + nome):
        lib.delete_asset(DEST + "/" + nome)
    return tools.create_asset(nome, DEST, unreal.Material, unreal.MaterialFactoryNew())


def expr(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def const(m, v, x, y):
    return expr(m, E.MaterialExpressionConstant, x, y, r=v)


def const3(m, rgb, x, y):
    return expr(m, E.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def ligar(a, b, entrada="", saida=""):
    mel.connect_material_expressions(a, saida, b, entrada)


def op(m, cls, a, b, x, y):
    e = expr(m, cls, x, y)
    ligar(a, e, "A")
    if b is not None:
        ligar(b, e, "B")
    return e


def dados_instancia(m, n, x, y):
    """Le n dados da instancia no vertex shader e passa para o pixel (na 4.27 so existem no vertex)."""
    d = [expr(m, E.MaterialExpressionPerInstanceCustomData, x, y + 120 * i, data_index=i) for i in range(n)]
    v = d[0]
    for i in range(1, n):
        v = op(m, E.MaterialExpressionAppendVector, v, d[i], x + 200, y + 60 * i)
    vi = expr(m, E.MaterialExpressionVertexInterpolator, x + 420, y)
    ligar(v, vi)
    canais = []
    for i in range(n):
        mask = expr(m, E.MaterialExpressionComponentMask, x + 620, y + 100 * i, r=(i == 0), g=(i == 1), b=(i == 2), a=(i == 3))
        ligar(vi, mask)
        canais.append(mask)
    return canais


def uv_atlas(m, quadro, x, y):
    """UV do quadro (0..3) num atlas 2x2: coluna = quadro mod 2, linha = floor(quadro / 2)."""
    uv = expr(m, E.MaterialExpressionTextureCoordinate, x, y)
    meio = op(m, E.MaterialExpressionMultiply, uv, const(m, 0.5, x, y + 80), x + 180, y)
    col = op(m, E.MaterialExpressionFmod, quadro, const(m, 2.0, x, y + 160), x + 180, y + 140)
    lin = expr(m, E.MaterialExpressionFloor, x + 360, y + 220)
    ligar(op(m, E.MaterialExpressionMultiply, quadro, const(m, 0.5, x, y + 240), x + 180, y + 240), lin)
    off = op(m, E.MaterialExpressionAppendVector, col, lin, x + 520, y + 160)
    off = op(m, E.MaterialExpressionMultiply, off, const(m, 0.5, x + 520, y + 260), x + 680, y + 160)
    return op(m, E.MaterialExpressionAdd, meio, off, x + 840, y + 60)


def canal(m, s, c, x, y):
    """Um canal da saida padrao (RGB) da textura: 'r' = transparencia, 'g' = sombreado (ver gerar_texturas_efeitos.py)."""
    k = expr(m, E.MaterialExpressionComponentMask, x, y, r=(c == "r"), g=(c == "g"), b=False, a=False)
    ligar(s, k)
    return k


def amostra(m, tex, uv, x, y):
    s = expr(m, E.MaterialExpressionTextureSample, x, y, texture=tex)
    if uv is not None:
        ligar(uv, s, "UVs")
    return s


def sprite_translucido(nome, tex, cor, emissivo, opacidade_max):
    """Fumaca/nevoa: translucido com luz volumetrica, borda suave no chao (DepthFade)."""
    m = novo(nome)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    opac, quadro, brilho = dados_instancia(m, 3, -1800, 0)
    s = amostra(m, tex, uv_atlas(m, quadro, -1200, 300), -250, 300)
    corb = op(m, E.MaterialExpressionMultiply, canal(m, s, "g", -150, 120), const3(m, cor, -250, 0), -50, 0)
    corb = op(m, E.MaterialExpressionMultiply, corb, brilho, 120, 0)
    mel.connect_material_property(corb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emi = op(m, E.MaterialExpressionMultiply, corb, const(m, emissivo, 120, 120), 300, 120)
    mel.connect_material_property(emi, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    a = op(m, E.MaterialExpressionMultiply, canal(m, s, "r", -150, 520), opac, 0, 520)   # R da textura = densidade
    a = op(m, E.MaterialExpressionMultiply, a, const(m, opacidade_max, 0, 620), 180, 520)
    fade = expr(m, E.MaterialExpressionDepthFade, 360, 600, fade_distance_default=60.0)
    ligar(a, fade, "Opacity")
    mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    return m


t_fumaca = importar_textura("T_Fumaca")
t_jato = importar_textura("T_Sangue_Jato")
t_mancha = importar_textura("T_Sangue_Mancha")
if os.environ.get("EAW_SO_TEXTURAS"):
    raise SystemExit   # so reimporta as texturas (os materiais e o cache de shaders ficam)

# Fumaca de polvora negra: branco-acinzentado quente (como no Holdfast), um pouco de luz propria para nao ficar preta na sombra.
sprite_translucido("M_Fumaca_Sprite", t_fumaca, (0.92, 0.91, 0.88), 0.32, 1.0)
# Nevoa de sangue no impacto: vermelho escuro.
sprite_translucido("M_Sangue_Nevoa", t_fumaca, (0.22, 0.012, 0.01), 0.25, 0.9)

# Filetes e gotas: mascarado (borda nitida), liquido brilhante.
m = novo("M_Sangue_Sprite")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("two_sided", True)
m.set_editor_property("used_with_instanced_static_meshes", True)
m.set_editor_property("opacity_mask_clip_value", 0.4)
quadro, some = dados_instancia(m, 2, -1800, 0)
s = amostra(m, t_jato, uv_atlas(m, quadro, -1200, 300), -250, 300)
cor = op(m, E.MaterialExpressionMultiply, canal(m, s, "g", -150, 120), const3(m, (0.13, 0.003, 0.003), -250, 0), -50, 0)
mel.connect_material_property(cor, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(op(m, E.MaterialExpressionMultiply, cor, const(m, 0.25, -50, 120), 150, 120), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(const(m, 0.22, 150, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(m, 0.4, 150, 300), "", unreal.MaterialProperty.MP_SPECULAR)
mascara = op(m, E.MaterialExpressionMultiply, canal(m, s, "r", -150, 520), some, 0, 520)
mel.connect_material_property(mascara, "", unreal.MaterialProperty.MP_OPACITY_MASK)
mel.recompile_material(m)

# Mancha no chao: decal translucido com 4 formatos de respingo; parametro Variante escolhe o quadro.
d = novo("M_Sangue_Mancha")
d.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
d.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
var = expr(d, E.MaterialExpressionScalarParameter, -1500, 300, parameter_name="Variante", default_value=0.0)
s = amostra(d, t_mancha, uv_atlas(d, var, -1200, 300), -250, 300)
cor = op(d, E.MaterialExpressionMultiply, canal(d, s, "g", -150, 120), const3(d, (0.07, 0.002, 0.002), -250, 0), -50, 0)
mel.connect_material_property(cor, "", unreal.MaterialProperty.MP_BASE_COLOR)
# Fosco o bastante para nao refletir o ceu claro (ficava rosado no teste de 2026-11-25).
mel.connect_material_property(const(d, 0.45, 150, 220), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(d, 0.3, 150, 300), "", unreal.MaterialProperty.MP_SPECULAR)
alfa = op(d, E.MaterialExpressionMultiply, canal(d, s, "r", -150, 520), const(d, 0.95, -150, 620), 0, 520)
mel.connect_material_property(alfa, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(d)

lib.save_directory(DEST)
for p in lib.list_assets(DEST, recursive=False):
    unreal.log("EFEITO: " + p)
