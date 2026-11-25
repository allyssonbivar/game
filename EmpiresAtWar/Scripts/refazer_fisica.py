"""Refaz o PhysicsAsset do soldado (um corpo por osso principal + limites das juntas).
Rodar com o editor FECHADO: UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
Rodar de novo sempre que reimportar o SK_Soldado (a importacao recria um PhysicsAsset ruim, so 4 corpos).
"""
import unreal

lib = unreal.EditorAssetLibrary
malha = lib.load_asset("/Game/Personagem/SK_Soldado")
unreal.log("FISICA: " + unreal.EAWEditorTools.rebuild_physics_asset(malha, 6.0))
lib.save_asset("/Game/Personagem/SK_Soldado_PhysicsAsset", only_if_is_dirty=False)
lib.save_asset("/Game/Personagem/SK_Soldado", only_if_is_dirty=False)
