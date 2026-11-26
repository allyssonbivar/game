---
tags: [caminhos]
atualizado: 2026-11-24
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
