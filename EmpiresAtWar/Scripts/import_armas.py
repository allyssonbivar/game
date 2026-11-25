"""Importa as armas (malhas no esqueleto do soldado) para /Game/Armas.
Cada arma e uma SkeletalMesh presa aos ossos da arma (sabre: weapon_r; mosquete: arma, arma_cao, vareta)
e segue o soldado como "master pose" no jogo - troca de arma = trocar a malha, sem mexer no corpo.
Rodar: UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
"""
import unreal

PASTA_FBX = r"C:\Users\allys\Desktop\Projeto\game\Modelos\Export"
DEST = "/Game/Armas"
ARMAS = ["SK_Arma_Sabre", "SK_Arma_Mosquete", "SK_Arma_Faca", "SK_Arma_Machado"]
lib = unreal.EditorAssetLibrary

if lib.does_directory_exist(DEST):
    lib.delete_directory(DEST)
skel = lib.load_asset("/Game/Personagem/SK_Soldado_Skeleton")

tasks = []
for nome in ARMAS:
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = True
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    ui.import_animations = False
    ui.import_materials = True
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

for p in lib.list_assets(DEST, recursive=False):
    a = lib.load_asset(p)
    info = type(a).__name__
    if isinstance(a, unreal.SkeletalMesh):
        info += " extent=%s" % a.get_bounds().box_extent
    unreal.log("ARMA: %s | %s" % (p, info))
lib.save_directory(DEST)
