"""Importa as fardas (Modelos/fardas -> Modelos/Export) para /Game/Personagem/Fardas (editor FECHADO):
- texturas T_Farda_<NAC>_C / _N / _ORM (EmpiresAtWar/Texturas/Fardas), material M_Farda e instancias MI_Farda_FR/GB;
- SK_Soldado_FR / SK_Soldado_GB no esqueleto do soldado, com LOD1..3 (FBX proprios) e a fisica do SK_Soldado;
- pecas do desmembramento por nacao em /Game/Personagem/Fardas/Pecas.
Rodar: UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
"""
import os
import unreal

FBX = r"C:\Users\allys\Desktop\Projeto\game\Modelos\Export"
TEX = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Texturas", "Fardas"))
DEST = "/Game/Personagem/Fardas"
PECAS_DEST = DEST + "/Pecas"
NACOES = ("FR", "GB")
LOD_TELA = (1.0, 0.42, 0.2, 0.08)      # tamanho na tela em que cada LOD entra
lib = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

if lib.does_directory_exist(DEST):
    lib.delete_directory(DEST)
skel = lib.load_asset("/Game/Personagem/SK_Soldado_Skeleton")
fisica = lib.load_asset("/Game/Personagem/SK_Soldado_PhysicsAsset")
unreal.log("FARDAS: esqueleto=%s fisica=%s" % (skel, fisica))


def importar_textura(nome, tipo):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(TEX, nome + ".png")
    t.destination_path = DEST
    t.destination_name = nome
    t.automated = True
    t.replace_existing = True
    t.save = True
    tools.import_asset_tasks([t])
    tex = lib.load_asset(DEST + "/" + nome)
    if tipo == "N":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("flip_green_channel", False)   # ja sai no padrao DirectX
    elif tipo == "ORM":
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
    lib.save_loaded_asset(tex)
    return tex


def expr(m, cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


TEXS = {}
for _n in NACOES:
    for _t in ("C", "N", "ORM"):
        TEXS[(_n, _t)] = importar_textura("T_Farda_%s_%s" % (_n, _t), _t)


def material_mestre():
    m = tools.create_asset("M_Farda", DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_skeletal_mesh", True)
    cor = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, -200)
    cor.set_editor_property("parameter_name", "Cor")
    cor.set_editor_property("texture", TEXS[("FR", "C")])
    nrm = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, 100)
    nrm.set_editor_property("parameter_name", "Normal")
    nrm.set_editor_property("texture", TEXS[("FR", "N")])
    nrm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, 400)
    orm.set_editor_property("parameter_name", "ORM")
    orm.set_editor_property("texture", TEXS[("FR", "ORM")])
    orm.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_property(cor, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    # especular um pouco abaixo do padrao (la e couro nao brilham como plastico)
    esp = expr(m, unreal.MaterialExpressionConstant, -300, 650)
    esp.set_editor_property("r", 0.4)
    mel.connect_material_property(esp, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(m)
    lib.save_loaded_asset(m)
    return m


def instancia(mestre, nac):
    mi = tools.create_asset("MI_Farda_" + nac, DEST, unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mestre)
    for par, tipo in (("Cor", "C"), ("Normal", "N"), ("ORM", "ORM")):
        mel.set_material_instance_texture_parameter_value(mi, par, TEXS[(nac, tipo)])
    lib.save_loaded_asset(mi)
    return mi


def importar_malha(arquivo, destino, nome):
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.import_animations = False
    ui.import_materials = False
    ui.import_textures = False
    ui.create_physics_asset = False
    ui.skeleton = skel
    ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    ui.skeletal_mesh_import_data.set_editor_property("import_uniform_scale", 100.0)
    ui.skeletal_mesh_import_data.set_editor_property("normal_import_method",
                                                     unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    t = unreal.AssetImportTask()
    t.filename = arquivo
    t.destination_path = destino
    t.destination_name = nome
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = ui
    tools.import_asset_tasks([t])
    return lib.load_asset(destino + "/" + nome)


def por_material(mesh, mi):
    mats = mesh.get_editor_property("materials")
    novos = []
    for sm in mats:
        sm.set_editor_property("material_interface", mi)
        novos.append(sm)
    mesh.set_editor_property("materials", novos)


mestre = material_mestre()
for nac in NACOES:
    mi = instancia(mestre, nac)
    nome = "SK_Soldado_" + nac
    mesh = importar_malha(os.path.join(FBX, nome + ".fbx"), DEST, nome)
    if not mesh:
        unreal.log_error("FARDAS: falhou " + nome)
        continue
    for k in (1, 2, 3):
        arq = os.path.join(FBX, "%s_LOD%d.fbx" % (nome, k))
        r = unreal.EditorSkeletalMeshLibrary.import_lod(mesh, k, arq)
        unreal.log("FARDAS: %s LOD%d -> %s" % (nome, k, r))
    por_material(mesh, mi)
    infos = mesh.get_editor_property("lod_info")
    for k, info in enumerate(infos):
        if k < len(LOD_TELA):
            pf = info.get_editor_property("screen_size")
            pf.set_editor_property("default", LOD_TELA[k])
            info.set_editor_property("screen_size", pf)
    mesh.set_editor_property("lod_info", infos)
    if fisica:
        mesh.set_editor_property("physics_asset", fisica)
    lib.save_loaded_asset(mesh)
    b = mesh.get_bounds()
    unreal.log("FARDAS: %s lods=%d extent=%s verts0=%d" % (
        nome, unreal.EditorSkeletalMeshLibrary.get_lod_count(mesh), b.box_extent,
        unreal.EditorSkeletalMeshLibrary.get_num_verts(mesh, 0)))
    for parte in ("Cabeca", "BracoE", "BracoD", "PernaE", "PernaD"):
        pn = "SK_Peca_%s_%s" % (nac, parte)
        pm = importar_malha(os.path.join(FBX, pn + ".fbx"), PECAS_DEST, pn)
        if pm:
            por_material(pm, mi)
            lib.save_loaded_asset(pm)
            unreal.log("FARDAS: peca " + pn)
lib.save_directory(DEST)
unreal.log("FARDAS: fim")
