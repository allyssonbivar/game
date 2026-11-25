"""Importa o personagem e as animacoes do sabre (FBX do Blender) para /Game/Personagem.
Rodar: UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
"""
import unreal

FBX = r"C:\Users\allys\Desktop\Projeto\game\Modelos\Export\SK_Soldado.fbx"
DEST = "/Game/Personagem"

if unreal.EditorAssetLibrary.does_directory_exist(DEST):
    unreal.EditorAssetLibrary.delete_directory(DEST)

ui = unreal.FbxImportUI()
ui.import_mesh = True
ui.import_as_skeletal = True
ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
ui.import_animations = True
ui.import_materials = True
ui.import_textures = True
ui.create_physics_asset = True
ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
ui.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose", False)
ui.anim_sequence_import_data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
ui.anim_sequence_import_data.set_editor_property("import_bone_tracks", True)
# Receita testada (2026-11-24): no Blender o esqueleto tem escala 1 e o FBX sai com "FBX_SCALE_ALL";
# malha e animacoes chegam juntas em metros -> x100 aqui deixa as duas em centimetros.
ui.skeletal_mesh_import_data.set_editor_property("import_uniform_scale", 100.0)
ui.anim_sequence_import_data.set_editor_property("import_uniform_scale", 100.0)

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.destination_name = "SK_Soldado"
task.automated = True
task.replace_existing = True
task.save = True
task.options = ui
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

lib = unreal.EditorAssetLibrary
for path in task.imported_object_paths:
    unreal.log("IMPORTADO: " + path)

# Remove animacoes que nao vao para o jogo: sobras do Mixamo e fontes editaveis "SRC_" do Blender
for path in lib.list_assets(DEST, recursive=False):
    if "mixamo" in path.lower() or "SRC_" in path:
        lib.delete_asset(path)

# Renomeia as animacoes para nomes curtos: AS_Sabre_*, AS_Mosquete_* ...
for path in lib.list_assets(DEST, recursive=False):
    name = path.split(".")[-1]
    if "AS_" in name and not name.startswith("AS_"):
        new = DEST + "/" + name[name.index("AS_"):]
        if lib.does_asset_exist(new):
            lib.delete_asset(new)
        lib.rename_asset(path, new)

for path in lib.list_assets(DEST, recursive=False):
    asset = lib.load_asset(path)
    info = type(asset).__name__
    if isinstance(asset, unreal.AnimSequence):
        info += " len=%.3f" % asset.get_editor_property("sequence_length")
    elif isinstance(asset, unreal.SkeletalMesh):
        b = asset.get_bounds()
        info += " bounds_extent=%s origin=%s" % (b.box_extent, b.origin)
    unreal.log("ASSET: %s | %s" % (path, info))
lib.save_directory(DEST)
