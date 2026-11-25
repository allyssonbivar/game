"""Importa as pecas do desmembramento (cabeca, bracos, pernas) para /Game/Personagem/Pecas.
Usam o mesmo esqueleto do SK_Soldado; a fisica vem do PhysicsAsset do soldado (no codigo).
Rodar: UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
"""
import unreal

PASTA_FBX = r"C:\Users\allys\Desktop\Projeto\game\Modelos\Export"
DEST = "/Game/Personagem/Pecas"
PECAS = ["SK_Peca_Cabeca", "SK_Peca_BracoE", "SK_Peca_BracoD", "SK_Peca_PernaE", "SK_Peca_PernaD"]
lib = unreal.EditorAssetLibrary

if lib.does_directory_exist(DEST):
    lib.delete_directory(DEST)

skel = lib.load_asset("/Game/Personagem/SK_Soldado_Skeleton")
unreal.log("ESQUELETO: %s" % skel)

tasks = []
for nome in PECAS:
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
    t = unreal.AssetImportTask()
    t.filename = PASTA_FBX + "\\" + nome + ".fbx"
    t.destination_path = DEST
    t.destination_name = nome
    t.automated = True
    t.replace_existing = True
    t.save = True
    t.options = ui
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# Mesmo material do soldado (as pecas vieram sem material).
corpo = lib.load_asset("/Game/Personagem/SK_Soldado")
mats = corpo.get_editor_property("materials") if corpo else []
for p in lib.list_assets(DEST, recursive=False):
    a = lib.load_asset(p)
    if isinstance(a, unreal.SkeletalMesh):
        if mats:
            por_nome = {str(m.get_editor_property("material_slot_name")): m.get_editor_property("material_interface") for m in mats}
            novos = []
            for m in a.get_editor_property("materials"):
                nome_slot = str(m.get_editor_property("material_slot_name"))
                mi = por_nome.get(nome_slot, mats[0].get_editor_property("material_interface"))
                m.set_editor_property("material_interface", mi)
                unreal.log("  slot %s -> %s" % (nome_slot, mi.get_name() if mi else None))
                novos.append(m)
            a.set_editor_property("materials", novos)
        b = a.get_bounds()
        unreal.log("PECA: %s skel=%s extent=%s" % (p, a.get_editor_property("skeleton").get_name(), b.box_extent))
    else:
        unreal.log("OUTRO: %s %s" % (p, type(a).__name__))
lib.save_directory(DEST)
