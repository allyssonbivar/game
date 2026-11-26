---
tags: [caminhos]
atualizado: 2026-11-26
---
# Onde está cada coisa

| O quê | Caminho |
|---|---|
| Este cofre (Obsidian) | `Desktop\Projeto\game\gamewarlog\gamewar\` |
| Projeto Unreal (C++) | `Desktop\Projeto\game\EmpiresAtWar\` |
| Personagem e animações | `Desktop\Projeto\game\Modelos\Soldado.blend` |
| FBX exportado para o Unreal | `Desktop\Projeto\game\Modelos\Export\SK_Soldado.fbx` |
| Script de importação no Unreal | `EmpiresAtWar\Scripts\import_soldado.py` |
| Física do esqueleto (rodar após reimportar) | `EmpiresAtWar\Scripts\refazer_fisica.py` → `Source\EmpiresAtWar\Tools\EAWEditorTools.cpp` |
| Peças do desmembramento | Blender → `Modelos\Export\SK_Peca_*.fbx` → `Scripts\import_pecas.py` → `/Game/Personagem/Pecas` |
| Materiais de sangue (antigos: gotas e ferida) | `Scripts\criar_materiais_sangue.py` → `/Game/Efeitos/M_Sangue` |
| Texturas de sangue e fumaça | `Scripts\gerar_texturas_efeitos.py` (Python com numpy/Pillow) → `Texturas\*.png` |
| Materiais de sprite (fumaça, sangue, névoa, mancha) | `Scripts\criar_materiais_efeitos.py` → `/Game/Efeitos/M_Fumaca_Sprite`, `M_Sangue_Sprite`, `M_Sangue_Nevoa`, `M_Sangue_Mancha` (editor fechado; `EAW_SO_TEXTURAS=1` só reimporta texturas) |
| Sprites (código) | `Source\EmpiresAtWar\Effects\EAWSprites.h`, `EAWBloodFX`, `EAWSmokeFX` |
| Morte por tiro no pescoço | Blender `AS_Morte_Pescoco` (de `Modelos\ex\Dying.fbx`, Mixamo, fora do Git) → `EAWCharacter::StartNeckDeath` |
| Gotas, manchas e jato (C++) | `Source\EmpiresAtWar\Effects\EAWBloodFX.h/.cpp` |
| Corte de membros | `EAWCharacter.cpp` → `NoteHit`, `TryDismember`, `AddCap`, `ClearGore` |
| Tela vermelha | `Source\EmpiresAtWar\EAWHUD.h/.cpp` |
| Personagem/animações no Unreal | `EmpiresAtWar\Content\Personagem\` |
| Capturas dos testes | `EmpiresAtWar\Saved\Testes\` |
| Backup antes da escala 1 | `Desktop\Projeto\game\Modelos\Soldado_backup_antes_escala.blend` |
| FBX do Mixamo (Y Bot) | `Desktop\Projeto\game\Modelos\Soldado_TPose.fbx` |
| Documentos de concepção | `Desktop\Projeto\game\*.pdf` |
| Referência Chivalry 2 (só estudo) | `Downloads\0b91b3ee...\Output\Exports\TBL\` |
| REA (engenharia reversa) | `Documents\testar\rea\` + Node portátil em `Documents\testar\.tools\` |
| Ghidra 12.2 | `Documents\ghidra_12.2_PUBLIC\` |
| Unreal Engine 4.27 | `C:\Program Files\Epic Games\UE_4.27\` |
| Baioneta do vídeo x1 e agachado (poses e montagem) | `Soldado.blend` → texto `eaw_baioneta` (`build_bay()`, `build_crouch()`; precisa de `eaw_rifle_anims` antes) |
| Agachado no animador | `Source\EmpiresAtWar\Animation\EAWAnimInstance.cpp` (camada agachada + arma presa ao quadril) |
| Referência baioneta/agachar/fumaça (só estudo, fora do Git) | `Modelos\ex\x1.mp4` |
| Ataques da fase 2 (golpe correndo, salto, chute) | `Soldado.blend` → texto `eaw_ataques` (`build_attacks()`, `build_attacks_fp()`) |
| Fardas (gerador das malhas, pesos, UV, LODs, FBX) | `Modelos\fardas\gerar_fardas.py` + `fardas_lib.py` → `blender -b Modelos\Soldado.blend --python Modelos\fardas\gerar_fardas.py -- exportar` |
| Texturas das fardas | `Modelos\fardas\gerar_texturas_fardas.py` (Python 3.11 com numpy/Pillow) → `EmpiresAtWar\Texturas\Fardas\T_Farda_<FR/GB>_C/N/ORM.png` |
| Prévia das fardas em poses / teste de colisão | `Modelos\fardas\preview_poses.py`, `teste_colisao.py` (rodam no `Modelos\fardas\dados\fardas_gerado.blend`, gerado, fora do Git) |
| FBX das fardas | `Modelos\Export\SK_Soldado_FR(_LOD1..3).fbx`, `SK_Soldado_GB(...)`, `SK_Peca_FR_*`, `SK_Peca_GB_*` |
| Importar fardas no Unreal | `EmpiresAtWar\Scripts\import_fardas.py` (editor fechado) → `/Game/Personagem/Fardas` |
| Nação no código | `EAWCharacter` (`Nacao`, `AplicarNacao`) e `EAWGameMode` (`NacaoDoJogador`, `NacaoDoBot`, `SpawnSoldier`) |
| Referência de fardas (Holdfast extraído, só estudo, fora do Git) | `Modelos\ex\Textures`, `Modelos\ex\Meshes` |
