"""Reimporta tudo do Blender na ordem certa (editor FECHADO):
1) soldado + animacoes  2) armas  3) pecas do desmembramento  4) fisica do esqueleto (obrigatorio, bug #41).
Rodar: UE4Editor-Cmd.exe <uproject> -run=pythonscript -script="<este arquivo>"
"""
import os
import unreal

PASTA = os.path.dirname(os.path.abspath(__file__))
for nome in ("import_soldado.py", "import_armas.py", "import_pecas.py", "refazer_fisica.py"):
    unreal.log("=== REIMPORTAR: " + nome)
    with open(os.path.join(PASTA, nome), encoding="utf-8") as f:
        exec(compile(f.read(), nome, "exec"), {"__name__": "__main__", "__file__": os.path.join(PASTA, nome)})
unreal.log("=== REIMPORTAR: fim")
